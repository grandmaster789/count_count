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

    template <typename T>
    [[nodiscard]]
    double percentile_threshold(std::vector<T> values);

    // Otsu's method on continuous data: discretises into `bins` buckets and
    // returns the value that maximises between-class variance.
    template <typename T>
    [[nodiscard]]
    double otsu_threshold(const std::vector<T>& values, int bins = 256);
}

#include "statistics.inl"

#endif
