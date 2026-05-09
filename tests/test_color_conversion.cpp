#include <catch2/catch_test_macros.hpp>

#include "types/color.h"
#include "types/image.h"
#include "processing/foreground.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static cc::Image solid_image(int w, int h, uint8_t b, uint8_t g, uint8_t r) {
    cc::Image img(h, w, 3);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            uint8_t* p = img.at(y, x);
            p[0] = b; p[1] = g; p[2] = r;
        }
    return img;
}

static int count_foreground(const cc::Image& mask) {
    int n = 0;
    for (int y = 0; y < mask.rows(); ++y)
        for (int x = 0; x < mask.cols(); ++x)
            if (mask.at(y, x)[0] > 0) ++n;
    return n;
}

// Run determine_foreground and return the mask (pre-allocated 1-channel).
static cc::Image run_foreground(const cc::Image& src, const cc::Color3& color, int tol,
                                bool invert = false, bool use_chebyshev = false) {
    cc::Image mask(src.rows(), src.cols(), 1);
    cc::Image fg  (src.rows(), src.cols(), 3);
    cc::Image tmp (src.rows(), src.cols(), 1);
    cc::processing::determine_foreground(color, tol, src, mask, fg, tmp, invert, use_chebyshev);
    return mask;
}

// ---------------------------------------------------------------------------
// Brightness invariance — the primary motivation for HSV thresholding
// ---------------------------------------------------------------------------

TEST_CASE("HSV foreground: same hue at different brightness levels are both detected", "[color_conversion]") {
    // With BGR thresholding a small tolerance fails to match the same hue
    // across brightness levels. HSV thresholding should match both because
    // hue is identical and val_tol (fixed 80) covers the ±60 V spread used here.
    //
    // BGR box check with tol=40: each channel ±20 → green range [130,170].
    // dark (90) and bright (210) are both outside that range → BGR would reject them.
    // HSV: all three have H=60, V diff ≤ 60 ≤ val_tol(80) → all accepted.
    cc::Image dark  = solid_image(10, 10,   0,  90,  0); // dark green,   V=90
    cc::Image mid   = solid_image(10, 10,   0, 150,  0); // medium green, V=150
    cc::Image bright= solid_image(10, 10,   0, 210,  0); // bright green, V=210

    cc::Color3 selected(0, 150, 0); // medium green as reference, V=150
    int tolerance = 40; // hue_tol=20, sat_tol=80(fixed), val_tol=80(fixed)

    auto mask_dark   = run_foreground(dark,   selected, tolerance);
    auto mask_mid    = run_foreground(mid,    selected, tolerance);
    auto mask_bright = run_foreground(bright, selected, tolerance);

    // All three brightness levels should be detected
    REQUIRE(count_foreground(mask_dark)   == 100);
    REQUIRE(count_foreground(mask_mid)    == 100);
    REQUIRE(count_foreground(mask_bright) == 100);
}

TEST_CASE("HSV foreground: different hue is rejected even at same brightness", "[color_conversion]") {
    // Blue pixels should not match when green is selected
    cc::Image blue_img = solid_image(10, 10, 150, 0, 0); // pure blue (BGR)
    cc::Color3 green(0, 150, 0);
    int tolerance = 40;

    auto mask = run_foreground(blue_img, green, tolerance);
    REQUIRE(count_foreground(mask) == 0);
}

// ---------------------------------------------------------------------------
// Hue wraparound — red sits near hue 0/180 boundary
// ---------------------------------------------------------------------------

TEST_CASE("HSV foreground: red hue wraparound — near-red colors both sides detected", "[color_conversion]") {
    // Red straddles the hue=0/180 boundary. Verify circular distance works in both directions.
    //
    // Pure red:     BGR(0,  0, 255) → H=0
    // Warm red:     BGR(0, 30, 255) → H≈7  (shifts toward yellow; linear dist from H=0 = 7)
    // Cool red:     BGR(30, 0, 255) → H≈177 (shifts toward magenta; linear dist = 177,
    //                                         but circular dist = 180-177 = 3)
    //
    // All three should match H=0 with hue_tol=20.

    cc::Image red_pure  = solid_image(10, 10,  0,  0, 255); // H=0
    cc::Image red_warm  = solid_image(10, 10,  0, 30, 255); // H≈7
    cc::Image red_cool  = solid_image(10, 10, 30,  0, 255); // H≈177 — tests wraparound

    cc::Color3 selected_red(0, 0, 255);
    int tolerance = 40; // hue_tol = 20

    REQUIRE(count_foreground(run_foreground(red_pure, selected_red, tolerance)) == 100);
    REQUIRE(count_foreground(run_foreground(red_warm, selected_red, tolerance)) == 100);
    REQUIRE(count_foreground(run_foreground(red_cool, selected_red, tolerance)) == 100);
}

TEST_CASE("HSV foreground: complementary color (cyan) rejected when red is selected", "[color_conversion]") {
    // Cyan: BGR(255, 255, 0) → H=90, which is far from red H=0
    cc::Image cyan_img = solid_image(10, 10, 255, 255, 0);
    cc::Color3 red(0, 0, 255);
    int tolerance = 40;

    auto mask = run_foreground(cyan_img, red, tolerance);
    REQUIRE(count_foreground(mask) == 0);
}

// ---------------------------------------------------------------------------
// Pure color hue values — verify known-correct HSV conversions indirectly
// ---------------------------------------------------------------------------

