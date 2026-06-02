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

    // Estimates tooth count by FFT of the radial distance profile.
    // Resamples the contour to a uniform angular grid, finds the dominant
    // frequency in the power spectrum, and maps it to a tooth count.
    // Returns -1 if the contour is too small or covers less than half a revolution.
    int fft_tooth_count(
        const std::vector<cc::Point2i>& contour,
        const std::vector<double>&      distances,
        const cc::Point2f&              centroid
    );
}

#endif
