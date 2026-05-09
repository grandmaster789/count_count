#ifndef CC_GUI_VISUALIZATION_H
#define CC_GUI_VISUALIZATION_H

#include "types/image.h"
#include "types/point.h"
#include "types/color.h"
#include "types/tooth_measurement.h"

namespace cc {
    void draw_gear_arrow(
              cc::Image&    output_image,
        const cc::Point2d&  gear_center,
        double              gear_radius,
        double              angle,
        const cc::Color3&   color     = cc::Color3(255, 255, 127),
        int                 thickness = 3
    );

    void display_results(
        cc::Point2i                          centroid_i,
        const std::vector<ToothMeasurement>& teeth,
        const std::vector<uint8_t>&          tooth_anomaly_mask,
        cc::Image&                           output_image,
        int                                  speculative_count = -1,
        int                                  direct_count      = -1
    );
}

#endif
