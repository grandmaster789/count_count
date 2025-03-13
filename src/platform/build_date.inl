#ifndef COUNTVONCOUNT_BUILD_DATE_INL
#define COUNTVONCOUNT_BUILD_DATE_INL

#include "build_date.h"

namespace cvc {
    constexpr std::chrono::system_clock::time_point get_build_date() {
        using namespace std::chrono;

        // parse the __DATE__ macro into a chrono timepoint
        // (sort of) const char* __DATE__ = "Mmm dd yyyy";
        // 12 characters including a null terminator
        // see https://en.cppreference.com/w/cpp/preprocessor/replace#Predefined_macros

        // - Bytes 0-2 indicate month, as provided by asctime:
        //   https://en.cppreference.com/w/cpp/chrono/c/asctime
        //
        //   Jan | Feb | Mar | Apr | May | Jun | Jul | Aug | Sep | Okt | Nov | Dec
        //   ----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----
        //     1 |   2 |   3 |   4 |   5 |   6 |   7 |   8 |   9 |  10 |  11 |  12

        constexpr month m {
            []() {
                if constexpr (__DATE__[0] == 'J' && __DATE__[1] == 'a' && __DATE__[2] == 'n') return 1u;
                if constexpr (__DATE__[0] == 'F' && __DATE__[1] == 'e' && __DATE__[2] == 'b') return 2u;
                if constexpr (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r') return 3u;
                if constexpr (__DATE__[0] == 'A' && __DATE__[1] == 'p' && __DATE__[2] == 'r') return 4u;
                if constexpr (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y') return 5u;
                if constexpr (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n') return 6u;
                if constexpr (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l') return 7u;
                if constexpr (__DATE__[0] == 'A' && __DATE__[1] == 'u' && __DATE__[2] == 'g') return 8u;
                if constexpr (__DATE__[0] == 'S' && __DATE__[1] == 'e' && __DATE__[2] == 'p') return 9u;
                if constexpr (__DATE__[0] == 'O' && __DATE__[1] == 'k' && __DATE__[2] == 't') return 10u;
                if constexpr (__DATE__[0] == 'N' && __DATE__[1] == 'o' && __DATE__[2] == 'v') return 11u;
                if constexpr (__DATE__[0] == 'D' && __DATE__[1] == 'e' && __DATE__[2] == 'c') return 12u;

                return 0u;
            } ()
        };

        // Bytes 4-5 are ascii encoded days; byte 4 may also contain a ' ' space
        constexpr day d {
            []() {
                if constexpr (__DATE__[4] == ' ')
                    return static_cast<unsigned int>(__DATE__[5] - '0');

                return static_cast<unsigned int>(
                    (__DATE__[4] - '0') * 10 +
                    (__DATE__[5] - '0')
                );
            }()
        };

        // Bytes 7-10 are year; no spaces here
        constexpr year y {
            []() {
                return static_cast<unsigned int>(
                    (__DATE__[7]  - '0') * 1000 +
                    (__DATE__[8]  - '0') * 100 +
                    (__DATE__[9]  - '0') * 10 +
                    (__DATE__[10] - '0')
                );
            }()
        };

        constexpr auto sd = sys_days(year_month_day(y, m, d));

        return time_point<system_clock, days>(sd);
    }

    inline int get_days_since_build() {
        using namespace std::chrono;

        constexpr auto bd = get_build_date();
        auto now = system_clock::now();

        return duration_cast<days>(now - bd).count();
    }
}

#endif
