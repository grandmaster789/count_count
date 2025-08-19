#include "angles.h"
#include <numbers>

namespace cc::math {
    double arc_length(
        double starting_radians,
        double ending_radians
    ) {
        while (starting_radians > ending_radians)
            ending_radians += 2.0 * std::numbers::pi;

        return ending_radians - starting_radians;
    }
}