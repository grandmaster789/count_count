#ifndef CC_PROCESSING_COUNT_TEETH_H
#define CC_PROCESSING_COUNT_TEETH_H

#include <optional>
#include <vector>
#include <cstdint>

#include "types/point.h"
#include "types/tooth_measurement.h"

namespace cc::processing {
    std::optional<size_t> find_tooth_start(const std::vector<uint8_t>& mask);

    std::vector<ToothMeasurement> count_teeth(
              size_t                      first_tooth,
        const std::vector<uint8_t>&       tooth_mask,
        const std::vector<cc::Point2i>&   largest_contour,
        const std::vector<double>&        distances,
        const cc::Point2f&                centroid_f
    );

    // Estimates tooth count from the angular pitch of detected teeth.
    // Takes the P25 of consecutive angular spacings as estimated pitch, then
    // returns round(2π / pitch). More reliable than the direct count when the
    // distance threshold merges adjacent teeth. Returns -1 if fewer than 2
    // teeth are provided.
    int estimate_tooth_count(const std::vector<ToothMeasurement>& teeth);
}

#endif
