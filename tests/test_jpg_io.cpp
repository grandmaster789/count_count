// tests/test_jpg_io.cpp
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iostream>
#include <random>
#include "io/jpg.h"

namespace fs = std::filesystem;

class JpgIOTestFixture {
public:
    JpgIOTestFixture() {
        // Create test directory
        test_dir = fs::temp_directory_path() / "count_von_count_tests";
        fs::create_directories(test_dir);

        // Create test images
        createTestImages();
    }

    ~JpgIOTestFixture() {
        // Clean up test directory
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
    }

    void createTestImages() {
        // Create a simple 3-channel (BGR) test image with a blue rectangle
        test_image_bgr = cc::Image(100, 100, 3);
        for (int y = 25; y <= 75; ++y) {
            for (int x = 25; x <= 75; ++x) {
                uint8_t* p = test_image_bgr.at(y, x);
                p[0] = 255; // B
                p[1] = 0;   // G
                p[2] = 0;   // R
            }
        }

        // Create a grayscale test image with a white circle
        test_image_gray = cc::Image(50, 50, 1);
        for (int y = 0; y < 50; ++y) {
            for (int x = 0; x < 50; ++x) {
                int dx = x - 25;
                int dy = y - 25;
                if (dx * dx + dy * dy <= 15 * 15)
                    test_image_gray.at(y, x)[0] = 255;
            }
        }
    }

    fs::path test_dir;
    cc::Image test_image_bgr;
    cc::Image test_image_gray;
};

// Test successful loading of a valid JPEG file
TEST_CASE_METHOD(JpgIOTestFixture, "LoadValidJpeg", "[jpg_io]") {
    using cc::io::save_jpg;
    using cc::io::load_jpg;

    fs::path test_file = test_dir / "test_valid.jpg";

    // Save a test image first
    REQUIRE_NOTHROW(save_jpg(test_image_bgr, test_file));
    REQUIRE(fs::exists(test_file));

    // Load the image back
    cc::Image loaded_image;
    REQUIRE_NOTHROW(loaded_image = load_jpg(test_file));

    // Verify properties
    REQUIRE(!loaded_image.empty());
    REQUIRE( loaded_image.rows()     == test_image_bgr.rows());
    REQUIRE( loaded_image.cols()     == test_image_bgr.cols());
    REQUIRE( loaded_image.channels() == 3); // JPEG should be 3-channel
}

// Test loading a non-existent file
TEST_CASE_METHOD(JpgIOTestFixture, "LoadNonExistentFile", "[jpg_io]") {
    using cc::io::ImageError;
    using cc::io::load_jpg;

    fs::path non_existent = test_dir / "does_not_exist.jpg";

    REQUIRE_THROWS_AS(load_jpg(non_existent), ImageError);
}

// Test loading an invalid file
TEST_CASE_METHOD(JpgIOTestFixture, "LoadInvalidFile", "[jpg_io]") {
    using cc::io::load_jpg;
    using cc::io::ImageError;

    fs::path invalid_file = test_dir / "invalid.jpg";

    // Create a file with invalid content
    std::ofstream file(invalid_file, std::ios::binary);
    file << "This is not a valid JPEG file";
    file.close();

    REQUIRE_THROWS_AS(load_jpg(invalid_file), ImageError);
}

// Test saving a BGR image
TEST_CASE_METHOD(JpgIOTestFixture, "SaveBGRImage", "[jpg_io]") {
    using cc::io::save_jpg;

    fs::path output_file = test_dir / "test_bgr.jpg";

    REQUIRE_NOTHROW(save_jpg(test_image_bgr, output_file));
    REQUIRE(fs::exists(output_file));

    // Verify file is not empty
    REQUIRE(fs::file_size(output_file) > 0);
}

// Test saving an empty image
TEST_CASE_METHOD(JpgIOTestFixture, "SaveEmptyImage", "[jpg_io]") {
    using cc::io::save_jpg;
    using cc::io::ImageError;

    cc::Image empty_image;
    fs::path output_file = test_dir / "empty.jpg";

    REQUIRE_THROWS_AS(save_jpg(empty_image, output_file), ImageError);
}

// Test round-trip (save and load) for BGR image
TEST_CASE_METHOD(JpgIOTestFixture, "RoundTripBGR", "[jpg_io]") {
    using cc::io::save_jpg;
    using cc::io::load_jpg;

    fs::path temp_file = test_dir / "roundtrip_bgr.jpg";

    // Save the test image
    REQUIRE_NOTHROW(save_jpg(test_image_bgr, temp_file));

    // Load it back
    cc::Image loaded_image;
    REQUIRE_NOTHROW(loaded_image = load_jpg(temp_file));

    // Verify basic properties
    REQUIRE(loaded_image.rows()     == test_image_bgr.rows());
    REQUIRE(loaded_image.cols()     == test_image_bgr.cols());
    REQUIRE(loaded_image.channels() == test_image_bgr.channels());
}

// Test saving to non-existent directory
TEST_CASE_METHOD(JpgIOTestFixture, "SaveToNonExistentDirectory", "[jpg_io]") {
    using cc::io::save_jpg;
    using cc::io::ImageError;

    fs::path non_existent_dir = test_dir / "does_not_exist" / "test.jpg";

    // This should fail because the directory doesn't exist
    REQUIRE_THROWS_AS(save_jpg(test_image_bgr, non_existent_dir), ImageError);
}

// Test file path with special characters
TEST_CASE_METHOD(JpgIOTestFixture, "SpecialCharactersInPath", "[jpg_io]") {
    using cc::io::save_jpg;
    using cc::io::load_jpg;

    fs::path special_path = test_dir / "test_with_spaces and-symbols.jpg";

    REQUIRE_NOTHROW(save_jpg(test_image_bgr, special_path));
    REQUIRE(fs::exists(special_path));

    cc::Image loaded;
    REQUIRE_NOTHROW(loaded = load_jpg(special_path));
    REQUIRE(!loaded.empty());
}

// Performance test for large images
TEST_CASE_METHOD(JpgIOTestFixture, "LargeImageHandling", "[jpg_io]") {
    using namespace std::chrono;
    using cc::io::save_jpg;
    using cc::io::load_jpg;

    // Create a larger test image with random data
    cc::Image large_image(1000, 1000, 3);
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 255);
    for (int y = 0; y < 1000; ++y) {
        uint8_t* row = large_image.ptr(y);
        for (int x = 0; x < 1000 * 3; ++x)
            row[x] = static_cast<uint8_t>(dist(rng));
    }

    fs::path large_file = test_dir / "large_image.jpg";

    auto start = high_resolution_clock::now();
    REQUIRE_NOTHROW(save_jpg(large_image, large_file));
    auto save_end = high_resolution_clock::now();

    cc::Image loaded_large;
    REQUIRE_NOTHROW(loaded_large = load_jpg(large_file));
    auto load_end = high_resolution_clock::now();

    // Verify the image was processed correctly
    REQUIRE(loaded_large.rows() == large_image.rows());
    REQUIRE(loaded_large.cols() == large_image.cols());
    REQUIRE(loaded_large.channels() == large_image.channels());

    // Log timing (optional - mainly for development)
    auto save_time = duration_cast<milliseconds>(save_end - start);
    auto load_time = duration_cast<milliseconds>(load_end - save_end);

    std::cout << "Large image save time: " << save_time.count() << "ms" << std::endl;
    std::cout << "Large image load time: " << load_time.count() << "ms" << std::endl;
}
