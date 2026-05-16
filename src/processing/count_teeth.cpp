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

        // Remove noise artifacts: teeth whose span of high-points is implausibly narrow
        // compared to the median across all detected teeth.
        if (teeth.size() >= 3) {
            size_t N = tooth_mask.size();

            std::vector<size_t> spans;
            spans.reserve(teeth.size());
            for (const auto& t : teeth) {
                size_t start = (t.m_LowHighTransitionIdx + 1) % N;
                size_t end   = t.m_HighLowTransitionIdx;
                spans.push_back((end >= start) ? (end - start + 1) : (N - start + end + 1));
            }

            auto sorted_spans = spans;
            std::sort(sorted_spans.begin(), sorted_spans.end());
            size_t median_span = sorted_spans[sorted_spans.size() / 2];
            size_t min_span    = std::max(size_t(1), median_span / 3);

            std::vector<ToothMeasurement> kept;
            kept.reserve(teeth.size());
            for (size_t i = 0; i < teeth.size(); ++i) {
                if (spans[i] >= min_span) {
                    kept.push_back(teeth[i]);
                    kept.back().m_ToothIdx = kept.size();
                }
            }
            teeth = std::move(kept);
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

        // Search over integer tooth counts N directly.  For each N, the implied
        // pitch is exactly 2π/N — satisfying the physical constraint that N teeth
        // fill one full revolution.  Each observed spacing must then be a positive
        // integer multiple of that pitch (k=1 for adjacent detected teeth, k>1
        // where teeth were merged or missed by the direct counter).
        //
        // Searching over integer N rather than over pitch values makes sub-harmonic
        // collapse implicit: N=36 is considered independently of N=72, and because
        // single-pitch spacings map to k=0.5 for N=36 (rejected), N=72 naturally
        // collects more inliers.  When inlier counts tie (e.g. N=72 vs N=144 on a
        // perfect gear), the smaller total residual of N=72 breaks the tie — each
        // spacing is 2× as far from its nearest integer multiple when seen through
        // the N=144 pitch.
        constexpr double k_Tolerance = 0.25; // fraction of candidate pitch
        constexpr int    k_MaxMissed = 5;    // max consecutive missed teeth in one gap

        double min_spacing = *std::min_element(spacings.begin(), spacings.end());
        double max_spacing = *std::max_element(spacings.begin(), spacings.end());

        int N_min = std::max(2, static_cast<int>(std::floor(2.0 * std::numbers::pi / max_spacing)));
        int N_max = std::min(500,
                        static_cast<int>(std::ceil(2.0 * std::numbers::pi / min_spacing)) * k_MaxMissed);

        int    best_N        = static_cast<int>(teeth.size());
        int    best_inliers  = 0;
        double best_residual = std::numeric_limits<double>::max();

        for (int N = N_min; N <= N_max; ++N) {
            double pitch = 2.0 * std::numbers::pi / static_cast<double>(N);

            int    inliers  = 0;
            double residual = 0.0;
            for (double s : spacings) {
                double nearest = std::round(s / pitch);
                double err     = std::abs(s / pitch - nearest);
                if (nearest >= 1.0 && err < k_Tolerance) {
                    ++inliers;
                    residual += err;
                }
            }

            if (inliers > best_inliers ||
                (inliers == best_inliers && residual < best_residual))
            {
                best_N        = N;
                best_inliers  = inliers;
                best_residual = residual;
            }
        }

        return best_N;
    }
}
