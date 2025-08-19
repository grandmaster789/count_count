#include "foreground.h"
#include "types/color_range.h"

namespace cc::processing {
    void determine_foreground(
        const RGB&     selected_color,
              int      tolerance_range,
        const cv::Mat& source_image,
              cv::Mat& foreground_mask,
              cv::Mat& foreground
    ) {
        foreground_mask = cv::Mat::zeros(source_image.size(), CV_8UC1);

        auto [min_rgb, max_rgb] = determine_color_range(selected_color, tolerance_range);

        cv::inRange(
            source_image,
            min_rgb,
            max_rgb,
            foreground_mask
        );

        // apply slight blur to get rid of noise and small details
        cv::medianBlur(foreground_mask, foreground_mask, 9);

        foreground = cv::Mat::zeros(source_image.size(), CV_8UC3);

        cv::copyTo(
            source_image,
            foreground,
            foreground_mask
        );
    }
}