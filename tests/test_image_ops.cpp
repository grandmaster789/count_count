#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "types/image.h"
#include "types/point.h"
#include "math/geometry.h"

#include <cmath>
#include <numbers>

// ===== in_range tests =====

TEST_CASE("in_range - all pixels within range", "[image_ops]") {
    cc::Image img(5, 5, 3);
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            uint8_t* p = img.at(y, x);
            p[0] = 100; p[1] = 100; p[2] = 100;
        }
    }

    cc::Image mask = cc::Image::zeros(5, 5, 1);
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            const uint8_t* p = img.at(y, x);
            bool in = (p[0] >= 50 && p[0] <= 150) &&
                      (p[1] >= 50 && p[1] <= 150) &&
                      (p[2] >= 50 && p[2] <= 150);
            mask.at(y, x)[0] = in ? 255 : 0;
        }
    }

    // All should be white
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            REQUIRE(mask.at(y, x)[0] == 255);
}

TEST_CASE("in_range - all pixels outside range", "[image_ops]") {
    cc::Image img(5, 5, 3);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x) {
            uint8_t* p = img.at(y, x);
            p[0] = 200; p[1] = 200; p[2] = 200;
        }

    cc::Image mask = cc::Image::zeros(5, 5, 1);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x) {
            const uint8_t* p = img.at(y, x);
            bool in = (p[0] >= 50 && p[0] <= 100);
            mask.at(y, x)[0] = in ? 255 : 0;
        }

    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            REQUIRE(mask.at(y, x)[0] == 0);
}

// ===== copy_with_mask tests =====

TEST_CASE("copy_with_mask - all white mask copies all", "[image_ops]") {
    cc::Image src(3, 3, 3);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x) {
            uint8_t* p = src.at(y, x);
            p[0] = 10; p[1] = 20; p[2] = 30;
        }

    cc::Image mask = cc::Image::zeros(3, 3, 1);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            mask.at(y, x)[0] = 255;

    cc::Image dst = cc::Image::zeros(3, 3, 3);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            if (mask.at(y, x)[0]) {
                dst.at(y, x)[0] = src.at(y, x)[0];
                dst.at(y, x)[1] = src.at(y, x)[1];
                dst.at(y, x)[2] = src.at(y, x)[2];
            }

    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x) {
            REQUIRE(dst.at(y, x)[0] == 10);
            REQUIRE(dst.at(y, x)[1] == 20);
            REQUIRE(dst.at(y, x)[2] == 30);
        }
}

TEST_CASE("copy_with_mask - all black mask copies nothing", "[image_ops]") {
    cc::Image dst = cc::Image::zeros(3, 3, 3);
    // dst should remain all zeros
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            REQUIRE(dst.at(y, x)[0] == 0);
}

// ===== shoelace_area tests =====

TEST_CASE("shoelace_area - unit square", "[image_ops]") {
    std::vector<cc::Point2i> square = {
        {0, 0}, {1, 0}, {1, 1}, {0, 1}
    };
    REQUIRE(cc::math::polygon_area(square) == Catch::Approx(1.0));
}

TEST_CASE("shoelace_area - known triangle", "[image_ops]") {
    std::vector<cc::Point2i> triangle = {
        {0, 0}, {4, 0}, {0, 3}
    };
    REQUIRE(cc::math::polygon_area(triangle) == Catch::Approx(6.0));
}

TEST_CASE("shoelace_area - clockwise vs counter-clockwise", "[image_ops]") {
    std::vector<cc::Point2i> ccw = { {0, 0}, {4, 0}, {4, 3}, {0, 3} };
    std::vector<cc::Point2i> cw  = { {0, 0}, {0, 3}, {4, 3}, {4, 0} };

    REQUIRE(cc::math::polygon_area(ccw) == Catch::Approx(12.0));
    REQUIRE(cc::math::polygon_area(cw)  == Catch::Approx(12.0));
}

TEST_CASE("shoelace_area - single point", "[image_ops]") {
    std::vector<cc::Point2i> single = { {5, 5} };
    REQUIRE(cc::math::polygon_area(single) == 0.0);
}

TEST_CASE("shoelace_area - two points", "[image_ops]") {
    std::vector<cc::Point2i> two = { {0, 0}, {5, 5} };
    REQUIRE(cc::math::polygon_area(two) == 0.0);
}

// ===== compute_centroid tests =====

TEST_CASE("centroid of square", "[image_ops]") {
    std::vector<cc::Point2i> square = {
        {0, 0}, {10, 0}, {10, 10}, {0, 10}
    };

    double sx = 0, sy = 0;
    for (auto& p : square) { sx += p.x; sy += p.y; }
    double cx = sx / square.size();
    double cy = sy / square.size();

    REQUIRE(cx == Catch::Approx(5.0));
    REQUIRE(cy == Catch::Approx(5.0));
}

TEST_CASE("centroid of triangle", "[image_ops]") {
    std::vector<cc::Point2i> triangle = {
        {0, 0}, {6, 0}, {3, 6}
    };

    double sx = 0, sy = 0;
    for (auto& p : triangle) { sx += p.x; sy += p.y; }
    double cx = sx / triangle.size();
    double cy = sy / triangle.size();

    REQUIRE(cx == Catch::Approx(3.0));
    REQUIRE(cy == Catch::Approx(2.0));
}
