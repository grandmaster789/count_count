#ifndef CC_PROCESSING_CONTOURS_H
#define CC_PROCESSING_CONTOURS_H

#include <vector>
#include <tuple>
#include <optional>

#include <opencv2/opencv.hpp>

#include "types/tooth_measurement.h"

namespace cc::processing {
    struct ContourResult {
        std::vector<ToothMeasurement> m_Teeth;
        cv::Point2i                   m_Centroid;
    };

    std::optional<ContourResult> process_contours(
        const std::vector<std::vector<cv::Point>>& contours,
        const std::vector<cv::Vec4i>&              hierarchy,
              cv::Mat&                             output_image
    );
}

#endif