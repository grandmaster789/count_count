#ifndef CC_IO_DATA_LOCATION_H
#define CC_IO_DATA_LOCATION_H

#include <filesystem>

namespace cc {
    std::filesystem::path find_data_folder(const std::filesystem::path& exe_path);

    // Folder for writable state (settings, saved frames): %LOCALAPPDATA%\CountVonCount.
    // The install tree is not writable when the application is installed system-wide,
    // so anything the app writes at runtime belongs here instead.
    // Returns an empty path if the folder is unavailable -- callers should fall back.
    std::filesystem::path find_user_data_folder();
}

#endif