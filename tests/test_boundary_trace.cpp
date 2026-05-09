#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "processing/boundary_trace.h"
#include "types/image.h"
#include "types/point.h"

#include <cmath>
#include <algorithm>

using namespace cc::processing;

TEST_CASE("find_contours - empty image", "[boundary_trace]") {
    cc::Image img = cc::Image::zeros(10, 10, 1);
    auto contours = find_contours(img);
    REQUIRE(contours.empty());
}

TEST_CASE("find_contours - single white pixel", "[boundary_trace]") {
    cc::Image img = cc::Image::zeros(10, 10, 1);
    img.at(5, 5)[0] = 255;

    auto contours = find_contours(img);
    REQUIRE(contours.size() == 1);
    REQUIRE(contours[0].size() >= 1);
}

TEST_CASE("find_contours - two separate rectangles", "[boundary_trace]") {
    cc::Image img = cc::Image::zeros(30, 30, 1);

    // Rectangle 1: rows 2-5, cols 2-5
    for (int y = 2; y <= 5; ++y)
        for (int x = 2; x <= 5; ++x)
            img.at(y, x)[0] = 255;

    // Rectangle 2: rows 15-20, cols 15-20
    for (int y = 15; y <= 20; ++y)
        for (int x = 15; x <= 20; ++x)
            img.at(y, x)[0] = 255;

    auto contours = find_contours(img);
    REQUIRE(contours.size() == 2);
}

TEST_CASE("find_contours - L-shaped region is one component", "[boundary_trace]") {
    cc::Image img = cc::Image::zeros(20, 20, 1);

    // Vertical bar
    for (int y = 2; y <= 15; ++y)
        for (int x = 2; x <= 5; ++x)
            img.at(y, x)[0] = 255;

    // Horizontal bar (connected)
    for (int y = 12; y <= 15; ++y)
        for (int x = 5; x <= 12; ++x)
            img.at(y, x)[0] = 255;

    auto contours = find_contours(img);
    REQUIRE(contours.size() == 1);
}

TEST_CASE("find_contours - full white image", "[boundary_trace]") {
    cc::Image img(10, 10, 1);
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x)
            img.at(y, x)[0] = 255;

    auto contours = find_contours(img);
    REQUIRE(contours.size() >= 1);
}

TEST_CASE("find_contours - filled rectangle boundary", "[boundary_trace]") {
    cc::Image img = cc::Image::zeros(20, 20, 1);

    for (int y = 5; y <= 14; ++y)
        for (int x = 5; x <= 14; ++x)
            img.at(y, x)[0] = 255;

    auto contours = find_contours(img);
    REQUIRE(contours.size() >= 1);

    // The boundary should contain points along the rectangle perimeter
    auto& boundary = contours[0];
    REQUIRE(boundary.size() > 4);

    // All boundary points should be inside the rectangle region
    for (auto& pt : boundary) {
        bool inside =
            pt.x >= 5 && pt.x <= 14 && pt.y >= 5 && pt.y <= 14;
        REQUIRE(inside);
    }
}

TEST_CASE("find_contours - largest contour from gear-like mask", "[boundary_trace]") {
    // Create a synthetic circular mask (simplified gear shape)
    cc::Image img = cc::Image::zeros(100, 100, 1);
    int cx = 50, cy = 50;

    for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 100; ++x) {
            double dist = std::hypot(x - cx, y - cy);
            double angle = std::atan2(y - cy, x - cx);
            double radius = 30 + 5 * std::cos(12 * angle); // 12 "teeth"

            if (dist <= radius)
                img.at(y, x)[0] = 255;
        }
    }

    // Add some noise (small separate components)
    img.at(5, 5)[0] = 255;
    img.at(95, 95)[0] = 255;

    auto contours = find_contours(img);
    REQUIRE(!contours.empty());

    // Find the largest contour
    size_t largest_idx = 0;
    size_t largest_size = 0;
    for (size_t i = 0; i < contours.size(); ++i) {
        if (contours[i].size() > largest_size) {
            largest_size = contours[i].size();
            largest_idx = i;
        }
    }

    // The gear contour should be significantly larger than noise
    REQUIRE(contours[largest_idx].size() > 50);
}
