#include "contours.h"

#include "centroid.h"
#include "count_teeth.h"
#include "anomalies.h"

#include "math/geometry.h"
#include "math/statistics.h"
#include "types/tooth_measurement.h"
#include "util/logger.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cc::processing {
    namespace {
        // Radial consistency score: mean(r) / std_dev(r) of point distances from the
        // contour centroid. Perfect circle = ∞, shallow-toothed gear ≈ 10..30, square ≈ 6,
        // elongated blob ≈ 2. Higher = rounder.
        double radial_consistency(const std::vector<cc::Point2i>& contour) {
            if (contour.size() < 8) return 0.0;

            double cx = 0.0, cy = 0.0;
            for (const auto& p : contour) { cx += p.x; cy += p.y; }
            cx /= static_cast<double>(contour.size());
            cy /= static_cast<double>(contour.size());

            double sum = 0.0, sum_sq = 0.0;
            for (const auto& p : contour) {
                double dx = p.x - cx;
                double dy = p.y - cy;
                double r  = std::hypot(dx, dy);
                sum    += r;
                sum_sq += r * r;
            }
            double n    = static_cast<double>(contour.size());
            double mean = sum / n;
            if (mean <= 0.0) return 0.0;

            double variance = std::max(0.0, (sum_sq / n) - mean * mean);
            double stddev   = std::sqrt(variance);
            if (stddev < 1e-6) return std::numeric_limits<double>::infinity();

            return mean / stddev;
        }

        int pick_most_circular(const std::vector<std::vector<cc::Point2i>>& contours, double min_area) {
            int idx = -1;
            double best_score = 0.0;
            for (size_t i = 0; i < contours.size(); ++i) {
                double a = cc::math::polygon_area(contours[i]);
                if (a < min_area) continue;

                double score = radial_consistency(contours[i]);
                if (score > best_score) {
                    best_score = score;
                    idx = static_cast<int>(i);
                }
            }
            return idx;
        }
    }

    std::optional<ContourResult> process_contours(
        const std::vector<std::vector<cc::Point2i>>& all_contours,
              cc::Image&                             output_image
    ) {
        // minimum area filter: contour must be at least 1% of image area
        double image_area = static_cast<double>(output_image.rows()) * output_image.cols();
        double min_area   = image_area * 0.01;

        LOG_DEBUG("contours: {} candidates, image area={:.0f}, min_area={:.0f}",
                  all_contours.size(), image_area, min_area);

        for (size_t i = 0; i < all_contours.size(); ++i) {
            double a = cc::math::polygon_area(all_contours[i]);
            LOG_DEBUG("  contour[{}]: {} pts, area={:.0f}{}", i, all_contours[i].size(), a,
                      a < min_area ? " (below threshold)" : "");
        }

        int chosen_idx = pick_most_circular(all_contours, min_area);

        if (chosen_idx < 0) {
            LOG_DEBUG("contours: no contour passed the selector");
            return std::nullopt;
        }

        const auto& largest_contour = all_contours[chosen_idx];
        LOG_DEBUG("contours: selected contour[{}] ({} pts)", chosen_idx, largest_contour.size());

        // draw contour onto output image (red)
        for (size_t i = 0; i < largest_contour.size(); ++i) {
            auto& pt = largest_contour[i];
            if (pt.y >= 0 && pt.y < output_image.rows() && pt.x >= 0 && pt.x < output_image.cols()) {
                uint8_t* pixel = output_image.at(pt.y, pt.x);
                pixel[0] = 0;   // B
                pixel[1] = 0;   // G
                pixel[2] = 255; // R
            }
        }

        // find centroid of the contour
        auto [centroid_d, centroid_f, centroid_i] = find_centroid(largest_contour);

        // loop over the largest contour, collect 'similar' distances to the center point
        std::vector<double> distances;
        for (const auto &pt: largest_contour) {
            auto distance = std::hypot(
                pt.x - centroid_i.x,
                pt.y - centroid_i.y
            );

            distances.push_back(distance);
        }

        double dist_min = *std::min_element(distances.begin(), distances.end());
        double dist_max = *std::max_element(distances.begin(), distances.end());
        auto distance_threshold = cc::math::otsu_threshold(distances);

        LOG_DEBUG("contours: centroid=({},{}), dist min={:.1f} max={:.1f} threshold={:.1f} swing={:.1f}",
                  centroid_i.x, centroid_i.y,
                  dist_min, dist_max, distance_threshold, dist_max - dist_min);

        std::vector<uint8_t> tooth_mask(largest_contour.size(), 0);

        for (size_t i = 0; i < largest_contour.size(); ++i)
            tooth_mask[i] = (distances[i] >= distance_threshold) ? 1 : 0;

        int high_count = static_cast<int>(std::count(tooth_mask.begin(), tooth_mask.end(), 1));
        LOG_DEBUG("contours: tooth_mask high={} ({:.0f}% of {} pts)",
                  high_count,
                  100.0 * high_count / static_cast<double>(tooth_mask.size()),
                  tooth_mask.size());

        auto first_tooth = find_tooth_start(tooth_mask);
        if (!first_tooth) {
            LOG_DEBUG("contours: no tooth start found");
            return std::nullopt;
        }

        auto teeth = count_teeth(
            *first_tooth,
            tooth_mask,
            largest_contour,
            distances,
            centroid_f
        );

        int speculative = fft_tooth_count(largest_contour, distances, centroid_f);
        LOG_DEBUG("contours: direct={} speculative(fft)={}",
                  teeth.size(), speculative);

        return ContourResult {
            .m_Teeth           = std::move(teeth),
            .m_Centroid        = centroid_i,
            .m_SpeculativeCount = speculative
        };
    }
}
