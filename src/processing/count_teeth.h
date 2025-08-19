#ifndef CC_PROCESSING_COUNT_TEETH_H
#define CC_PROCESSING_COUNT_TEETH_H

#include <optional>
#include <vector>
#include <cstdint>

#include <opencv2/opencv.hpp>

#include "types/tooth_measurement.h"

namespace cc::processing {
    std::optional<size_t> find_tooth_start(const std::vector<uint8_t>& mask);

    std::vector<ToothMeasurement> count_teeth(
              size_t                         first_tooth,
        const std::vector<uint8_t>&          tooth_mask,
        const std::vector<cv::Point>&        largest_contour,
        const std::vector<double>&           distances,
        const cv::Point2f&                   centroid_f
    );
}

#endif