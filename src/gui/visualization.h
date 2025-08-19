#ifndef CC_GUI_VISUALIZATION_H
#define CC_GUI_VISUALIZATION_H

#include <opencv2/opencv.hpp>

#include "types/tooth_measurement.h"

namespace cc {
    void draw_gear_arrow(
              cv::Mat&     output_image,
        const cv::Point2d& gear_center,
        double             gear_radius,
        double             angle,
        const cv::Scalar&  color     = cv::Scalar(255, 255, 127),
        int                thickness = 3
    );

    void display_results(
        size_t                               tooth_count,
        cv::Point2i                          centroid_i,
        const std::vector<ToothMeasurement>& teeth,
        const std::vector<uint8_t>&          tooth_anomaly_mask,
        cv::Mat&                             output_image
    );
}

#endif