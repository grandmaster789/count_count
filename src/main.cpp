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

std::filesystem::path find_data_folder(const std::filesystem::path& exe_path) {
    namespace fs = std::filesystem;

    fs::path current_path = exe_path;

    while (current_path.has_parent_path()) {
        if (fs::exists(current_path / "data"))
            return current_path / "data";

        current_path = current_path.parent_path();
    }

    throw std::runtime_error("Failed to find data folder");
}

int main(int, char* argv[]) {
    namespace fs = std::filesystem;

    std::cout << "Starting Counting...\n";
    std::cout << "Running on: " << cvc::ePlatform::current << '\n';
    std::cout << "Built " << cvc::get_days_since_build() << " days ago\n";

    fs::path exe_path(argv[0]);
    auto data_path = find_data_folder(exe_path);
    std::cout << "Exe path: " << exe_path.string() << '\n';
    std::cout << "Data path: " << data_path.string() << '\n';

    return 0;
}
/*
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
 */