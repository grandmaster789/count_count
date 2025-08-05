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

        cv::Mat convert_to_opencv_format(
            stbi_uc* data,
            int width,
            int height,
            int num_channels
        ) {
            switch (num_channels) {
                case 1:
                    return cv::Mat(height, width, CV_8UC1, data);

                case 3: {
                    // Convert RGB (STB) to BGR (OpenCV)
                    cv::Mat rgb(height, width, CV_8UC3, data);
                    cv::Mat bgr;
                    cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
                    return bgr;
                }

                case 4:
                    return cv::Mat(height, width, CV_8UC4, data);

                default:
                    throw ImageError("Unsupported number of channels: " + std::to_string(num_channels));
            }

        }
    }

    ImageError::ImageError(const std::string& message):
        std::runtime_error(message)
    {
    }

    cv::Mat load_jpg(const std::filesystem::path& p) {
        int width, height, channels;

        StbiResource raw_data(
            stbi_load(
                p.string().c_str(), // filename
                &width,             // (out) image width
                &height,            // (out) image height
                &channels,          // (out) number of channels
                0                   // (desired number of channels when converting)
            )
        );

        if (!raw_data) {
            throw ImageError(
                "Failed to load jpg: '" + p.string() +
                "': " + stbi_failure_reason()
            );
        }

        return detail::convert_to_opencv_format(
            raw_data.get(),
            width,
            height,
            channels
        );
    }

    void save_jpg(const cv::Mat& image, const std::filesystem::path& p) {
        if (image.empty())
            throw ImageError("Cannot save empty image");

        // openCV defaults to BGR images, while stb defaults to RGB... copy and convert
        // convert to RGB if needed
        cv::Mat to_write;
        if (image.channels() == 3)
            cv::cvtColor(image, to_write, cv::COLOR_BGR2RGB);
        else
            to_write = image;

        int success = stbi_write_jpg(
            p.string().c_str(), // filename
            to_write.cols,           // image width
            to_write.rows,           // image height
            to_write.channels(),     // number of channels
            to_write.data,           // data bytes
            90                  // compression ratio
        );

        if (!success) {
            throw ImageError(
                "Failed to save jpg: '" + p.string() +
                "': " + stbi_failure_reason()
            );
        }
    }
}