#include "count_teeth.h"

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
        const std::vector<cv::Point>&        largest_contour,
        const std::vector<double>&           distances,
        const cv::Point2f&                   centroid_f
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

                // because of the wrapping structure, this is actually challenging for std::minmax_element...
                measurement.m_MinDistance =  std::numeric_limits<double>::max();
                measurement.m_MaxDistance = -std::numeric_limits<double>::max();

                // find the min and max distances for this tooth
                // at the low->high transition index the distance should still be low, so start at the next index
                // at the high->low transition index the distance should still be high
                for (
                    size_t j = (measurement.m_LowHighTransitionIdx + 1) % tooth_mask.size();
                           j <= measurement.m_HighLowTransitionIdx;
                    ++j, j %= tooth_mask.size()
                ) {
                    if (distances[j] < measurement.m_MinDistance)
                        measurement.m_MinDistance = distances[j];

                    if (distances[j] > measurement.m_MaxDistance)
                        measurement.m_MaxDistance = distances[j];
                }
            }
        }

        return teeth;
    }
}