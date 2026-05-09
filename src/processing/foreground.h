#ifndef CC_PROCESSING_FOREGROUND_H
#define CC_PROCESSING_FOREGROUND_H

#include "types/image.h"
#include "types/color.h"

namespace cc::processing {
    // All output buffers (foreground_mask, foreground, blur_temp) must be
    // pre-allocated to the correct dimensions before calling this function.
    // When invert=true the mask is flipped after blurring, so selected_color
    // targets the background and the output mask covers the foreground object.
    // When use_chebyshev=true the per-pixel test uses BGR Chebyshev distance
    // <= tolerance_range directly, bypassing HSV conversion.  This matches the
    // units returned by detect_background_sensitivity().
    void determine_foreground(
        const cc::Color3& selected_color,
              int         tolerance_range,
        const cc::Image&  source_image,
              cc::Image&  foreground_mask,
              cc::Image&  foreground,
              cc::Image&  blur_temp,
              bool        invert         = false,
              bool        use_chebyshev  = false
    );
}

#endif
