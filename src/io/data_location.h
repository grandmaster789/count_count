#ifndef IO_DATA_LOCATION_H
#define IO_DATA_LOCATION_H

#include <filesystem>

namespace cc {
    std::filesystem::path find_data_folder(const std::filesystem::path& exe_path);
}

#endif