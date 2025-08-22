#ifndef CC_PROCESSING_FOREGROUND_H
#define CC_PROCESSING_FOREGROUND_H

#include <opencv2/opencv.hpp>

namespace cc::processing {
    void determine_foreground(
        const cv::Scalar& selected_color,
              int         tolerance_range,
        const cv::Mat&    source_image,
              cv::Mat&    foreground_mask,
              cv::Mat&    foreground
    );
}

#endif