#include "resolution.h"
#include <ostream>

namespace cc {
    std::ostream& operator << (std::ostream& os, const Resolution& res) {
        os << '[' << res.m_Width << " x " << res.m_Height << ']';
        return os;
    }
}