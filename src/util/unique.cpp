#include "unique.h"

namespace cc::util {
    UniqueFile open_FILE(
        const std::filesystem::path& p,
        const char* flags
    ) {
        FILE* f = std::fopen(p.string().c_str(), flags);

        if (!f)
            throw std::runtime_error("Failed to open file: " + p.string());

        return { f, std::fclose };
    }
}