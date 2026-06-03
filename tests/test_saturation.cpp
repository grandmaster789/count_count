// tests/test_saturation.cpp
//
// Saturation-based foreground segmentation: separates a chromatic object from an
// achromatic (grey/white) background, robustly under brightness shadows. Verifies
// saturation_threshold(), detect_saturation_threshold() calibration, and the full
// determine_foreground_by_saturation() -> find_contours() -> process_contours()
// pipeline on a synthetic chromatic gear sitting on a shaded grey background.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <filesystem>
#include <numbers>

#include "io/jpg.h"
#include "io/data_location.h"
#include "processing/foreground.h"
#include "processing/auto_sensitivity.h"
#include "processing/boundary_trace.h"
#include "processing/contours.h"
#include "types/image.h"

using namespace cc;
using namespace cc::processing;

namespace {
    // A pixel's HSV saturation (OpenCV convention), mirrors the library.
    int sat_of(int b, int g, int r) {
        int vi = std::max({b, g, r});
        int mi = std::min({b, g, r});
        return (vi == 0) ? 0 : ((vi - mi) * 255 + vi / 2) / vi;
    }

    // Grey background with a strong left->right brightness gradient (simulated
    // shadow): value ramps but the pixel stays near-achromatic (low saturation),
    // so a saturation threshold must reject all of it regardless of brightness.
    void fill_shaded_grey(cc::Image& img) {
        for (int y = 0; y < img.rows(); ++y) {
            uint8_t* row = img.ptr(y);
            for (int x = 0; x < img.cols(); ++x) {
                int v = 110 + (90 * x) / std::max(1, img.cols() - 1); // 110..200
                // tiny tint so background saturation is small but non-zero (~5)
                int b = v;
                int g = v - 2;
                int r = v - 4;
                row[x * 3 + 0] = (uint8_t)b;
                row[x * 3 + 1] = (uint8_t)g;
                row[x * 3 + 2] = (uint8_t)r;
            }
        }
    }

    // Draw a saturated (gold-ish) square-tooth gear centred in the image.
    void draw_gear(cc::Image& img, int cx, int cy, double base, double amp, int teeth) {
        for (int y = 0; y < img.rows(); ++y) {
            uint8_t* row = img.ptr(y);
            for (int x = 0; x < img.cols(); ++x) {
                double dx = x - cx, dy = y - cy;
                double r = std::hypot(dx, dy);
                double theta = std::atan2(dy, dx);
                double edge = base + amp * (std::cos(teeth * theta) >= 0 ? 1.0 : -1.0);
                if (r <= edge) {
                    // BGR gold: V=200, min=40 -> S ~ 204 (strongly chromatic)
                    row[x * 3 + 0] = 40;
                    row[x * 3 + 1] = 160;
                    row[x * 3 + 2] = 200;
                }
            }
        }
    }

    long fill_count(const cc::Image& mask) {
        long f = 0;
        for (int y = 0; y < mask.rows(); ++y) {
            const uint8_t* r = mask.ptr(y);
            for (int x = 0; x < mask.cols(); ++x) if (r[x]) ++f;
        }
        return f;
    }
}

TEST_CASE("saturation_threshold - separates chromatic from achromatic", "[saturation]") {
    cc::Image img(40, 40, 3);
    // left half achromatic grey (S=0), right half saturated red (S=255)
    for (int y = 0; y < 40; ++y) {
        uint8_t* row = img.ptr(y);
        for (int x = 0; x < 40; ++x) {
            if (x < 20) { row[x*3+0] = 150; row[x*3+1] = 150; row[x*3+2] = 150; }
            else        { row[x*3+0] = 0;   row[x*3+1] = 0;   row[x*3+2] = 200; }
        }
    }
    cc::Image mask(40, 40, 1);
    std::memset(mask.data(), 0, mask.total_bytes());
    saturation_threshold(100, img, mask);

    // every right-half pixel set, every left-half pixel clear
    for (int y = 0; y < 40; ++y) {
        for (int x = 0; x < 40; ++x) {
            uint8_t m = mask.ptr(y)[x];
            if (x < 20) REQUIRE(m == 0);
            else        REQUIRE(m == 255);
        }
    }
}

TEST_CASE("detect_saturation_threshold - lands between background and object", "[saturation]") {
    cc::Image img(400, 400, 3);
    fill_shaded_grey(img);
    draw_gear(img, 200, 200, 150, 0, 0); // solid saturated disc, no teeth

    int t = detect_saturation_threshold(img);

    // background saturation here is ~5; the gold disc is ~204.
    // A good threshold sits comfortably above the background and below the object.
    REQUIRE(t >= 25);   // never floods into the low-S background
    REQUIRE(t < 204);   // never erodes the chromatic object
}

TEST_CASE("detect_saturation_threshold - achromatic-only image falls back", "[saturation]") {
    cc::Image img(100, 100, 3);
    fill_shaded_grey(img); // no chromatic object at all
    int t = detect_saturation_threshold(img);
    REQUIRE(t >= 25);     // clamped/default, still a sane positive threshold
    REQUIRE(t <= 180);
}

