#ifndef CC_IO_JPG_H
#define CC_IO_JPG_H

#include <stb_image.h>

#include <filesystem>

#include "types/image.h"

namespace cc::io {
    namespace detail {
        struct StbiDeleter {
            void operator()(stbi_uc* data) const;
        };
    }

    using StbiResource = std::unique_ptr<stbi_uc, detail::StbiDeleter>;

    class ImageError:
        public std::runtime_error
    {
    public:
        explicit ImageError(const std::string& message);
    };

    cc::Image load_jpg(const std::filesystem::path& p);
    void      save_jpg(const cc::Image& image, const std::filesystem::path& p);
}

#endif
