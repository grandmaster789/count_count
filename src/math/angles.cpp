#include "angles.h"
#include <cmath>
#include <numbers>

namespace cc::math {
    double arc_length(
        double starting_radians,
        double ending_radians
    ) {
        double diff = ending_radians - starting_radians;
        double result = std::fmod(diff, 2.0 * std::numbers::pi);

        if (result < 0.0)
            result += 2.0 * std::numbers::pi;

        return result;
    }
}