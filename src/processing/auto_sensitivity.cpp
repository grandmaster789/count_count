#include "auto_sensitivity.h"

#include "foreground.h"
#include "boundary_trace.h"
#include "contours.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace {
    constexpr int k_HistBins       = 32;
    constexpr int k_HistTotal      = k_HistBins * k_HistBins * k_HistBins; // 32768
    constexpr int k_Subsample      = 4;
    constexpr int k_SuppressRadius = 2;

    cc::Color3 find_background_color(const cc::Image& image) {
        std::vector<int> histogram(k_HistTotal, 0);

        for (int y = 0; y < image.rows(); y += k_Subsample) {
            const uint8_t* row = image.ptr(y);
            for (int x = 0; x < image.cols(); x += k_Subsample) {
                const uint8_t* px = row + static_cast<size_t>(x) * 3;
                int bi = px[0] >> 3;
                int gi = px[1] >> 3;
                int ri = px[2] >> 3;
                histogram[static_cast<size_t>(bi) * k_HistBins * k_HistBins +
                          static_cast<size_t>(gi) * k_HistBins + ri]++;
            }
        }

        auto it  = std::max_element(histogram.begin(), histogram.end());
        auto idx = static_cast<size_t>(std::distance(histogram.begin(), it));

        int b = static_cast<int>(idx / (k_HistBins * k_HistBins));
        int g = static_cast<int>((idx / k_HistBins) % k_HistBins);
        int r = static_cast<int>(idx % k_HistBins);

        return cc::Color3(b * 8 + 4, g * 8 + 4, r * 8 + 4);
    }

    cc::Color3 find_dominant_color(const cc::Image& image) {
        std::vector<int> histogram(k_HistTotal, 0);

        for (int y = 0; y < image.rows(); y += k_Subsample) {
            const uint8_t* row = image.ptr(y);
            for (int x = 0; x < image.cols(); x += k_Subsample) {
                const uint8_t* px = row + static_cast<size_t>(x) * 3;
                int bi = px[0] >> 3;
                int gi = px[1] >> 3;
                int ri = px[2] >> 3;
                histogram[static_cast<size_t>(bi) * k_HistBins * k_HistBins +
                          static_cast<size_t>(gi) * k_HistBins + ri]++;
            }
        }

        // Find background peak (highest count)
        auto bg_it  = std::max_element(histogram.begin(), histogram.end());
        auto bg_idx = static_cast<size_t>(std::distance(histogram.begin(), bg_it));

        int bg_b = static_cast<int>(bg_idx / (k_HistBins * k_HistBins));
        int bg_g = static_cast<int>((bg_idx / k_HistBins) % k_HistBins);
        int bg_r = static_cast<int>(bg_idx % k_HistBins);

        // Suppress region around background peak
        for (int db = -k_SuppressRadius; db <= k_SuppressRadius; ++db) {
            for (int dg = -k_SuppressRadius; dg <= k_SuppressRadius; ++dg) {
                for (int dr = -k_SuppressRadius; dr <= k_SuppressRadius; ++dr) {
                    int b = bg_b + db;
                    int g = bg_g + dg;
                    int r = bg_r + dr;
                    if (b >= 0 && b < k_HistBins &&
                        g >= 0 && g < k_HistBins &&
                        r >= 0 && r < k_HistBins)
                    {
                        histogram[static_cast<size_t>(b) * k_HistBins * k_HistBins +
                                  static_cast<size_t>(g) * k_HistBins + r] = 0;
                    }
                }
            }
        }

        // Find foreground peak (next highest)
        auto fg_it  = std::max_element(histogram.begin(), histogram.end());

        // If no second peak exists, return black (will fail validation)
        if (*fg_it == 0)
            return { 0, 0, 0 };

        auto fg_idx = static_cast<size_t>(std::distance(histogram.begin(), fg_it));

        int fg_b = static_cast<int>(fg_idx / (k_HistBins * k_HistBins));
        int fg_g = static_cast<int>((fg_idx / k_HistBins) % k_HistBins);
        int fg_r = static_cast<int>(fg_idx % k_HistBins);

        // Bin center = bin_index * 8 + 4
        return cc::Color3(fg_b * 8 + 4, fg_g * 8 + 4, fg_r * 8 + 4);
    }

    // Saturation-threshold calibration tunables.
    constexpr int k_SatDefault = 45;  // fallback when no clear background peak
    constexpr int k_SatMargin  = 8;   // push past the background shoulder into the valley
    constexpr int k_SatMin     = 25;  // never threshold below this (achromatic noise floor)
    constexpr int k_SatMax     = 180; // never threshold above this (would erode the object)

    // Saturation, OpenCV convention: S = (V - min) * 255 / V.
    inline int saturation_of(int b, int g, int r) {
        int vi = std::max({b, g, r});
        int mi = std::min({b, g, r});
        return (vi == 0) ? 0 : ((vi - mi) * 255 + vi / 2) / vi;
    }

    int find_optimal_tolerance(const cc::Image& image, const cc::Color3& target_color) {
        std::array<int, 256> dist_hist {};

        int ref_b = static_cast<int>(target_color.b);
        int ref_g = static_cast<int>(target_color.g);
        int ref_r = static_cast<int>(target_color.r);
        int total_pixels = 0;

        for (int y = 0; y < image.rows(); y += k_Subsample) {
            const uint8_t* row = image.ptr(y);
            for (int x = 0; x < image.cols(); x += k_Subsample) {
                const uint8_t* px = row + static_cast<size_t>(x) * 3;
                int db = std::abs(static_cast<int>(px[0]) - ref_b);
                int dg = std::abs(static_cast<int>(px[1]) - ref_g);
                int dr = std::abs(static_cast<int>(px[2]) - ref_r);
                int dist = std::max({ db, dg, dr });
                dist_hist[static_cast<size_t>(dist)]++;
                total_pixels++;
            }
        }

        if (total_pixels == 0)
            return 0;

        // Otsu's method: find threshold maximizing between-class variance
        double total_sum = 0.0;
        for (int i = 0; i < 256; ++i)
            total_sum += static_cast<double>(i) * dist_hist[static_cast<size_t>(i)];

        double best_variance = 0.0;
        int    best_threshold = 0;
        double w0   = 0.0;
        double sum0 = 0.0;

        for (int t = 0; t < 255; ++t) {
            w0   += dist_hist[static_cast<size_t>(t)];
            sum0 += static_cast<double>(t) * dist_hist[static_cast<size_t>(t)];

            if (w0 == 0.0)
                continue;

            double w1 = total_pixels - w0;
            if (w1 == 0.0)
                break;

            double mean0 = sum0 / w0;
            double mean1 = (total_sum - sum0) / w1;
            double between_variance = w0 * w1 * (mean0 - mean1) * (mean0 - mean1);

            if (between_variance > best_variance) {
                best_variance  = between_variance;
                best_threshold = t;
            }
        }

        // tolerance = 2 * threshold (determine_color_range uses +/- tolerance/2)
        return std::min(best_threshold * 2, 255);
    }
}

