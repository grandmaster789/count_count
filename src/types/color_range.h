#ifndef CC_TYPES_COLOR_RANGE_H
#define CC_TYPES_COLOR_RANGE_H

#include <iosfwd>

#include "color.h"

namespace cc {
    struct ColorRange {
        Color3 m_MinRGB = { 0x00, 0x00, 0x00 };
        Color3 m_MaxRGB = { 0xFF, 0xFF, 0xFF };

        friend std::ostream& operator << (std::ostream& os, const ColorRange& cr);
    };

    ColorRange determine_color_range(
        const Color3& selected_color,
        int           tolerance_range
    );
}

#endif
