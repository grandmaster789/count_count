#include "application.h"

#include "io/data_location.h"
#include "io/jpg.h"

#include "platform/platform.h"
#include "platform/build_date.h"

#include "processing/foreground.h"
#include "processing/edge_mask.h"
#include "processing/anomalies.h"
#include "processing/contours.h"
#include "processing/boundary_trace.h"
#include "processing/auto_sensitivity.h"

#include "gui/visualization.h"

#include "util/logger.h"

#include <cstring>

namespace {
    void save_image(const cc::Image& img, const std::filesystem::path& data_path) {
        using cc::io::save_jpg;

        if (img.empty()) {
            LOG_WARNING("Cannot save empty image; Skipping");
            return;
        }

        auto timestamped_filename = std::format(
            "screengrab_{0:%F}_{0:%OH%OM%OS}.jpg",
            std::chrono::system_clock::now()
        );

        auto full_path = data_path / timestamped_filename;
        save_jpg(img, full_path);

        LOG_INFO("Saved image to {}", full_path.string());
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

        LOG_INFO("Exiting application");

        return 0;
    }

    void Application::shutdown() {
        m_Running = false;
    }

    void Application::initialize_buffers() {
        int rows = m_SourceImage.rows();
        int cols = m_SourceImage.cols();

        m_OutputImage.ensure_size(rows, cols, 3);
        m_Foreground.ensure_size(rows, cols, 3);
        m_ForegroundMask.ensure_size(rows, cols, 1);
        m_BlurTemp.ensure_size(rows, cols, 1);
    }

