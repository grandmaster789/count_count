#include "application.h"

#include "io/data_location.h"
#include "io/jpg.h"

#include "platform/platform.h"
#include "platform/build_date.h"

#include "processing/foreground.h"
#include "processing/anomalies.h"
#include "processing/contours.h"
#include "processing/boundary_trace.h"
#include "processing/auto_sensitivity.h"

#include "gui/visualization.h"

#include "util/logger.h"

#include <cstring>
#include <map>

namespace {
    int mode_of(const std::deque<int>& values) {
        std::map<int, int> freq;
        for (int v : values) if (v > 0) ++freq[v];
        if (freq.empty()) return -1;
        return std::max_element(freq.begin(), freq.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; })->first;
    }

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

                        if (!m_CameraManager->initialize(settings.m_SelectedCamera, settings.m_FocusValue)) {
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
                // Saturation segmentation: the chromatic gear is high-saturation, the
                // achromatic grey/white background is low-saturation. Matte-surface
                // shadows move value (V), not saturation (S), so this is shadow-robust
                // where BGR Chebyshev distance leaked shadows into the mask.
                //
                // The trackbar value (m_ForegroundColorTolerance, 0..255) doubles as a
                // manual S threshold; values below k_AutoSaturationBelow mean
                // "auto-calibrate from this frame" — the robust default that works
                // whether or not the user has pressed 'A' or tuned the slider.
                int s_threshold = settings.m_ForegroundColorTolerance;
                if (s_threshold < k_AutoSaturationBelow)
                    s_threshold = processing::detect_saturation_threshold(m_SourceImage);

                processing::determine_foreground_by_saturation(
                    s_threshold,
                    m_SourceImage,
                    m_ForegroundMask,
                    m_Foreground,
                    m_BlurTemp
                );

                {
                    int filled = 0;
                    int total  = m_ForegroundMask.rows() * m_ForegroundMask.cols();
                    for (int y = 0; y < m_ForegroundMask.rows(); ++y) {
                        const uint8_t* row = m_ForegroundMask.ptr(y);
                        for (int x = 0; x < m_ForegroundMask.cols(); ++x)
                            if (row[x]) ++filled;
                    }
                    LOG_DEBUG("foreground mask: {}/{} pixels filled ({:.1f}%)",
                              filled, total, 100.0 * filled / total);
                }

                // Find contours using custom boundary tracing
                auto contours = processing::find_contours(m_ForegroundMask);
                LOG_DEBUG("boundary trace: {} contour(s) found", contours.size());

                if (!contours.empty()) {
                    auto maybe_result = processing::process_contours(
                        contours,
                        m_OutputImage
                    );

                    if (maybe_result) {
                        auto& result = *maybe_result;

                        LOG_DEBUG("contour result: {} teeth direct, {} speculative",
                                  result.m_Teeth.size(), result.m_SpeculativeCount);

                        // early exit -- if we have found less than 8 teeth, it's probably not a gear that we found
                        if (result.m_Teeth.size() >= k_MinimumToothCount) {
                            auto tooth_anomaly_mask = processing::find_anomalies(result.m_Teeth);

                            // Temporal mode filter: accumulate recent counts and display
                            // the most frequent value to suppress single-frame noise.
                            auto push = [](std::deque<int>& q, int v, size_t max_size) {
                                q.push_back(v);
                                if (q.size() > max_size) q.pop_front();
                            };
                            push(m_RecentDirectCounts, static_cast<int>(result.m_Teeth.size()), k_TemporalWindow);
                            push(m_RecentSpecCounts,   result.m_SpeculativeCount,                k_TemporalWindow);

                            int smoothed_direct = mode_of(m_RecentDirectCounts);
                            int smoothed_spec   = mode_of(m_RecentSpecCounts);

                            display_results(
                                result.m_Centroid,
                                result.m_Teeth,
                                tooth_anomaly_mask,
                                m_OutputImage,
                                smoothed_spec,
                                smoothed_direct
                            );
                        }
                        else {
                            LOG_DEBUG("too few teeth ({} < {}), skipping display",
                                      result.m_Teeth.size(), k_MinimumToothCount);
                        }
                    }
                    else {
                        LOG_DEBUG("process_contours returned no result");
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

                case 'd':
                case 'D': {
                    auto& logger = cc::util::Logger::instance();
                    if (logger.get_level() == cc::util::LogLevel::DEBUG) {
                        logger.set_level(cc::util::LogLevel::INFO);
                        LOG_INFO("Log level: INFO");
                    } else {
                        logger.set_level(cc::util::LogLevel::DEBUG);
                        LOG_DEBUG("Log level: DEBUG");
                    }
                    break;
                }

                case 13: // enter
                    //cycle through shown images
                    switch (m_Show) {
                        case e_ShowImage::processed_image: m_Show = e_ShowImage::foreground;      break;
                        case e_ShowImage::foreground:      m_Show = e_ShowImage::processed_image; break;
                    }
                    break;

                case '[':
                case ']': {
                    long min_f, max_f, step_f;
                    if (m_CameraManager->get_focus_range(min_f, max_f, step_f)) {
                        long current_f;
                        if (m_CameraManager->get_focus(current_f)) {
                            long next_f = current_f + (key == ']' ? step_f : -step_f);
                            next_f = std::clamp(next_f, min_f, max_f);

                            if (m_CameraManager->set_focus(next_f)) {
                                LOG_INFO("Focus adjusted to {}", next_f);
                                m_SettingsManager->get().m_FocusValue = static_cast<int>(next_f);
                                m_SettingsManager->save();
                            }
                        }
                    }
                    break;
                }

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

        // Calibrate the saturation threshold from the current frame and freeze it
        // into the trackbar/settings so the segmentation uses a fixed value until
        // the user changes it. (Without pressing 'A', main_loop auto-calibrates
        // every frame.)
        int s_threshold = processing::detect_saturation_threshold(m_SourceImage);

        m_SettingsManager->get().m_ForegroundColorTolerance = s_threshold;
        m_UiController->set_trackbar_position(s_threshold);

        LOG_INFO("Auto-detected saturation threshold: S >= {}", s_threshold);
    }

    void Application::print_startup_info() const {
        LOG_INFO("Starting Counting...");
        LOG_INFO("Running on: {}",          ePlatform::current);
        LOG_INFO("Built       {} days ago", get_days_since_build());
        LOG_INFO("Exe path:   {}",          m_ExePath.string());
        LOG_INFO("Data path:  {}",          m_DataPath.string());

        LOG_INFO("Selected resolution: {}", m_SettingsManager->get().m_SourceResolution);

        print_keyboard_controls();
    }

    void Application::print_keyboard_controls() const {
        LOG_INFO("Keyboard controls:");
        LOG_INFO("  ESC, Q    - Exit application");
        LOG_INFO("  ENTER     - Cycle display (Processed / Foreground mask)");
        LOG_INFO("  L         - Toggle live video / static image");
        LOG_INFO("  A         - Auto-calibrate saturation threshold (else auto each frame)");
        LOG_INFO("  G         - Save current frame as screenshot");
        LOG_INFO("  D         - Toggle debug logging");
        LOG_INFO("  [ / ]     - Adjust camera focus");
    }
}
