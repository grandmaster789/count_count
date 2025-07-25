#include "jpg.h"

#include <iostream>


#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#undef STB_IMAGE_IMPLEMENTATION

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#undef STB_IMAGE_WRITE_IMPLEMENTATION

namespace cc::io {
    cv::Mat load_jpg(const std::filesystem::path& p) {
        int width, height, channels;

        StbResource raw_data(
            stbi_load(
                p.string().c_str(), // filename
                &width,             // (out) image width
                &height,            // (out) image height
                &channels,          // (out) number of channels
                0                   // (desired number of channels when converting)
            ),
            &stbi_image_free
        );

        if (!raw_data) {
            std::cout << "Failed to load jpg: " << p.string() << '\n';
            std::cout << stbi_failure_reason();
            return {};
        }

        switch (channels) {
            case 1: return { height, width, CV_8UC1, raw_data.get() };
            case 3: {
                // assume data is in RGB -- openCV expects BGR so convert it
                cv::Mat rgb(height, width, CV_8UC3, raw_data.get());
                cv::Mat bgr;
                cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
                return bgr;
            }

            case 4: return { height, width, CV_8UC4, raw_data.get() };

            default:
                return {};
        }
    }

    void save_jpg(const cv::Mat& image, const std::filesystem::path& p) {
        // openCV defaults to BGR images, while stb defaults to RGB... copy and convert

        if (image.channels() == 3) {
            cv::Mat rgb;

            cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);

            stbi_write_jpg(
                p.string().c_str(), // filename
                rgb.cols,           // image width
                rgb.rows,           // image height
                rgb.channels(),     // number of channels
                rgb.data,           // data bytes
                90                  // compression ratio
            );
        }
        else {
            stbi_write_jpg(
                p.string().c_str(), // filename
                image.cols,         // image width
                image.rows,         // image height
                image.channels(),   // number of channels
                image.data,         // data bytes
                90                  // compression ratio
            );
        }
    }
}