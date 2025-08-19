#include <iostream>
#include "platform/platform.h"
#include "platform/build_date.h"
#include "io/jpg.h"
#include "io/data_location.h"
#include "types/tooth_measurement.h"
#include "types/tooth_anomaly.h"
#include "types/rgb.h"
#include "types/color_range.h"
#include "types/resolution.h"
#include "types/settings.h"
#include "math/statistics.h"
#include "math/angles.h"
#include "gui/visualization.h"
#include "processing/foreground.h"
#include "processing/centroid.h"
#include "processing/count_teeth.h"
#include "processing/anomalies.h"
#include "processing/contours.h"

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio/registry.hpp>

#include <chrono>
#include <format>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <algorithm>
#include <numeric>

#include <execution>

#if CVC_PLATFORM != CVC_PLATFORM_WINDOWS
    #error "Currently only Windows is supported"
#endif

cv::Mat g_foreground;      // BGR
cv::Mat g_foreground_mask; // grayscale
cv::Mat g_source_image;    // BGR

cc::Settings g_Settings;

static constexpr const size_t k_MinimumToothCount = 8;

void save_image(const cv::Mat& img) {
    using cc::io::save_jpg;

    if (img.empty()) {
        std::cout << "Cannot save empty image; skipping\n";
        return;
    }

    auto timestamped_filename = std::format(
        "screengrab_{0:%F}_{0:%OH%OM%OS}.jpg",
        std::chrono::system_clock::now()
    );

    save_jpg(img, timestamped_filename);
    std::cout << "Saved " << timestamped_filename << '\n';
}

// the openCV api doesn't provide a method to actually query supported resolutions
// so we'll just check a couple common ones
bool check_resolution(int camera_id, const cc::Resolution& res) {
    cv::VideoCapture cap(camera_id);
    cap.set(cv::CAP_PROP_FRAME_WIDTH, res.m_Width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, res.m_Height);

    int actual_width  = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int actual_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));

    return
        (actual_width == res.m_Width) &&
        (actual_height == res.m_Height);
}

void save_settings() {
    std::ofstream cfg("count_count.cfg");
    cfg << g_Settings;
}

void load_settings(int camera_id = cc::FIRST_CAMERA) {
    // if a resolution was selected before, use that
    if (std::filesystem::exists("count_count.cfg")) {
        std::ifstream cfg("count_count.cfg");

        cfg >> g_Settings;

        return;
    }

    // no settings available, provide some defaults
    std::vector<cc::Resolution> resolutions = {
        cc::Resolution { 3840, 2160 }, // 4k
        cc::Resolution { 1920, 1080},  // 1080p
        cc::Resolution { 1280, 720 },  // 720p
        cc::Resolution { 640,  480 }   // 480p
    };

    size_t selected_resolution = 0;

    for (; selected_resolution < resolutions.size(); ++selected_resolution) {
        if (check_resolution(camera_id, resolutions[selected_resolution]))
            break;
    }

    g_Settings.m_SourceResolution = resolutions[selected_resolution];
}

struct SensitivityBarHandler {
    void on_track_sensitivity_changed(int new_value) {
        g_Settings.m_ForegroundColorTolerance = new_value;
    }
};

static void track_sensitivity_changed(
    int   new_value,
    void* userdata
) {
    static_cast<SensitivityBarHandler*>(userdata)->on_track_sensitivity_changed(new_value);
}

static void main_window_mouse_event(
    int   evt,
    int   x,
    int   y,
    int   /* flags ~ cv::MouseEventFlags */,
    void* /* userdata */
) {
    // https://docs.opencv.org/4.11.0/d7/dfc/group__highgui.html#gab7aed186e151d5222ef97192912127a4
    switch (evt) {
    case cv::MouseEventTypes::EVENT_LBUTTONDOWN:
        std::cout << "Clicked at (" << x << ", " << y << ")\n";

        g_Settings.m_ForegroundColor[0] = g_source_image.at<cv::Vec3b>(y, x)[0];
        g_Settings.m_ForegroundColor[1] = g_source_image.at<cv::Vec3b>(y, x)[1];
        g_Settings.m_ForegroundColor[2] = g_source_image.at<cv::Vec3b>(y, x)[2];

        std::cout << "RGB is #"
            << std::hex
            << std::setw(2)
            << std::setfill('0')
            << static_cast<int>(g_Settings.m_ForegroundColor[0])
            << static_cast<int>(g_Settings.m_ForegroundColor[1])
            << static_cast<int>(g_Settings.m_ForegroundColor[2])
            << '\n';
        break;

    default:
        break;
    };
}

void initialize_image_buffers() {
    if (g_foreground.empty())
        g_foreground.create(g_source_image.size(), CV_8UC3);
    if (g_foreground_mask.empty())
        g_foreground_mask.create(g_source_image.size(), CV_8UC1);
}

