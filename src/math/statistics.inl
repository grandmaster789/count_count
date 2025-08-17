#ifndef CC_MATH_STATISTICS_INL
#define CC_MATH_STATISTICS_INL

#include "statistics.h"
#include "square.h"

#include <numeric>
#include <cmath>

namespace cc::math {
    template <typename T>
    double calculate_mean(const std::vector<T>& values) {
        return std::accumulate(
                std::begin(values),
                std::end(values),
                0.0
        ) / values.size();
    }

    template <typename T>
    double calculate_variance(const std::vector<T>& values) {
        auto mean = calculate_mean(values);

        return std::accumulate(
                std::begin(values),
                std::end(values),
                0.0,
                [mean](double acc, const auto &value) {
                    return acc + cc::square(value - mean);
                }
        ) / values.size();;
    }

    template <typename T>
    double calculate_standard_deviation(const std::vector<T>& values) {
        return std::sqrt(calculate_variance(values));
    }
}

#endif
