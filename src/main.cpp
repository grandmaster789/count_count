#include <iostream>
#include "platform/platform.h"
#include "platform/build_date.h"

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio/registry.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#undef STB_IMAGE_IMPLEMENTATION

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#undef STB_IMAGE_WRITE_IMPLEMENTATION

#include <chrono>
#include <format>
#include <filesystem>
#include <fstream>

#if CVC_PLATFORM != CVC_PLATFORM_WINDOWS
    #error "Currently only Windows is supported"
#endif

cv::Mat g_source_image;      // BGR
cv::Mat g_webcam_image;      // BGR
cv::Mat g_hsv_image;         // HSV
cv::Mat g_hue_image;         // grayscale (hue)

int g_Sensitivity = 255;

cv::SimpleBlobDetector::Params g_blob_detector_params;
cv::Ptr<cv::SimpleBlobDetector> g_blob_detector;

using StbResource = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;

cv::Mat load_jpg(const std::filesystem::path& p) {
    int width, height, channels;

    StbResource raw_data(
        stbi_load(
            p.string().c_str(),
            &width,
            &height,
            &channels,
            0
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
                p.string().c_str(),
                rgb.cols,
                rgb.rows,
                rgb.channels(),
                rgb.data,
                90 // compression ratio
        );
    }
    else {
        stbi_write_jpg(
                p.string().c_str(),
                image.cols,
                image.rows,
                image.channels(),
                image.data,
                90 // compression ratio
        );
    }
}

void save_image(const cv::Mat& img) {
    if (img.empty()) {
        std::cout << "Cannot save empty image; skipping\n";
        return;
    }

    auto timestamped_filename = std::format(
        "screengrab_{0:%F}_{0:%OH%OM%OS}.jpg",
        std::chrono::system_clock::now()
    );

    save_jpg(img, timestamped_filename);
    std::cout << "Saved " << timestamped_filename << '\n';
}

struct Resolution {
    int width, height;

    friend std::ostream& operator << (std::ostream& os, const Resolution& res) {
        os << '[' << res.width << " x " << res.height << ']';
        return os;
    }
};

// the openCV api doesn't provide a method to actually query supported resolutions
// so we'll just check a couple common ones
bool check_resolution(int camera_id, const Resolution& res) {
    cv::VideoCapture cap(camera_id);
    cap.set(cv::CAP_PROP_FRAME_WIDTH, res.width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, res.height);

    int actual_width  = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int actual_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));

    return
        (actual_width == res.width) &&
        (actual_height == res.height);
}

Resolution select_resolution(int camera_id = 0) {
    // if a resolution was selected before, use that
    if (std::filesystem::exists("count_count.cfg")) {
        std::ifstream cfg("count_count.cfg");
        int width = 0;
        int height = 0;

        cfg >> width >> height;

        return { width, height };
    }

    std::vector<Resolution> resolutions = {
        Resolution { 3840, 2160 }, // 4k
        Resolution { 1920, 1080},  // 1080p
        Resolution { 1280, 720 },  // 720p
        Resolution { 640,  480 }   // 480p
    };

    size_t selected_resolution = 0;

    for (; selected_resolution < resolutions.size(); ++selected_resolution) {
        if (check_resolution(camera_id, resolutions[selected_resolution]))
            break;
    }

    auto result = resolutions[selected_resolution];

    // cache the result on disk
    std::ofstream cfg("count_count.cfg");
    cfg << result.width << ' ' << result.height;

    return result;
}

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

void reset_detector() {
    g_blob_detector = cv::SimpleBlobDetector::create(g_blob_detector_params);
}

static void track_sensitivity_changed(
    int   new_value,
    void* /* userdata */
) {
    g_blob_detector_params.maxThreshold = static_cast<float>(new_value) / 100.0f;
    reset_detector();
}

static void main_window_mouse_event(
    int   evt,
    int   x,
    int   y,
    int   /* flags ~ cv::MouseEventFlags */,
    void* /* userdata */
) {
    // https://docs.opencv.org/4.11.0/d7/dfc/group__highgui.html#gab7aed186e151d5222ef97192912127a4
    switch (evt) {
    case cv::MouseEventTypes::EVENT_LBUTTONDOWN:
        std::cout << "Clicked at (" << x << ", " << y << ")\n";
        g_blob_detector_params.blobColor = g_hue_image.at<unsigned char>(y, x);
        reset_detector();
        break;
    default:
        break;
    };
}

