// tests/test_jpg_io.cpp
#include <catch2/catch_test_macros.hpp>
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iostream>
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
        // Create a simple 3-channel (BGR) test image
        test_image_bgr = cv::Mat::zeros(100, 100, CV_8UC3);
        cv::rectangle(
            test_image_bgr,
            cv::Point(25, 25),
            cv::Point(75, 75),
            cv::Scalar(255, 0, 0), // Blue rectangle
            -1
        );

        // Create a grayscale test image
        test_image_gray = cv::Mat::zeros(50, 50, CV_8UC1);
        cv::circle(
            test_image_gray,
            cv::Point(25, 25), 15,
            cv::Scalar(255),    // White circle
            -1
        );

        // Create a 4-channel (BGRA) test image
        test_image_bgra = cv::Mat::zeros(75, 75, CV_8UC4);
        cv::rectangle(
            test_image_bgra,
            cv::Point(10, 10),
            cv::Point(65, 65),
            cv::Scalar(0, 255, 0, 255), // Green rectangle
            -1
        );
    }

    fs::path test_dir;
    cv::Mat test_image_bgr;
    cv::Mat test_image_gray;
    cv::Mat test_image_bgra;
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
    cv::Mat loaded_image;
    REQUIRE_NOTHROW(loaded_image = load_jpg(test_file));

    // Verify properties
    REQUIRE(!loaded_image.empty());
    REQUIRE( loaded_image.rows       == test_image_bgr.rows);
    REQUIRE( loaded_image.cols       == test_image_bgr.cols);
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
    using cv::Mat;

    Mat empty_image;
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
    cv::Mat loaded_image;
    REQUIRE_NOTHROW(loaded_image = load_jpg(temp_file));

    // Verify basic properties
    REQUIRE(loaded_image.rows       == test_image_bgr.rows);
    REQUIRE(loaded_image.cols       == test_image_bgr.cols);
    REQUIRE(loaded_image.channels() == test_image_bgr.channels());
    REQUIRE(loaded_image.type()     == test_image_bgr.type());
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

    cv::Mat loaded;
    REQUIRE_NOTHROW(loaded = load_jpg(special_path));
    REQUIRE(!loaded.empty());
}

// Performance test for large images
TEST_CASE_METHOD(JpgIOTestFixture, "LargeImageHandling", "[jpg_io]") {
    using namespace std::chrono;
    using cc::io::save_jpg;
    using cc::io::load_jpg;
    using cv::Mat;
    using cv::Scalar;
    using cv::randu;

    // Create a larger test image
    Mat large_image = Mat::zeros(1000, 1000, CV_8UC3);
    randu(large_image, Scalar::all(0), Scalar::all(255));

    fs::path large_file = test_dir / "large_image.jpg";

    auto start = high_resolution_clock::now();
    REQUIRE_NOTHROW(save_jpg(large_image, large_file));
    auto save_end = high_resolution_clock::now();

    Mat loaded_large;
    REQUIRE_NOTHROW(loaded_large = load_jpg(large_file));
    auto load_end = high_resolution_clock::now();

    // Verify the image was processed correctly
    REQUIRE(loaded_large.rows == large_image.rows);
    REQUIRE(loaded_large.cols == large_image.cols);
    REQUIRE(loaded_large.channels() == large_image.channels());

    // Log timing (optional - mainly for development)
    auto save_time = duration_cast<milliseconds>(save_end - start);
    auto load_time = duration_cast<milliseconds>(load_end - save_end);

    std::cout << "Large image save time: " << save_time.count() << "ms" << std::endl;
    std::cout << "Large image load time: " << load_time.count() << "ms" << std::endl;
}
