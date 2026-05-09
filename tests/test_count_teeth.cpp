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
// estimate_tooth_count
// ---------------------------------------------------------------------------

namespace {
    cc::ToothMeasurement tooth_at(double angle_rad) {
        cc::ToothMeasurement t;
        t.m_StartingAngle = angle_rad;
        t.m_EndingAngle   = angle_rad;
        t.m_MinDistance   = 40.0;
        t.m_MaxDistance   = 50.0;
        return t;
    }

    std::vector<cc::ToothMeasurement> teeth_from_degrees(std::initializer_list<double> degrees) {
        std::vector<cc::ToothMeasurement> v;
        for (double d : degrees)
            v.push_back(tooth_at(d * std::numbers::pi / 180.0));
        return v;
    }
}

TEST_CASE("estimate_tooth_count - too few teeth returns -1", "[count_teeth]") {
    REQUIRE(estimate_tooth_count({}) == -1);
    REQUIRE(estimate_tooth_count({ tooth_at(0.0) }) == -1);
}

TEST_CASE("estimate_tooth_count - exact uniform spacing", "[count_teeth]") {
    // 8 teeth at 45° each → estimate = 8
    REQUIRE(estimate_tooth_count(teeth_from_degrees({0, 45, 90, 135, 180, 225, 270, 315})) == 8);
    // 12 teeth at 30° each → estimate = 12
    REQUIRE(estimate_tooth_count(teeth_from_degrees({0,30,60,90,120,150,180,210,240,270,300,330})) == 12);
}

TEST_CASE("estimate_tooth_count - two teeth 180 deg apart", "[count_teeth]") {
    // Both spacings are 180° → pitch=180° → count=2
    REQUIRE(estimate_tooth_count(teeth_from_degrees({0, 180})) == 2);
}

TEST_CASE("estimate_tooth_count - RANSAC recovers true count despite missed detections", "[count_teeth]") {
    // True gear: 12 teeth at 30° pitch.  Detected only 7 of them.
    // All observed spacings (30°, 60°, 90°) are multiples of 30°,
    // so pitch=30° maximises inliers → estimate = 12.
    auto teeth = teeth_from_degrees({0, 30, 60, 120, 150, 240, 330});
    REQUIRE(estimate_tooth_count(teeth) == 12);
}

TEST_CASE("estimate_tooth_count - spurious near-pitch detection resolved by larger-pitch tie-break", "[count_teeth]") {
    // True pitch 45° (8 teeth).  One detection shifted to 41° instead of 45°.
    // Spacings: 41, 49, 45, 45, 45, 45, 45, 45 degrees.
    // Both pitch=45° and pitch=41° collect 8 inliers (all within 25% tolerance).
    // Tie broken by larger pitch → 45° wins → estimate = 8.
    auto teeth = teeth_from_degrees({0, 41, 90, 135, 180, 225, 270, 315});
    REQUIRE(estimate_tooth_count(teeth) == 8);
}
