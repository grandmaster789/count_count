#ifndef COUNTVONCOUNT_UTIL_UNIQUE_H
#define COUNTVONCOUNT_UTIL_UNIQUE_H

#include <memory>
#include <cstdio>
#include <filesystem>

namespace cc::util {
    using UniqueFile = std::unique_ptr<FILE, decltype(&std::fclose)>; // decltype(&std::fclose) == int(*)(FILE*)

    /*
     * see https://en.cppreference.com/w/cpp/io/c/fopen for valid open flags
     */
    UniqueFile open_FILE(
        const std::filesystem::path& p,
        const char* open_flags = "rb"
    );
}

#endif
