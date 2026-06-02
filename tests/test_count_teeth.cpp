#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <numbers>

#include "processing/count_teeth.h"

using namespace cc::processing;
using namespace cc;

TEST_CASE("find_tooth_start - empty mask", "[count_teeth]") {
    std::vector<uint8_t> empty_mask;
    auto result = find_tooth_start(empty_mask);
    REQUIRE(!result.has_value());
}

TEST_CASE("find_tooth_start - no transitions", "[count_teeth]") {
    SECTION("all zeros") {
        std::vector<uint8_t> mask = {0, 0, 0, 0, 0};
        auto result = find_tooth_start(mask);
        REQUIRE(!result.has_value());
    }

    SECTION("all ones") {
        std::vector<uint8_t> mask = {1, 1, 1, 1, 1};
        auto result = find_tooth_start(mask);
        REQUIRE(!result.has_value());
    }
}

TEST_CASE("find_tooth_start - single transition", "[count_teeth]") {
    SECTION("transition in middle") {
        std::vector<uint8_t> mask = {0, 0, 1, 1, 1};
        auto result = find_tooth_start(mask);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == 1); // position before the rising edge
    }

    SECTION("transition at beginning") {
        std::vector<uint8_t> mask = {1, 0, 0, 0, 0};
        auto result = find_tooth_start(mask);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == 4); // wraps around - last position before rising edge
    }

    SECTION("edge case - transition at end") {
        std::vector<uint8_t> mask = {1, 1, 1, 1, 0};
        auto result = find_tooth_start(mask);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == 4); // last position before rising edge at wraparound
    }
}

TEST_CASE("find_tooth_start - multiple transitions", "[count_teeth]") {
    std::vector<uint8_t> mask = {0, 1, 0, 1, 0};
    auto result = find_tooth_start(mask);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 0); // first rising edge at position 0->1
}

TEST_CASE("count_teeth - empty mask", "[count_teeth]") {
    std::vector<uint8_t> empty_mask;
    std::vector<cc::Point2i> contour;
    std::vector<double> distances;
    cc::Point2f centroid(0, 0);

    auto teeth = count_teeth(0, empty_mask, contour, distances, centroid);
    REQUIRE(teeth.empty());
}

TEST_CASE("count_teeth - single tooth", "[count_teeth]") {
    // Create a simple mask with one tooth (low-high-low pattern)
    std::vector<uint8_t> mask = {0, 0, 1, 1, 1, 0, 0};

    // Create corresponding contour points in a circle
    std::vector<cc::Point2i> contour;
    std::vector<double> distances;
    cc::Point2f centroid(50, 50);

    for (size_t i = 0; i < mask.size(); ++i) {
        double angle = (2.0 * std::numbers::pi * i) / mask.size();
        double radius = mask[i] ? 40 : 30; // closer when mask is 0, farther when mask is 1

        cc::Point2i pt(
            static_cast<int>(centroid.x + radius * cos(angle)),
            static_cast<int>(centroid.y + radius * sin(angle))
        );
        contour.push_back(pt);
        distances.push_back(radius);
    }

    auto maybe_tooth_start = find_tooth_start(mask);
    REQUIRE(maybe_tooth_start.has_value());

    size_t first_tooth_idx = *maybe_tooth_start;
    auto teeth = count_teeth(first_tooth_idx, mask, contour, distances, centroid);

    REQUIRE(teeth.size() == 1);
    REQUIRE(teeth[0].m_ToothIdx == 1);
    REQUIRE(teeth[0].m_LowHighTransitionIdx == 1);
    REQUIRE(teeth[0].m_HighLowTransitionIdx == 4);
    REQUIRE(teeth[0].m_MinDistance == Catch::Approx(40.0).epsilon(0.1));
    REQUIRE(teeth[0].m_MaxDistance == Catch::Approx(40.0).epsilon(0.1));
}

