#ifndef COUNTVONCOUNT_GUI_DESKTOP_WINDOW_H
#define COUNTVONCOUNT_GUI_DESKTOP_WINDOW_H

#include <iosfwd>
#include <memory>
#include <functional>

namespace cvc::gui {
    struct Position {
        int m_X = 0;
        int m_Y = 0;

        friend std::ostream& operator << (std::ostream& os, const Position& p);
    };

    struct Size {
        int m_Width = 0;
        int m_Height = 0;

        friend std::ostream& operator << (std::ostream& os, const Size& s);
    };

    // uses native win32 on windows
    //      gtk          on linux

    class DesktopWindow {
    public:
        DesktopWindow();
        ~DesktopWindow();

        void set_visible(bool visible);
        void set_resizable(bool enabled);
        void set_closable(bool enabled);

        void set_resize_callback(std::function<void()> callback);
        void set_close_callback(std::function<void()> callback);

        void set(const Position& p);
        void set(const Size& s);

        void set_centered(const Size& sz);
        void to_front();

        Size     get_size() const;
        Position get_position() const;

        void* get_native_handle() const;

    private:
        struct Pimpl;
        std::unique_ptr<Pimpl> m_Pimpl;
    };
}

#endif
