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
#include "processing/gear_template.h"
#include "processing/focus.h"

#include "gui/visualization.h"
#include "gui/drawing.h"

#include "util/logger.h"

#include <map>

namespace {
    int mode_of(const std::deque<int>& values) {
        std::map<int, int> freq;
        for (int v : values) if (v > 0) ++freq[v];
        if (freq.empty()) return -1;
        return std::ranges::max_element(freq,
                                        [](const auto& a, const auto& b) { return a.second < b.second; })->first;
    }

    void save_image(const cc::Image& img, const std::filesystem::path& data_path) {
        using cc::io::save_jpg;

        if (img.empty()) {
            LOG_WARNING("Cannot save empty image; Skipping");
            return;
        }

        const auto timestamped_filename = std::format(
            "screengrab_{0:%F}_{0:%OH%OM%OS}.jpg",
            std::chrono::system_clock::now()
        );

        const auto full_path = data_path / timestamped_filename;
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
        Image static_image;

        if (!m_UseLiveVideo) {
            auto path = m_DataPath / "test_real_gear_004.jpg";
            try {
                static_image = io::load_jpg(path);
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
                ++m_FramesSinceRecalib;                   // self-heal cooldown tick

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

                    // Auto-tune once, after the camera is up and delivering frames.
                    // NOTE: exposure/white-balance are deliberately NOT auto-locked at
                    // startup — a bad converge-then-lock (e.g. a blue-tinted WB) raises
                    // the wall's saturation until it overlaps the gear's and breaks
                    // segmentation. The camera's native AE/AWB is left running; 'E'/'W'
                    // remain available to lock manually. We do run autofocus (the
                    // hardware AF won't converge) and the count-guided saturation
                    // calibration.
                    if (!m_StartupTuneDone) {
                        autofocus();
                        auto_detect_sensitivity();
                        m_StartupTuneDone = true;
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
                // Saturation segmentation: the chromatic gear is high saturation, the
                // achromatic gray/white background is low saturation. Matte-surface
                // shadows move value (V), not saturation (S), so this is shadow-robust
                // where BGR Chebyshev distance leaked shadows into the mask.
                //
                // The trackbar value (m_ForegroundColorTolerance, 0..255) doubles as a
                // manual S threshold; values below k_AutoSaturationBelow mean
                // "auto-calibrate from this frame" — the robust default that works
                // whether the user has pressed 'A' or tuned the slider.
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

                        // Self-heal: a broken saturation threshold breaks the rim, so the
                        // FFT latches onto a low-frequency shape artifact (fft << direct).
                        // If that persists, auto-recalibrate the threshold (count-guided),
                        // throttled by a cooldown so a hard scene can't recalibrate every
                        // frame. The new threshold takes effect next frame.
                        const int direct_n = static_cast<int>(result.m_Teeth.size());
                        const bool broken = result.m_SpeculativeCount > 0 &&
                                            result.m_SpeculativeCount * 10 < direct_n * 7;
                        m_BrokenStreak = broken ? m_BrokenStreak + 1 : 0;
                        if (m_BrokenStreak >= k_BrokenStreakTrigger &&
                            m_FramesSinceRecalib >= k_SelfHealCooldown) {
                            LOG_INFO("self-heal: count looks broken (fft={} << direct={}), recalibrating saturation threshold",
                                     result.m_SpeculativeCount, direct_n);
                            auto_detect_sensitivity();
                            m_BrokenStreak       = 0;
                            m_FramesSinceRecalib = 0;
                        }

                        // early exit -- if we have found less than 8 teeth, it's probably not a gear that we found
                        if (result.m_Teeth.size() >= k_MinimumToothCount) {
                            auto tooth_anomaly_mask = processing::find_anomalies(result.m_Teeth);

                            // Temporal mode filter: accumulate recent counts and display
                            // the most frequent value to suppress single-frame noise.
                            auto push = [](std::deque<int>& q, const int v, size_t max_size) {
                                q.push_back(v);

                                if (q.size() > max_size)
                                    q.pop_front();
                            };
                            push(m_RecentDirectCounts, static_cast<int>(result.m_Teeth.size()), k_TemporalWindow);
                            push(m_RecentSpecCounts,   result.m_SpeculativeCount,                k_TemporalWindow);

                            int smoothed_direct = mode_of(m_RecentDirectCounts);
                            int smoothed_spec   = mode_of(m_RecentSpecCounts);

                            // Analysis-by-synthesis: fit an ideal gear of the displayed
                            // count to the rim, draw the fitted template, and surface a
                            // goodness-of-fit confidence (flags non-gears / bad frames).
                            const int fit_count = smoothed_spec > 0 ? smoothed_spec : smoothed_direct;
                            auto fit = processing::fit_gear_template(
                                m_ForegroundMask, result.m_Centroid, fit_count);
                            if (fit.valid && fit.outline.size() > 1)
                                drawing::draw_polyline(m_OutputImage, fit.outline,
                                                       255, 255, 0, /*closed=*/true, 1); // cyan

                            display_results(
                                result.m_Centroid,
                                result.m_Teeth,
                                tooth_anomaly_mask,
                                m_OutputImage,
                                smoothed_spec,
                                smoothed_direct,
                                fit.valid ? fit.score : -1.0
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

            switch (int key = m_UiController->wait_key()) {
                case 27: // escape key
                case 'q':
                case 'Q':
                    m_Running = false;
                    break;

                case 'g':
                case 'G':
                    save_image(m_SourceImage, m_DataPath);
                    break;

                case 'f':
                case 'F':
                    autofocus();
                    break;

                case 'e':
                case 'E':
                    auto_exposure();
                    break;

                case 'w':
                case 'W':
                    auto_white_balance();
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
                    auto& logger = util::Logger::instance();
                    if (logger.get_level() == util::LogLevel::DEBUG) {
                        logger.set_level(util::LogLevel::INFO);
                        LOG_INFO("Log level: INFO");
                    } else {
                        logger.set_level(util::LogLevel::DEBUG);
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

                // non-ascii characters should use the virtualkey macros
                // VK_OEM_4 '[{' for US
                // VK_OEM_6 ']}' for US
                case VK_OEM_4:
                case VK_OEM_6: {
                    long min_f, max_f, step_f;

                    if (m_CameraManager->get_focus_range(min_f, max_f, step_f)) {
                        long current_f;

                        if (m_CameraManager->get_focus(current_f)) {
                            long next_f = current_f + (key == VK_OEM_4 ? step_f : -step_f);
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

    void Application::auto_detect_sensitivity() const {
        if (m_SourceImage.empty())
            return;

        // Count-guided calibration: sweep saturation thresholds, segment + count at
        // each, and pick the one whose gear contour gives the strongest stable tooth
        // count. Robust to worn gears / saturated walls where the cheap histogram
        // threshold breaks the rim. Frozen into trackbar/settings so per-frame
        // segmentation reuses it (no per-frame search).
        int s_threshold = processing::detect_saturation_threshold_by_teeth(m_SourceImage);

        m_SettingsManager->get().m_ForegroundColorTolerance = s_threshold;
        m_UiController->set_trackbar_position(s_threshold);

        LOG_INFO("Auto-detected saturation threshold (count-guided): S >= {}", s_threshold);
    }

    void Application::autofocus() const {
        if (!m_UseLiveVideo || !m_CameraManager->is_initialized()) {
            LOG_INFO("Autofocus skipped (needs a live camera)");
            return;
        }

        long min_f, max_f, step_f;
        if (!m_CameraManager->get_focus_range(min_f, max_f, step_f)) {
            LOG_WARNING("Autofocus: cannot read focus range");
            return;
        }
        if (step_f <= 0) step_f = 1;

        // Let auto-exposure/white-balance settle before locking them: right after the
        // camera opens, exposure is usually still ramping for the first ~dozen frames,
        // and locking a mid-convergence value would bias the whole sweep.
        LOG_INFO("Autofocus: warming up (letting exposure settle)...");
        {
            Image warm;
            for (int i = 0; i < 15; ++i)
                if (!m_CameraManager->grab_frame(warm)) break;
        }

        // Lock exposure/WB/gain so the sharpness metric isn't confounded by
        // auto-exposure drift across the sweep.
        m_CameraManager->begin_focus_sweep();

        // Segment the gear once to fix the ROI (so the per-pixel metric compares
        // focus to focus over a constant region, not a shifting one).
        processing::FocusRoi roi;
        {
            Image frame;

            if (m_CameraManager->grab_frame(frame)) {
                Image mask(frame.rows(), frame.cols(), 1);
                Image fg(frame.rows(), frame.cols(), 3);
                Image blur(frame.rows(), frame.cols(), 1);

                int s = processing::detect_saturation_threshold(frame);

                processing::determine_foreground_by_saturation(s, frame, mask, fg, blur);

                roi = processing::roi_from_mask(mask);
            }
        }
        LOG_INFO("Autofocus: ROI [{},{}]-[{},{}], sweeping focus {}..{} step {} "
                 "(window may be unresponsive for a few seconds)...",
                 roi.x0, roi.y0, roi.x1, roi.y1, min_f, max_f, step_f);

        Image probe;
        auto evaluate = [&](long f) -> double {
            m_CameraManager->set_focus(f);
            m_CameraManager->grab_stable_frame(probe, roi.x0, roi.y0, roi.x1, roi.y1);

            double score = processing::focus_measure(probe, roi);
            LOG_DEBUG("autofocus: focus={} score={:.1f}", f, score);

            return score;
        };

        long best = processing::find_best_focus(min_f, max_f, step_f, evaluate);
        m_CameraManager->set_focus(best);

        m_CameraManager->end_focus_sweep();

        m_SettingsManager->get().m_FocusValue = static_cast<int>(best);
        m_SettingsManager->save();
        LOG_INFO("Autofocus: converged on focus = {}", best);
    }

    void Application::auto_exposure() const {
        if (!m_UseLiveVideo || !m_CameraManager->is_initialized()) {
            LOG_INFO("Auto-exposure skipped (needs a live camera)");
            return;
        }

        // Converge-then-lock: let the camera's own auto-exposure settle, then pin the
        // converged value to manual so it stays deterministic (and the focus sweep
        // and saturation segmentation see a stable exposure).
        LOG_INFO("Auto-exposure: letting the camera converge, then locking...");
        long value = 0;
        if (m_CameraManager->autotune_exposure(value)) {
            m_SettingsManager->get().m_ExposureValue = static_cast<int>(value);
            m_SettingsManager->save();
            LOG_INFO("Auto-exposure: locked exposure = {}", value);
        } else {
            LOG_WARNING("Auto-exposure: failed (control unavailable)");
        }
    }

    void Application::auto_white_balance() const {
        if (!m_UseLiveVideo || !m_CameraManager->is_initialized()) {
            LOG_INFO("Auto-white-balance skipped (needs a live camera)");
            return;
        }

        // Converge-then-lock the camera's own white balance: a neutral, unclipped
        // background keeps the wall low-saturation, which is what the saturation
        // segmentation relies on to separate the chromatic gear.
        LOG_INFO("Auto-white-balance: letting the camera converge, then locking...");
        long value = 0;
        if (m_CameraManager->autotune_white_balance(value)) {
            m_SettingsManager->get().m_WhiteBalanceValue = static_cast<int>(value);
            m_SettingsManager->save();
            LOG_INFO("Auto-white-balance: locked white-balance = {}", value);
        } else {
            LOG_WARNING("Auto-white-balance: failed (control unavailable)");
        }
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

    void Application::print_keyboard_controls() {
        LOG_INFO("Keyboard controls:");
        LOG_INFO("  ESC, Q    - Exit application");
        LOG_INFO("  ENTER     - Cycle display (Processed / Foreground mask)");
        LOG_INFO("  L         - Toggle live video / static image");
        LOG_INFO("  A         - Auto-calibrate saturation threshold (else auto each frame)");
        LOG_INFO("  F         - Autofocus (contrast-detection focus sweep on the gear)");
        LOG_INFO("  E         - Auto-exposure (converge the camera's AE, then lock it)");
        LOG_INFO("  W         - Auto white-balance (converge the camera's AWB, then lock it)");
        LOG_INFO("  G         - Save current frame as screenshot");
        LOG_INFO("  D         - Toggle debug logging");
        LOG_INFO("  [ / ]     - Adjust camera focus");
    }
}
