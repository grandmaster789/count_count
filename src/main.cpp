#include <iostream>
#include "platform/platform.h"
#include "platform/build_date.h"
#include "io/jpg.h"
#include "io/data_location.h"
#include "types/tooth_anomaly.h"
#include "types/resolution.h"
#include "gui/visualization.h"
#include "app/camera_manager.h"
#include "app/settings_manager.h"
#include "app/main_window_controller.h"
#include "processing/foreground.h"
#include "processing/anomalies.h"
#include "processing/contours.h"

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio/registry.hpp>

#include <chrono>
#include <format>
#include <filesystem>
#include <numbers>

#include <execution>

#if CVC_PLATFORM != CVC_PLATFORM_WINDOWS
    #error "Currently only Windows is supported"
#endif

cv::Mat g_foreground;      // BGR
cv::Mat g_foreground_mask; // grayscale
cv::Mat g_source_image;    // BGR

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

void initialize_image_buffers() {
    if (g_foreground.empty())
        g_foreground.create(g_source_image.size(), CV_8UC3);
    if (g_foreground_mask.empty())
        g_foreground_mask.create(g_source_image.size(), CV_8UC1);
}

int main(int, char* argv[]) {
    bool use_live_video = false;

    namespace fs = std::filesystem;

    using cc::processing::determine_foreground;
    using cc::processing::process_contours;
    using cc::processing::find_anomalies;

    std::cout << "Starting Counting...\n";
    std::cout << "Running on: " << cc::ePlatform::current << '\n';
    std::cout << "Built "       << cc::get_days_since_build() << " days ago\n";

    fs::path exe_path(argv[0]);
    auto data_path = cc::find_data_folder(exe_path);
    std::cout << "Exe path:  " << exe_path.string() << '\n';
    std::cout << "Data path: " << data_path.string() << '\n';

    cc::app::SettingsManager settings_manager(data_path / "count_count.cfg");

    {
        std::cout << "Selected resolution: " << settings_manager.get().m_SourceResolution << '\n';

        cv::Mat static_image;
        cv::Mat output_image;

        if (!use_live_video)
            static_image = cc::io::load_jpg((data_path / "test_broken_tooth_002.jpg"));

        int show = 0;

        cc::app::MainWindowController main_window(&settings_manager);
        cc::app::CameraManager        camera;

        // press 'q' or escape to terminate the loop
        bool done = false;
        while (!done) {
            // ----- video input -----
            if (use_live_video) {
                if (!camera.is_initialized()) {
                    if (!camera.set_resolution(settings_manager.get().m_SourceResolution))
                        std::cerr << "Cannot set resolution to " << settings_manager.get().m_SourceResolution << '\n';

                    if (!camera.initialize(settings_manager.get().m_SelectedCamera)) {
                        std::cerr << "Cannot initialize camera\n";
                        return -1;
                    }
                }

                g_source_image = camera.grab_frame(); // live video
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
            determine_foreground(
                settings_manager.get().m_ForegroundColor,
                settings_manager.get().m_ForegroundColorTolerance,
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
                auto maybe_result =  process_contours(
                    contours,
                    hierarchy,
                    output_image
                );

                if (maybe_result) {
                    auto [teeth, centroid_i] = *maybe_result;

                    // early exit -- if we have found less than 8 teeth, it's probably not a gear that we found
                    if (teeth.size() >= k_MinimumToothCount) {
                        auto tooth_anomaly_mask = find_anomalies(teeth);

                        // and display the result in-image at the center of the gear
                        display_results(
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
            case 0: main_window.show(output_image); break;
            case 1: main_window.show(g_foreground); break;
            }

            // ----- key input handling -----
            int key = main_window.wait_key();

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

                case 'l':
                case 'L':
                    use_live_video = !use_live_video;

                    if (!use_live_video)
                        static_image = g_source_image.clone(); // store the last live image as the static image

                    break;

                case 13: // enter
                    //cycle through shown images
                    if (++show > 1)
                        show = 0;
                    break;

                case -1: // timeout
                    break;

            default:
                std::cout << "Pressed: " << key << '\n';
                break;
            }

            // if the window closed, we're done
            if (!main_window.is_open())
                done = true;
        }
    }

    std::cout << "Completed\n";

    return 0;
}
