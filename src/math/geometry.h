#ifndef CC_MATH_GEOMETRY_H
#define CC_MATH_GEOMETRY_H

#include "types/point.h"

#include <cmath>
#include <vector>

namespace cc::math {
    // Shoelace formula for polygon area (always non-negative)
    inline double polygon_area(const std::vector<cc::Point2i>& contour) {
        double area = 0.0;
        size_t n = contour.size();

        for (size_t i = 0; i < n; ++i) {
            size_t j = (i + 1) % n;
            area += static_cast<double>(contour[i].x) * contour[j].y;
            area -= static_cast<double>(contour[j].x) * contour[i].y;
        }

        return std::fabs(area) / 2.0;
    }
}

#endif
