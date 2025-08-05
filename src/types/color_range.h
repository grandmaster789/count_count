#ifndef TYPES_COLOR_RANGE_H
#define TYPES_COLOR_RANGE_H

#include "rgb.h"
#include <iosfwd>

namespace cc {
    struct ColorRange {
        RGB m_MinRGB = { 0x00, 0x00, 0x00 };
        RGB m_MaxRGB = { 0xFF, 0xFF, 0xFF };

        friend std::ostream& operator << (std::ostream& os, const ColorRange& cr);
    };

    ColorRange determine_color_range(
        const RGB& selected_color,
        int        tolerance_range
    );
}

#endif