TEST_CASE("count_teeth - multiple teeth", "[count_teeth]") {
    // Create a mask with three teeth
    std::vector<uint8_t> mask = {0, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0};

    std::vector<cc::Point2i> contour;
    std::vector<double> distances;
    cc::Point2f centroid(100, 100);

    for (size_t i = 0; i < mask.size(); ++i) {
        double angle = (2.0 * std::numbers::pi * i) / mask.size();
        double radius = mask[i] ? 25 : 50;

        cc::Point2i pt(
            static_cast<int>(centroid.x + radius * cos(angle)),
            static_cast<int>(centroid.y + radius * sin(angle))
        );
        contour.push_back(pt);
        distances.push_back(radius);
    }

    size_t first_tooth_idx = 0; // start at rising edge 0->1
    auto teeth = count_teeth(first_tooth_idx, mask, contour, distances, centroid);

    REQUIRE(teeth.size() == 3);

    // Check the first tooth
    REQUIRE(teeth[0].m_ToothIdx             == 1);
    REQUIRE(teeth[0].m_LowHighTransitionIdx == 0);
    REQUIRE(teeth[0].m_HighLowTransitionIdx == 2);

    // Check the second tooth
    REQUIRE(teeth[1].m_ToothIdx             == 2);
    REQUIRE(teeth[1].m_LowHighTransitionIdx == 4);
    REQUIRE(teeth[1].m_HighLowTransitionIdx == 5);

    // Check the third tooth
    REQUIRE(teeth[2].m_ToothIdx             == 3);
    REQUIRE(teeth[2].m_LowHighTransitionIdx == 7);
    REQUIRE(teeth[2].m_HighLowTransitionIdx == 9);
}

TEST_CASE("count_teeth - angle calculations", "[count_teeth]") {
    // Create a simple two-tooth pattern to verify angle calculations
    std::vector<uint8_t> mask = {0, 1, 0, 0, 1, 0};

    std::vector<cc::Point2i> contour;
    std::vector<double> distances;
    cc::Point2f centroid(0, 0);

    // Create points at specific angles for predictable results
    std::vector<double> expected_angles;
    for (size_t i = 0; i < mask.size(); ++i) {
        double angle = (2.0 * std::numbers::pi * i) / mask.size();
        expected_angles.push_back(angle);

        cc::Point2i pt(
            static_cast<int>(100 * cos(angle)),
            static_cast<int>(100 * sin(angle))
        );
        contour.push_back(pt);
        distances.push_back(100.0);
    }

    size_t first_tooth_idx = 0;
    auto teeth = count_teeth(first_tooth_idx, mask, contour, distances, centroid);

    REQUIRE(teeth.size() == 2);

    // Check that angles are calculated correctly
    double expected_start_angle_1 = expected_angles[0];
    double expected_end_angle_1   = expected_angles[1];
    double expected_start_angle_2 = expected_angles[3];
    double expected_end_angle_2   = expected_angles[4];

    REQUIRE(teeth[0].m_StartingAngle == Catch::Approx(expected_start_angle_1).epsilon(0.01));
    REQUIRE(teeth[0].m_EndingAngle   == Catch::Approx(expected_end_angle_1)  .epsilon(0.01));
    REQUIRE(teeth[1].m_StartingAngle == Catch::Approx(expected_start_angle_2).epsilon(0.01));
    REQUIRE(teeth[1].m_EndingAngle   == Catch::Approx(expected_end_angle_2)  .epsilon(0.01));
}

TEST_CASE("count_teeth - wraparound behavior", "[count_teeth]") {
    // Test behavior when teeth wrap around the end of the mask
    std::vector<uint8_t> mask = {1, 0, 1, 0, 1, 1};

    std::vector<cc::Point2i> contour;
    std::vector<double> distances;
    cc::Point2f centroid(50, 50);

    for (size_t i = 0; i < mask.size(); ++i) {
        double radius = mask[i] ? 40 : 30;
        cc::Point2i pt(static_cast<int>(centroid.x + radius), static_cast<int>(centroid.y));
        contour.push_back(pt);
        distances.push_back(radius);
    }

    size_t first_tooth_idx = 1; // start at rising edge 1 -> 2
    auto teeth = count_teeth(first_tooth_idx, mask, contour, distances, centroid);

    REQUIRE(teeth.size() == 2);

    // The first tooth should be detected at position 1 -> 2
    REQUIRE(teeth[0].m_ToothIdx             == 1);
    REQUIRE(teeth[0].m_LowHighTransitionIdx == 1);
    REQUIRE(teeth[0].m_HighLowTransitionIdx == 2);

    // The second tooth wraps around from end to beginning
    REQUIRE(teeth[1].m_ToothIdx             == 2);
    REQUIRE(teeth[1].m_LowHighTransitionIdx == 3); // wraps to position 0 in the next iteration
    REQUIRE(teeth[1].m_HighLowTransitionIdx == 0);
}

TEST_CASE("find_tooth_start - single element mask", "[count_teeth]") {
    SECTION("single zero") {
        std::vector<uint8_t> mask = {0};
        auto result = find_tooth_start(mask);
        REQUIRE(!result.has_value());
    }

    SECTION("single one") {
        std::vector<uint8_t> mask = {1};
        auto result = find_tooth_start(mask);
        REQUIRE(!result.has_value()); // no 0->1 transition possible
    }
}

