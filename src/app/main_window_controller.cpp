#include "main_window_controller.h"
#include "settings_manager.h"

#include "util/logger.h"

#include <commctrl.h>
#include <windowsx.h> // GET_X_LPARAM, GET_Y_LPARAM
#pragma comment(lib, "comctl32.lib")

namespace cc::app {
    // Custom window class name
    static constexpr const char* k_WndClassName = "CountCountWindowClass";
    static bool s_ClassRegistered = false;

    MainWindowController::MainWindowController(
        SettingsManager*   settings_manager,
        const std::string& window_name
    ):
        m_WindowName(window_name),
        m_SettingsManager(settings_manager)
    {
        create_window();
    }

    MainWindowController::~MainWindowController() {
        if (m_Hwnd && IsWindow(m_Hwnd))
            DestroyWindow(m_Hwnd);
    }

    void MainWindowController::create_window() {
        if (!s_ClassRegistered) {
            WNDCLASSEXA wc {};
            wc.cbSize        = sizeof(WNDCLASSEXA);
            wc.style         = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc   = wnd_proc;
            wc.cbClsExtra    = 0;
            wc.cbWndExtra    = sizeof(void*);
            wc.hInstance     = GetModuleHandle(nullptr);
            wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
            wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
            wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
            wc.lpszMenuName  = nullptr;
            wc.lpszClassName = k_WndClassName;
            wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);

            if (!RegisterClassExA(&wc)) {
                LOG_ERROR("RegisterClassExA failed: {}", GetLastError());
                return;
            }
            s_ClassRegistered = true;
        }

        // Initial window size (will resize when image is shown)
        m_Hwnd = CreateWindowExA(
            0,
            k_WndClassName,
            m_WindowName.c_str(),
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT,
            800, 600 + 40, // extra height for trackbar
            nullptr,
            nullptr,
            GetModuleHandle(nullptr),
            this // pass 'this' via lpParam
        );

        if (!m_Hwnd) {
            LOG_ERROR("CreateWindowExA failed: {}", GetLastError());
            return;
        }

        // 'this' pointer is stored via WM_CREATE handler from lpParam

        // Create trackbar (slider) as child window
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC  = ICC_BAR_CLASSES;
        InitCommonControlsEx(&icex);

        m_Trackbar = CreateWindowExA(
            0,
            TRACKBAR_CLASSA,
            "Sensitivity",
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_TOOLTIPS,
            0, 0, 800, 30,
            m_Hwnd,
            reinterpret_cast<HMENU>(1), // control ID
            GetModuleHandle(nullptr),
            nullptr
        );

        SendMessageA(m_Trackbar, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
        SendMessageA(m_Trackbar, TBM_SETPOS, TRUE,
            m_SettingsManager->get().m_ForegroundColorTolerance);

        m_IsOpen = true;

        ShowWindow(m_Hwnd, SW_SHOW);
        UpdateWindow(m_Hwnd);
    }

    void MainWindowController::show(const cc::Image& image) {
        if (!m_Hwnd || image.empty())
            return;

        m_LastImage = image.clone();
        blit_image(image);
    }

