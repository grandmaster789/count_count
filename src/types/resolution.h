#ifndef TYPES_RESOLUTION_H
#define TYPES_RESOLUTION_H

#include <iosfwd>
#include <format>

namespace cc {
    struct Resolution {
        int m_Width;
        int m_Height;

        friend std::ostream& operator << (std::ostream& os, const Resolution& res);
        friend std::istream& operator >> (std::istream& is,       Resolution& res);
    };
}

template<>
struct std::formatter<cc::Resolution> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const cc::Resolution& res, format_context& ctx) const {
        return std::format_to(ctx.out(), "[{} x {}]", res.m_Width, res.m_Height);
    }
};

#endif
