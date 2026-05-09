#include <catch2/catch_test_macros.hpp>

#include "types/image.h"

namespace {
    // Dilate: per-pixel max in kernel
    cc::Image dilate(const cc::Image& src, int ksize) {
        cc::Image dst = cc::Image::zeros(src.rows(), src.cols(), 1);
        int half = ksize / 2;

        for (int y = 0; y < src.rows(); ++y) {
            for (int x = 0; x < src.cols(); ++x) {
                uint8_t max_val = 0;
                for (int ky = -half; ky <= half; ++ky) {
                    for (int kx = -half; kx <= half; ++kx) {
                        int ny = y + ky, nx = x + kx;
                        if (ny >= 0 && ny < src.rows() && nx >= 0 && nx < src.cols())
                            max_val = std::max(max_val, src.at(ny, nx)[0]);
                    }
                }
                dst.at(y, x)[0] = max_val;
            }
        }
        return dst;
    }

    // Erode: per-pixel min in kernel
    cc::Image erode(const cc::Image& src, int ksize) {
        cc::Image dst = cc::Image::zeros(src.rows(), src.cols(), 1);
        int half = ksize / 2;

        for (int y = 0; y < src.rows(); ++y) {
            for (int x = 0; x < src.cols(); ++x) {
                uint8_t min_val = 255;
                for (int ky = -half; ky <= half; ++ky) {
                    for (int kx = -half; kx <= half; ++kx) {
                        int ny = y + ky, nx = x + kx;
                        if (ny >= 0 && ny < src.rows() && nx >= 0 && nx < src.cols())
                            min_val = std::min(min_val, src.at(ny, nx)[0]);
                    }
                }
                dst.at(y, x)[0] = min_val;
            }
        }
        return dst;
    }

    // Morphological close = dilate then erode
    cc::Image morph_close(const cc::Image& src, int ksize) {
        return erode(dilate(src, ksize), ksize);
    }
}

TEST_CASE("Dilate - single white pixel creates 3x3 square", "[morphology]") {
    cc::Image img = cc::Image::zeros(7, 7, 1);
    img.at(3, 3)[0] = 255;

    auto result = dilate(img, 3);

    // 3x3 area around (3,3) should be white
    for (int y = 2; y <= 4; ++y)
        for (int x = 2; x <= 4; ++x)
            REQUIRE(result.at(y, x)[0] == 255);

    // Pixels outside should remain 0
    REQUIRE(result.at(0, 0)[0] == 0);
    REQUIRE(result.at(6, 6)[0] == 0);
}

TEST_CASE("Erode - 3x3 white square to single pixel", "[morphology]") {
    cc::Image img = cc::Image::zeros(7, 7, 1);
    for (int y = 2; y <= 4; ++y)
        for (int x = 2; x <= 4; ++x)
            img.at(y, x)[0] = 255;

    auto result = erode(img, 3);

    // Only center should remain
    REQUIRE(result.at(3, 3)[0] == 255);

    // Edges of the original square should be eroded
    REQUIRE(result.at(2, 2)[0] == 0);
    REQUIRE(result.at(2, 3)[0] == 0);
    REQUIRE(result.at(4, 4)[0] == 0);
}

TEST_CASE("Morphological close fills 1-pixel hole", "[morphology]") {
    cc::Image img = cc::Image::zeros(7, 7, 1);
    // Fill a 5x5 white region with a 1-pixel hole in the center
    for (int y = 1; y <= 5; ++y)
        for (int x = 1; x <= 5; ++x)
            img.at(y, x)[0] = 255;

    img.at(3, 3)[0] = 0; // 1-pixel hole

    auto result = morph_close(img, 3);

    // Hole should be filled
    REQUIRE(result.at(3, 3)[0] == 255);
}

TEST_CASE("Morphological close preserves solid region", "[morphology]") {
    cc::Image img = cc::Image::zeros(10, 10, 1);
    for (int y = 2; y <= 7; ++y)
        for (int x = 2; x <= 7; ++x)
            img.at(y, x)[0] = 255;

    auto result = morph_close(img, 3);

    // Interior should still be white
    for (int y = 3; y <= 6; ++y)
        for (int x = 3; x <= 6; ++x)
            REQUIRE(result.at(y, x)[0] == 255);
}

TEST_CASE("Morphological close on mask with small gaps", "[morphology]") {
    cc::Image img = cc::Image::zeros(10, 10, 1);
    // Create a region with a thin gap
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            if (x != 5) // 1-pixel vertical gap at x=5
                img.at(y, x)[0] = 255;
        }
    }

    auto result = morph_close(img, 3);

    // The gap should be closed
    for (int y = 1; y < 9; ++y) // avoid edges
        REQUIRE(result.at(y, 5)[0] == 255);
}
