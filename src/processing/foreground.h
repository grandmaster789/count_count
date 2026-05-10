#ifndef CC_PROCESSING_FOREGROUND_H
#define CC_PROCESSING_FOREGROUND_H

#include "types/image.h"
#include "types/color.h"

namespace cc::processing {
    // Write 255 to foreground_mask where the BGR Chebyshev distance from
    // selected_color is <= tolerance_range, 0 elsewhere. No blur or other
    // post-processing. foreground_mask must be pre-allocated (1 channel,
    // same dimensions as source_image).
    void chebyshev_threshold(
        const cc::Color3& selected_color,
              int         tolerance_range,
        const cc::Image&  source_image,
              cc::Image&  foreground_mask
    );

    // Flip every byte in foreground_mask: 0→255, 255→0.
    void invert_mask(cc::Image& foreground_mask);

    // Apply a 9×9 majority-vote box blur to a 1-channel 0/255 mask in-place.
    // Interior pixels are processed with AVX2 int32 batches; border rows and
    // the scalar tail use the same integral-image formula as the scalar path.
    // scratch must be pre-allocated to the same dimensions as mask.
    void majority_vote_blur(cc::Image& mask, cc::Image& scratch);

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
