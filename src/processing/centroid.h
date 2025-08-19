#ifndef CC_PROCESSING_CENTROID_H
#define CC_PROCESSING_CENTROID_H

#include <tuple>
#include <vector>
#include <opencv2/opencv.hpp>

namespace cc::processing {
    std::tuple<
        cv::Point2d,
        cv::Point2f,
        cv::Point2i
    > find_centroid(
        const std::vector<std::vector<cv::Point>>& contours,
        int                                        largest_component_idx
    );
}

#endif