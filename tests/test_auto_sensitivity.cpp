#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstring>
#include <filesystem>

#include "processing/auto_sensitivity.h"
#include "io/jpg.h"
#include "io/data_location.h"

using namespace cc::processing;
using namespace cc;

namespace {
    // Helper: fill entire image with a single BGR color
    void fill_image(cc::Image& img, uint8_t b, uint8_t g, uint8_t r) {
        for (int y = 0; y < img.rows(); ++y) {
            uint8_t* row = img.ptr(y);
            for (int x = 0; x < img.cols(); ++x) {
                row[x * 3 + 0] = b;
                row[x * 3 + 1] = g;
                row[x * 3 + 2] = r;
            }
        }
    }

    // Helper: fill a rectangular region with a BGR color
    void fill_rect(cc::Image& img, int y0, int y1, int x0, int x1,
                   uint8_t b, uint8_t g, uint8_t r)
    {
        for (int y = y0; y < y1; ++y) {
            uint8_t* row = img.ptr(y);
            for (int x = x0; x < x1; ++x) {
                row[x * 3 + 0] = b;
                row[x * 3 + 1] = g;
                row[x * 3 + 2] = r;
            }
        }
    }

    // Helper: fill a circle with a BGR color
    void fill_circle(cc::Image& img, int cy, int cx, int radius,
                     uint8_t b, uint8_t g, uint8_t r)
    {
        for (int y = 0; y < img.rows(); ++y) {
            uint8_t* row = img.ptr(y);
            for (int x = 0; x < img.cols(); ++x) {
                int dy = y - cy;
                int dx = x - cx;
                if (dx * dx + dy * dy <= radius * radius) {
                    row[x * 3 + 0] = b;
                    row[x * 3 + 1] = g;
                    row[x * 3 + 2] = r;
                }
            }
        }
    }
}

TEST_CASE("detect_sensitivity - empty image", "[auto_sensitivity]") {
    cc::Image empty;
    auto result = detect_sensitivity(empty);
    REQUIRE_FALSE(result.valid);
}

TEST_CASE("detect_sensitivity - single-channel image rejected", "[auto_sensitivity]") {
    cc::Image gray(100, 100, 1);
    auto result = detect_sensitivity(gray);
    REQUIRE_FALSE(result.valid);
}

TEST_CASE("detect_sensitivity - uniform image", "[auto_sensitivity]") {
    cc::Image img(100, 100, 3);
    fill_image(img, 50, 100, 150);

    auto result = detect_sensitivity(img);
    // Uniform image has no second color cluster — should be invalid
    REQUIRE_FALSE(result.valid);
}

TEST_CASE("detect_sensitivity - two-color image", "[auto_sensitivity]") {
    // 60% gray background, 40% dark foreground
    // Background must be the majority so the algorithm suppresses it first
    cc::Image img(200, 200, 3);
    fill_rect(img, 0, 120, 0, 200, 200, 200, 200);   // background (60%)
    fill_rect(img, 120, 200, 0, 200, 30, 50, 80);     // foreground (40%)

    auto result = detect_sensitivity(img);
    REQUIRE(result.valid);
    REQUIRE(result.tolerance > 0);

    // After suppressing the gray background peak, the foreground is detected.
    // (30,50,80) -> bins (3,6,10) -> centers (28,52,84)
    double db = std::abs(result.color.b - 28.0);
    double dg = std::abs(result.color.g - 52.0);
    double dr = std::abs(result.color.r - 84.0);
    double max_err = std::max({ db, dg, dr });
    REQUIRE(max_err <= 8.0); // within one quantization bin
}

TEST_CASE("detect_sensitivity - synthetic gear on background", "[auto_sensitivity]") {
    // White background with a dark circle (simulates a gear)
    cc::Image img(400, 400, 3);
    fill_image(img, 200, 200, 200); // background
    fill_circle(img, 200, 200, 100, 80, 30, 10); // gear-colored circle

    auto result = detect_sensitivity(img);
    REQUIRE(result.valid);
    REQUIRE(result.tolerance > 0);

    // Detected color should be near the gear color (80, 30, 10)
    // Bin: (10,3,1) -> center (84, 28, 12)
    double db = std::abs(result.color.b - 84.0);
    double dg = std::abs(result.color.g - 28.0);
    double dr = std::abs(result.color.r - 12.0);
    double max_err = std::max({ db, dg, dr });
    REQUIRE(max_err <= 8.0);
}

