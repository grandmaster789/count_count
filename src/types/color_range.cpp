#include "color_range.h"

namespace cc {
    ColorRange determine_color_range(
        const RGB& selected_color,
        int        tolerance_range
    ) {
        ColorRange result;

        for (int i = 0; i < 3; ++i) {
            int lower = selected_color[i] - (tolerance_range / 2);
            int upper = selected_color[i] + (tolerance_range / 2);

            if (lower < 0)
                lower = 0;

            if (upper > 255)
                upper = 255;

            result.m_MinRGB[i] = static_cast<uint8_t>(lower);
            result.m_MaxRGB[i] = static_cast<uint8_t>(upper);
        }

        return result;
    }
}