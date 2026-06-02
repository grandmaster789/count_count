#ifndef CC_PROCESSING_AUTO_SENSITIVITY_H
#define CC_PROCESSING_AUTO_SENSITIVITY_H

#include "types/image.h"
#include "types/color.h"

namespace cc::processing {
    struct AutoSensitivityResult {
        cc::Color3 color;
        int        tolerance;
        bool       valid;       // true if detection produced usable values
    };

    AutoSensitivityResult detect_sensitivity(const cc::Image& source_image);

    // Variant for background-subtraction mode: targets the dominant (background) color
    // directly rather than suppressing it to find the foreground peak.
    AutoSensitivityResult detect_background_sensitivity(const cc::Image& source_image);

    // Auto saturation threshold for determine_foreground_by_saturation().
    // Locates the low-saturation background peak in the S histogram and returns a
    // threshold just above its shoulder. Deliberately NOT Otsu-on-S: when the
    // chromatic object is a small fraction of pixels (~5%), Otsu overshoots into
    // the object's own distribution and erodes low-saturation tooth tips. Returns
    // a value clamped to a sane range; falls back to a default if no clear peak.
    int detect_saturation_threshold(const cc::Image& source_image);
}

#endif
