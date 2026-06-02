// tests/test_gear_template.cpp
//
// analysis-by-synthesis goodness-of-fit: fit_gear_template() fits an ideal N-tooth
// gear to the rim sampled from a mask and returns a confidence score + overlay.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <numbers>

#include "io/jpg.h"
#include "io/data_location.h"
#include "processing/gear_template.h"
#include "processing/foreground.h"
#include "processing/auto_sensitivity.h"
#include "processing/boundary_trace.h"
#include "processing/contours.h"
#include "types/image.h"

using namespace cc;
using namespace cc::processing;

namespace {
    // Rasterize a solid N-tooth gear silhouette into a 1-channel 0/255 mask.
    cc::Image gear_mask(int w, int h, int cx, int cy, double base, double amp, int teeth, double duty = 0.5) {
        cc::Image m(h, w, 1);
        std::memset(m.data(), 0, m.total_bytes());
        double period = 2.0 * std::numbers::pi / teeth;
        for (int y = 0; y < h; ++y) {
            uint8_t* row = m.ptr(y);
            for (int x = 0; x < w; ++x) {
                double dx = x - cx, dy = y - cy;
                double r = std::hypot(dx, dy);
                double th = std::atan2(dy, dx);
                double frac = std::fmod(th + 8.0 * std::numbers::pi, period) / period;
                double edge = base + (frac < duty ? amp : 0.0);
                if (r <= edge) row[x] = 255;
            }
        }
        return m;
    }

    std::filesystem::path locate_data() {
        try { return cc::find_data_folder(std::filesystem::current_path()); }
        catch (...) { return {}; }
    }
}

TEST_CASE("fit_gear_template - clean gear scores as a confident fit", "[gear_template]") {
    cc::Image m = gear_mask(400, 400, 200, 200, 150, 14, 24);
    GearFit fit = fit_gear_template(m, {200, 200}, 24);

    REQUIRE(fit.valid);
    REQUIRE(fit.teeth == 24);
    REQUIRE(fit.score > 0.85);         // the rendered gear explains the silhouette well
    REQUIRE(fit.outline.size() > 1);   // usable overlay
}

TEST_CASE("fit_gear_template - damaged segmentation lowers the fit score", "[gear_template]") {
    // The score is a shape/segmentation confidence: erasing a wedge of the gear
    // (bad segmentation / occlusion) must drop the fit below the clean case.
    cc::Image clean = gear_mask(400, 400, 200, 200, 150, 14, 24);
    cc::Image damaged = clean.clone();
    for (int y = 0; y < 400; ++y)            // wipe the right-hand quadrant
        for (int x = 200; x < 400; ++x)
            if (y < 200) damaged.ptr(y)[x] = 0;

    double s_clean   = fit_gear_template(clean,   {200, 200}, 24).score;
    double s_damaged = fit_gear_template(damaged, {200, 200}, 24).score;
    REQUIRE(s_damaged < s_clean);
    REQUIRE(s_damaged < 0.85);
}

TEST_CASE("fit_gear_template - too few teeth is invalid", "[gear_template]") {
    cc::Image m = gear_mask(400, 400, 200, 200, 150, 14, 24);
    GearFit fit = fit_gear_template(m, {200, 200}, 3);
    REQUIRE(!fit.valid);
}

TEST_CASE("fit_gear_template - real gears score as confident gears", "[gear_template][realimage]") {
    std::filesystem::path data = locate_data();
    if (data.empty() || !std::filesystem::exists(data / "test_real_gear_004.jpg"))
        SKIP("reference gear images not found next to the test binary");

    for (const char* name : {"test_real_gear_003.jpg", "test_real_gear_004.jpg"}) {
        cc::Image img = cc::io::load_jpg(data / name);
        cc::Image mask(img.rows(), img.cols(), 1), fg(img.rows(), img.cols(), 3), blur(img.rows(), img.cols(), 1);
        int t = detect_saturation_threshold(img);
        determine_foreground_by_saturation(t, img, mask, fg, blur);
        auto contours = find_contours(mask);
        auto res = process_contours(contours, img);
        REQUIRE(res.has_value());

        GearFit fit = fit_gear_template(mask, res->m_Centroid, res->m_SpeculativeCount);
        std::cout << "  " << name << " count=" << res->m_SpeculativeCount
                  << " fit.score=" << fit.score << "\n";
        REQUIRE(fit.valid);
        // Clean real gears clear the low-confidence flag (0.40). Measured: solid
        // disc 004 ~0.97, thin-ring 003 ~0.52.
        REQUIRE(fit.score > 0.40);
    }
}
