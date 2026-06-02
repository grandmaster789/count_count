#ifndef CC_APP_MAIN_WINDOW_CONTROLLER_H
#define CC_APP_MAIN_WINDOW_CONTROLLER_H

#include <string>
#include <functional>

#include "types/image.h"
#include "types/color.h"

#include "platform/platform.h"

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

        ~MainWindowController();

        MainWindowController             (const MainWindowController&)     = delete;
        MainWindowController& operator = (const MainWindowController&)     = delete;
        MainWindowController             (MainWindowController&&) noexcept = delete;
        MainWindowController& operator = (MainWindowController&&) noexcept = delete;

        void show(const cc::Image& image);
        void set_sensitivity(int value) const;
        void set_trackbar_position(int value) const;
        void select_color(int x, int y) const;

        int  wait_key(int delay_ms = 30);
        bool is_open() const;

        cc::Color3 get_color_at(int x, int y) const;

    private:
        std::string                m_WindowName;
        SettingsManager*           m_SettingsManager;
        cc::Image                  m_LastImage;
        std::vector<uint8_t>       m_BlitBuffer; // padded buffer for DIB 4-byte row alignment

        // Win32 handles
        HWND   m_Hwnd      = nullptr;
        HWND   m_Trackbar  = nullptr;
        bool   m_IsOpen    = false;

        void create_window();
        void blit_image(const cc::Image& image, HDC hdc = nullptr);

        static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    };
}

#endif
