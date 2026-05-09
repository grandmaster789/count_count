#ifndef CC_MATH_STATISTICS_INL
#define CC_MATH_STATISTICS_INL

#include "statistics.h"
#include "square.h"

#include <algorithm>
#include <numeric>
#include <cmath>

namespace cc::math {
    template <typename T>
    double calculate_mean(const std::vector<T>& values) {
        if (values.empty())
            return 0.0;

        return std::accumulate(
                std::begin(values),
                std::end(values),
                0.0
        ) / values.size();
    }

    template <typename T>
    double calculate_variance(const std::vector<T>& values) {
        if (values.size() <= 1)
            return 0.0;

        auto mean = calculate_mean(values);

        return std::accumulate(
                std::begin(values),
                std::end(values),
                0.0,
                [mean](double acc, const auto &value) {
                    return acc + cc::square(value - mean);
                }
        ) / (values.size() - 1);
    }

    template <typename T>
    double calculate_standard_deviation(const std::vector<T>& values) {
        return std::sqrt(calculate_variance(values));
    }

    template <typename T>
    double percentile_threshold(std::vector<T> values) {
        if (values.empty()) return 0.0;
        if (values.size() == 1) return static_cast<double>(values[0]);

        std::sort(values.begin(), values.end());
        auto p25 = static_cast<double>(values[values.size() / 4]);
        auto p75 = static_cast<double>(values[values.size() * 3 / 4]);
        return (p25 + p75) / 2.0;
    }
}

#endif
