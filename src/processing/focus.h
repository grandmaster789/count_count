#ifndef CC_PROCESSING_FOCUS_H
#define CC_PROCESSING_FOCUS_H

#include <functional>

#include "types/image.h"

namespace cc::processing {
    // Inclusive pixel bounding box for a focus region of interest.
    struct FocusRoi {
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        [[nodiscard]] bool valid() const { return x1 > x0 && y1 > y0; }
    };

    // Bounding box of the foreground (non-zero) pixels of a 1-channel mask, with a
    // small padding. Falls back to the central third of the image if the mask is
    // empty, so a focus ROI is always available.
    FocusRoi roi_from_mask(const cc::Image& mask, int pad = 8);

    // Contrast-detection focus measure: the mean Sobel gradient magnitude
    // (|Gx|+|Gy|) on luma over `roi`. Higher = sharper. Pixels whose 3x3
    // neighbourhood contains a clipped/specular sample (max channel >=
    // clip_threshold) are excluded, so shiny highlights don't fake a sharp edge
    // when defocused. Returns 0 if there are no valid pixels.
    double focus_measure(const cc::Image& bgr, const FocusRoi& roi, int clip_threshold = 250);

    // Coarse-global then fine-local search for the focus value in [min_focus,
    // max_focus] (granularity `step`) that maximizes `evaluate`. `evaluate(focus)`
    // returns a sharpness score; the camera is injected by the caller. The global
    // coarse pass deliberately avoids the local-maxima trap that defeats
    // hill-climbing autofocus. Returns the best focus value found (min_focus if the
    // range is degenerate).
    long find_best_focus(
        long                                min_focus,
        long                                max_focus,
        long                                step,
        const std::function<double(long)>&  evaluate,
        int                                 coarse_positions = 20
    );
}

#endif
