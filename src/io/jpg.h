#ifndef COUNT_COUNT_IO_JPG_H
#define COUNT_COUNT_IO_JPG_H

#include <stb_image.h>

#include <filesystem>
#include <opencv2/opencv.hpp>

namespace cc::io {
    using StbResource = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;

    cv::Mat load_jpg(const std::filesystem::path& p);
    void    save_jpg(const cv::Mat& image, const std::filesystem::path& p);
}

#endif