#ifndef TYPES_COLOR_RANGE_H
#define TYPES_COLOR_RANGE_H

#include <iosfwd>

#include <opencv2/opencv.hpp>

namespace cc {
    struct ColorRange {
        cv::Scalar m_MinRGB = { 0x00, 0x00, 0x00 };
        cv::Scalar m_MaxRGB = { 0xFF, 0xFF, 0xFF };

        friend std::ostream& operator << (std::ostream& os, const ColorRange& cr);
    };

    ColorRange determine_color_range(
        const cv::Scalar& selected_color,
        int               tolerance_range
    );
}

#endif
