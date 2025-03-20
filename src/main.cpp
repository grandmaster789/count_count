#include <iostream>
#include "platform/platform.h"
#include "platform/build_date.h"

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio/registry.hpp>
#include <chrono>
#include <format>
#include <filesystem>
#include <fstream>

#if CVC_PLATFORM != CVC_PLATFORM_WINDOWS
    #error "Currently only Windows is supported (webcam API)"
#endif
/*

  ## Python sketch, should be a fair starting point

import cv2
import numpy as np

# Load the gear image
image = cv2.imread('gear.png')
gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)

# Threshold the image
_, thresh = cv2.threshold(gray, 127, 255, cv2.THRESH_BINARY_INV | cv2.THRESH_OTSU)

# Find contours
contours, _ = cv2.findContours(thresh, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)

# Detect the largest contour (assumed to be the gear)
largest_contour = max(contours, key=cv2.contourArea)

# Approximate the contour for teeth detection
epsilon = 0.01 * cv2.arcLength(largest_contour, True)
approx = cv2.approxPolyDP(largest_contour, epsilon, True)

# Count the number of vertices in approximated contour
teeth_count = len(approx)

print(f"Number of teeth: {teeth_count}")

# Visualize the result
cv2.drawContours(image, [largest_contour], -1, (0, 255, 0), 2)
cv2.imshow('Gear', image)
cv2.waitKey(0)
cv2.destroyAllWindows()
 */

void save_image(const cv::Mat& img) {
    if (img.empty()) {
        std::cout << "Cannot save empty image; skipping\n";
        return;
    }

    auto timestamped_filename = std::format(
            "screengrab_{0:%F}_{0:%OH%OM%OS}.jpg",
            std::chrono::system_clock::now()
    );

    cv::imwrite(timestamped_filename, img);
    std::cout << "Saved " << timestamped_filename << '\n';
}

struct Resolution {
    int width, height;

    friend std::ostream& operator << (std::ostream& os, const Resolution& res) {
        os << '[' << res.width << " x " << res.height << ']';
        return os;
    }
};

// the openCV api doesn't provide a method to actually query supported resolutions
// so we'll just check a couple common ones
bool check_resolution(int camera_id, const Resolution& res) {
    cv::VideoCapture cap(camera_id);
    cap.set(cv::CAP_PROP_FRAME_WIDTH, res.width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, res.height);

    int actual_width  = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int actual_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));

    return
        (actual_width == res.width) &&
        (actual_height == res.height);
}

Resolution select_resolution(int camera_id = 0) {
    // if a resolution was selected before, use that
    if (std::filesystem::exists("count_count.cfg")) {
        std::ifstream cfg("count_count.cfg");
        int width = 0;
        int height = 0;

        cfg >> width >> height;

        return { width, height };
    }

    std::vector<Resolution> resolutions = {
            Resolution { 3840, 2160 }, // 4k
            Resolution { 1920, 1080},  // 1080p
            Resolution { 1280, 720 },  // 720p
            Resolution { 640,  480 }   // 480p
    };

    size_t selected_resolution = 0;

    for (; selected_resolution < resolutions.size(); ++selected_resolution) {
        if (check_resolution(camera_id, resolutions[selected_resolution]))
            break;
    }

    auto result = resolutions[selected_resolution];

    // cache the result on disk
    std::ofstream cfg("count_count.cfg");
    cfg << result.width << ' ' << result.height;

    return result;
}