    void Application::main_loop() {
        cc::Image static_image;

        if (!m_UseLiveVideo) {
            auto path = m_DataPath / "test_real_gear_002.jpg";
            try {
                static_image = cc::io::load_jpg(path);
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to load static image '{}': {}", path.string(), e.what());
                LOG_INFO("Falling back to live video");
                m_UseLiveVideo = true;
            }
        }

        // Track whether m_SourceImage already holds the static image
        bool static_image_loaded = false;

        m_Running = true;

        while (m_Running) {
            try {
                auto settings = m_SettingsManager->get(); // fetch settings once per frame

                // ----- video input -----
                if (m_UseLiveVideo) {
                    static_image_loaded = false;

                    if (!m_CameraManager->is_initialized()) {
                        if (!m_CameraManager->set_resolution(settings.m_SourceResolution))
                            LOG_ERROR("Cannot set resolution to {}", settings.m_SourceResolution);

                        if (!m_CameraManager->initialize(settings.m_SelectedCamera)) {
                            LOG_ERROR("Cannot initialize camera");
                            m_Running = false;
                            break;
                        }
                    }

                    if (!m_CameraManager->grab_frame(m_SourceImage)) {
                        LOG_ERROR("Cannot retrieve frame from webcam");
                        break;
                    }
                }
                else if (!static_image_loaded) {
                    m_SourceImage = static_image.clone();
                    static_image_loaded = true;
                }

                if (m_SourceImage.empty()) {
                    LOG_ERROR("Cannot retrieve image from webcam");
                    break;
                }

                // if the other images haven't been initialized yet, do so now
                initialize_buffers();
                std::memcpy(m_OutputImage.data(), m_SourceImage.data(), m_SourceImage.total_bytes());

                // ----- video processing -----
                switch (m_SegmentationMode) {
                    case e_SegmentationMode::edge_detection:
                        processing::determine_foreground_by_edges(
                            settings.m_ForegroundColorTolerance,
                            m_SourceImage,
                            m_ForegroundMask,
                            m_Foreground
                        );
                        break;
                    case e_SegmentationMode::background_subtraction:
                        processing::determine_foreground(
                            settings.m_ForegroundColor,
                            settings.m_ForegroundColorTolerance,
                            m_SourceImage,
                            m_ForegroundMask,
                            m_Foreground,
                            m_BlurTemp,
                            true,  // invert: target is background, mask output covers the gear
                            true   // use_chebyshev: tolerance is a BGR distance, not hue range
                        );
                        break;
                    default: // color_threshold
                        processing::determine_foreground(
                            settings.m_ForegroundColor,
                            settings.m_ForegroundColorTolerance,
                            m_SourceImage,
                            m_ForegroundMask,
                            m_Foreground,
                            m_BlurTemp
                        );
                        break;
                }

                // Find contours using custom boundary tracing
                auto contours = processing::find_contours(m_ForegroundMask);

                processing::e_ContourSelector selector;
                switch (m_SegmentationMode) {
                    case e_SegmentationMode::edge_detection:
                        selector = processing::e_ContourSelector::most_circular;
                        break;
                    case e_SegmentationMode::background_subtraction:
                        selector = processing::e_ContourSelector::nearest_to_center;
                        break;
                    default:
                        selector = processing::e_ContourSelector::largest_by_area;
                        break;
                }

                if (!contours.empty()) {
                    auto maybe_result = processing::process_contours(
                        contours,
                        m_OutputImage,
                        selector
                    );

                    if (maybe_result) {
                        auto& result = *maybe_result;

                        // early exit -- if we have found less than 8 teeth, it's probably not a gear that we found
                        if (result.m_Teeth.size() >= k_MinimumToothCount) {
                            auto tooth_anomaly_mask = processing::find_anomalies(result.m_Teeth);

                            // and display the result in-image at the center of the gear
                            display_results(
                                result.m_Centroid,
                                result.m_Teeth,
                                tooth_anomaly_mask,
                                m_OutputImage,
                                result.m_SpeculativeCount
                            );
                        }
                    }
                }

                // ----- rendering -----
                switch (m_Show) {
                    case e_ShowImage::processed_image: m_UiController->show(m_OutputImage); break;
                    case e_ShowImage::foreground:      m_UiController->show(m_Foreground);  break;
                    default:
                        break;
                }
            }
            catch (const std::exception& e) {
                LOG_ERROR("Frame processing error: {}", e.what());
            }

            // ----- key input handling -----
            int key = m_UiController->wait_key();

            switch (key) {
                case 27: // escape key
                case 'q':
                case 'Q':
                    m_Running = false;
                    break;

                case 'g':
                case 'G':
                    save_image(m_SourceImage, m_DataPath);
                    break;

                case 'a':
                case 'A':
                    auto_detect_sensitivity();
                    break;

                case 'l':
                case 'L':
                    m_UseLiveVideo = !m_UseLiveVideo;

                    if (!m_UseLiveVideo)
                        static_image = m_SourceImage.clone(); // store the last live image as the static image

                    break;

                case 'e':
                case 'E':
                    switch (m_SegmentationMode) {
                        case e_SegmentationMode::color_threshold:
                            m_SegmentationMode = e_SegmentationMode::edge_detection;
                            LOG_INFO("Segmentation mode: edges (Sobel)");
                            break;
                        case e_SegmentationMode::edge_detection:
                            m_SegmentationMode = e_SegmentationMode::background_subtraction;
                            LOG_INFO("Segmentation mode: background subtraction");
                            break;
                        case e_SegmentationMode::background_subtraction:
                            m_SegmentationMode = e_SegmentationMode::color_threshold;
                            LOG_INFO("Segmentation mode: colour threshold");
                            break;
                    }
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
                    LOG_INFO("Key pressed: {}", key);
                    break;
            }

            // if the window closed, we're done
            if (!m_UiController->is_open())
                m_Running = false;
        }
    }

    void Application::auto_detect_sensitivity() {
        if (m_SourceImage.empty())
            return;

        auto result = (m_SegmentationMode == e_SegmentationMode::background_subtraction)
            ? processing::detect_background_sensitivity(m_SourceImage)
            : processing::detect_sensitivity(m_SourceImage);

        if (!result.valid) {
            LOG_WARNING("Auto-sensitivity detection failed — no usable values found");
            return;
        }

        m_SettingsManager->set_selected_color(result.color);
        m_SettingsManager->get().m_ForegroundColorTolerance = result.tolerance;
        m_UiController->set_trackbar_position(result.tolerance);

        LOG_INFO("Auto-detected color: BGR({},{},{}) tolerance: {}",
            static_cast<int>(result.color.b),
            static_cast<int>(result.color.g),
            static_cast<int>(result.color.r),
            result.tolerance);
    }

    void Application::print_startup_info() const {
        LOG_INFO("Starting Counting...");
        LOG_INFO("Running on: {}",          ePlatform::current);
        LOG_INFO("Built       {} days ago", get_days_since_build());
        LOG_INFO("Exe path:   {}",          m_ExePath.string());
        LOG_INFO("Data path:  {}",          m_DataPath.string());

        LOG_INFO("Selected resolution: {}", m_SettingsManager->get().m_SourceResolution);
    }
}
