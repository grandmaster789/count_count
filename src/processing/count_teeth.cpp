#include "count_teeth.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace cc::processing {
    // find the first (low) position where the mask changes from low to high
    std::optional<size_t> find_tooth_start(const std::vector<uint8_t>& mask) {
        if (mask.empty())
            return std::nullopt;

        for (size_t i = 0; i < mask.size(); ++i)
            if (!mask[i] && mask[(i + 1) % mask.size()])
                return i;

        // super edge case where there is just one change, and it's right at the end of the list
        if (!mask.back() && mask.front())
            return mask.size() - 1;

        return std::nullopt;
    };

    // Here we figure out how often the threshold is crossed to determine a tooth count
    // -- only count the 'rising' edges to establish a count
    // -- also figure out some tooth measurements
    std::vector<ToothMeasurement> count_teeth(
              size_t                         first_tooth_idx,
        const std::vector<uint8_t>&          tooth_mask,
        const std::vector<cc::Point2i>&      largest_contour,
        const std::vector<double>&           distances,
        const cc::Point2f&                   centroid_f
    ) {
        int tooth_count = 0;

        std::vector<ToothMeasurement> teeth;

        // from the first position, iterate over the entire set and collect measurements during traversal
        for (size_t i = first_tooth_idx; i < first_tooth_idx + tooth_mask.size(); ++i) {
            uint8_t current_mask_value = tooth_mask[i % tooth_mask.size()];
            uint8_t next_mask_value    = tooth_mask[(i + 1) % tooth_mask.size()];

            // count rising edges as the start of a tooth
            // the algorithm starts at a position where this is the case
            if (!current_mask_value && next_mask_value) {
                ++tooth_count;

                cc::ToothMeasurement new_measurement;
                new_measurement.m_LowHighTransitionIdx = i % tooth_mask.size();
                new_measurement.m_ToothIdx             = tooth_count;
                new_measurement.m_StartingAngle        = std::atan2f(
                    largest_contour[new_measurement.m_LowHighTransitionIdx].y - centroid_f.y,
                    largest_contour[new_measurement.m_LowHighTransitionIdx].x - centroid_f.x
                );

                teeth.push_back(new_measurement);
            }

            // when we transition from high to low, we have found the end of a tooth and can complete the measurement
            if (current_mask_value && !next_mask_value) {
                auto& measurement = teeth.back();

                measurement.m_HighLowTransitionIdx = i % tooth_mask.size();
                measurement.m_EndingAngle = std::atan2f(
                    largest_contour[measurement.m_HighLowTransitionIdx].y - centroid_f.y,
                    largest_contour[measurement.m_HighLowTransitionIdx].x - centroid_f.x
                );

                // wrap angles to [0, 2pi]
                if (measurement.m_StartingAngle < 0)
                    measurement.m_StartingAngle += 2 * std::numbers::pi;
                if (measurement.m_EndingAngle < 0)
                    measurement.m_EndingAngle += 2 * std::numbers::pi;

                measurement.m_MinDistance =  std::numeric_limits<double>::max();
                measurement.m_MaxDistance = -std::numeric_limits<double>::max();

                // find the min and max distances for this tooth
                // walk forward from (LowHigh+1) to HighLow, wrapping around if needed
                {
                    size_t start = (measurement.m_LowHighTransitionIdx + 1) % tooth_mask.size();
                    size_t end   = measurement.m_HighLowTransitionIdx;
                    size_t n     = tooth_mask.size();
                    size_t span  = (end >= start) ? (end - start + 1) : (n - start + end + 1);

                    size_t j = start;
                    for (size_t step = 0; step < span; ++step) {
                        if (distances[j] < measurement.m_MinDistance)
                            measurement.m_MinDistance = distances[j];
                        if (distances[j] > measurement.m_MaxDistance)
                            measurement.m_MaxDistance = distances[j];
                        j = (j + 1) % n;
                    }
                }
            }
        }

        return teeth;
    }

    int estimate_tooth_count(const std::vector<ToothMeasurement>& teeth) {
        if (teeth.size() < 2)
            return -1;

        std::vector<double> angles;
        angles.reserve(teeth.size());
        for (const auto& t : teeth)
            angles.push_back(t.m_StartingAngle);
        std::sort(angles.begin(), angles.end());

        std::vector<double> spacings;
        spacings.reserve(angles.size());
        for (size_t i = 0; i < angles.size(); ++i) {
            double delta = angles[(i + 1) % angles.size()] - angles[i];
            if (delta <= 0.0) delta += 2.0 * std::numbers::pi;
            spacings.push_back(delta);
        }

        // RANSAC-style pitch estimation.
        //
        // For each observed spacing s (and sub-harmonics s/k), treat it as a candidate
        // pitch. Count inliers: other spacings within 25% of a positive integer multiple
        // of that pitch. The true pitch explains ALL spacings (as multiples 1×, 2×, 3×...),
        // while a spurious sub-pitch only fits the handful of spacings near that value.
        //
        // Tie-breaking by larger pitch prevents a sub-harmonic (e.g. pitch/2) from
        // winning when it ties with the true pitch on inlier count.
        constexpr int    k_MaxMultiple = 5;
        constexpr double k_Tolerance   = 0.25; // fraction of candidate pitch

        double best_pitch   = spacings[0];
        int    best_inliers = 0;

        for (double s : spacings) {
            for (int k = 1; k <= k_MaxMultiple; ++k) {
                double candidate = s / static_cast<double>(k);
                if (candidate < 1e-6) continue;

                int inliers = 0;
                for (double sj : spacings) {
                    double nearest = std::round(sj / candidate);
                    if (nearest >= 1.0 && std::abs(sj / candidate - nearest) < k_Tolerance)
                        ++inliers;
                }

                // More inliers wins; ties broken by larger pitch (= fewer teeth, more conservative)
                if (inliers > best_inliers || (inliers == best_inliers && candidate > best_pitch)) {
                    best_inliers = inliers;
                    best_pitch   = candidate;
                }
            }
        }

        return static_cast<int>(std::round(2.0 * std::numbers::pi / best_pitch));
    }
}
