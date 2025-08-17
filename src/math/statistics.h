#ifndef CC_MATH_STATISTICS_H
#define CC_MATH_STATISTICS_H

#include <vector>

namespace cc::math {
    template <typename T>
    [[nodiscard]]
    double calculate_mean(const std::vector<T>& values);

    template <typename T>
    [[nodiscard]]
    double calculate_variance(const std::vector<T>& values);

    template <typename T>
    [[nodiscard]]
    double calculate_standard_deviation(const std::vector<T>& values);
}

#include "statistics.inl"

#endif
