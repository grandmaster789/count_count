#include "focus.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <vector>

namespace cc::processing {
    namespace {
        inline int luma(const uint8_t* px) {
            // BT.601 fixed-point: (R*77 + G*150 + B*29) >> 8. px is BGR.
            return (px[2] * 77 + px[1] * 150 + px[0] * 29) >> 8;
        }
        inline int max_channel(const uint8_t* px) {
            return std::max({(int)px[0], (int)px[1], (int)px[2]});
        }
    }

    FocusRoi roi_from_mask(const cc::Image& mask, int pad) {
        FocusRoi roi;
        if (mask.empty() || mask.channels() != 1) return roi;

        int min_x = mask.cols(), min_y = mask.rows(), max_x = -1, max_y = -1;
        for (int y = 0; y < mask.rows(); ++y) {
            const uint8_t* row = mask.ptr(y);
            for (int x = 0; x < mask.cols(); ++x) {
                if (row[x]) {
                    min_x = std::min(min_x, x); max_x = std::max(max_x, x);
                    min_y = std::min(min_y, y); max_y = std::max(max_y, y);
                }
            }
        }

        if (max_x < 0) {
            // empty mask -> central third of the image
            roi.x0 = mask.cols() / 3; roi.x1 = 2 * mask.cols() / 3;
            roi.y0 = mask.rows() / 3; roi.y1 = 2 * mask.rows() / 3;
            return roi;
        }

        roi.x0 = std::max(0, min_x - pad);
        roi.y0 = std::max(0, min_y - pad);
        roi.x1 = std::min(mask.cols() - 1, max_x + pad);
        roi.y1 = std::min(mask.rows() - 1, max_y + pad);
        return roi;
    }

    double focus_measure(const cc::Image& bgr, const FocusRoi& roi, int clip_threshold) {
        if (bgr.empty() || bgr.channels() != 3) return 0.0;

        // Clamp ROI to a 1px interior border (Sobel needs 8 neighbours).
        int x0 = std::max(1, roi.x0);
        int y0 = std::max(1, roi.y0);
        int x1 = std::min(bgr.cols() - 2, roi.x1);
        int y1 = std::min(bgr.rows() - 2, roi.y1);
        if (x1 < x0 || y1 < y0) return 0.0;

        double sum = 0.0;
        long   count = 0;

        for (int y = y0; y <= y1; ++y) {
            const uint8_t* r_prev = bgr.ptr(y - 1);
            const uint8_t* r_curr = bgr.ptr(y);
            const uint8_t* r_next = bgr.ptr(y + 1);

            for (int x = x0; x <= x1; ++x) {
                const uint8_t* p_tl = r_prev + (size_t)(x - 1) * 3;
                const uint8_t* p_tc = r_prev + (size_t)(x    ) * 3;
                const uint8_t* p_tr = r_prev + (size_t)(x + 1) * 3;
                const uint8_t* p_ml = r_curr + (size_t)(x - 1) * 3;
                const uint8_t* p_mr = r_curr + (size_t)(x + 1) * 3;
                const uint8_t* p_bl = r_next + (size_t)(x - 1) * 3;
                const uint8_t* p_bc = r_next + (size_t)(x    ) * 3;
                const uint8_t* p_br = r_next + (size_t)(x + 1) * 3;
                const uint8_t* p_mc = r_curr + (size_t)x * 3;

                // Exclude if any neighbour (or centre) is clipped/specular.
                int mx = std::max({ max_channel(p_tl), max_channel(p_tc), max_channel(p_tr),
                                    max_channel(p_ml), max_channel(p_mc), max_channel(p_mr),
                                    max_channel(p_bl), max_channel(p_bc), max_channel(p_br) });
                if (mx >= clip_threshold) continue;

                int tl = luma(p_tl), tc = luma(p_tc), tr = luma(p_tr);
                int ml = luma(p_ml),                  mr = luma(p_mr);
                int bl = luma(p_bl), bc = luma(p_bc), br = luma(p_br);

                int gx = -tl - 2 * ml - bl + tr + 2 * mr + br;
                int gy = -tl - 2 * tc - tr + bl + 2 * bc + br;
                sum += std::abs(gx) + std::abs(gy);
                ++count;
            }
        }

        return count > 0 ? sum / (double)count : 0.0;
    }

    long find_best_focus(
        long min_focus, long max_focus, long step,
        const std::function<double(long)>& evaluate,
        int coarse_positions)
    {
        if (max_focus <= min_focus) return min_focus;
        if (step <= 0) step = 1;
        if (coarse_positions < 2) coarse_positions = 2;

        std::map<long, double> cache;  // focus -> score, avoids re-evaluating
        auto eval = [&](long f) -> double {
            f = std::clamp(f, min_focus, max_focus);
            auto it = cache.find(f);
            if (it != cache.end()) return it->second;
            double s = evaluate(f);
            cache.emplace(f, s);
            return s;
        };

        // --- coarse global pass ---
        const long span = max_focus - min_focus;
        long  best_f = min_focus;
        double best_s = -std::numeric_limits<double>::infinity();
        for (int i = 0; i < coarse_positions; ++i) {
            long f = min_focus + (span * i) / (coarse_positions - 1);
            double s = eval(f);
            if (s > best_s) { best_s = s; best_f = f; }
        }

        // --- fine local pass: +/- one coarse interval around the coarse peak ---
        const long coarse_interval = std::max<long>(step, span / (coarse_positions - 1));
        long lo = std::clamp(best_f - coarse_interval, min_focus, max_focus);
        long hi = std::clamp(best_f + coarse_interval, min_focus, max_focus);
        for (long f = lo; f <= hi; f += step) {
            double s = eval(f);
            if (s > best_s) { best_s = s; best_f = f; }
        }

        return best_f;
    }
}
