#ifndef CC_PROCESSING_WHITE_BALANCE_H
#define CC_PROCESSING_WHITE_BALANCE_H

#include "types/image.h"

namespace cc::processing {
    // Colour-cast metric for auto white balance: |medianR - medianG| +
    // |medianG - medianB| over the (sub-sampled) BGR frame. 0 = neutral.
    //
    // Uses the per-channel MEDIAN, not the mean, so a small strongly-chromatic
    // object (e.g. a gold gear, ~5% of the frame) cannot bias the target the way
    // gray-world-on-the-mean does — the large neutral background sets the median.
    // Minimising this over a sweep of camera white-balance values finds the value
    // that neutralises the scene, without trusting the driver's (stale-in-Auto)
    // white-balance read-back.
    //
    // Assumes the neutral background is the MAJORITY of the frame (the median is the
    // background colour). If a chromatic object ever fills >50% of the frame the
    // median becomes the object's colour and this would white-balance it to grey.
    double color_imbalance(const cc::Image& bgr, int subsample = 4);
}

#endif
