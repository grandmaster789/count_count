#include "data_location.h"

namespace cc {
    std::filesystem::path find_data_folder(const std::filesystem::path& exe_path) {
        namespace fs = std::filesystem;

        fs::path current_path = exe_path;

        while (current_path.has_parent_path()) {
            if (fs::exists(current_path / "data"))
                return current_path / "data";

            current_path = current_path.parent_path();
        }

        throw std::runtime_error("Failed to find data folder");
    }
}