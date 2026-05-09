#ifndef CC_PROCESSING_EDGE_MASK_H
#define CC_PROCESSING_EDGE_MASK_H

#include "types/image.h"

namespace cc::processing {
    // Build a foreground mask from gradient edges instead of colour thresholding.
    // Robust to lighting changes and coloured backgrounds where determine_foreground()
    // struggles.
    //
    // Pipeline: BGR → luma → Sobel magnitude → percentile threshold → morphological
    // close → border-flood fill. The resulting mask is 255 inside closed edge
    // regions (the gear body) and 0 elsewhere, so it plugs directly into
    // find_contours() as a drop-in replacement for the colour mask.
    //
    // sensitivity is the same 0..100-ish slider the colour path uses; higher keeps
    // more edges.
    //
    // Both output buffers must be pre-allocated to source_image's dimensions.
    void determine_foreground_by_edges(
              int         sensitivity,
        const cc::Image&  source_image,
              cc::Image&  foreground_mask,
              cc::Image&  foreground
    );
}

#endif
