#include "desktop_window.h"

#include <ostream>

namespace cvc::gui {
    std::ostream& operator << (std::ostream& os, const Position& p) {
        os << '(' << p.m_X << ", " << p.m_Y << ')';
        return os;
    }

    std::ostream& operator << (std::ostream& os, const Size& s) {
        os << '[' << s.m_Width << " x " << s.m_Height << ']';
        return os;
    }
}