#ifndef COUNT_COUNT_IO_JPG_H
#define COUNT_COUNT_IO_JPG_H

#include <stb_image.h>

#include <filesystem>
#include <opencv2/opencv.hpp>

namespace cc::io {
    namespace detail {
        struct StbiDeleter {
            void operator()(stbi_uc* data) const;
        };

        cv::Mat convert_to_opencv_format(
            stbi_uc* data,
            int width,
            int height,
            int num_channels
        );
    }

    using StbiResource = std::unique_ptr<stbi_uc, detail::StbiDeleter>;

    class ImageError:
        public std::runtime_error
    {
    public:
        explicit ImageError(const std::string& message);
    };

    cv::Mat load_jpg(const std::filesystem::path& p);
    void    save_jpg(const cv::Mat& image, const std::filesystem::path& p);
}

#endif