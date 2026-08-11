#include "data_location.h"
#include <cstdlib>
#include <iostream>

namespace cc {
    std::filesystem::path find_data_folder(const std::filesystem::path& exe_path) {
        namespace fs = std::filesystem;

        fs::path current_path = exe_path;

        while (current_path.has_parent_path()) {
            if (fs::exists(current_path / "data"))
                return current_path / "data";

            std::cout << "Checking path: " << current_path << "\\data\n";

            auto parent = current_path.parent_path();
            if (parent == current_path)
                break;

            current_path = parent;
        }

        throw std::runtime_error("Failed to find data folder");
    }

    std::filesystem::path find_user_data_folder() {
        namespace fs = std::filesystem;

        const char* local_app_data = std::getenv("LOCALAPPDATA");
        if (!local_app_data)
            return {};

        auto folder = fs::path(local_app_data) / "CountVonCount";

        // non-throwing: an unavailable folder is reported by returning empty,
        // so the caller can fall back to the install folder
        std::error_code ec;
        fs::create_directories(folder, ec);

        if (ec)
            return {};

        return folder;
    }
}