int main(int, char* argv[]) {
    namespace fs = std::filesystem;

    std::cout << "Starting Counting...\n";
    std::cout << "Running on: " << cvc::ePlatform::current << '\n';
    std::cout << "Built " << cvc::get_days_since_build() << " days ago\n";

    fs::path exe_path(argv[0]);
    auto data_path = find_data_folder(exe_path);
    std::cout << "Exe path:  " << exe_path.string() << '\n';
    std::cout << "Data path: " << data_path.string() << '\n';

    // default detector parameters
    g_blob_detector_params.filterByColor     = true;
    g_blob_detector_params.blobColor         = 0;
    g_blob_detector_params.collectContours   = true;
    g_blob_detector_params.filterByConvexity = true;
    g_blob_detector_params.minThreshold      = 0;
    g_blob_detector_params.maxThreshold      = 10;

    {
        auto res = select_resolution();

        std::cout << "Selected resolution: " << res << '\n';

        // for this project, it really doesn't matter all that much which API is used
        /*
        cv::VideoCapture webcam(0); // live webcam
        if (!webcam.isOpened()) {
            std::cerr << "Cannot open webcam\n";
            return -1;
        }
        webcam.set(cv::CAP_PROP_FRAME_WIDTH, res.width);
        webcam.set(cv::CAP_PROP_FRAME_HEIGHT, res.height);
        */

        // load via stb image
        auto static_image = load_jpg((data_path / "test_gear_003.jpg"));

        cv::String main_window = "Webcam Video Stream";
        cv::namedWindow(main_window, cv::WINDOW_KEEPRATIO | cv::WINDOW_AUTOSIZE); // create resizable window
        cv::createTrackbar("Sensitivity", main_window, nullptr, 100, track_sensitivity_changed);
        cv::setMouseCallback(main_window, main_window_mouse_event);

        int show = 0;

        // press 'q' or escape to terminate the loop
        bool done = false;
        while (!done) {
            // ----- video input -----
            g_webcam_image = static_image.clone(); // single image
            //webcam >> g_webcam_image; // live video

            if (g_webcam_image.empty()) {
                std::cerr << "Failed to retrieve image from webcam; exiting application\n";
                break;
            }

            // if the other images haven't been initialized yet, do so now
            if (g_hsv_image.empty())
                g_hsv_image.create(g_webcam_image.size(), CV_8UC3);
            if (g_hue_image.empty())
                g_hue_image.create(g_webcam_image.size(), CV_8UC1);

            // ----- video processing -----
            cv::cvtColor(g_webcam_image, g_hsv_image, cv::COLOR_BGR2HSV_FULL);
            int channel_selection[] = {1, 0};
            cv::mixChannels(&g_hsv_image, 1, &g_hue_image, 1, channel_selection, 1);

            /*
            std::vector<std::vector<cv::Point>> contours;
            std::vector<cv::Vec4i> hierarchy;
            cv::findContours(edges, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

            if (!contours.empty()) {
                auto largest_contour_idx = [&] {
                   int idx = 0;
                   size_t contour_size = contours[idx].size();

                   for (int i = 0; i < contours.size(); ++i) {
                       size_t sz = contours[idx].size();
                       if (sz > contour_size) {
                           idx = i;
                           contour_size = sz;
                       }
                   }

                   return idx;
                }();

                std::cout << largest_contour_idx << " => " << contours[largest_contour_idx].size() << '\n';

                cv::drawContours(
                        webcam_image,
                        contours,
                        largest_contour_idx,
                        cv::Scalar(128, 255, 255),
                        5,
                        cv::LINE_8,
                        hierarchy
                );
            }
            */

            // ----- rendering -----
            switch (show) {
            case 0: cv::imshow(main_window, g_webcam_image); break;
            case 1: cv::imshow(main_window, g_hsv_image); break;
            case 2: cv::imshow(main_window, g_hue_image); break;
            //case 3: cv::imshow(main_window, blurred); break;
            //case 4: cv::imshow(main_window, edges); break;
            }

            // ----- key input handling -----

            // waiting 30ms should be enough to display and capture input
            int key = cv::waitKey(30);

            switch (key) {
                case 27: // escape key
                case 'q':
                case 'Q':
                    std::cout << "Exiting application\n";
                    done = true;
                    break;

                case 'g':
                case 'G':
                    save_image(g_webcam_image);
                    break;

                case 32: // space bar
                    //cycle through shown images
                    if (++show > 4)
                        show = 0;
                    break;

                case -1: // timeout
                    break;

            default:
                std::cout << "Pressed: " << key << '\n';
                break;
            }
        }
    }

    std::cout << "Completed\n";

    return 0;
}