    void MainWindowController::blit_image(const cc::Image& image, HDC hdc_param) {
        if (!m_Hwnd || image.empty())
            return;

        HDC hdc = hdc_param ? hdc_param : GetDC(m_Hwnd);

        int src_stride = image.cols() * image.channels();
        int dib_stride = (src_stride + 3) & ~3; // DIB rows must be 4-byte aligned

        BITMAPINFO bmi {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = image.cols();
        bmi.bmiHeader.biHeight      = -image.rows(); // negative = top-down
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = static_cast<WORD>(image.channels() * 8);
        bmi.bmiHeader.biCompression = BI_RGB;

        const uint8_t* pixel_data = image.data();

        // If rows are not 4-byte aligned, copy to a padded buffer
        if (src_stride != dib_stride) {
            auto padded_size = static_cast<size_t>(dib_stride) * image.rows();
            m_BlitBuffer.resize(padded_size);
            for (int y = 0; y < image.rows(); ++y) {
                std::memcpy(
                    m_BlitBuffer.data() + static_cast<size_t>(y) * dib_stride,
                    image.ptr(y),
                    src_stride
                );
            }
            pixel_data = m_BlitBuffer.data();
        }

        // Get client area dimensions (excluding trackbar)
        RECT client_rect;
        GetClientRect(m_Hwnd, &client_rect);
        int trackbar_height = 30;
        int draw_height = client_rect.bottom - trackbar_height;

        // Use HALFTONE mode for proper color averaging when scaling
        SetStretchBltMode(hdc, HALFTONE);
        SetBrushOrgEx(hdc, 0, 0, nullptr);

        StretchDIBits(
            hdc,
            0, trackbar_height,                     // dest x, y (below trackbar)
            client_rect.right, draw_height,         // dest width, height
            0, 0,                                   // src x, y
            image.cols(), image.rows(),             // src width, height
            pixel_data,
            &bmi,
            DIB_RGB_COLORS,
            SRCCOPY
        );

        if (!hdc_param)
            ReleaseDC(m_Hwnd, hdc);
    }

    void MainWindowController::set_sensitivity(int value) const {
        m_SettingsManager->get().m_ForegroundColorTolerance = value;
    }

    void MainWindowController::set_trackbar_position(int value) const {
        if (m_Trackbar)
            SendMessageA(m_Trackbar, TBM_SETPOS, TRUE, value);
    }

    void MainWindowController::select_color(int x, int y) const {
        if (m_LastImage.empty())
            return;

        auto color = get_color_at(x, y);
        m_SettingsManager->set_selected_color(color);

        LOG_INFO("Selected color: #{:02x}{:02x}{:02x}",
            static_cast<uint8_t>(color[0]),
            static_cast<uint8_t>(color[1]),
            static_cast<uint8_t>(color[2])
        );
    }

    int MainWindowController::wait_key(int delay_ms) {
        if (!m_Hwnd)
            return -1;

        MSG msg;
        ULONGLONG start = GetTickCount64();
        int key = -1;

        while (true) {
            while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    m_IsOpen = false;
                    return 27; // ESC equivalent
                }

                if (msg.message == WM_KEYDOWN) {
                    key = static_cast<int>(msg.wParam);
                    // Map VK codes to key values
                    if (key == VK_ESCAPE) key = 27;
                    else if (key == VK_RETURN) key = 13;
                    // For letter keys, VK codes match ASCII for uppercase
                    // Convert to lowercase for consistency
                    else if (key >= 'A' && key <= 'Z') {
                        if (!(GetKeyState(VK_SHIFT) & 0x8000))
                            key = key + 32; // to lowercase
                    }
                }

                TranslateMessage(&msg);
                DispatchMessageA(&msg);

                if (key != -1)
                    return key;
            }

            ULONGLONG elapsed = GetTickCount64() - start;
            if (elapsed >= static_cast<ULONGLONG>(delay_ms))
                break;

            Sleep(1);
        }

        return -1; // timeout
    }

    bool MainWindowController::is_open() const {
        return m_IsOpen && m_Hwnd && IsWindow(m_Hwnd);
    }

    cc::Color3 MainWindowController::get_color_at(int x, int y) const {
        if (m_LastImage.empty() ||
            x < 0 || x >= m_LastImage.cols() ||
            y < 0 || y >= m_LastImage.rows())
        {
            return { 0, 0, 0 };
        }

        const uint8_t* pixel = m_LastImage.at(y, x);
        return {
            static_cast<double>(pixel[0]),
            static_cast<double>(pixel[1]),
            static_cast<double>(pixel[2])
        };
    }

    LRESULT CALLBACK MainWindowController::wnd_proc(
        HWND   hwnd,
        UINT   msg,
        WPARAM wParam,
        LPARAM lParam
    ) {
        auto* self = reinterpret_cast<MainWindowController*>(GetWindowLongPtrA(hwnd, 0));

        switch (msg) {
            case WM_CREATE: {
                auto* cs = reinterpret_cast<CREATESTRUCTA*>(lParam);
                SetWindowLongPtrA(hwnd, 0, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
                return 0;
            }

            case WM_LBUTTONDOWN: {
                if (!self || self->m_LastImage.empty())
                    break;

                int click_x = GET_X_LPARAM(lParam);
                int click_y = GET_Y_LPARAM(lParam) - 30; // subtract trackbar height

                if (click_y < 0)
                    break;

                // Map window coordinates to image coordinates
                RECT client_rect;
                GetClientRect(hwnd, &client_rect);
                int draw_width  = client_rect.right;
                int draw_height = client_rect.bottom - 30;

                if (draw_width <= 0 || draw_height <= 0)
                    break;

                int img_x = click_x * self->m_LastImage.cols() / draw_width;
                int img_y = click_y * self->m_LastImage.rows() / draw_height;

                LOG_INFO("Clicked at ({}, {})", img_x, img_y);
                self->select_color(img_x, img_y);
                return 0;
            }

            case WM_HSCROLL: {
                if (!self || reinterpret_cast<HWND>(lParam) != self->m_Trackbar)
                    break;

                int pos = static_cast<int>(SendMessageA(self->m_Trackbar, TBM_GETPOS, 0, 0));
                self->set_sensitivity(pos);
                return 0;
            }

            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                if (self && !self->m_LastImage.empty())
                    self->blit_image(self->m_LastImage, hdc);
                EndPaint(hwnd, &ps);
                return 0;
            }

            case WM_SIZE: {
                if (!self || !self->m_Trackbar)
                    break;

                int width = LOWORD(lParam);
                MoveWindow(self->m_Trackbar, 0, 0, width, 30, TRUE);
                return 0;
            }

            case WM_CLOSE:
                if (self)
                    self->m_IsOpen = false;
                DestroyWindow(hwnd);
                return 0;

            case WM_DESTROY:
                if (self)
                    self->m_IsOpen = false;
                return 0;

            default:
                break;
        }

        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}
