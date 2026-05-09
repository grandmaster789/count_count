#include "resolution.h"
#include <ostream>
#include <istream>
#include <string>
#include <string_view>

namespace cc {
    std::ostream& operator << (std::ostream& os, const Resolution& res) {
        os << '[' << res.m_Width << " x " << res.m_Height << ']';
        return os;
    }

    std::istream& operator >> (std::istream& is, Resolution& res) {
        char a, b, c;
        is >> a >> res.m_Width >> b >> res.m_Height >> c;

        if (!is.fail() && (a != '[' || b != 'x' || c != ']'))
            is.setstate(std::ios::failbit);

        if (!is.fail() && (res.m_Width <= 0 || res.m_Height <= 0))
            is.setstate(std::ios::failbit);

        return is;
    }
}
