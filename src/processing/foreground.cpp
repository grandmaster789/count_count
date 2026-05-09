#include "foreground.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace {
    // Convert a BGR pixel to HSV.
    // H: [0, 179], S: [0, 255], V: [0, 255]
    // Hue convention matches OpenCV: H = degrees/2, so red ≈ 0, green ≈ 60, blue ≈ 120.
    void bgr_to_hsv_pixel(uint8_t b, uint8_t g, uint8_t r,
                          uint8_t& h, uint8_t& s, uint8_t& v)
    {
        int vi    = std::max({static_cast<int>(b), static_cast<int>(g), static_cast<int>(r)});
        int mi    = std::min({static_cast<int>(b), static_cast<int>(g), static_cast<int>(r)});
        int delta = vi - mi;

        v = static_cast<uint8_t>(vi);
        s = (vi == 0) ? 0 : static_cast<uint8_t>((delta * 255 + vi / 2) / vi);

        if (delta == 0) {
            h = 0; // achromatic — hue undefined, set to 0
            return;
        }

        float hf;
        if (vi == static_cast<int>(r))
            hf = 60.0f * (static_cast<float>(g - b) / static_cast<float>(delta));
        else if (vi == static_cast<int>(g))
            hf = 60.0f * (static_cast<float>(b - r) / static_cast<float>(delta) + 2.0f);
        else
            hf = 60.0f * (static_cast<float>(r - g) / static_cast<float>(delta) + 4.0f);

        if (hf < 0.0f) hf += 360.0f;

        // Scale [0, 360) → [0, 180), then round to nearest integer
        auto hi = static_cast<int>(hf * 0.5f + 0.5f);
        h = static_cast<uint8_t>(hi >= 180 ? 0 : hi); // 360° wraps to 0°
    }
}

namespace cc::processing {
    void determine_foreground(
        const cc::Color3& selected_color,
              int         tolerance_range,
        const cc::Image&  source_image,
              cc::Image&  foreground_mask,
              cc::Image&  foreground,
              cc::Image&  blur_temp,
              bool        invert,
              bool        use_chebyshev
    ) {
        int rows = source_image.rows();
        int cols = source_image.cols();

        if (source_image.channels() != 3)
            return;

        std::memset(foreground_mask.data(), 0, foreground_mask.total_bytes());

        if (use_chebyshev) {
            // BGR Chebyshev distance: tolerance_range is the direct threshold.
            // Units match what detect_background_sensitivity() returns, so no
            // conversion is needed.  Works correctly for achromatic targets
            // (e.g. white background) where HSV hue is undefined.
            int ref_b = static_cast<int>(selected_color.b);
            int ref_g = static_cast<int>(selected_color.g);
            int ref_r = static_cast<int>(selected_color.r);

            for (int y = 0; y < rows; ++y) {
                const uint8_t* src_row  = source_image.ptr(y);
                uint8_t*       mask_row = foreground_mask.ptr(y);

                for (int x = 0; x < cols; ++x) {
                    int idx  = x * 3;
                    int dist = std::max({
                        std::abs(static_cast<int>(src_row[idx + 0]) - ref_b),
                        std::abs(static_cast<int>(src_row[idx + 1]) - ref_g),
                        std::abs(static_cast<int>(src_row[idx + 2]) - ref_r)
                    });
                    mask_row[x] = (dist <= tolerance_range) ? 255 : 0;
                }
            }
        } else {
            // HSV comparison: tolerance_range controls hue selectivity.
            // sat/val use generous fixed tolerances so the slider primarily
            // controls hue, not brightness.
            uint8_t target_h, target_s, target_v;
            bgr_to_hsv_pixel(
                static_cast<uint8_t>(selected_color.b),
                static_cast<uint8_t>(selected_color.g),
                static_cast<uint8_t>(selected_color.r),
                target_h, target_s, target_v
            );

            int hue_tol = tolerance_range / 2;
            constexpr int sat_tol = 80;
            constexpr int val_tol = 80;

            for (int y = 0; y < rows; ++y) {
                const uint8_t* src_row  = source_image.ptr(y);
                uint8_t*       mask_row = foreground_mask.ptr(y);

                for (int x = 0; x < cols; ++x) {
                    int idx = x * 3;
                    uint8_t ph, ps, pv;
                    bgr_to_hsv_pixel(src_row[idx], src_row[idx+1], src_row[idx+2], ph, ps, pv);

                    int hue_diff = std::abs(static_cast<int>(ph) - static_cast<int>(target_h));
                    if (hue_diff > 90) hue_diff = 180 - hue_diff;

                    bool in_range =
                        hue_diff <= hue_tol &&
                        std::abs(static_cast<int>(ps) - static_cast<int>(target_s)) <= sat_tol &&
                        std::abs(static_cast<int>(pv) - static_cast<int>(target_v)) <= val_tol;

                    mask_row[x] = in_range ? 255 : 0;
                }
            }
        }

        // majority-vote median blur (9x9) via integral image — O(1) per pixel
        {
            std::memset(blur_temp.data(), 0, blur_temp.total_bytes());
            constexpr int half = 4; // 9x9 kernel → half = 4

            // Build integral image (summed area table) using blur_temp's memory
            // We need int storage, so use a separate vector. Size = (rows+1)*(cols+1).
            std::vector<int> integral(static_cast<size_t>(rows + 1) * (cols + 1), 0);
            auto sat = [&](int r, int c) -> int& {
                return integral[static_cast<size_t>(r) * (cols + 1) + c];
            };

            for (int y = 0; y < rows; ++y) {
                const uint8_t* mask_row = foreground_mask.ptr(y);
                int row_sum = 0;
                for (int x = 0; x < cols; ++x) {
                    row_sum += (mask_row[x] > 0) ? 1 : 0;
                    sat(y + 1, x + 1) = row_sum + sat(y, x + 1);
                }
            }

            // Apply majority vote using the integral image
            for (int y = 0; y < rows; ++y) {
                uint8_t* out_row = blur_temp.ptr(y);
                int y0 = std::max(0, y - half);
                int y1 = std::min(rows - 1, y + half);
                for (int x = 0; x < cols; ++x) {
                    int x0 = std::max(0, x - half);
                    int x1 = std::min(cols - 1, x + half);

                    int count = sat(y1 + 1, x1 + 1) - sat(y0, x1 + 1)
                              - sat(y1 + 1, x0)     + sat(y0, x0);
                    int total = (y1 - y0 + 1) * (x1 - x0 + 1);

                    out_row[x] = (count > total / 2) ? 255 : 0;
                }
            }

            std::memcpy(foreground_mask.data(), blur_temp.data(), blur_temp.total_bytes());
        }

        if (invert) {
            for (int y = 0; y < rows; ++y) {
                uint8_t* mask_row = foreground_mask.ptr(y);
                for (int x = 0; x < cols; ++x)
                    mask_row[x] = mask_row[x] ? 0 : 255;
            }
        }

        // copy with mask
        std::memset(foreground.data(), 0, foreground.total_bytes());

        for (int y = 0; y < rows; ++y) {
            const uint8_t* src_row  = source_image.ptr(y);
            const uint8_t* mask_row = foreground_mask.ptr(y);
            uint8_t*       dst_row  = foreground.ptr(y);

            for (int x = 0; x < cols; ++x) {
                if (mask_row[x]) {
                    int idx = x * 3;
                    dst_row[idx + 0] = src_row[idx + 0];
                    dst_row[idx + 1] = src_row[idx + 1];
                    dst_row[idx + 2] = src_row[idx + 2];
                }
            }
        }
    }
}
