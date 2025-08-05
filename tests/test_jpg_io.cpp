// tests/test_jpg_io.cpp
#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <fstream>
#include "io/jpg.h"

namespace fs = std::filesystem;

class JpgIOTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test directory
        test_dir = fs::temp_directory_path() / "count_von_count_tests";
        fs::create_directories(test_dir);

        // Create test images
        createTestImages();
    }

    void TearDown() override {
        // Clean up test directory
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
    }

    void createTestImages() {
        // Create a simple 3-channel (BGR) test image
        test_image_bgr = cv::Mat::zeros(100, 100, CV_8UC3);
        cv::rectangle(test_image_bgr, cv::Point(25, 25), cv::Point(75, 75), cv::Scalar(255, 0, 0), -1); // Blue rectangle

        // Create a grayscale test image
        test_image_gray = cv::Mat::zeros(50, 50, CV_8UC1);
        cv::circle(test_image_gray, cv::Point(25, 25), 15, cv::Scalar(255), -1); // White circle

        // Create a 4-channel (BGRA) test image
        test_image_bgra = cv::Mat::zeros(75, 75, CV_8UC4);
        cv::rectangle(test_image_bgra, cv::Point(10, 10), cv::Point(65, 65), cv::Scalar(0, 255, 0, 255), -1); // Green rectangle
    }

    fs::path test_dir;
    cv::Mat test_image_bgr;
    cv::Mat test_image_gray;
    cv::Mat test_image_bgra;
};

// Test successful loading of a valid JPEG file
TEST_F(JpgIOTest, LoadValidJpeg) {
    fs::path test_file = test_dir / "test_valid.jpg";

    // Save a test image first
    ASSERT_NO_THROW(cc::io::save_jpg(test_image_bgr, test_file));
    ASSERT_TRUE(fs::exists(test_file));

    // Load the image back
    cv::Mat loaded_image;
    ASSERT_NO_THROW(loaded_image = cc::io::load_jpg(test_file));

    // Verify properties
    EXPECT_FALSE(loaded_image.empty());
    EXPECT_EQ(loaded_image.rows, test_image_bgr.rows);
    EXPECT_EQ(loaded_image.cols, test_image_bgr.cols);
    EXPECT_EQ(loaded_image.channels(), 3); // JPEG should be 3-channel
}

// Test loading a non-existent file
TEST_F(JpgIOTest, LoadNonExistentFile) {
    fs::path non_existent = test_dir / "does_not_exist.jpg";

    EXPECT_THROW(cc::io::load_jpg(non_existent), cc::io::ImageError);
}

// Test loading an invalid file
TEST_F(JpgIOTest, LoadInvalidFile) {
    fs::path invalid_file = test_dir / "invalid.jpg";

    // Create a file with invalid content
    std::ofstream file(invalid_file, std::ios::binary);
    file << "This is not a valid JPEG file";
    file.close();

    EXPECT_THROW(cc::io::load_jpg(invalid_file), cc::io::ImageError);
}

// Test saving a BGR image
TEST_F(JpgIOTest, SaveBGRImage) {
    fs::path output_file = test_dir / "test_bgr.jpg";

    ASSERT_NO_THROW(cc::io::save_jpg(test_image_bgr, output_file));
    EXPECT_TRUE(fs::exists(output_file));

    // Verify file is not empty
    EXPECT_GT(fs::file_size(output_file), 0);
}

// Test saving an empty image
TEST_F(JpgIOTest, SaveEmptyImage) {
    cv::Mat empty_image;
    fs::path output_file = test_dir / "empty.jpg";

    EXPECT_THROW(cc::io::save_jpg(empty_image, output_file), cc::io::ImageError);
}

// Test round-trip (save and load) for BGR image
TEST_F(JpgIOTest, RoundTripBGR) {
    fs::path temp_file = test_dir / "roundtrip_bgr.jpg";

    // Save the test image
    ASSERT_NO_THROW(cc::io::save_jpg(test_image_bgr, temp_file));

    // Load it back
    cv::Mat loaded_image;
    ASSERT_NO_THROW(loaded_image = cc::io::load_jpg(temp_file));

    // Verify basic properties
    EXPECT_EQ(loaded_image.rows, test_image_bgr.rows);
    EXPECT_EQ(loaded_image.cols, test_image_bgr.cols);
    EXPECT_EQ(loaded_image.channels(), test_image_bgr.channels());
    EXPECT_EQ(loaded_image.type(), test_image_bgr.type());
}

// Test saving to non-existent directory
TEST_F(JpgIOTest, SaveToNonExistentDirectory) {
    fs::path non_existent_dir = test_dir / "does_not_exist" / "test.jpg";

    // This should fail because the directory doesn't exist
    EXPECT_THROW(cc::io::save_jpg(test_image_bgr, non_existent_dir), cc::io::ImageError);
}

// Test file path with special characters
TEST_F(JpgIOTest, SpecialCharactersInPath) {
    fs::path special_path = test_dir / "test_with_spaces and-symbols.jpg";

    ASSERT_NO_THROW(cc::io::save_jpg(test_image_bgr, special_path));
    EXPECT_TRUE(fs::exists(special_path));

    cv::Mat loaded;
    ASSERT_NO_THROW(loaded = cc::io::load_jpg(special_path));
    EXPECT_FALSE(loaded.empty());
}

// Performance test for large images
TEST_F(JpgIOTest, LargeImageHandling) {
    // Create a larger test image
    cv::Mat large_image = cv::Mat::zeros(1000, 1000, CV_8UC3);
    cv::randu(large_image, cv::Scalar::all(0), cv::Scalar::all(255));

    fs::path large_file = test_dir / "large_image.jpg";

    auto start = std::chrono::high_resolution_clock::now();
    ASSERT_NO_THROW(cc::io::save_jpg(large_image, large_file));
    auto save_end = std::chrono::high_resolution_clock::now();

    cv::Mat loaded_large;
    ASSERT_NO_THROW(loaded_large = cc::io::load_jpg(large_file));
    auto load_end = std::chrono::high_resolution_clock::now();

    // Verify the image was processed correctly
    EXPECT_EQ(loaded_large.rows, large_image.rows);
    EXPECT_EQ(loaded_large.cols, large_image.cols);
    EXPECT_EQ(loaded_large.channels(), large_image.channels());

    // Log timing (optional - mainly for development)
    auto save_time = std::chrono::duration_cast<std::chrono::milliseconds>(save_end - start);
    auto load_time = std::chrono::duration_cast<std::chrono::milliseconds>(load_end - save_end);

    std::cout << "Large image save time: " << save_time.count() << "ms" << std::endl;
    std::cout << "Large image load time: " << load_time.count() << "ms" << std::endl;
}
