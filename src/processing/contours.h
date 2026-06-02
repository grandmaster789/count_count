#ifndef CC_PROCESSING_CONTOURS_H
#define CC_PROCESSING_CONTOURS_H

#include <vector>
#include <tuple>
#include <optional>

#include "types/point.h"
#include "types/image.h"
#include "types/tooth_measurement.h"

namespace cc::processing {
    struct ContourResult {
        std::vector<ToothMeasurement> m_Teeth;
        cc::Point2i                   m_Centroid;
        int                           m_SpeculativeCount = -1; // -1 = not available
    };

    std::optional<ContourResult> process_contours(
        const std::vector<std::vector<cc::Point2i>>& contours,
              cc::Image&                             output_image
    );
}

#endif
