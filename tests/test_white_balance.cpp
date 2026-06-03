// tests/test_white_balance.cpp — color-cast metric for auto white balance.
#include <catch2/catch_test_macros.hpp>

#include <cstring>

#include "processing/white_balance.h"
#include "types/image.h"

using namespace cc;
using namespace cc::processing;

namespace {
    cc::Image solid(int w, int h, uint8_t b, uint8_t g, uint8_t r) {
        cc::Image img(h, w, 3);
        for (int y = 0; y < h; ++y) {
            uint8_t* row = img.ptr(y);
            for (int x = 0; x < w; ++x) { row[x*3+0]=b; row[x*3+1]=g; row[x*3+2]=r; }
        }
        return img;
    }
}

TEST_CASE("color_imbalance - neutral image is ~0", "[white_balance]") {
    REQUIRE(color_imbalance(solid(200, 200, 150, 150, 150)) == 0.0);
}

TEST_CASE("color_imbalance - a colour cast scores high", "[white_balance]") {
    // Blue-tinted (B high, R low) — the symptom the user reports.
    double blue = color_imbalance(solid(200, 200, 200, 150, 90));
    REQUIRE(blue > 50.0);
    // Worse cast -> higher score (monotone in the cast magnitude).
    double worse = color_imbalance(solid(200, 200, 230, 150, 60));
    REQUIRE(worse > blue);
}

TEST_CASE("color_imbalance - a small chromatic object does NOT bias the metric", "[white_balance]") {
    // Neutral background with a saturated gold patch covering ~16% of the frame.
    // The per-channel median ignores the outlier, so the scene still reads neutral —
    // unlike gray-world-on-the-mean, which would be pulled warm (and the WB pushed blue).
    cc::Image img = solid(200, 200, 150, 150, 150);
    for (int y = 50; y < 130; ++y)        // 80x80 patch = 16% of 200x200
        for (int x = 50; x < 130; ++x) {
            uint8_t* p = img.at(y, x);
            p[0] = 30; p[1] = 160; p[2] = 210; // gold (B low, R high)
        }
    REQUIRE(color_imbalance(img) <= 2.0);  // still effectively neutral
}
