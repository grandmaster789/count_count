#include "gear_template.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace cc::processing {
    namespace {
        // Largest radius that still stays inside the image from `center`.
        double max_in_image_radius(const cc::Image& mask, cc::Point2i c) {
            double left   = c.x;
            double top    = c.y;
            double right  = mask.cols() - 1 - c.x;
            double bottom = mask.rows() - 1 - c.y;
            return std::max(0.0, std::min({left, top, right, bottom}));
        }

        double percentile(std::vector<double> v, double p) {
            if (v.empty()) return 0.0;
            std::sort(v.begin(), v.end());
            return v[(size_t)std::clamp(p * (v.size() - 1), 0.0, (double)(v.size() - 1))];
        }
    }

    GearFit fit_gear_template(const cc::Image& mask, cc::Point2i center, int teeth, int angular_samples) {
        GearFit fit;
        fit.teeth = teeth;
        if (teeth < 4 || mask.empty() || mask.channels() != 1 || angular_samples < 16)
            return fit;

        const int    n     = angular_samples;
        const double r_max  = max_in_image_radius(mask, center);
        const double r_step = 1.0;
        if (r_max < 8.0) return fit;

        // Ray-cast: per angle, record innermost and outermost foreground radius, and
        // sample the mask along the ray onto a fixed radius grid (reused for IoU so
        // the phase/duty sweep never re-touches the image).
        const int n_r = (int)(r_max / r_step) + 1;
        std::vector<double>  r_outer((size_t)n, 0.0), r_inner((size_t)n, 0.0);
        std::vector<uint8_t> actual((size_t)n * n_r, 0);  // [angle][radius idx]
        std::vector<double>  cosv((size_t)n), sinv((size_t)n);

        int fg_rays = 0;
        for (int a = 0; a < n; ++a) {
            double th = -std::numbers::pi + 2.0 * std::numbers::pi * a / n;
            double ct = std::cos(th), st = std::sin(th);
            cosv[(size_t)a] = ct; sinv[(size_t)a] = st;
            double inner = 0.0, outer = 0.0;
            uint8_t* arow = actual.data() + (size_t)a * n_r;
            for (int ri = 0; ri < n_r; ++ri) {
                double rr = ri * r_step;
                int x = (int)std::lround(center.x + rr * ct);
                int y = (int)std::lround(center.y + rr * st);
                if (x < 0 || y < 0 || x >= mask.cols() || y >= mask.rows()) break;
                if (mask.at(y, x)[0]) {
                    arow[ri] = 1;
                    if (inner == 0.0) inner = rr;
                    outer = rr;
                }
            }
            r_inner[(size_t)a] = inner;
            r_outer[(size_t)a] = outer;
            if (outer > 0) ++fg_rays;
        }
        if (fg_rays < n / 2) return fit;  // less than half the rays hit the object

        // Rim geometry from the outer profile; inner annulus edge from the inner profile.
        std::vector<double> nz_outer, nz_inner;
        for (int a = 0; a < n; ++a)
            if (r_outer[(size_t)a] > 0) { nz_outer.push_back(r_outer[(size_t)a]); nz_inner.push_back(r_inner[(size_t)a]); }
        double r_tip   = percentile(nz_outer, 0.95);
        double r_root  = percentile(nz_outer, 0.15);
        double r_in    = percentile(nz_inner, 0.50);  // robust annulus inner edge (≈0 for solid discs)
        if (r_tip - r_root < 1.0) r_root = std::max(0.0, r_tip - 2.0);
        if (r_in >= r_root) r_in = std::max(0.0, r_root - 2.0);

        // Region IoU between the fitted filled template and the mask over the gear's
        // radial extent [r_in, r_tip]. The template is foreground iff r_in <= r <=
        // R(theta), R(theta) = tip on a tooth, root in a gap. The "body" band
        // [r_in, r_root) is template-foreground for every phase/duty, so its
        // contribution is constant; only the thin tooth band [r_root, r_tip] flips
        // with the template. Precompute per-angle foreground sums once, then the
        // phase/duty sweep is O(angles) instead of O(angles * radii).
        const int ri_lo   = std::max(0, (int)std::floor(r_in / r_step));
        const int ri_root = std::clamp((int)std::lround(r_root / r_step), ri_lo, n_r - 1);
        const int ri_hi   = std::min(n_r - 1, (int)std::ceil(r_tip / r_step));
        const long body_cells_per_angle = std::max(0, ri_root - ri_lo);
        const long band_cells_per_angle = std::max(0, ri_hi - ri_root + 1);

        std::vector<long> act_band((size_t)n, 0);
        long inter_body = 0;
        for (int a = 0; a < n; ++a) {
            const uint8_t* arow = actual.data() + (size_t)a * n_r;
            for (int ri = ri_lo; ri < ri_root; ++ri) inter_body += arow[ri];      // body: template=1
            long b = 0;
            for (int ri = ri_root; ri <= ri_hi; ++ri) b += arow[ri];
            act_band[(size_t)a] = b;
        }
        const long uni_body = (long)n * body_cells_per_angle; // body template=1 everywhere

        const double period = 2.0 * std::numbers::pi / teeth;
        const int    phase_steps = 48;
        double best_iou = 0.0, best_phase = 0.0, best_duty = 0.5;
        for (double duty : {0.40, 0.50, 0.60}) {
            for (int ps = 0; ps < phase_steps; ++ps) {
                double phi = period * ps / phase_steps;
                long inter = inter_body, uni = uni_body;
                for (int a = 0; a < n; ++a) {
                    double th = -std::numbers::pi + 2.0 * std::numbers::pi * a / n;
                    double frac = std::fmod((th - phi) + 8.0 * std::numbers::pi, period) / period;
                    if (frac < duty) {                 // tooth: template fills the whole band
                        inter += act_band[(size_t)a];
                        uni   += band_cells_per_angle;
                    } else {                           // gap: template empty in band
                        uni   += act_band[(size_t)a];  // any actual foreground is template-miss
                    }
                }
                double iou = uni ? (double)inter / uni : 0.0;
                if (iou > best_iou) { best_iou = iou; best_phase = phi; best_duty = duty; }
            }
        }

        fit.valid = true;
        fit.score = std::clamp(best_iou, 0.0, 1.0);

        // Fitted template outline as a closed polyline (outer toothed boundary).
        const int outline_n = std::max(angular_samples, teeth * 8);
        fit.outline.reserve((size_t)outline_n);
        for (int a = 0; a < outline_n; ++a) {
            double th   = -std::numbers::pi + 2.0 * std::numbers::pi * a / outline_n;
            double frac = std::fmod((th - best_phase) + 8.0 * std::numbers::pi, period) / period;
            double rr   = (frac < best_duty) ? r_tip : r_root;
            fit.outline.push_back({
                (int)std::lround(center.x + rr * std::cos(th)),
                (int)std::lround(center.y + rr * std::sin(th))
            });
        }
        return fit;
    }
}
