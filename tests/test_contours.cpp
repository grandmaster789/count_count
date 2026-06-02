#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <numbers>

#include "processing/centroid.h"
#include "processing/contours.h"

using namespace cc::processing;
using namespace cc;

namespace {
    std::vector<cc::Point2i> circle_contour(int cx, int cy, int radius, int n = 360) {
        std::vector<cc::Point2i> pts;
        pts.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            double a = i * 2.0 * std::numbers::pi / n;
            pts.push_back({
                cx + static_cast<int>(radius * std::cos(a)),
                cy + static_cast<int>(radius * std::sin(a))
            });
        }
        return pts;
    }
}

// --- Centroid tests ---

TEST_CASE("find_centroid - empty contour", "[centroid]") {
    std::vector<cc::Point2i> empty_contour;
    auto [cd, cf, ci] = find_centroid(empty_contour);

    REQUIRE(cd.x == 0.0);
    REQUIRE(cd.y == 0.0);
    REQUIRE(ci.x == 0);
    REQUIRE(ci.y == 0);
}

TEST_CASE("find_centroid - single point", "[centroid]") {
    std::vector<cc::Point2i> contour = { {42, 17} };
    auto [cd, cf, ci] = find_centroid(contour);

    REQUIRE(cd.x == Catch::Approx(42.0));
    REQUIRE(cd.y == Catch::Approx(17.0));
}

TEST_CASE("find_centroid - symmetric square", "[centroid]") {
    std::vector<cc::Point2i> contour = { {0, 0}, {10, 0}, {10, 10}, {0, 10} };
    auto [cd, cf, ci] = find_centroid(contour);

    REQUIRE(cd.x == Catch::Approx(5.0));
    REQUIRE(cd.y == Catch::Approx(5.0));
}

// --- Contour processing tests ---

TEST_CASE("process_contours - area below threshold returns nullopt", "[contours]") {
    // Create a 100x100 image (area = 10000)
    cc::Image output(100, 100, 3);

    // Create a tiny contour (area < 1% of image area = 100)
    // A 3x3 square has area ~4.5 via shoelace
    std::vector<cc::Point2i> tiny = { {10, 10}, {12, 10}, {12, 12}, {10, 12} };
    std::vector<std::vector<cc::Point2i>> contours = { tiny };

    auto result = process_contours(contours, output);
    REQUIRE(!result.has_value());
}

TEST_CASE("process_contours - area at threshold proceeds", "[contours]") {
    // 100x100 image, threshold = 100 pixels area
    // A contour with area >= 100 should proceed
    cc::Image output(100, 100, 3);

    // Build a circle-like contour with enough area
    std::vector<cc::Point2i> contour;
    for (int i = 0; i < 360; ++i) {
        double angle = i * 3.14159265 / 180.0;
        contour.push_back({
            50 + static_cast<int>(15 * std::cos(angle)),
            50 + static_cast<int>(15 * std::sin(angle))
        });
    }

    std::vector<std::vector<cc::Point2i>> contours = { contour };
    auto result = process_contours(contours, output);
    REQUIRE(result.has_value());
}

