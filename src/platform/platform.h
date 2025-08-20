#ifndef COUNTVONCOUNT_PLATFORM_H
#define COUNTVONCOUNT_PLATFORM_H

// detect which platform this is compiled on and provide compile-time constants that indicate which it is

#include <iosfwd>
#include <formaT>

#define CVC_PLATFORM_WINDOWS 1
#define CVC_PLATFORM_LINUX   2

#if defined (_WIN32) || defined (_WIN64)
    #define CVC_PLATFORM CVC_PLATFORM_WINDOWS

    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>

    // We're using the MediaFoundation approach for webcam support
    #include <mfapi.h>
    #include <mfidl.h>
    #include <mfreadwrite.h>

    // sigh... the amount of trouble these have caused...
    #ifdef min
    #undef min
    #endif

    #ifdef max
    #undef max
    #endif

#elif defined (LINUX) || defined (__linux__)
    #define CVC_PLATFORM CVC_PLATFORM_LINUX

#else
    #error "Unsupported platform"
#endif

namespace cc {
    enum class ePlatform {
        windows = CVC_PLATFORM_WINDOWS,
        linux   = CVC_PLATFORM_LINUX,

        current = CVC_PLATFORM
    };

    std::ostream& operator << (std::ostream& os, ePlatform x);
}

template<>
struct std::formatter<cc::ePlatform> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const cc::ePlatform& platform, format_context& ctx) const {
        switch (platform) {
            case cc::ePlatform::windows: return std::format_to(ctx.out(), "Windows"); break;
            case cc::ePlatform::linux:   return std::format_to(ctx.out(), "Linux");   break;
        }

        return std::format_to(ctx.out(), "[Unknown]");
    }
};

#endif