namespace cc::processing {
    AutoSensitivityResult detect_sensitivity(const cc::Image& source_image) {
        if (source_image.empty() || source_image.channels() != 3)
            return { {}, 0, false };

        auto color     = find_dominant_color(source_image);
        int  tolerance = find_optimal_tolerance(source_image, color);

        bool valid = tolerance > 0 && tolerance <= 255;
        return { color, tolerance, valid };
    }

    AutoSensitivityResult detect_background_sensitivity(const cc::Image& source_image) {
        if (source_image.empty() || source_image.channels() != 3)
            return { {}, 0, false };

        auto color     = find_background_color(source_image);
        int  tolerance = find_optimal_tolerance(source_image, color);

        // Otsu returns 0 when the background is perfectly uniform (all pixels at
        // distance 0 from the target).  Apply a floor of one histogram bin width
        // (8 values) so the result is always usable.
        tolerance = std::max(tolerance, 8);

        bool valid = tolerance > 0 && tolerance <= 255;
        return { color, tolerance, valid };
    }

    int detect_saturation_threshold(const cc::Image& source_image) {
        if (source_image.empty() || source_image.channels() != 3)
            return k_SatDefault;

        std::array<long, 256> hist {};
        long total = 0;
        for (int y = 0; y < source_image.rows(); y += k_Subsample) {
            const uint8_t* row = source_image.ptr(y);
            for (int x = 0; x < source_image.cols(); x += k_Subsample) {
                const uint8_t* px = row + static_cast<size_t>(x) * 3;
                int s = saturation_of(px[0], px[1], px[2]);
                ++hist[static_cast<size_t>(s)];
                ++total;
            }
        }
        if (total == 0)
            return k_SatDefault;

        // Background = the dominant low-saturation peak.
        auto peak_it  = std::max_element(hist.begin(), hist.end());
        int  peak_bin = static_cast<int>(std::distance(hist.begin(), peak_it));
        long peak_val = *peak_it;

        // If the dominant peak is already saturated, there is no achromatic
        // background to separate from — fall back to the default.
        if (peak_bin > 128 || peak_val == 0)
            return k_SatDefault;

        // Walk up from the background peak until its tail falls to a small
        // fraction of the peak height: that bin is the shoulder of the
        // background cluster, i.e. the valley before the object's saturation mode.
        long floor = std::max<long>(1, peak_val / 100);  // 1% of peak
        int shoulder = peak_bin;
        for (int s = peak_bin; s < 256; ++s) {
            if (hist[static_cast<size_t>(s)] <= floor) {
                shoulder = s;
                break;
            }
        }

        int threshold = shoulder + k_SatMargin;
        return std::clamp(threshold, k_SatMin, k_SatMax);
    }

