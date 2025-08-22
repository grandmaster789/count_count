#ifndef CC_APP_MAIN_WINDOW_CONTROLLER_H
#define CC_APP_MAIN_WINDOW_CONTROLLER_H

#include <string>
#include <functional>

#include <opencv2/opencv.hpp>

namespace cc::app {
    class SettingsManager;

    class MainWindowController {
    public:
        using ColorSelectedCallback      = std::function<void(uint8_t, uint8_t, uint8_t)>;
        using SensitivityChangedCallback = std::function<void(int)>;

        MainWindowController(
            SettingsManager*   settings_manager,
            const std::string& window_name = "CountCount"
        );

        MainWindowController             (const MainWindowController&)     = delete;
        MainWindowController& operator = (const MainWindowController&)     = delete;
        MainWindowController             (MainWindowController&&) noexcept = delete;
        MainWindowController& operator = (MainWindowController&&) noexcept = delete;

        void show(const cv::Mat& image);
        void set_sensitivity(int value);
        void select_color(int x, int y);

        int  wait_key(int delay_ms = 30);
        bool is_open() const;

        cv::Scalar get_color_at(int x, int y) const;

    private:
        std::string                m_WindowName;
        ColorSelectedCallback      m_ColorCallback;
        SensitivityChangedCallback m_SensitivityCallback;
        SettingsManager*           m_SettingsManager;
        cv::Mat                    m_LastImage;

        static void mouse_callback   (int event, int x, int y, int flags, void* userdata);
        static void trackbar_callback(int value, void* userdata);
    };
}

#endif