int main() {
    std::cout << "Starting Counting...\n";
    std::cout << "Running on: " << cvc::ePlatform::current << '\n';
    std::cout << "Built " << cvc::get_days_since_build() << " days ago\n";

    // figure out an appropriate openCV video I/O backend
    auto capture_backends = cv::videoio_registry::getBackends();
    auto camera_backends = cv::videoio_registry::getCameraBackends();
    auto stream_backends = cv::videoio_registry::getStreamBackends();
    auto writer_backends = cv::videoio_registry::getWriterBackends();

    std::cout << "Capture backends: \n";
    for (const auto& x: capture_backends)
        std::cout << ' ' << cv::videoio_registry::getBackendName(x) << '\n';

    std::cout << "Camera backends: \n";
    for (const auto& x: camera_backends)
        std::cout << ' ' << cv::videoio_registry::getBackendName(x) << '\n';

    std::cout << "Stream backends: \n";
    for (const auto& x: stream_backends)
        std::cout << ' ' << cv::videoio_registry::getBackendName(x) << '\n';

    std::cout << "Writer backends: \n";
    for (const auto& x: writer_backends)
        std::cout << ' ' << cv::videoio_registry::getBackendName(x) << '\n';

    {
        auto res = select_resolution();

        std::cout << "Selected resolution: " << res << '\n';

        // for this project, it really doesn't matter all that much which API is used
        cv::VideoCapture webcam(0);
        if (!webcam.isOpened()) {
            std::cerr << "Cannot open webcam\n";
            return -1;
        }

        webcam.set(cv::CAP_PROP_FRAME_WIDTH, res.width);
        webcam.set(cv::CAP_PROP_FRAME_HEIGHT, res.height);

        cv::String main_window = "Webcam Video Stream";
        cv::namedWindow(main_window, cv::WINDOW_KEEPRATIO | cv::WINDOW_AUTOSIZE); // create resizable window

        cv::Mat webcam_image;      // BGR
        cv::Mat hsv_image;         // HSV
        cv::Mat hue_image;         // grayscale (hue)
        cv::Mat blurred;           // grayscale
        cv::Mat edges;
        cv::Mat thresholded_image; // grayscale

        int show = 0;

        // press 'q' or escape to terminate the loop
        bool done = false;
        while (!done) {
            // ----- video input -----
            webcam >> webcam_image;

            if (webcam_image.empty()) {
                std::cerr << "Failed to retrieve image from webcam; exiting application\n";
                break;
            }

            // if the other images haven't been initialized yet, do so now
            if (hsv_image.empty())
                hsv_image.create(webcam_image.size(), CV_8UC3);
            if (hue_image.empty())
                hue_image.create(webcam_image.size(), CV_8UC1);
            if (blurred.empty())
                blurred.create(webcam_image.size(), CV_8UC1);
            if (edges.empty())
                edges.create(webcam_image.size(), CV_8UC1);
            if (thresholded_image.empty())
                thresholded_image.create(webcam_image.size(), CV_8UC1);

            // ----- video processing -----
            cv::cvtColor(webcam_image, hsv_image, cv::COLOR_BGR2HSV_FULL);
            int channel_selection[] = {1, 0};
            cv::mixChannels(&hsv_image, 1, &hue_image, 1, channel_selection, 1);
            cv::GaussianBlur(hue_image, blurred, cv::Size(5, 5), 0); // slight blur seems to improve results
            cv::Canny(blurred, edges, 100, 200);

            std::vector<std::vector<cv::Point>> contours;
            std::vector<cv::Vec4i> hierarchy;
            cv::findContours(edges, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

            if (!contours.empty()) {
                auto largest_contour_idx = [&] {
                   int idx = 0;
                   size_t contour_size = contours[idx].size();

                   for (int i = 0; i < contours.size(); ++i) {
                       size_t sz = contours[idx].size();
                       if (sz > contour_size) {
                           idx = i;
                           contour_size = sz;
                       }
                   }

                   return idx;
                }();

                std::cout << largest_contour_idx << " => " << contours[largest_contour_idx].size() << '\n';

                cv::drawContours(
                        webcam_image,
                        contours,
                        largest_contour_idx,
                        cv::Scalar(128, 255, 255),
                        5,
                        cv::LINE_8,
                        hierarchy
                );
            }

            // ----- rendering -----
            switch (show) {
            case 0: cv::imshow(main_window, webcam_image); break;
            case 1: cv::imshow(main_window, hsv_image); break;
            case 2: cv::imshow(main_window, hue_image); break;
            case 3: cv::imshow(main_window, blurred); break;
            case 4: cv::imshow(main_window, edges); break;
            }

            // ----- key input handling -----

            // waiting 30ms should be enough to display and capture input
            int key = cv::waitKey(30);

            switch (key) {
                case 27: // escape key
                case 'q':
                case 'Q':
                    std::cout << "Exiting application\n";
                    done = true;
                    break;

                case 'g':
                case 'G':
                    save_image(webcam_image);
                    break;

                case 32: // space bar
                    //cycle through shown images
                    if (++show > 4)
                        show = 0;
                    break;

                case -1: // timeout
                    break;

            default:
                std::cout << "Pressed: " << key << '\n';
                break;
            }
        }
    }

    std::cout << "Completed\n";

    return 0;
}