#include "foreground.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#pragma warning(push, 0)
#include <xsimd/xsimd.hpp>
#pragma warning(pop)

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
    void chebyshev_threshold(
        const cc::Color3& selected_color,
              int         tolerance_range,
        const cc::Image&  source_image,
              cc::Image&  foreground_mask
    ) {
        using batch_u8 = xsimd::batch<uint8_t>;
        constexpr int lanes = static_cast<int>(batch_u8::size); // 32 on AVX2

        const int ref_b = static_cast<int>(selected_color.b);
        const int ref_g = static_cast<int>(selected_color.g);
        const int ref_r = static_cast<int>(selected_color.r);

        const batch_u8 vref_b(static_cast<uint8_t>(ref_b));
        const batch_u8 vref_g(static_cast<uint8_t>(ref_g));
        const batch_u8 vref_r(static_cast<uint8_t>(ref_r));
        const batch_u8 vtol(static_cast<uint8_t>(std::clamp(tolerance_range, 0, 255)));

        // Deinterleave buffers reused across rows to avoid per-row allocation.
        // max(a,b) - min(a,b) computes |a-b| for uint8 without overflow.
        const int rows = source_image.rows();
        const int cols = source_image.cols();
        std::vector<uint8_t> ch_b(cols), ch_g(cols), ch_r(cols);

        for (int y = 0; y < rows; ++y) {
            const uint8_t* src_row  = source_image.ptr(y);
            uint8_t*       mask_row = foreground_mask.ptr(y);

            for (int x = 0; x < cols; ++x) {
                ch_b[x] = src_row[x * 3 + 0];
                ch_g[x] = src_row[x * 3 + 1];
                ch_r[x] = src_row[x * 3 + 2];
            }

            int x = 0;
            for (; x + lanes <= cols; x += lanes) {
                batch_u8 vb = batch_u8::load_unaligned(ch_b.data() + x);
                batch_u8 vg = batch_u8::load_unaligned(ch_g.data() + x);
                batch_u8 vr = batch_u8::load_unaligned(ch_r.data() + x);

                batch_u8 db   = xsimd::max(vb, vref_b) - xsimd::min(vb, vref_b);
                batch_u8 dg   = xsimd::max(vg, vref_g) - xsimd::min(vg, vref_g);
                batch_u8 dr   = xsimd::max(vr, vref_r) - xsimd::min(vr, vref_r);
                batch_u8 dist = xsimd::max(xsimd::max(db, dg), dr);

                xsimd::select(dist <= vtol, batch_u8(uint8_t{255}), batch_u8(uint8_t{0}))
                    .store_unaligned(mask_row + x);
            }

            for (; x < cols; ++x) {
                int idx = x * 3;
                int d   = std::max({
                    std::abs(static_cast<int>(src_row[idx + 0]) - ref_b),
                    std::abs(static_cast<int>(src_row[idx + 1]) - ref_g),
                    std::abs(static_cast<int>(src_row[idx + 2]) - ref_r)
                });
                mask_row[x] = (d <= tolerance_range) ? 255 : 0;
            }
        }
    }

    void saturation_threshold(
              int        s_threshold,
        const cc::Image& source_image,
              cc::Image& foreground_mask
    ) {
        int rows = source_image.rows();
        int cols = source_image.cols();

        for (int y = 0; y < rows; ++y) {
            const uint8_t* src_row  = source_image.ptr(y);
            uint8_t*       mask_row = foreground_mask.ptr(y);

            for (int x = 0; x < cols; ++x) {
                int idx = x * 3;
                int b = src_row[idx + 0];
                int g = src_row[idx + 1];
                int r = src_row[idx + 2];

                int vi = std::max({b, g, r});
                int mi = std::min({b, g, r});
                // HSV saturation, OpenCV convention: S = (V - min) * 255 / V.
                int s = (vi == 0) ? 0 : ((vi - mi) * 255 + vi / 2) / vi;

                mask_row[x] = (s >= s_threshold) ? 255 : 0;
            }
        }
    }

    void invert_mask(cc::Image& foreground_mask) {
        using batch_u8 = xsimd::batch<uint8_t>;
        constexpr auto L = static_cast<ptrdiff_t>(batch_u8::size);
        uint8_t* p   = foreground_mask.data();
        uint8_t* end = p + static_cast<ptrdiff_t>(foreground_mask.total_bytes());
        for (; p + L <= end; p += L)
            (~batch_u8::load_unaligned(p)).store_unaligned(p);
        for (; p < end; ++p)
            *p = *p ? 0 : 255;
    }

    void majority_vote_blur(cc::Image& mask, cc::Image& scratch) {
        using batch_i32 = xsimd::batch<int32_t>;
        constexpr int lanes = static_cast<int>(batch_i32::size); // 8 on AVX2
        constexpr int half  = 4; // 9×9 kernel → radius = 4

        const int rows = mask.rows();
        const int cols = mask.cols();

        // Build integral image (summed area table): (rows+1) × (cols+1) ints.
        // The within-row prefix-sum dependency prevents vectorisation here.
        std::vector<int> integral(static_cast<size_t>(rows + 1) * (cols + 1), 0);
        auto sat = [&](int r, int c) -> int& {
            return integral[static_cast<size_t>(r) * (cols + 1) + c];
        };

        for (int y = 0; y < rows; ++y) {
            const uint8_t* mask_row = mask.ptr(y);
            int row_sum = 0;
            for (int x = 0; x < cols; ++x) {
                row_sum += (mask_row[x] > 0) ? 1 : 0;
                sat(y + 1, x + 1) = row_sum + sat(y, x + 1);
            }
        }

        // General scalar pixel: handles clamped borders correctly.
        auto scalar_px = [&](int y0, int y1, int x) -> uint8_t {
            int x0    = std::max(0, x - half);
            int x1    = std::min(cols - 1, x + half);
            int count = sat(y1 + 1, x1 + 1) - sat(y0, x1 + 1)
                      - sat(y1 + 1, x0)     + sat(y0, x0);
            int total = (y1 - y0 + 1) * (x1 - x0 + 1);
            return (count > total / 2) ? 255 : 0;
        };

        // For interior pixels the kernel is fully contained: total = 9×9 = 81.
        constexpr int k_InnerTotal = (2 * half + 1) * (2 * half + 1); // 81
        constexpr int k_InnerHalf  = k_InnerTotal / 2;                // 40

        for (int y = 0; y < rows; ++y) {
            uint8_t* out_row = scratch.ptr(y);
            int y0 = std::max(0, y - half);
            int y1 = std::min(rows - 1, y + half);

            if (y >= half && y <= rows - 1 - half && cols > 2 * half) {
                // Interior row: fixed y0/y1, so row pointers into the integral
                // image are constant for the whole x-sweep.
                const int* pA = integral.data() + static_cast<size_t>(y1 + 1) * (cols + 1);
                const int* pB = integral.data() + static_cast<size_t>(y0)     * (cols + 1);

                // Scalar left border [0, half)
                for (int x = 0; x < half; ++x)
                    out_row[x] = scalar_px(y0, y1, x);

                // SIMD interior [half, cols - half), 8 pixels per iteration.
                // Reads: pA/pB at offsets x-half and x+half+1 — both sequential.
                // Bounds verified: x-half >= 0 (x >= half) and
                //   x+half+1+lanes-1 = x+lanes+half <= cols (loop condition).
                int x = half;
                for (; x + lanes <= cols - half; x += lanes) {
                    batch_i32 vA_hi = batch_i32::load_unaligned(pA + x + half + 1);
                    batch_i32 vA_lo = batch_i32::load_unaligned(pA + x - half);
                    batch_i32 vB_hi = batch_i32::load_unaligned(pB + x + half + 1);
                    batch_i32 vB_lo = batch_i32::load_unaligned(pB + x - half);
                    batch_i32 count = (vA_hi - vA_lo) - (vB_hi - vB_lo);

                    alignas(32) int32_t buf[lanes];
                    xsimd::select(count > batch_i32(k_InnerHalf),
                                  batch_i32(255), batch_i32(0)).store_unaligned(buf);
                    for (int i = 0; i < lanes; ++i)
                        out_row[x + i] = static_cast<uint8_t>(buf[i]);
                }

                // Scalar right border and tail [x, cols)
                for (; x < cols; ++x)
                    out_row[x] = scalar_px(y0, y1, x);

            } else {
                // Border row: fully scalar
                for (int x = 0; x < cols; ++x)
                    out_row[x] = scalar_px(y0, y1, x);
            }
        }

        std::memcpy(mask.data(), scratch.data(), scratch.total_bytes());
    }

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
            chebyshev_threshold(selected_color, tolerance_range, source_image, foreground_mask);
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

        majority_vote_blur(foreground_mask, blur_temp);

        if (invert)
            invert_mask(foreground_mask);

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

    void determine_foreground_by_saturation(
              int        s_threshold,
        const cc::Image& source_image,
              cc::Image& foreground_mask,
              cc::Image& foreground,
              cc::Image& blur_temp
    ) {
        int rows = source_image.rows();
        int cols = source_image.cols();

        if (source_image.channels() != 3)
            return;

        std::memset(foreground_mask.data(), 0, foreground_mask.total_bytes());

        saturation_threshold(s_threshold, source_image, foreground_mask);

        majority_vote_blur(foreground_mask, blur_temp);

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
