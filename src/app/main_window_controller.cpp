#include "main_window_controller.h"
#include "settings_manager.h"

namespace cc::app {
    SettingsManager* MainWindowController::m_SettingsManager = nullptr;

    MainWindowController::MainWindowController(
        SettingsManager*   settings_manager,
        const std::string& window_name
    ):
        m_WindowName(window_name)
    {
        m_SettingsManager = settings_manager;

        cv::namedWindow(m_WindowName, cv::WINDOW_KEEPRATIO | cv::WINDOW_AUTOSIZE);
        cv::createTrackbar("Sensitivity", m_WindowName, nullptr, 255, trackbar_callback, this);
        cv::setTrackbarPos("Sensitivity", m_WindowName, m_SettingsManager->get().m_ForegroundColorTolerance);
        cv::setMouseCallback(m_WindowName, mouse_callback, this);
    }

    void MainWindowController::show(const cv::Mat& image) {
        cv::imshow(m_WindowName, image);
        m_LastImage = image;
    }

    void MainWindowController::set_sensitivity(int value) {
        m_SettingsManager->get().m_ForegroundColorTolerance = value;
    }

    void MainWindowController::select_color(int x, int y) {
        m_SettingsManager->get().m_ForegroundColor[0] = m_LastImage.at<cv::Vec3b>(y, x)[0];
        m_SettingsManager->get().m_ForegroundColor[1] = m_LastImage.at<cv::Vec3b>(y, x)[1];
        m_SettingsManager->get().m_ForegroundColor[2] = m_LastImage.at<cv::Vec3b>(y, x)[2];

        std::cout << "RGB is #"
                  << std::hex
                  << std::setw(2)
                  << std::setfill('0')
                  << static_cast<int>(m_SettingsManager->get().m_ForegroundColor[0])
                  << static_cast<int>(m_SettingsManager->get().m_ForegroundColor[1])
                  << static_cast<int>(m_SettingsManager->get().m_ForegroundColor[2])
                  << '\n';
    }

    int MainWindowController::wait_key(int delay_ms) {
        return cv::waitKey(delay_ms);
    }

    bool MainWindowController::is_open() const {
        return cv::getWindowProperty(m_WindowName, cv::WND_PROP_VISIBLE) >= 0;
    }

    void MainWindowController::mouse_callback(
        int   event,
        int   x,
        int   y,
        int   /*flags*/,
        void* userdata
    ) {
        auto* self = static_cast<MainWindowController*>(userdata);

        // https://docs.opencv.org/4.11.0/d7/dfc/group__highgui.html#gab7aed186e151d5222ef97192912127a4
        switch (event) {
            case cv::MouseEventTypes::EVENT_LBUTTONDOWN:
                std::cout << "Clicked at (" << x << ", " << y << ")\n";
                self->select_color(x, y);
                break;

            default:
                break;
        };
    }

    void MainWindowController::trackbar_callback(
        int   value,
        void* userdata
    ) {
        auto* self = static_cast<MainWindowController*>(userdata);

        self->set_sensitivity(value);
    }
}