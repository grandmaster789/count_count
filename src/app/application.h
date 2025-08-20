#ifndef CC_APP_APPLICATION_H
#define CC_APP_APPLICATION_H

#include <filesystem>

#include <opencv2/opencv.hpp>

#include "main_window_controller.h"
#include "camera_manager.h"
#include "settings_manager.h"

namespace cc::app {
    class CameraManager;
    class SettingsManager;

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
        std::filesystem::path m_DataPath;

        std::unique_ptr<SettingsManager>      m_SettingsManager;
        std::unique_ptr<CameraManager>        m_CameraManager;
        std::unique_ptr<MainWindowController> m_UiController;

        bool m_Running      = false;
        bool m_UseLiveVideo = false;

        static constexpr const size_t k_MinimumToothCount = 8;

        enum class e_ShowImage {
            processed_image,
            foreground
        } m_Show = e_ShowImage::processed_image;

        cv::Mat m_SourceImage;    // BGR
        cv::Mat m_Foreground;     // BGR
        cv::Mat m_ForegroundMask; // grayscale

        void initialize_buffers(); // can only be done once the source image is set at least once
        void main_loop();
        void print_startup_info() const;
    };
}

#endif