TEST_CASE("find_tooth_start - two element mask", "[count_teeth]") {
    SECTION("0 then 1") {
        std::vector<uint8_t> mask = {0, 1};
        auto result = find_tooth_start(mask);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == 0);
    }

    SECTION("1 then 0") {
        std::vector<uint8_t> mask = {1, 0};
        auto result = find_tooth_start(mask);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == 1); // wraps: mask[1]=0, mask[0]=1
    }
}

// ---------------------------------------------------------------------------
// count_teeth span filter
// ---------------------------------------------------------------------------

TEST_CASE("count_teeth - narrow noise spike is filtered out", "[count_teeth]") {
    // 3 real teeth (span=6 each) plus one 1-point noise spike.
    // median span=6, min_span=max(1,6/3)=2 → spike(span=1) removed.
    std::vector<uint8_t> mask = {
        0, 1, 1, 1, 1, 1, 1, 0,  // tooth 1: span=6
        0, 1, 0,                  // noise spike: span=1
        0, 1, 1, 1, 1, 1, 1, 0,  // tooth 2: span=6
        0, 1, 1, 1, 1, 1, 1, 0   // tooth 3: span=6
    };

    std::vector<cc::Point2i> contour;
    std::vector<double> distances;
    cc::Point2f centroid(100, 100);
    for (size_t i = 0; i < mask.size(); ++i) {
        double angle  = (2.0 * std::numbers::pi * i) / mask.size();
        double radius = mask[i] ? 50.0 : 30.0;
        contour.push_back({ static_cast<int>(centroid.x + radius * std::cos(angle)),
                            static_cast<int>(centroid.y + radius * std::sin(angle)) });
        distances.push_back(radius);
    }

    auto start = find_tooth_start(mask);
    REQUIRE(start.has_value());
    auto teeth = count_teeth(*start, mask, contour, distances, centroid);

    // The noise spike (span=1) is below median(4)/3≈1, so it is filtered.
    REQUIRE(teeth.size() == 3);
}

// ---------------------------------------------------------------------------
// fft_tooth_count
// ---------------------------------------------------------------------------

namespace {
    // Build a synthetic gear contour with the given tooth count. The radial
    // distance profile is distance(θ) = base + amp * shape(N_teeth * θ), where
    // shape is either a sinusoid (smooth) or a square wave (realistic teeth).
    void make_gear_contour(
        int N_teeth, size_t N_samples, double base, double amp, bool square_wave,
        cc::Point2f centroid,
        std::vector<cc::Point2i>& contour_out,
        std::vector<double>& distances_out
    ) {
        contour_out.clear();
        distances_out.clear();
        contour_out.reserve(N_samples);
        distances_out.reserve(N_samples);
        for (size_t i = 0; i < N_samples; ++i) {
            double angle = -std::numbers::pi + 2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(N_samples);
            double r;
            if (square_wave)
                r = base + (std::sin(N_teeth * angle) >= 0 ? amp : -amp);
            else
                r = base + amp * std::cos(N_teeth * angle);
            distances_out.push_back(r);
            contour_out.push_back({
                static_cast<int>(centroid.x + r * std::cos(angle)),
                static_cast<int>(centroid.y + r * std::sin(angle))
            });
        }
    }
}

TEST_CASE("fft_tooth_count - too few contour points returns -1", "[count_teeth]") {
    std::vector<cc::Point2i> contour(10, {100, 100});
    std::vector<double> distances(10, 50.0);
    REQUIRE(fft_tooth_count(contour, distances, {0.0f, 0.0f}) == -1);
}

TEST_CASE("fft_tooth_count - sinusoidal radial profile", "[count_teeth]") {
    cc::Point2f centroid(200.0f, 200.0f);
    std::vector<cc::Point2i> contour;
    std::vector<double> distances;

    SECTION("24 teeth") {
        make_gear_contour(24, 2000, 100.0, 20.0, false, centroid, contour, distances);
        REQUIRE(fft_tooth_count(contour, distances, centroid) == 24);
    }
    SECTION("48 teeth") {
        make_gear_contour(48, 3000, 150.0, 15.0, false, centroid, contour, distances);
        REQUIRE(fft_tooth_count(contour, distances, centroid) == 48);
    }
}

TEST_CASE("fft_tooth_count - square-wave radial profile picks fundamental not harmonic", "[count_teeth]") {
    // Square wave has strong harmonics at 3N, 5N, ... but the fundamental at N
    // always dominates in power. Verifies the peak-picker returns N not 2N or 3N.
    cc::Point2f centroid(0.0f, 0.0f);
    std::vector<cc::Point2i> contour;
    std::vector<double> distances;
    make_gear_contour(16, 2000, 100.0, 25.0, true, centroid, contour, distances);
    REQUIRE(fft_tooth_count(contour, distances, centroid) == 16);
}
