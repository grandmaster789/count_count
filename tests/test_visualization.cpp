#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

#include "gui/visualization.h"
#include "types/image.h"
#include "types/point.h"
#include "types/tooth_anomaly.h"
#include "types/tooth_measurement.h"

namespace {
    cc::Image white_image(int w, int h) {
        cc::Image img(h, w, 3);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                uint8_t* p = img.at(y, x);
                p[0] = 255; p[1] = 255; p[2] = 255;
            }
        return img;
    }

    int count_non_white(const cc::Image& img) {
        int n = 0;
        for (int y = 0; y < img.rows(); ++y)
            for (int x = 0; x < img.cols(); ++x) {
                const uint8_t* p = img.at(y, x);
                if (p[0] != 255 || p[1] != 255 || p[2] != 255) ++n;
            }
        return n;
    }

    int count_non_white_near(const cc::Image& img, int cx, int cy, int radius) {
        int n = 0;
        for (int y = cy - radius; y <= cy + radius; ++y)
            for (int x = cx - radius; x <= cx + radius; ++x) {
                if (y < 0 || y >= img.rows() || x < 0 || x >= img.cols()) continue;
                const uint8_t* p = img.at(y, x);
                if (p[0] != 255 || p[1] != 255 || p[2] != 255) ++n;
            }
        return n;
    }

    std::vector<cc::ToothMeasurement> uniform_teeth(int n, double min_dist = 40.0, double max_dist = 50.0) {
        std::vector<cc::ToothMeasurement> teeth;
        for (int i = 0; i < n; ++i) {
            cc::ToothMeasurement t;
            t.m_ToothIdx      = static_cast<size_t>(i + 1);
            t.m_StartingAngle = i * 2.0 * std::numbers::pi / n;
            t.m_EndingAngle   = (i + 0.5) * 2.0 * std::numbers::pi / n;
            t.m_MinDistance   = min_dist;
            t.m_MaxDistance   = max_dist;
            teeth.push_back(t);
        }
        return teeth;
    }
}

// ---------------------------------------------------------------------------
// draw_gear_arrow
// ---------------------------------------------------------------------------

TEST_CASE("draw_gear_arrow - draws pixels on a blank image", "[visualization]") {
    cc::Image img = white_image(200, 200);
    REQUIRE(count_non_white(img) == 0);

    cc::draw_gear_arrow(img, cc::Point2d(100, 100), 60.0, 0.0);

    REQUIRE(count_non_white(img) > 0);
}

TEST_CASE("draw_gear_arrow - draws in the correct angular direction", "[visualization]") {
    // An arrow at angle=0 (pointing right) should produce pixels right of center.
    // An arrow at angle=pi/2 (pointing down) should produce pixels below center.
    cc::Image img_right = white_image(200, 200);
    cc::Image img_down  = white_image(200, 200);

    cc::draw_gear_arrow(img_right, cc::Point2d(100, 100), 60.0, 0.0);          // →
    cc::draw_gear_arrow(img_down,  cc::Point2d(100, 100), 60.0, std::numbers::pi / 2.0); // ↓

    // Pixels right of center from the rightward arrow
    int right_of_center = count_non_white_near(img_right, 140, 100, 30);
    // Pixels below center from the downward arrow
    int below_center    = count_non_white_near(img_down,  100, 140, 30);

    REQUIRE(right_of_center > 0);
    REQUIRE(below_center    > 0);
}

// ---------------------------------------------------------------------------
// display_results
// ---------------------------------------------------------------------------

TEST_CASE("display_results - renders centroid marker and tooth count text", "[visualization]") {
    cc::Image img = white_image(300, 300);
    auto teeth = uniform_teeth(8);
    std::vector<uint8_t> anomaly_mask(teeth.size(), cc::ToothAnomaly::none);

    REQUIRE(count_non_white(img) == 0);
    cc::display_results({150, 150}, teeth, anomaly_mask, img);
    REQUIRE(count_non_white(img) > 0);
}

TEST_CASE("display_results - speculative count label is wider than matching count label", "[visualization]") {
    // FFT count headlines: spec==direct(8) renders "8"; spec=72 vs direct 8 renders
    // "72 (direct 8)", which occupies more pixels.
    auto teeth = uniform_teeth(8);
    std::vector<uint8_t> anomaly_mask(teeth.size(), cc::ToothAnomaly::none);
    cc::Point2i centroid{150, 150};

    cc::Image img_match = white_image(300, 300);
    cc::Image img_spec  = white_image(300, 300);

    cc::display_results(centroid, teeth, anomaly_mask, img_match, 8);  // spec==direct → "8"
    cc::display_results(centroid, teeth, anomaly_mask, img_spec,  72); // wider        → "72 (direct 8)"

    int pixels_match = count_non_white_near(img_match, 150, 150, 100);
    int pixels_spec  = count_non_white_near(img_spec,  150, 150, 100);

    REQUIRE(pixels_spec > pixels_match);
}

TEST_CASE("display_results - direct_count overrides teeth.size() in displayed label", "[visualization]") {
    // With direct_count=72 and teeth.size()=8, "72/72" has more characters than "8/8"
    // so it should produce more non-white pixels near the centroid.
    auto teeth = uniform_teeth(8);
    std::vector<uint8_t> anomaly_mask(teeth.size(), cc::ToothAnomaly::none);
    cc::Point2i centroid{150, 150};

    cc::Image img_default   = white_image(300, 300);
    cc::Image img_overridden = white_image(300, 300);

    cc::display_results(centroid, teeth, anomaly_mask, img_default,    -1, -1); // "8/8"
    cc::display_results(centroid, teeth, anomaly_mask, img_overridden, -1, 72); // "72/72"

    int pixels_default    = count_non_white_near(img_default,    150, 150, 100);
    int pixels_overridden = count_non_white_near(img_overridden, 150, 150, 100);

    REQUIRE(pixels_overridden > pixels_default);
}

TEST_CASE("display_results - anomaly arrows drawn when anomalies present", "[visualization]") {
    auto teeth = uniform_teeth(8);
    std::vector<uint8_t> no_anomalies(teeth.size(), cc::ToothAnomaly::none);
    std::vector<uint8_t> with_arc(teeth.size(), cc::ToothAnomaly::none);
    with_arc[0] = cc::ToothAnomaly::arc;

    cc::Image img_clean = white_image(300, 300);
    cc::Image img_anom  = white_image(300, 300);

    cc::display_results({150, 150}, teeth, no_anomalies, img_clean);
    cc::display_results({150, 150}, teeth, with_arc,     img_anom);

    // Anomaly image has extra arrow pixels drawn
    REQUIRE(count_non_white(img_anom) > count_non_white(img_clean));
}