TEST_CASE("HSV foreground: pure primary colors detected by hue", "[color_conversion]") {
    // Pure green: H≈60 — should match green reference, not blue or red
    cc::Image green_img = solid_image(10, 10, 0, 255, 0);
    cc::Image blue_img  = solid_image(10, 10, 255, 0, 0);
    cc::Image red_img   = solid_image(10, 10, 0, 0, 255);

    cc::Color3 green_ref(0, 255, 0);
    int tolerance = 40;

    REQUIRE(count_foreground(run_foreground(green_img, green_ref, tolerance)) == 100);
    REQUIRE(count_foreground(run_foreground(blue_img,  green_ref, tolerance)) ==   0);
    REQUIRE(count_foreground(run_foreground(red_img,   green_ref, tolerance)) ==   0);
}

// ---------------------------------------------------------------------------
// Achromatic pixels (gray/black/white) — low saturation behaviour
// ---------------------------------------------------------------------------

TEST_CASE("HSV foreground: gray pixels rejected when chromatic color selected", "[color_conversion]") {
    // Gray has S=0; sat_tol=80 means any color with S > 80 from the target won't match
    // when a saturated color is selected. Pure gray (S=0) vs saturated green (S=255).
    cc::Image gray_img = solid_image(10, 10, 128, 128, 128);
    cc::Color3 green(0, 255, 0); // S=255
    int tolerance = 60;

    // |0 - 255| = 255 > sat_tol(80): gray is rejected
    auto mask = run_foreground(gray_img, green, tolerance);
    REQUIRE(count_foreground(mask) == 0);
}

// ---------------------------------------------------------------------------
// Original stability test (preserved, now verifies HSV behaviour)
// ---------------------------------------------------------------------------

TEST_CASE("Foreground mask stability under brightness variation", "[color_conversion]") {
    cc::Image img_dark  = solid_image(10, 10,   0, 100, 0); // dark green
    cc::Image img_bright= solid_image(10, 10,   0, 200, 0); // bright green

    cc::Color3 color(0, 150, 0);
    int tolerance = 200; // wide tolerance

    auto mask_dark   = run_foreground(img_dark,   color, tolerance);
    auto mask_bright = run_foreground(img_bright, color, tolerance);

    // Both masks should detect the foreground (all pixels match within tolerance)
    REQUIRE(count_foreground(mask_dark)   > 50);
    REQUIRE(count_foreground(mask_bright) > 50);
}

// ---------------------------------------------------------------------------
// Chebyshev distance path (use_chebyshev=true)
// ---------------------------------------------------------------------------

TEST_CASE("Chebyshev foreground: pixel within distance threshold is included", "[color_conversion]") {
    // White target (255,255,255), tolerance=30.
    // Near-white (230,230,230): max-channel diff = 25 ≤ 30 → in mask.
    // Far-white  (200,200,200): max-channel diff = 55 > 30 → not in mask.
    cc::Image near_white = solid_image(5, 5, 230, 230, 230);
    cc::Image far_white  = solid_image(5, 5, 200, 200, 200);
    cc::Color3 white(255, 255, 255);

    REQUIRE(count_foreground(run_foreground(near_white, white, 30, false, true)) == 25);
    REQUIRE(count_foreground(run_foreground(far_white,  white, 30, false, true)) ==  0);
}

TEST_CASE("Chebyshev foreground: uses max channel diff, not per-channel average", "[color_conversion]") {
    // B-diff=40, G-diff=5, R-diff=5 → Chebyshev=40. Tolerance=30 → rejected
    // even though two of three channels are well within range.
    cc::Image img = solid_image(5, 5, 215, 250, 250);
    cc::Color3 white(255, 255, 255);

    REQUIRE(count_foreground(run_foreground(img, white, 30, false, true)) == 0);
}

TEST_CASE("Chebyshev foreground: dark gray rejected from white target", "[color_conversion]") {
    // Both are achromatic so HSV hue is undefined. Chebyshev distance correctly
    // rejects on brightness alone: max-channel diff = 205 > 50.
    cc::Image dark_gray = solid_image(5, 5, 50, 50, 50);
    cc::Color3 white(255, 255, 255);

    REQUIRE(count_foreground(run_foreground(dark_gray, white, 50, false, true)) == 0);
}

// ---------------------------------------------------------------------------
// Invert path (invert=true)
// ---------------------------------------------------------------------------

TEST_CASE("Foreground invert: mask is flipped after blur", "[color_conversion]") {
    // Solid green. With invert=false → all 100 pixels match → mask all 255.
    // With invert=true  → all 100 pixels match → flipped to 0.
    cc::Image green = solid_image(10, 10, 0, 200, 0);
    cc::Color3 green_color(0, 200, 0);

    REQUIRE(count_foreground(run_foreground(green, green_color, 200, false)) == 100);
    REQUIRE(count_foreground(run_foreground(green, green_color, 200, true )) ==   0);
}

TEST_CASE("Foreground invert + Chebyshev: background subtraction scenario", "[color_conversion]") {
    // Top half white (255,255,255), bottom half dark (50,50,50).
    // Target=white, tolerance=60, invert=true, use_chebyshev=true:
    //   white pixels → dist=0 ≤ 60 → mask=255 → inverted to 0 (background)
    //   dark  pixels → dist=205 > 60 → mask=0 → inverted to 255 (foreground object)
    cc::Image img(10, 10, 3);
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x) {
            uint8_t* p = img.at(y, x);
            if (y < 5) { p[0] = 255; p[1] = 255; p[2] = 255; }
            else        { p[0] =  50; p[1] =  50; p[2] =  50; }
        }

    cc::Color3 white(255, 255, 255);
    auto mask = run_foreground(img, white, 60, true, true);

    int white_half_fg = 0, dark_half_fg = 0;
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x)
            (y < 5 ? white_half_fg : dark_half_fg) += (mask.at(y, x)[0] > 0) ? 1 : 0;

    REQUIRE(white_half_fg ==  0); // white → inverted to background
    REQUIRE(dark_half_fg  == 50); // dark  → inverted to foreground
}
