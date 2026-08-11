#ifndef CC_APP_APPLICATION_H
#define CC_APP_APPLICATION_H

#include <deque>
#include <filesystem>

#include "types/image.h"

#include "main_window_controller.h"
#include "camera_manager.h"
#include "settings_manager.h"

namespace cc::app {
    class Application {
    public:
        explicit Application(const std::filesystem::path& exe_path);
        ~Application() = default;

        Application             (const Application&) = delete;
        Application& operator = (const Application&) = delete;
        Application             (Application&&) noexcept = delete;
        Application& operator = (Application&&) noexcept = delete;

        int run();
        void shutdown();

    private:
        std::filesystem::path m_ExePath;
        std::filesystem::path m_DataPath;     // read-only assets, ships with the app
        std::filesystem::path m_UserDataPath; // writable state (settings, saved frames)

        std::unique_ptr<SettingsManager>      m_SettingsManager;
        std::unique_ptr<CameraManager>        m_CameraManager;
        std::unique_ptr<MainWindowController> m_UiController;

        bool m_Running         = false;
        bool m_UseLiveVideo    = true;  // when this is false, use a reference image instead
        bool m_StartupTuneDone = false; // auto-tune exposure/WB/focus once after camera init

        static constexpr size_t k_MinimumToothCount = 8;
        static constexpr size_t k_TemporalWindow    = 10;

        // Self-heal: if the count looks broken (FFT latched onto a low-frequency
        // shape artifact, i.e. fft << direct) for this many consecutive frames,
        // auto-recalibrate the saturation threshold — throttled by a cooldown so a
        // genuinely hard scene can't recalibrate every frame (the search is heavy).
        static constexpr int k_BrokenStreakTrigger = 4;
        static constexpr int k_SelfHealCooldown    = 30; // frames between recalibrations
        int m_BrokenStreak       = 0;
        int m_FramesSinceRecalib = k_SelfHealCooldown; // allow an immediate first heal

        // Trackbar/tolerance values below this are treated as "auto-calibrate the
        // saturation threshold each frame" rather than a manual S threshold. Below
        // ~25 a saturation threshold would flood the mask with a low-S background, so
        // such values never make sense as a manual setting anyway.
        static constexpr int k_AutoSaturationBelow = 25;

        enum class e_ShowImage {
            processed_image,
            foreground
        } m_Show = e_ShowImage::processed_image;

        std::deque<int> m_RecentDirectCounts;
        std::deque<int> m_RecentSpecCounts;

        Image m_SourceImage;    // BGR
        Image m_OutputImage;    // BGR — reused each frame
        Image m_Foreground;     // BGR
        Image m_ForegroundMask; // grayscale
        Image m_BlurTemp;       // grayscale — scratch buffer for median blur

        void initialize_buffers(); // can only be done once the source image is set at least once
        void main_loop();
        void auto_detect_sensitivity() const;
        void autofocus() const;          // contrast-detection focus sweep on the gear
        void auto_exposure() const;      // converge-then-lock the camera's auto-exposure
        void auto_white_balance() const; // converge-then-lock the camera's auto white-balance
        void print_startup_info() const;

        static void print_keyboard_controls();
    };
}

#endif
