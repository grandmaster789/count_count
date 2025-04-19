#include "unique.h"

namespace cvc::util {
    UniqueFile open_FILE(
        const std::filesystem::path& p,
        const char* flags
    ) {
        FILE* f = std::fopen(p.string().c_str(), flags);

        return UniqueFile(f, &std::fclose);
    }
}