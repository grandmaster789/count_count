#ifndef COUNTVONCOUNT_BUILD_DATE_H
#define COUNTVONCOUNT_BUILD_DATE_H

#include <chrono>

// uses the __DATE__ macro to determine when this project was built
namespace cc {
    constexpr std::chrono::system_clock::time_point get_build_date();
    inline    int                                   get_days_since_build();
}

#include "build_date.inl"

#endif //COUNTVONCOUNT_BUILD_DATE_H