int main(int, char* argv[]) {
    bool use_live_video = false;

    namespace fs = std::filesystem;

    std::cout << "Starting Counting...\n";
    std::cout << "Running on: " << cc::ePlatform::current << '\n';
    std::cout << "Built "       << cc::get_days_since_build() << " days ago\n";

    fs::path exe_path(argv[0]);
    auto data_path = cc::find_data_folder(exe_path);
    std::cout << "Exe path:  " << exe_path.string() << '\n';
    std::cout << "Data path: " << data_path.string() << '\n';

    {
        load_settings();

        std::cout << "Selected resolution: " << g_Settings.m_SourceResolution << '\n';

        cv::Mat          static_image;
        cv::VideoCapture webcam;
        cv::Mat          output_image;

        if (!use_live_video)
            static_image = cc::io::load_jpg((data_path / "test_broken_tooth_002.jpg"));

        SensitivityBarHandler trackbar_handler;

        cv::String main_window = "Webcam Video Stream";
        cv::namedWindow(main_window, cv::WINDOW_KEEPRATIO | cv::WINDOW_AUTOSIZE); // create a resizeable window
        cv::createTrackbar("Sensitivity", main_window, nullptr, 255, track_sensitivity_changed, &trackbar_handler);
        cv::setTrackbarPos("Sensitivity", main_window, g_Settings.m_ForegroundColorTolerance);
        cv::setMouseCallback(main_window, main_window_mouse_event);

        int show = 0;

        // press 'q' or escape to terminate the loop
        bool done = false;
        while (!done) {
            // ----- video input -----
            if (use_live_video) {
                if (!webcam.isOpened()) {
                    webcam = cv::VideoCapture(0);

                    if (!webcam.isOpened()) {
                        std::cerr << "Cannot open webcam\n";
                        return -1;
                    }

                    webcam.set(cv::CAP_PROP_FRAME_WIDTH,  g_Settings.m_SourceResolution.m_Width);
                    webcam.set(cv::CAP_PROP_FRAME_HEIGHT, g_Settings.m_SourceResolution.m_Height);
                }

                webcam >> g_source_image; // live video
            }
            else
                g_source_image = static_image.clone(); // single image

            if (g_source_image.empty()) {
                std::cerr << "Failed to retrieve image from webcam; exiting application\n";
                break;
            }

            // if the other images haven't been initialized yet, do so now
            initialize_image_buffers();
            output_image = g_source_image.clone();

            // ----- video processing -----
            cc::processing::determine_foreground(
                g_Settings.m_ForegroundColor,
                g_Settings.m_ForegroundColorTolerance,
                g_source_image,
                g_foreground_mask,
                g_foreground
            );

            std::vector<std::vector<cv::Point>> contours;
            std::vector<cv::Vec4i> hierarchy;

            // https://docs.opencv.org/3.4/d3/dc0/group__imgproc__shape.html#ga17ed9f5d79ae97bd4c7cf18403e1689a
            cv::findContours(
                g_foreground_mask,
                contours,
                hierarchy,
                cv::RETR_CCOMP, // organizes in a multi-level list, with external boundaries at the top level
                cv::CHAIN_APPROX_SIMPLE
            );

            if (!contours.empty()) {
                auto maybe_result =  cc::processing::process_contours(
                    contours,
                    hierarchy,
                    output_image
                );

                if (maybe_result) {
                    auto [tooth_count, teeth, centroid_i] = *maybe_result;

                    // early exit -- if we have found less than 8 teeth, it's probably not a gear that we found
                    if (tooth_count >= k_MinimumToothCount) {
                        auto tooth_anomaly_mask = cc::processing::find_anomalies(teeth);

                        // and display the result in-image at the center of the gear
                        display_results(
                            tooth_count,
                            centroid_i,
                            teeth,
                            tooth_anomaly_mask,
                            output_image
                        );
                    }
                }
            }

            // ----- rendering -----
            switch (show) {
            case 0: cv::imshow(main_window,  output_image); break;
            case 1: cv::imshow(main_window, g_foreground);  break;
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
                    save_image(g_source_image);
                    break;

                case ' ': // space bar
                    //cycle through shown images
                    if (++show > 1)
                        show = 0;
                    break;

                case 13: // enter
                    use_live_video = !use_live_video;

                    if (!use_live_video)
                        static_image = g_source_image.clone(); // store the last live image as the static image

                    break;

                case -1: // timeout
                    break;

            default:
                std::cout << "Pressed: " << key << '\n';
                break;
            }

            // if the window closed, we're done
            if (cv::getWindowProperty(main_window, cv::WND_PROP_VISIBLE) < 1)
                done = true;
        }
    }

    save_settings();

    std::cout << "Completed\n";

    return 0;
}
