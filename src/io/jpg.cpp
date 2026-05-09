#include "jpg.h"

#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#undef STB_IMAGE_IMPLEMENTATION

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#undef STB_IMAGE_WRITE_IMPLEMENTATION

namespace cc::io {
    namespace detail {
        void StbiDeleter::operator()(stbi_uc* data) const {
            stbi_image_free(data);
        }
    }

    ImageError::ImageError(const std::string& message):
        std::runtime_error(message)
    {
    }

    cc::Image load_jpg(const std::filesystem::path& p) {
        int width, height, channels;

        StbiResource raw_data(
            stbi_load(
                p.generic_string().c_str(), // filename
                &width,             // (out) image width
                &height,            // (out) image height
                &channels,          // (out) number of channels
                0                   // (desired number of channels when converting)
            )
        );

        if (!raw_data) {
            const char* reason = stbi_failure_reason();
            throw ImageError(
                "Failed to load jpg: '" + p.generic_string() +
                "': " + (reason ? reason : "unknown error")
            );
        }

        // STB loads as RGB, we need BGR
        if (channels == 3) {
            // Swap R and B channels in-place
            uint8_t* data = raw_data.get();
            size_t total_pixels = static_cast<size_t>(width) * height;
            for (size_t i = 0; i < total_pixels; ++i) {
                size_t idx = i * 3;
                std::swap(data[idx], data[idx + 2]);
            }
        }

        return cc::Image(height, width, channels, raw_data.get());
    }

    void save_jpg(const cc::Image& image, const std::filesystem::path& p) {
        if (image.empty())
            throw ImageError("Cannot save empty image");

        // We store BGR, but stb expects RGB — need to swap channels
        std::vector<uint8_t> write_data;
        const uint8_t* src = image.data();
        size_t total_bytes = image.total_bytes();

        if (image.channels() == 3) {
            write_data.resize(total_bytes);
            size_t total_pixels = static_cast<size_t>(image.cols()) * image.rows();
            for (size_t i = 0; i < total_pixels; ++i) {
                size_t idx = i * 3;
                write_data[idx + 0] = src[idx + 2]; // R
                write_data[idx + 1] = src[idx + 1]; // G
                write_data[idx + 2] = src[idx + 0]; // B
            }
            src = write_data.data();
        }

        int success = stbi_write_jpg(
            p.generic_string().c_str(),
            image.cols(),
            image.rows(),
            image.channels(),
            src,
            90
        );

        if (!success) {
            throw ImageError(
                "Failed to save jpg: '" + p.generic_string() + "'"
            );
        }
    }
}