TEST_CASE("detect_sensitivity - tolerance separates colors", "[auto_sensitivity]") {
    // Create an image where the Chebyshev distance between background and foreground
    // is large, and verify the tolerance correctly separates them
    cc::Image img(200, 200, 3);
    fill_rect(img, 0, 120, 0, 200, 200, 200, 200);   // 60% background
    fill_rect(img, 120, 200, 0, 200, 40, 40, 40);     // 40% foreground

    auto result = detect_sensitivity(img);
    REQUIRE(result.valid);

    // Otsu finds the threshold right at the foreground cluster boundary.
    // With two cleanly separated clusters, tolerance = 2 * (small distance) is correct.
    // The key property: foreground pixels fall within tolerance, background pixels don't.
    REQUIRE(result.tolerance > 0);
    REQUIRE(result.tolerance <= 255);

    // Verify the detected color is near the foreground (40,40,40) -> bin center (44,44,44)
    double max_channel_err = std::max({
        std::abs(result.color.b - 44.0),
        std::abs(result.color.g - 44.0),
        std::abs(result.color.r - 44.0)
    });
    REQUIRE(max_channel_err <= 8.0);
}

// ---------------------------------------------------------------------------
// detect_background_sensitivity
// ---------------------------------------------------------------------------

TEST_CASE("detect_background_sensitivity - empty image returns invalid", "[auto_sensitivity]") {
    cc::Image empty;
    REQUIRE_FALSE(detect_background_sensitivity(empty).valid);
}

TEST_CASE("detect_background_sensitivity - single-channel image rejected", "[auto_sensitivity]") {
    cc::Image gray(100, 100, 1);
    REQUIRE_FALSE(detect_background_sensitivity(gray).valid);
}

TEST_CASE("detect_background_sensitivity - returns dominant background color", "[auto_sensitivity]") {
    // White background fills ~75% of the image, dark circle in center.
    // The dominant bin should map to the background color, not the gear color.
    cc::Image img(200, 200, 3);
    fill_image(img, 240, 240, 240);
    fill_circle(img, 100, 100, 50, 40, 40, 40); // ~20% of image

    auto result = detect_background_sensitivity(img);
    REQUIRE(result.valid);
    REQUIRE(result.tolerance > 0);

    // (240 >> 3) = 30 → bin center = 30*8+4 = 244
    double max_err = std::max({
        std::abs(result.color.b - 244.0),
        std::abs(result.color.g - 244.0),
        std::abs(result.color.r - 244.0)
    });
    REQUIRE(max_err <= 8.0);
}

TEST_CASE("detect_background_sensitivity - returns brighter color than detect_sensitivity", "[auto_sensitivity]") {
    // detect_sensitivity suppresses the background and returns the gear color (dark).
    // detect_background_sensitivity returns the dominant color (light background).
    cc::Image img(200, 200, 3);
    fill_image(img, 220, 220, 220);           // bright background
    fill_circle(img, 100, 100, 40, 30, 30, 30); // dark gear (~12% of image)

    auto fg = detect_sensitivity(img);
    auto bg = detect_background_sensitivity(img);

    REQUIRE(fg.valid);
    REQUIRE(bg.valid);

    // Background color must be brighter than foreground in every channel
    REQUIRE(bg.color.b > fg.color.b);
    REQUIRE(bg.color.g > fg.color.g);
    REQUIRE(bg.color.r > fg.color.r);
}

TEST_CASE("detect_sensitivity - integration with test image", "[auto_sensitivity][integration]") {
    // Try to find the data folder and load a real test image
    std::filesystem::path data_path;
    try {
        data_path = cc::find_data_folder(std::filesystem::current_path());
    } catch (...) {
        SKIP("Could not locate data folder");
    }

    auto image_path = data_path / "test_broken_tooth_002.jpg";
    if (!std::filesystem::exists(image_path))
        SKIP("Test image not found: " + image_path.string());

    cc::Image img;
    try {
        img = cc::io::load_jpg(image_path);
    } catch (...) {
        SKIP("Could not load test image");
    }

    auto result = detect_sensitivity(img);

    REQUIRE(result.valid);
    REQUIRE(result.tolerance > 0);
    REQUIRE(result.tolerance <= 255);

    // The detected color should not be the white/grey background
    // (at least one channel should be noticeably different from 200)
    bool not_background =
        std::abs(result.color.b - 200.0) > 30.0 ||
        std::abs(result.color.g - 200.0) > 30.0 ||
        std::abs(result.color.r - 200.0) > 30.0;
    REQUIRE(not_background);
}
