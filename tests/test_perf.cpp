#include <catch2/catch_test_macros.hpp>

#include "processing/edge_mask.h"
#include "processing/foreground.h"
#include "processing/gear_template.h"
#include "types/color.h"
#include "types/image.h"
#include "types/point.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
    cc::Image make_bgr_image(int w, int h) {
        cc::Image img(h, w, 3);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                uint8_t* p = img.at(y, x);
                p[0] = static_cast<uint8_t>((x *  7 + y *  3) & 0xFF);
                p[1] = static_cast<uint8_t>((x * 11 + y *  5) & 0xFF);
                p[2] = static_cast<uint8_t>((x * 13 + y *  7) & 0xFF);
            }
        return img;
    }

    long long time_ms(auto fn) {
        auto t0 = std::chrono::steady_clock::now();
        fn();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
    }

    // Scalar reference implementation for Chebyshev threshold.
    void chebyshev_threshold_scalar(const cc::Color3& color, int tol,
                                    const cc::Image& src, cc::Image& mask) {
        int ref_b = static_cast<int>(color.b);
        int ref_g = static_cast<int>(color.g);
        int ref_r = static_cast<int>(color.r);
        for (int y = 0; y < src.rows(); ++y) {
            const uint8_t* row = src.ptr(y);
            uint8_t*       out = mask.ptr(y);
            for (int x = 0; x < src.cols(); ++x) {
                int d = std::max({
                    std::abs(static_cast<int>(row[x * 3 + 0]) - ref_b),
                    std::abs(static_cast<int>(row[x * 3 + 1]) - ref_g),
                    std::abs(static_cast<int>(row[x * 3 + 2]) - ref_r)
                });
                out[x] = (d <= tol) ? 255 : 0;
            }
        }
    }

    // Scalar reference implementation for mask invert.
    void invert_mask_scalar(cc::Image& mask) {
        uint8_t* p   = mask.data();
        uint8_t* end = p + mask.total_bytes();
        for (; p < end; ++p)
            *p = *p ? 0 : 255;
    }

    // Scalar reference implementation for the 9×9 majority-vote blur.
    void majority_vote_blur_scalar(cc::Image& mask, cc::Image& scratch) {
        constexpr int half = 4;
        const int rows = mask.rows();
        const int cols = mask.cols();

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

        for (int y = 0; y < rows; ++y) {
            uint8_t* out_row = scratch.ptr(y);
            int y0 = std::max(0, y - half);
            int y1 = std::min(rows - 1, y + half);
            for (int x = 0; x < cols; ++x) {
                int x0    = std::max(0, x - half);
                int x1    = std::min(cols - 1, x + half);
                int count = sat(y1 + 1, x1 + 1) - sat(y0, x1 + 1)
                          - sat(y1 + 1, x0)     + sat(y0, x0);
                int total = (y1 - y0 + 1) * (x1 - x0 + 1);
                out_row[x] = (count > total / 2) ? 255 : 0;
            }
        }

        std::memcpy(mask.data(), scratch.data(), scratch.total_bytes());
    }
}

// ---------------------------------------------------------------------------
// Isolated hot-loop benchmarks: scalar vs SIMD
// ---------------------------------------------------------------------------

TEST_CASE("perf: chebyshev_threshold scalar vs SIMD, 1920x1080", "[perf]") {
    constexpr int W = 1920, H = 1080, reps = 200;
    auto src = make_bgr_image(W, H);
    cc::Image mask(H, W, 1);
    cc::Color3 color(128, 128, 128);

    long long scalar_ms = time_ms([&] {
        for (int i = 0; i < reps; ++i)
            chebyshev_threshold_scalar(color, 30, src, mask);
    });
    long long simd_ms = time_ms([&] {
        for (int i = 0; i < reps; ++i)
            cc::processing::chebyshev_threshold(color, 30, src, mask);
    });

    std::cout << "[perf] chebyshev_threshold x" << reps << " @ 1920x1080\n"
              << "  scalar : " << scalar_ms << " ms  (" << scalar_ms / reps << " ms/frame)\n"
              << "  SIMD   : " << simd_ms   << " ms  (" << simd_ms   / reps << " ms/frame)\n"
              << "  speedup: " << (simd_ms > 0 ? static_cast<double>(scalar_ms) / simd_ms : 0.0)
              << "x\n";
    SUCCEED();
}

TEST_CASE("perf: invert_mask scalar vs SIMD, 1920x1080", "[perf]") {
    constexpr int W = 1920, H = 1080, reps = 1000;
    cc::Image mask(H, W, 1);
    std::fill(mask.data(), mask.data() + mask.total_bytes(), uint8_t{255});

    long long scalar_ms = time_ms([&] {
        for (int i = 0; i < reps; ++i)
            invert_mask_scalar(mask);
    });
    long long simd_ms = time_ms([&] {
        for (int i = 0; i < reps; ++i)
            cc::processing::invert_mask(mask);
    });

    std::cout << "[perf] invert_mask x" << reps << " @ 1920x1080\n"
              << "  scalar : " << scalar_ms << " ms  (" << scalar_ms / reps << " ms/frame)\n"
              << "  SIMD   : " << simd_ms   << " ms  (" << simd_ms   / reps << " ms/frame)\n"
              << "  speedup: " << (simd_ms > 0 ? static_cast<double>(scalar_ms) / simd_ms : 0.0)
              << "x\n";
    SUCCEED();
}

