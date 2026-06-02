// tests/test_focus.cpp — contrast-detection autofocus core (no camera).
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstring>
#include <functional>

#include "processing/focus.h"
#include "types/image.h"

using namespace cc;
using namespace cc::processing;

namespace {
    // Sharp vertical stripes (hard edges every `period` px) in BGR.
    cc::Image stripes(int w, int h, int period, uint8_t lo = 30, uint8_t hi = 220) {
        cc::Image img(h, w, 3);
        for (int y = 0; y < h; ++y) {
            uint8_t* row = img.ptr(y);
            for (int x = 0; x < w; ++x) {
                uint8_t v = ((x / period) & 1) ? hi : lo;
                row[x * 3 + 0] = v; row[x * 3 + 1] = v; row[x * 3 + 2] = v;
            }
        }
        return img;
    }

    // Simple separable-ish 3x3 box blur (one pass), BGR.
    cc::Image box_blur(const cc::Image& src) {
        cc::Image out = src.clone();
        for (int y = 1; y < src.rows() - 1; ++y)
            for (int x = 1; x < src.cols() - 1; ++x)
                for (int c = 0; c < 3; ++c) {
                    int s = 0;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                            s += src.at(y + dy, x + dx)[c];
                    out.at(y, x)[c] = (uint8_t)(s / 9);
                }
        return out;
    }

    FocusRoi whole(const cc::Image& img) { return {0, 0, img.cols() - 1, img.rows() - 1}; }
}

TEST_CASE("focus_measure - sharp scores higher than blurred", "[focus]") {
    cc::Image sharp = stripes(200, 200, 4);
    cc::Image blurred = box_blur(box_blur(sharp));  // two passes -> clearly softer

    double s_sharp = focus_measure(sharp, whole(sharp));
    double s_blur  = focus_measure(blurred, whole(blurred));

    REQUIRE(s_sharp > 0.0);
    REQUIRE(s_sharp > s_blur);
}

TEST_CASE("focus_measure - clipped specular highlights are excluded", "[focus]") {
    // Flat gray field (no real detail) with a hard-edged clipped-white square.
    cc::Image img(200, 200, 3);
    std::memset(img.data(), 128, img.total_bytes());
    for (int y = 80; y < 120; ++y)
        for (int x = 80; x < 120; ++x) {
            uint8_t* p = img.at(y, x);
            p[0] = p[1] = p[2] = 255;  // clipped glint
        }

    double excluded = focus_measure(img, whole(img), 250); // glint dropped
    double included = focus_measure(img, whole(img), 256); // nothing dropped

    REQUIRE(included > excluded);        // the glint edge inflates the un-excluded score
    REQUIRE(excluded < 1.0);             // with exclusion, essentially no detail remains
}

TEST_CASE("roi_from_mask - bounds the foreground, falls back to centre", "[focus]") {
    cc::Image mask(100, 100, 1);
    std::memset(mask.data(), 0, mask.total_bytes());
    for (int y = 40; y <= 60; ++y)
        for (int x = 30; x <= 70; ++x)
            mask.ptr(y)[x] = 255;

    FocusRoi roi = roi_from_mask(mask, 5);
    REQUIRE(roi.valid());
    REQUIRE(roi.x0 == 25);  // 30 - 5
    REQUIRE(roi.x1 == 75);  // 70 + 5
    REQUIRE(roi.y0 == 35);
    REQUIRE(roi.y1 == 65);

    cc::Image empty(100, 100, 1);
    std::memset(empty.data(), 0, empty.total_bytes());
    FocusRoi c = roi_from_mask(empty);
    REQUIRE(c.valid());                  // central-third fallback
    REQUIRE(c.x0 > 0);
    REQUIRE(c.x1 < 100);
}

TEST_CASE("find_best_focus - locates a unimodal peak", "[focus]") {
    const long target = 350;
    auto eval = [&](long f) { return -std::pow((double)(f - target), 2.0); };

    long best = find_best_focus(0, 600, 5, eval);
    REQUIRE(std::abs(best - target) <= 5);  // within one fine step
}

TEST_CASE("find_best_focus - unimodal peak with noise", "[focus]") {
    const long target = 280;
    auto eval = [&](long f) {
        double base = -std::pow((double)(f - target), 2.0) / 100.0;
        double noise = ((f % 7) - 3) * 0.05;  // deterministic small ripple
        return base + noise;
    };
    long best = find_best_focus(0, 600, 5, eval);
    REQUIRE(std::abs(best - target) <= 15);
}

TEST_CASE("find_best_focus - peak at the range edge", "[focus]") {
    auto eval = [&](long f) { return (double)f; };  // monotonic increasing
    long best = find_best_focus(0, 600, 5, eval);
    REQUIRE(best >= 595);
}

TEST_CASE("find_best_focus - degenerate range returns min", "[focus]") {
    auto eval = [&](long) { return 1.0; };
    REQUIRE(find_best_focus(100, 100, 5, eval) == 100);
}
