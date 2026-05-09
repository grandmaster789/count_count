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

    template <typename T>
    double otsu_threshold(const std::vector<T>& values, int bins) {
        if (values.size() < 2) return values.empty() ? 0.0 : static_cast<double>(values[0]);

        auto [min_it, max_it] = std::minmax_element(values.begin(), values.end());
        double vmin = static_cast<double>(*min_it);
        double vmax = static_cast<double>(*max_it);
        if (vmax - vmin < 1e-6) return vmin;

        double scale = static_cast<double>(bins - 1) / (vmax - vmin);

        std::vector<double> hist(static_cast<size_t>(bins), 0.0);
        for (const auto& v : values) {
            int bin = static_cast<int>((static_cast<double>(v) - vmin) * scale);
            if (bin < 0) bin = 0;
            if (bin >= bins) bin = bins - 1;
            hist[static_cast<size_t>(bin)] += 1.0;
        }

        double total     = static_cast<double>(values.size());
        double total_sum = 0.0;
        for (int i = 0; i < bins; ++i)
            total_sum += static_cast<double>(i) * hist[static_cast<size_t>(i)];

        double best_variance = 0.0;
        int    best_bin      = 0;
        double w0   = 0.0;
        double sum0 = 0.0;

        for (int t = 0; t < bins - 1; ++t) {
            w0   += hist[static_cast<size_t>(t)];
            sum0 += static_cast<double>(t) * hist[static_cast<size_t>(t)];

            if (w0 == 0.0) continue;
            double w1 = total - w0;
            if (w1 == 0.0) break;

            double mean0    = sum0 / w0;
            double mean1    = (total_sum - sum0) / w1;
            double variance = w0 * w1 * (mean0 - mean1) * (mean0 - mean1);

            if (variance > best_variance) {
                best_variance = variance;
                best_bin      = t;
            }
        }

        // Return the midpoint of the threshold bin in original value space
        return vmin + (static_cast<double>(best_bin) + 0.5) / scale;
    }
}

#endif
