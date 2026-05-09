#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "gui/drawing.h"
#include "types/image.h"
#include "types/point.h"

using namespace cc::drawing;

TEST_CASE("draw_line - horizontal line", "[drawing]") {
    cc::Image img = cc::Image::zeros(20, 20, 3);
    draw_line(img, {2, 10}, {17, 10}, 255, 255, 255);

    for (int x = 2; x <= 17; ++x)
        REQUIRE(img.at(10, x)[0] == 255);
}

TEST_CASE("draw_line - vertical line", "[drawing]") {
    cc::Image img = cc::Image::zeros(20, 20, 3);
    draw_line(img, {10, 2}, {10, 17}, 255, 255, 255);

    for (int y = 2; y <= 17; ++y)
        REQUIRE(img.at(y, 10)[0] == 255);
}

TEST_CASE("draw_line - diagonal", "[drawing]") {
    cc::Image img = cc::Image::zeros(20, 20, 3);
    draw_line(img, {0, 0}, {19, 19}, 255, 255, 255);

    // Diagonal should have pixels set along the path
    int pixel_count = 0;
    for (int y = 0; y < 20; ++y)
        for (int x = 0; x < 20; ++x)
            if (img.at(y, x)[0] > 0) ++pixel_count;

    REQUIRE(pixel_count >= 20); // at least one pixel per row
}

TEST_CASE("draw_line - fully outside bounds", "[drawing]") {
    cc::Image img = cc::Image::zeros(10, 10, 3);
    // This should not crash
    draw_line(img, {-5, -5}, {-1, -1}, 255, 255, 255);

    // Image should be unchanged (all zeros)
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x)
            REQUIRE(img.at(y, x)[0] == 0);
}

TEST_CASE("draw_circle - radius 0 is single pixel", "[drawing]") {
    cc::Image img = cc::Image::zeros(10, 10, 3);
    draw_circle(img, {5, 5}, 0, 255, 0, 0);

    REQUIRE(img.at(5, 5)[0] == 255);
}

TEST_CASE("draw_circle - cardinal points at radius", "[drawing]") {
    cc::Image img = cc::Image::zeros(30, 30, 3);
    draw_circle(img, {15, 15}, 5, 255, 255, 255);

    // Top, bottom, left, right should have pixels
    REQUIRE(img.at(10, 15)[0] == 255); // top
    REQUIRE(img.at(20, 15)[0] == 255); // bottom
    REQUIRE(img.at(15, 10)[0] == 255); // left
    REQUIRE(img.at(15, 20)[0] == 255); // right
}

TEST_CASE("draw_circle - filled has approximately pi*r^2 pixels", "[drawing]") {
    cc::Image img = cc::Image::zeros(100, 100, 3);
    int r = 20;
    draw_circle(img, {50, 50}, r, 255, 255, 255, true);

    int count = 0;
    for (int y = 0; y < 100; ++y)
        for (int x = 0; x < 100; ++x)
            if (img.at(y, x)[0] > 0) ++count;

    double expected = 3.14159 * r * r;
    REQUIRE(count == Catch::Approx(static_cast<int>(expected)).margin(expected * 0.05));
}

TEST_CASE("draw_circle - partially outside bounds", "[drawing]") {
    cc::Image img = cc::Image::zeros(10, 10, 3);
    // Circle at corner - should not crash
    REQUIRE_NOTHROW(draw_circle(img, {0, 0}, 5, 255, 255, 255));
}

TEST_CASE("draw_arrowed_line - draws without crash", "[drawing]") {
    cc::Image img = cc::Image::zeros(50, 50, 3);
    REQUIRE_NOTHROW(draw_arrowed_line(img, {10, 25}, {40, 25}, 255, 255, 255, 2));

    // Some pixels should be set
    int count = 0;
    for (int y = 0; y < 50; ++y)
        for (int x = 0; x < 50; ++x)
            if (img.at(y, x)[0] > 0) ++count;

    REQUIRE(count > 20);
}

TEST_CASE("draw_arrowed_line - very short arrow", "[drawing]") {
    cc::Image img = cc::Image::zeros(10, 10, 3);
    REQUIRE_NOTHROW(draw_arrowed_line(img, {5, 5}, {6, 5}, 255, 255, 255));
}

TEST_CASE("draw_polyline - triangle", "[drawing]") {
    cc::Image img = cc::Image::zeros(30, 30, 3);
    std::vector<cc::Point2i> triangle = { {5, 5}, {25, 5}, {15, 25} };
    draw_polyline(img, triangle, 255, 255, 255, true); // closed

    // All three edges should have pixels
    // Top edge (y=5) between x=5 and x=25
    int top_count = 0;
    for (int x = 5; x <= 25; ++x)
        if (img.at(5, x)[0] > 0) ++top_count;
    REQUIRE(top_count > 15);
}

TEST_CASE("draw_polyline - single point no crash", "[drawing]") {
    cc::Image img = cc::Image::zeros(10, 10, 3);
    std::vector<cc::Point2i> single = { {5, 5} };
    REQUIRE_NOTHROW(draw_polyline(img, single, 255, 255, 255));
}

TEST_CASE("draw_text - renders non-empty region", "[drawing]") {
    cc::Image img = cc::Image::zeros(30, 60, 3);
    draw_text(img, "12", {5, 5}, 0, 255, 0, 1.0);

    int count = 0;
    for (int y = 0; y < 30; ++y)
        for (int x = 0; x < 60; ++x)
            if (img.at(y, x)[1] > 0) ++count;

    REQUIRE(count > 0);
}

TEST_CASE("measure_text - wider for more characters", "[drawing]") {
    auto s2 = measure_text("12", 1.0);
    auto s3 = measure_text("123", 1.0);
    REQUIRE(s3.width > s2.width);
}

TEST_CASE("draw_text - at edge no crash", "[drawing]") {
    cc::Image img = cc::Image::zeros(10, 10, 3);
    REQUIRE_NOTHROW(draw_text(img, "999", {8, 8}, 255, 255, 255, 1.0));
}

TEST_CASE("draw_text - all required glyphs", "[drawing]") {
    cc::Image img = cc::Image::zeros(20, 200, 3);
    REQUIRE_NOTHROW(draw_text(img, "0123456789/?!", {0, 0}, 255, 255, 255, 1.0));

    int count = 0;
    for (int y = 0; y < 20; ++y)
        for (int x = 0; x < 200; ++x)
            if (img.at(y, x)[0] > 0) ++count;

    REQUIRE(count > 50); // all glyphs should render something
}
