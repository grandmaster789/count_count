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
}

#endif
