#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "types/image.h"
#include "types/point.h"
#include "types/color.h"

// ===== Image tests =====

TEST_CASE("Image - default construction is empty", "[types][image]") {
    cc::Image img;
    REQUIRE(img.empty());
    REQUIRE(img.rows() == 0);
    REQUIRE(img.cols() == 0);
    REQUIRE(img.channels() == 0);
}

TEST_CASE("Image - construction with dimensions", "[types][image]") {
    cc::Image img(100, 200, 3);
    REQUIRE(!img.empty());
    REQUIRE(img.rows() == 100);
    REQUIRE(img.cols() == 200);
    REQUIRE(img.channels() == 3);
    REQUIRE(img.total_bytes() == 100 * 200 * 3);
}

TEST_CASE("Image - zeros creates zeroed image", "[types][image]") {
    auto img = cc::Image::zeros(50, 60, 1);
    REQUIRE(!img.empty());
    REQUIRE(img.rows() == 50);
    REQUIRE(img.cols() == 60);
    REQUIRE(img.channels() == 1);

    // Verify all pixels are zero
    for (int y = 0; y < img.rows(); ++y)
        for (int x = 0; x < img.cols(); ++x)
            REQUIRE(img.at(y, x)[0] == 0);
}

TEST_CASE("Image - at() read/write", "[types][image]") {
    cc::Image img(10, 10, 3);

    // Write a pixel
    uint8_t* p = img.at(5, 7);
    p[0] = 100; // B
    p[1] = 150; // G
    p[2] = 200; // R

    // Read it back
    const uint8_t* q = img.at(5, 7);
    REQUIRE(q[0] == 100);
    REQUIRE(q[1] == 150);
    REQUIRE(q[2] == 200);
}

TEST_CASE("Image - clone produces independent copy", "[types][image]") {
    cc::Image original(5, 5, 1);
    original.at(2, 3)[0] = 42;

    cc::Image copy = original.clone();
    REQUIRE(copy.rows() == original.rows());
    REQUIRE(copy.cols() == original.cols());
    REQUIRE(copy.channels() == original.channels());
    REQUIRE(copy.at(2, 3)[0] == 42);

    // Modify copy, original should be unchanged
    copy.at(2, 3)[0] = 99;
    REQUIRE(original.at(2, 3)[0] == 42);
    REQUIRE(copy.at(2, 3)[0] == 99);
}

TEST_CASE("Image - create resizes and zeroes", "[types][image]") {
    cc::Image img(10, 10, 3);
    img.at(0, 0)[0] = 255;

    img.create(20, 30, 1);
    REQUIRE(img.rows() == 20);
    REQUIRE(img.cols() == 30);
    REQUIRE(img.channels() == 1);
    REQUIRE(img.at(0, 0)[0] == 0); // should be zeroed
}

TEST_CASE("Image - construction from raw data", "[types][image]") {
    uint8_t data[] = { 10, 20, 30, 40, 50, 60 };
    cc::Image img(1, 2, 3, data);

    REQUIRE(img.at(0, 0)[0] == 10);
    REQUIRE(img.at(0, 0)[1] == 20);
    REQUIRE(img.at(0, 0)[2] == 30);
    REQUIRE(img.at(0, 1)[0] == 40);
    REQUIRE(img.at(0, 1)[1] == 50);
    REQUIRE(img.at(0, 1)[2] == 60);
}

// ===== Point2 tests =====

TEST_CASE("Point2i - construction and access", "[types][point]") {
    cc::Point2i p(10, 20);
    REQUIRE(p.x == 10);
    REQUIRE(p.y == 20);
}

TEST_CASE("Point2i - default construction", "[types][point]") {
    cc::Point2i p;
    REQUIRE(p.x == 0);
    REQUIRE(p.y == 0);
}

TEST_CASE("Point2i - arithmetic operators", "[types][point]") {
    cc::Point2i a(3, 5);
    cc::Point2i b(1, 2);

    auto sum = a + b;
    REQUIRE(sum.x == 4);
    REQUIRE(sum.y == 7);

    auto diff = a - b;
    REQUIRE(diff.x == 2);
    REQUIRE(diff.y == 3);

    auto scaled = a * 3;
    REQUIRE(scaled.x == 9);
    REQUIRE(scaled.y == 15);
}

TEST_CASE("Point2i - equality operators", "[types][point]") {
    cc::Point2i a(5, 10);
    cc::Point2i b(5, 10);
    cc::Point2i c(5, 11);

    REQUIRE(a == b);
    REQUIRE(a != c);
}

TEST_CASE("Point2f - float precision", "[types][point]") {
    cc::Point2f p(1.5f, 2.5f);
    REQUIRE(p.x == Catch::Approx(1.5f));
    REQUIRE(p.y == Catch::Approx(2.5f));
}

TEST_CASE("Point2d - double precision", "[types][point]") {
    cc::Point2d p(1.123456789, 2.987654321);
    REQUIRE(p.x == Catch::Approx(1.123456789));
    REQUIRE(p.y == Catch::Approx(2.987654321));
}

TEST_CASE("Point2 - type conversion", "[types][point]") {
    cc::Point2d d(1.7, 2.3);
    cc::Point2i i(d);
    REQUIRE(i.x == 1); // truncation
    REQUIRE(i.y == 2);
}

TEST_CASE("Point2i - compound assignment", "[types][point]") {
    cc::Point2i p(5, 10);
    p += cc::Point2i(3, 7);
    REQUIRE(p.x == 8);
    REQUIRE(p.y == 17);

    p -= cc::Point2i(1, 2);
    REQUIRE(p.x == 7);
    REQUIRE(p.y == 15);
}

// ===== Color3 tests =====

TEST_CASE("Color3 - construction", "[types][color]") {
    cc::Color3 c(100, 150, 200);
    REQUIRE(c.b == 100);
    REQUIRE(c.g == 150);
    REQUIRE(c.r == 200);
}

TEST_CASE("Color3 - default construction", "[types][color]") {
    cc::Color3 c;
    REQUIRE(c.b == 0);
    REQUIRE(c.g == 0);
    REQUIRE(c.r == 0);
}

TEST_CASE("Color3 - indexing", "[types][color]") {
    cc::Color3 c(10, 20, 30);
    REQUIRE(c[0] == 10.0); // b
    REQUIRE(c[1] == 20.0); // g
    REQUIRE(c[2] == 30.0); // r

    // Write through index
    c[1] = 99;
    REQUIRE(c.g == 99);
}

TEST_CASE("Color3 - equality", "[types][color]") {
    cc::Color3 a(1, 2, 3);
    cc::Color3 b(1, 2, 3);
    cc::Color3 c(1, 2, 4);

    REQUIRE(a == b);
    REQUIRE(a != c);
}
