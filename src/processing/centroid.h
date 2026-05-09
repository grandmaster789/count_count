#ifndef CC_PROCESSING_CENTROID_H
#define CC_PROCESSING_CENTROID_H

#include <tuple>
#include <vector>

#include "types/point.h"

namespace cc::processing {
    std::tuple<
        cc::Point2d,
        cc::Point2f,
        cc::Point2i
    > find_centroid(
        const std::vector<cc::Point2i>& contour
    );
}

#endif
