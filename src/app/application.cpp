#include "application.h"

#include "io/data_location.h"
#include "io/jpg.h"

#include "platform/platform.h"
#include "platform/build_date.h"

#include "processing/foreground.h"
#include "processing/anomalies.h"
#include "processing/contours.h"

#include "gui/visualization.h"

namespace {
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
}

namespace cc::app {
    Application::Application(const std::filesystem::path& exe_path):
        m_ExePath(exe_path)
    {
        m_DataPath = find_data_folder(exe_path);

        m_SettingsManager = std::make_unique<SettingsManager>(m_DataPath / "count_count.cfg");
        m_CameraManager   = std::make_unique<CameraManager>();
        m_UiController    = std::make_unique<MainWindowController>(m_SettingsManager.get());
    }

    int Application::run() {
        print_startup_info();
        main_loop();

        std::cout << "Exiting application\n";

        return 0;
    }

    void Application::shutdown() {
        m_Running = false;
    }

    void Application::initialize_buffers() {
        if (
            m_Foreground.empty() ||
            m_Foreground.size() != m_SourceImage.size()
        )
            m_Foreground.create(m_SourceImage.size(), CV_8UC3);

        if (
            m_ForegroundMask.empty() ||
            m_ForegroundMask.size() != m_SourceImage.size()
        )
            m_ForegroundMask.create(m_SourceImage.size(), CV_8UC1);
    }

    void Application::main_loop() {
        cv::Mat static_image;
        cv::Mat output_image;

        if (!m_UseLiveVideo)
            static_image = cc::io::load_jpg((m_DataPath / "test_broken_tooth_002.jpg"));

        m_Running = true;

        while (m_Running) {
            // ----- video input -----
            if (m_UseLiveVideo) {
                if (!m_CameraManager->is_initialized()) {
                    if (!m_CameraManager->set_resolution(m_SettingsManager->get().m_SourceResolution))
                        std::cerr << "Cannot set resolution to " << m_SettingsManager->get().m_SourceResolution << '\n';

                    if (!m_CameraManager->initialize(m_SettingsManager->get().m_SelectedCamera)) {
                        std::cerr << "Cannot initialize camera\n";
                        return;
                    }
                }

                m_SourceImage = m_CameraManager->grab_frame(); // live video
            }
            else
                m_SourceImage = static_image.clone(); // single image

            if (m_SourceImage.empty()) {
                std::cerr << "Failed to retrieve image from webcam; exiting application\n";
                break;
            }

            // if the other images haven't been initialized yet, do so now
            initialize_buffers();
            output_image = m_SourceImage.clone();

            // ----- video processing -----
            processing::determine_foreground(
                m_SettingsManager->get().m_ForegroundColor,
                m_SettingsManager->get().m_ForegroundColorTolerance,
                m_SourceImage,
                m_ForegroundMask,
                m_Foreground
            );

            std::vector<std::vector<cv::Point>> contours;
            std::vector<cv::Vec4i> hierarchy;

            // https://docs.opencv.org/3.4/d3/dc0/group__imgproc__shape.html#ga17ed9f5d79ae97bd4c7cf18403e1689a
            cv::findContours(
                m_ForegroundMask,
                contours,
                hierarchy,
                cv::RETR_CCOMP, // organizes in a multi-level list, with external boundaries at the top level
                cv::CHAIN_APPROX_SIMPLE
            );

            if (!contours.empty()) {
                auto maybe_result = processing::process_contours(
                    contours,
                    hierarchy,
                    output_image
                );

                if (maybe_result) {
                    auto [teeth, centroid_i] = *maybe_result;

                    // early exit -- if we have found less than 8 teeth, it's probably not a gear that we found
                    if (teeth.size() >= k_MinimumToothCount) {
                        auto tooth_anomaly_mask = processing::find_anomalies(teeth);

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
            switch (m_Show) {
                case e_ShowImage::processed_image: m_UiController->show(output_image); break;
                case e_ShowImage::foreground:      m_UiController->show(m_Foreground); break;
            }

            // ----- key input handling -----
            int key = m_UiController->wait_key();

            switch (key) {
                case 27: // escape key
                case 'q':
                case 'Q':
                    std::cout << "Exiting application\n";
                    m_Running = false;
                    break;

                case 'g':
                case 'G':
                    save_image(m_SourceImage);
                    break;

                case 'l':
                case 'L':
                    m_UseLiveVideo = !m_UseLiveVideo;

                    if (!m_UseLiveVideo)
                        static_image = m_SourceImage.clone(); // store the last live image as the static image

                    break;

                case 13: // enter
                    //cycle through shown images
                    switch (m_Show) {
                        case e_ShowImage::processed_image: m_Show = e_ShowImage::foreground;      break;
                        case e_ShowImage::foreground:      m_Show = e_ShowImage::processed_image; break;
                    }
                    break;

                case -1: // timeout
                    break;

                default:
                    std::cout << "Pressed: " << key << '\n';
                    break;
            }

            // if the window closed, we're done
            if (!m_UiController->is_open())
                m_Running = false;
        }
    }

    void Application::print_startup_info() const {
        std::cout << "Starting Counting...\n";
        std::cout << "Running on: " << cc::ePlatform::current << '\n';
        std::cout << "Built       " << cc::get_days_since_build() << " days ago\n";
        std::cout << "Exe path:   " << m_ExePath.string() << '\n';
        std::cout << "Data path:  " << m_DataPath.string() << '\n';

        std::cout << "Selected resolution: " << m_SettingsManager->get().m_SourceResolution << '\n';
    }
}