TEST_CASE("perf: majority_vote_blur scalar vs SIMD, 1920x1080", "[perf]") {
    constexpr int W = 1920, H = 1080, reps = 50;
    // Two independent masks so each path starts from the same initial state.
    cc::Image mask_s(H, W, 1), mask_v(H, W, 1);
    cc::Image scratch(H, W, 1);
    for (int i = 0; i < static_cast<int>(mask_s.total_bytes()); ++i) {
        uint8_t v = (i % 7 < 4) ? 255 : 0; // non-trivial pattern
        mask_s.data()[i] = v;
        mask_v.data()[i] = v;
    }

    long long scalar_ms = time_ms([&] {
        for (int i = 0; i < reps; ++i)
            majority_vote_blur_scalar(mask_s, scratch);
    });
    long long simd_ms = time_ms([&] {
        for (int i = 0; i < reps; ++i)
            cc::processing::majority_vote_blur(mask_v, scratch);
    });

    std::cout << "[perf] majority_vote_blur x" << reps << " @ 1920x1080\n"
              << "  scalar : " << scalar_ms << " ms  (" << scalar_ms / reps << " ms/frame)\n"
              << "  SIMD   : " << simd_ms   << " ms  (" << simd_ms   / reps << " ms/frame)\n"
              << "  speedup: " << (simd_ms > 0 ? static_cast<double>(scalar_ms) / simd_ms : 0.0)
              << "x\n";
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Full pipeline benchmarks: context for how the hot loops fit in practice
// ---------------------------------------------------------------------------

TEST_CASE("perf: determine_foreground full pipeline, 1920x1080", "[perf]") {
    constexpr int W = 1920, H = 1080, reps = 100;
    auto src = make_bgr_image(W, H);
    cc::Image mask(H, W, 1);
    cc::Image fg  (H, W, 3);
    cc::Image tmp (H, W, 1);
    cc::Color3 color(128, 128, 128);

    long long hsv_ms = time_ms([&] {
        for (int i = 0; i < reps; ++i)
            cc::processing::determine_foreground(color, 30, src, mask, fg, tmp, false, false);
    });
    long long cheb_ms = time_ms([&] {
        for (int i = 0; i < reps; ++i)
            cc::processing::determine_foreground(color, 30, src, mask, fg, tmp, false, true);
    });

    std::cout << "[perf] determine_foreground (full pipeline) x" << reps << " @ 1920x1080\n"
              << "  HSV (scalar) : " << hsv_ms  << " ms  (" << hsv_ms  / reps << " ms/frame)\n"
              << "  Cheb (SIMD)  : " << cheb_ms << " ms  (" << cheb_ms / reps << " ms/frame)\n"
              << "  speedup      : "
              << (cheb_ms > 0 ? static_cast<double>(hsv_ms) / cheb_ms : 0.0) << "x\n";
    SUCCEED();
}

TEST_CASE("perf: determine_foreground_by_edges, 1920x1080", "[perf]") {
    constexpr int W = 1920, H = 1080, reps = 10;
    auto src = make_bgr_image(W, H);
    cc::Image mask(H, W, 1);
    cc::Image fg  (H, W, 3);

    long long ms = time_ms([&] {
        for (int i = 0; i < reps; ++i)
            cc::processing::determine_foreground_by_edges(50, src, mask, fg);
    });

    std::cout << "[perf] determine_foreground_by_edges x" << reps << " @ 1920x1080\n"
              << "  " << ms << " ms  (" << ms / reps << " ms/frame)\n";
    SUCCEED();
}

TEST_CASE("perf: fit_gear_template (solid disc), 1920x1080", "[perf]") {
    // Worst case: a solid disc fills the whole annular IoU band [r_in, r_tip].
    constexpr int W = 1920, H = 1080, reps = 50;
    cc::Image mask(H, W, 1);
    std::memset(mask.data(), 0, mask.total_bytes());
    const int cx = W / 2, cy = H / 2, R = 200;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= R * R)
                mask.ptr(y)[x] = 255;

    long long ms = time_ms([&] {
        for (int i = 0; i < reps; ++i)
            (void)cc::processing::fit_gear_template(mask, {cx, cy}, 27);
    });

    std::cout << "[perf] fit_gear_template x" << reps << " @ 1920x1080 (R=200 disc)\n"
              << "  " << ms << " ms  (" << static_cast<double>(ms) / reps << " ms/frame)\n";
    SUCCEED();
}
