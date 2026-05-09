#ifndef CC_PROCESSING_BOUNDARY_TRACE_H
#define CC_PROCESSING_BOUNDARY_TRACE_H

#include <vector>

#include "types/image.h"
#include "types/point.h"

namespace cc::processing {
    // Find contours in a binary (single-channel) mask image.
    // Returns all external contours as vectors of points.
    // Equivalent to cv::findContours with RETR_EXTERNAL + CHAIN_APPROX_NONE.
    std::vector<std::vector<cc::Point2i>> find_contours(const cc::Image& binary_mask);
}

#endif
