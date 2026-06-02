#ifndef CC_PROCESSING_GEAR_TEMPLATE_H
#define CC_PROCESSING_GEAR_TEMPLATE_H

#include <vector>

#include "types/image.h"
#include "types/point.h"

namespace cc::processing {
    // Result of fitting an idealized gear model to an observed rim.
    struct GearFit {
        bool                     valid   = false;
        double                   score   = 0.0;  // [0,1] goodness of fit
        int                      teeth   = -1;
        std::vector<cc::Point2i> outline;        // closed polyline of the fitted template
    };

    // Analysis-by-synthesis goodness-of-fit. Samples the rim radial profile r(theta)
    // from `mask` around `center` (ray-cast, bypassing the contour), then fits an
    // ideal `teeth`-tooth square-wave rim, optimizing phase and duty. The score is
    // the best normalized cross-correlation between the measured profile and the
    // ideal rim, clamped to [0,1]: ~1 = a clean periodic gear rim, ~0 = no periodic
    // structure (non-gear / bad segmentation). `outline` is the fitted template as a
    // closed polyline for overlay rendering. Shape-agnostic: works for solid discs
    // and spoked rings alike because it only looks at the outer rim profile.
    //
    // Returns valid=false (score 0) when teeth < 4 or the mask has no usable rim.
    GearFit fit_gear_template(
        const cc::Image& mask,
        cc::Point2i      center,
        int              teeth,
        int              angular_samples = 720
    );
}

#endif
