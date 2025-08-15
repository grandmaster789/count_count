#ifndef TYPES_RESOLUTION_H
#define TYPES_RESOLUTION_H

#include <iosfwd>

namespace cc {
    struct Resolution {
        int m_Width;
        int m_Height;

        friend std::ostream& operator << (std::ostream& os, const Resolution& res);
        friend std::istream& operator >> (std::istream& is,       Resolution& res);
    };
}

#endif
