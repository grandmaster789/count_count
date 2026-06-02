#include "count_teeth.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace {
    // Cooley-Tukey radix-2 iterative FFT; N must be a power of 2.
    void fft_inplace(std::vector<std::complex<double>>& x) {
        const size_t N = x.size();
        for (size_t i = 1, j = 0; i < N; ++i) {
            size_t bit = N >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(x[i], x[j]);
        }
        for (size_t len = 2; len <= N; len <<= 1) {
            double ang = -2.0 * std::numbers::pi / static_cast<double>(len);
            std::complex<double> wlen(std::cos(ang), std::sin(ang));
            for (size_t i = 0; i < N; i += len) {
                std::complex<double> w(1.0, 0.0);
                for (size_t j = 0; j < len / 2; ++j) {
                    auto u = x[i + j];
                    auto v = x[i + j + len / 2] * w;
                    x[i + j]           = u + v;
                    x[i + j + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }
    }
}

namespace cc::processing {
    // find the first (low) position where the mask changes from low to high
    std::optional<size_t> find_tooth_start(const std::vector<uint8_t>& mask) {
        if (mask.empty())
            return std::nullopt;

        for (size_t i = 0; i < mask.size(); ++i)
            if (!mask[i] && mask[(i + 1) % mask.size()])
                return i;

        // super edge case where there is just one change, and it's right at the end of the list
        if (!mask.back() && mask.front())
            return mask.size() - 1;

        return std::nullopt;
    };

    // Here we figure out how often the threshold is crossed to determine a tooth count
    // -- only count the 'rising' edges to establish a count
    // -- also figure out some tooth measurements
    std::vector<ToothMeasurement> count_teeth(
              size_t                         first_tooth_idx,
        const std::vector<uint8_t>&          tooth_mask,
        const std::vector<cc::Point2i>&      largest_contour,
        const std::vector<double>&           distances,
        const cc::Point2f&                   centroid_f
    ) {
        int tooth_count = 0;

        std::vector<ToothMeasurement> teeth;

        // from the first position, iterate over the entire set and collect measurements during traversal
        for (size_t i = first_tooth_idx; i < first_tooth_idx + tooth_mask.size(); ++i) {
            uint8_t current_mask_value = tooth_mask[i % tooth_mask.size()];
            uint8_t next_mask_value    = tooth_mask[(i + 1) % tooth_mask.size()];

            // count rising edges as the start of a tooth
            // the algorithm starts at a position where this is the case
            if (!current_mask_value && next_mask_value) {
                ++tooth_count;

                cc::ToothMeasurement new_measurement;
                new_measurement.m_LowHighTransitionIdx = i % tooth_mask.size();
                new_measurement.m_ToothIdx             = tooth_count;
                new_measurement.m_StartingAngle        = std::atan2f(
                    largest_contour[new_measurement.m_LowHighTransitionIdx].y - centroid_f.y,
                    largest_contour[new_measurement.m_LowHighTransitionIdx].x - centroid_f.x
                );

                teeth.push_back(new_measurement);
            }

            // when we transition from high to low, we have found the end of a tooth and can complete the measurement
            if (current_mask_value && !next_mask_value) {
                auto& measurement = teeth.back();

                measurement.m_HighLowTransitionIdx = i % tooth_mask.size();
                measurement.m_EndingAngle = std::atan2f(
                    largest_contour[measurement.m_HighLowTransitionIdx].y - centroid_f.y,
                    largest_contour[measurement.m_HighLowTransitionIdx].x - centroid_f.x
                );

                // wrap angles to [0, 2pi]
                if (measurement.m_StartingAngle < 0)
                    measurement.m_StartingAngle += 2 * std::numbers::pi;
                if (measurement.m_EndingAngle < 0)
                    measurement.m_EndingAngle += 2 * std::numbers::pi;

                measurement.m_MinDistance =  std::numeric_limits<double>::max();
                measurement.m_MaxDistance = -std::numeric_limits<double>::max();

                // find the min and max distances for this tooth
                // walk forward from (LowHigh+1) to HighLow, wrapping around if needed
                {
                    size_t start = (measurement.m_LowHighTransitionIdx + 1) % tooth_mask.size();
                    size_t end   = measurement.m_HighLowTransitionIdx;
                    size_t n     = tooth_mask.size();
                    size_t span  = (end >= start) ? (end - start + 1) : (n - start + end + 1);

                    size_t j = start;
                    for (size_t step = 0; step < span; ++step) {
                        if (distances[j] < measurement.m_MinDistance)
                            measurement.m_MinDistance = distances[j];
                        if (distances[j] > measurement.m_MaxDistance)
                            measurement.m_MaxDistance = distances[j];
                        j = (j + 1) % n;
                    }
                }
            }
        }

        // Remove noise artifacts: teeth whose span of high-points is implausibly narrow
        // compared to the median across all detected teeth.
        if (teeth.size() >= 3) {
            size_t N = tooth_mask.size();

            std::vector<size_t> spans;
            spans.reserve(teeth.size());
            for (const auto& t : teeth) {
                size_t start = (t.m_LowHighTransitionIdx + 1) % N;
                size_t end   = t.m_HighLowTransitionIdx;
                spans.push_back((end >= start) ? (end - start + 1) : (N - start + end + 1));
            }

            auto sorted_spans = spans;
            std::sort(sorted_spans.begin(), sorted_spans.end());
            size_t median_span = sorted_spans[sorted_spans.size() / 2];
            size_t min_span    = std::max(size_t(1), median_span / 3);

            std::vector<ToothMeasurement> kept;
            kept.reserve(teeth.size());
            for (size_t i = 0; i < teeth.size(); ++i) {
                if (spans[i] >= min_span) {
                    kept.push_back(teeth[i]);
                    kept.back().m_ToothIdx = kept.size();
                }
            }
            teeth = std::move(kept);
        }

        return teeth;
    }

    int fft_tooth_count(
        const std::vector<cc::Point2i>& contour,
        const std::vector<double>&      distances,
        const cc::Point2f&              centroid
    ) {
        const size_t n = contour.size();
        if (n < 32 || distances.size() != n)
            return -1;

        // Sort (angle, distance) pairs by atan2 angle. Using sorted atan2 rather
        // than unwrapped traversal order handles CW/CCW contours uniformly and
        // avoids the multi-revolution unwrap edge case. For star-shaped gear
        // contours the atan2 mapping is single-valued, so this is exact.
        std::vector<std::pair<double, double>> profile;
        profile.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            double a = std::atan2(
                static_cast<double>(contour[i].y) - centroid.y,
                static_cast<double>(contour[i].x) - centroid.x
            );
            profile.emplace_back(a, distances[i]);
        }
        std::sort(profile.begin(), profile.end());

        const double a_start = profile.front().first;
        const double a_end   = profile.back().first;
        const double a_range = a_end - a_start;
        if (a_range < std::numbers::pi)
            return -1;

        // Resample to k_FFT_N uniform angular bins via linear interpolation.
        // Denominator k_FFT_N (not k_FFT_N-1) gives DFT-correct spacing: N
        // samples over one period, with sample N implicitly wrapping to sample 0.
        constexpr size_t k_FFT_N = 1024;
        std::vector<std::complex<double>> signal(k_FFT_N);
        {
            size_t ci = 0;
            for (size_t k = 0; k < k_FFT_N; ++k) {
                double target = a_start + static_cast<double>(k) * a_range / static_cast<double>(k_FFT_N);
                while (ci + 1 < n - 1 && profile[ci + 1].first < target)
                    ++ci;
                double da = profile[ci + 1].first - profile[ci].first;
                double t  = (da > 1e-9) ? (target - profile[ci].first) / da : 0.0;
                t = std::clamp(t, 0.0, 1.0);
                signal[k] = profile[ci].second * (1.0 - t) + profile[ci + 1].second * t;
            }
        }

        fft_inplace(signal);

        // Bin k covers k cycles over a_range radians → k * 2π / a_range teeth.
        // a_range ≈ 2π for a full revolution, so the scaling correction is tiny
        // but kept for correctness when the contour covers less than a full turn.
        constexpr int k_MinTeeth = 4;
        constexpr int k_MaxTeeth = 500;

        int    best_count = -1;
        double best_power = 0.0;
        for (size_t k = 1; k <= k_FFT_N / 2; ++k) {
            int count = static_cast<int>(
                std::round(static_cast<double>(k) * 2.0 * std::numbers::pi / a_range)
            );
            if (count < k_MinTeeth || count > k_MaxTeeth)
                continue;
            double power = std::norm(signal[k]);
            if (power > best_power) {
                best_power = power;
                best_count = count;
            }
        }

        return best_count;
    }
}