TEST_CASE("determine_foreground_by_saturation - isolates gear under shadow", "[saturation]") {
    cc::Image img(400, 400, 3);
    fill_shaded_grey(img);                 // strong brightness gradient
    draw_gear(img, 200, 200, 150, 12, 20); // 20-tooth gold gear

    cc::Image mask(400, 400, 1);
    cc::Image fg(400, 400, 3);
    cc::Image blur(400, 400, 1);

    int t = detect_saturation_threshold(img);
    determine_foreground_by_saturation(t, img, mask, fg, blur);

    long filled = fill_count(mask);
    double frac = (double)filled / (400.0 * 400.0);

    // The gear (~base radius 150) occupies roughly pi*150^2/160000 ~ 44% of the
    // frame; the shaded grey background must contribute essentially nothing.
    REQUIRE(frac > 0.30);
    REQUIRE(frac < 0.55);

    // Exactly one dominant contour (the gear), not the background.
    auto contours = find_contours(mask);
    REQUIRE(!contours.empty());
    auto result = process_contours(contours, img);
    REQUIRE(result.has_value());

    // FFT pitch estimate recovers the true tooth count for a clean synthetic gear.
    REQUIRE(result->m_SpeculativeCount == 20);
}

// ---------------------------------------------------------------------------
// Real reference-image regression: runs the production saturation -> contour ->
// count path on the actual controlled-setup gears and asserts against the known
// true tooth counts. Skips gracefully if the data folder isn't present.
//   true counts: test_real_gear_004 = 27, test_real_gear_003 = 84
// 003 is capture-resolution-limited (~7px tooth pitch) so the FFT estimate lands
// a hair short of 84; the band below reflects that. See [[saturation-segmentation]].
// ---------------------------------------------------------------------------

namespace {
    // Run the production path and return the speculative (FFT) tooth count, or -1.
    int real_gear_fft_count(const std::filesystem::path& jpg) {
        cc::Image img = cc::io::load_jpg(jpg);
        cc::Image mask(img.rows(), img.cols(), 1);
        cc::Image fg(img.rows(), img.cols(), 3);
        cc::Image blur(img.rows(), img.cols(), 1);

        int t = detect_saturation_threshold(img);
        determine_foreground_by_saturation(t, img, mask, fg, blur);

        auto contours = find_contours(mask);
        if (contours.empty()) return -1;
        auto result = process_contours(contours, img);
        return result ? result->m_SpeculativeCount : -1;
    }

    std::filesystem::path locate_data() {
        try {
            return cc::find_data_folder(std::filesystem::current_path());
        } catch (...) {
            return {};
        }
    }
}

TEST_CASE("real gears - saturation pipeline recovers known tooth counts", "[saturation][realimage]") {
    std::filesystem::path data = locate_data();
    if (data.empty() || !std::filesystem::exists(data / "test_real_gear_004.jpg"))
        SKIP("reference gear images not found next to the test binary");

    // 004 (~24-tooth brass gear): FFT recovers the true count exactly.
    int c004 = real_gear_fft_count(data / "test_real_gear_004.jpg");
    REQUIRE(c004 == 27);

    // 003 (fine-toothed gold rim gear): true count 84, but resolution-limited.
    // The FFT estimate must land in a tight band near the truth (not the wildly
    // aliased direct count). This guards the segmentation: a regression that
    // re-contaminates the mask with shadow would blow this band.
    int c003 = real_gear_fft_count(data / "test_real_gear_003.jpg");
    REQUIRE(c003 >= 80);
    REQUIRE(c003 <= 86);
}

namespace {
    // FFT count at a specific saturation threshold.
    int count_at(const std::filesystem::path& jpg, int t) {
        cc::Image img = cc::io::load_jpg(jpg);
        cc::Image mask(img.rows(), img.cols(), 1), fg(img.rows(), img.cols(), 3), blur(img.rows(), img.cols(), 1);
        determine_foreground_by_saturation(t, img, mask, fg, blur);
        auto contours = find_contours(mask);
        if (contours.empty()) return -1;
        auto r = process_contours(contours, img);
        return r ? r->m_SpeculativeCount : -1;
    }
}

TEST_CASE("real gears - count-guided threshold recovers a hard worn gear", "[saturation][realimage]") {
    std::filesystem::path data = locate_data();
    if (data.empty() || !std::filesystem::exists(data / "test_real_gear_005.jpg"))
        SKIP("reference gear images not found next to the test binary");

    // 005: worn 72-tooth gear on a more-saturated wall. The cheap histogram
    // threshold overshoots and breaks the rim (FFT collapses to ~4-6); the
    // count-guided search must find a threshold that recovers ~72.
    int t5 = detect_saturation_threshold_by_teeth(cc::io::load_jpg(data / "test_real_gear_005.jpg"));
    int c5 = count_at(data / "test_real_gear_005.jpg", t5);
    REQUIRE(c5 >= 66);
    REQUIRE(c5 <= 76);

    // Must not regress the clean gears.
    int t4 = detect_saturation_threshold_by_teeth(cc::io::load_jpg(data / "test_real_gear_004.jpg"));
    REQUIRE(count_at(data / "test_real_gear_004.jpg", t4) == 27);

    int t3 = detect_saturation_threshold_by_teeth(cc::io::load_jpg(data / "test_real_gear_003.jpg"));
    int c3 = count_at(data / "test_real_gear_003.jpg", t3);
    REQUIRE(c3 >= 80);
    REQUIRE(c3 <= 86);
}
