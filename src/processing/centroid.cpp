#include "centroid.h"

namespace cc::processing {
    std::tuple<
        cv::Point2d,
        cv::Point2f,
        cv::Point2i
    > find_centroid(
        const std::vector<std::vector<cv::Point>>& contours,
        int                                        largest_component_idx
    ) {
        // https://docs.opencv.org/3.4/d3/dc0/group__imgproc__shape.html#ga556a180f43cab22649c23ada36a8a139
        auto moment = cv::moments(
            contours[largest_component_idx],
            false
        );
        auto centroid_d = cv::Point2d(
            moment.m10 / moment.m00,
            moment.m01 / moment.m00
        );

        auto centroid_f = cv::Point2f(
            static_cast<float>(centroid_d.x),
            static_cast<float>(centroid_d.y)
        );

        auto centroid_i = cv::Point2i(
            static_cast<int>(centroid_d.x),
            static_cast<int>(centroid_d.y)
        );

        return std::make_tuple(
            centroid_d,
            centroid_f,
            centroid_i
        );
    }
}