    int detect_saturation_threshold_by_teeth(const cc::Image& image) {
        if (image.empty() || image.channels() != 3)
            return k_SatDefault;

        // Histogram-based default (cheap) is the fallback if nothing plausible is found.
        const int fallback = detect_saturation_threshold(image);

        cc::Image mask(image.rows(), image.cols(), 1);
        cc::Image fg(image.rows(), image.cols(), 3);
        cc::Image blur(image.rows(), image.cols(), 1);
        cc::Image scratch(image.rows(), image.cols(), 3);

        // Sweep candidate saturation thresholds; at each, segment → contour → count.
        // The discriminator (from diagnosis): a threshold that BREAKS the rim makes
        // the FFT latch onto a low-frequency shape artifact, so fft << direct; a good
        // threshold gives fft >= direct. Among the non-broken candidates, take the
        // strongest tooth count that occurs on a STABLE plateau (≥2 thresholds), and
        // return the highest such threshold (cleanest, lowest-background mask).
        constexpr int k_Lo = 28, k_Hi = 100, k_Step = 6;
        std::vector<std::pair<int,int>> good;  // (threshold, fft) for non-broken candidates

        for (int t = k_Lo; t <= k_Hi; t += k_Step) {
            determine_foreground_by_saturation(t, image, mask, fg, blur);
            auto contours = find_contours(mask);
            if (contours.empty()) continue;
            std::memcpy(scratch.data(), image.data(), scratch.total_bytes());
            auto res = process_contours(contours, scratch);
            if (!res) continue;

            int fft    = res->m_SpeculativeCount;
            int direct = static_cast<int>(res->m_Teeth.size());
            // Plausible tooth count, and NOT the broken-rim regime (fft << direct).
            if (fft >= 8 && fft <= 500 && fft * 10 >= direct * 7)
                good.emplace_back(t, fft);
        }

        if (good.empty())
            return fallback;

        // Best count that appears on a plateau (≥2 candidate thresholds); else the max.
        int best_count = 0;
        for (const auto& [t, fft] : good) {
            int occurrences = 0;
            for (const auto& [t2, fft2] : good)
                if (std::abs(fft2 - fft) <= 1) ++occurrences;     // ~equal counts
            if (occurrences >= 2 && fft > best_count) best_count = fft;
        }
        if (best_count == 0)  // no plateau — fall back to the single strongest
            for (const auto& [t, fft] : good) best_count = std::max(best_count, fft);

        // Pick the MIDDLE threshold of that plateau — maximum margin from both the
        // low edge (gear merges into the wall) and the high edge (rim breaks), which
        // is the most robust choice for noisy live frames.
        std::vector<int> plateau;
        for (const auto& [t, fft] : good)
            if (std::abs(fft - best_count) <= 1) plateau.push_back(t);
        if (plateau.empty())
            return fallback;
        std::sort(plateau.begin(), plateau.end());
        return plateau[plateau.size() / 2];
    }
}
