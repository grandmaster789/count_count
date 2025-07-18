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
#include <numbers>

#if CVC_PLATFORM != CVC_PLATFORM_WINDOWS
    #error "Currently only Windows is supported"
#endif

cv::Mat g_source_image;      // BGR
cv::Mat g_background;        // BGR
cv::Mat g_foreground;        // BGR
cv::Mat g_foreground_mask;   // grayscale
cv::Mat g_webcam_image;      // BGR
cv::Mat g_hsv_image;         // HSV
cv::Mat g_hue_image;         // grayscale (hue)

uint8_t g_selected_hue  = 0;
int     g_hue_tolerance = 10;

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

void save_background(const cv::Mat& img) {
    if (img.empty()) {
        std::cout << "Cannot save empty image as background; skipping\n";
        return;
    }

    g_background = img.clone();

    save_jpg(img, "background.jpg");
    std::cout << "Saved background.jpg\n";
}

struct Resolution {
    int width, height;

    friend std::ostream& operator << (std::ostream& os, const Resolution& res) {
        os << '[' << res.width << " x " << res.height << ']';
        return os;
    }
};

Resolution g_webcam_resolution;

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

void save_settings() {
    std::ofstream cfg("count_count.cfg");
    cfg
            << g_webcam_resolution.width << ' ' << g_webcam_resolution.height << '\n'
            << static_cast<int>(g_selected_hue) << ' ' << static_cast<int>(g_hue_tolerance) << '\n';
}

void load_settings(int camera_id = 0) {
    // if a background image was saved, load it
    g_background = load_jpg("background.jpg"); // if this fails, the background is an empty set

    // if a resolution was selected before, use that
    if (std::filesystem::exists("count_count.cfg")) {
        std::ifstream cfg("count_count.cfg");
        int width         = 0;
        int height        = 0;
        int selected_hue  = 0;
        int hue_tolerance = 0;

        cfg >> width >> height
            >> selected_hue >> hue_tolerance;

        g_webcam_resolution = { width, height };
        g_selected_hue  = static_cast<uint8_t>(selected_hue);
        g_hue_tolerance = hue_tolerance;

        return;
    }

    // no settings available, provide some defaults
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

    g_webcam_resolution = resolutions[selected_resolution];
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

static void track_sensitivity_changed(
    int   new_value,
    void* /* userdata */
) {
    g_hue_tolerance = static_cast<uint8_t>(new_value * 255 / 100);
    std::cout << "Hue tolerance is " << static_cast<int>(g_hue_tolerance) << '\n';
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
        g_selected_hue = g_hue_image.at<unsigned char>(y, x);
        std::cout << "Hue is " << static_cast<int>(g_selected_hue) << '\n';
        break;

    default:
        break;
    };
}

struct HueRange {
    uint8_t m_min_hue = 0;
    uint8_t m_max_hue = 255;
};

HueRange determine_hue_range() {
    int hue = g_selected_hue;
    int min_hue = hue - g_hue_tolerance / 2;
    int max_hue = hue + g_hue_tolerance / 2;

    if (min_hue < 0)
        min_hue = 0;

    if (max_hue > 255)
        max_hue = 255;

    return {
        static_cast<uint8_t>(min_hue),
        static_cast<uint8_t>(max_hue)
    };
}

int main(int, char* argv[]) {
    static const bool use_live_video = true;

    namespace fs = std::filesystem;

    std::cout << "Starting Counting...\n";
    std::cout << "Running on: " << cvc::ePlatform::current << '\n';
    std::cout << "Built " << cvc::get_days_since_build() << " days ago\n";

    fs::path exe_path(argv[0]);
    auto data_path = find_data_folder(exe_path);
    std::cout << "Exe path:  " << exe_path.string() << '\n';
    std::cout << "Data path: " << data_path.string() << '\n';

    {
        load_settings();

        std::cout << "Selected resolution: " << g_webcam_resolution << '\n';

        cv::Mat static_image;
        cv::VideoCapture webcam;

        // for this project, it really doesn't matter all that much which API is used
        if constexpr (use_live_video) {
            webcam = cv::VideoCapture(0);
            if (!webcam.isOpened()) {
                std::cerr << "Cannot open webcam\n";
                return -1;
            }
            webcam.set(cv::CAP_PROP_FRAME_WIDTH,  g_webcam_resolution.width);
            webcam.set(cv::CAP_PROP_FRAME_HEIGHT, g_webcam_resolution.height);
        }
        else {
            // load via stb image
            static_image = load_jpg((data_path / "test_gear_003.jpg"));
        }

        cv::String main_window = "Webcam Video Stream";
        cv::namedWindow(main_window, cv::WINDOW_KEEPRATIO | cv::WINDOW_AUTOSIZE); // create resizable window
        cv::createTrackbar("Sensitivity", main_window, &g_hue_tolerance, 100, track_sensitivity_changed);
        cv::setMouseCallback(main_window, main_window_mouse_event);

        int show = 0;

        // press 'q' or escape to terminate the loop
        bool done = false;
        while (!done) {
            // ----- video input -----
            if constexpr(use_live_video) {
                webcam >> g_webcam_image; // live video
            }
            else {
                g_webcam_image = static_image.clone(); // single image
            }

            if (g_webcam_image.empty()) {
                std::cerr << "Failed to retrieve image from webcam; exiting application\n";
                break;
            }

            // if the other images haven't been initialized yet, do so now
            if (g_hsv_image.empty())
                g_hsv_image.create(g_webcam_image.size(), CV_8UC3);
            if (g_hue_image.empty())
                g_hue_image.create(g_webcam_image.size(), CV_8UC1);
            if (g_foreground.empty())
                g_foreground.create(g_webcam_image.size(), CV_8UC3);
            if (g_foreground_mask.empty())
                g_foreground_mask.create(g_webcam_image.size(), CV_8UC1);

            // ----- video processing -----
            {
                // background filtering doesn't work great during testing, probably need to compensate for automatic
                // white balance as well
                //cv::absdiff(g_webcam_image, g_background, g_foreground);
            }

            cv::cvtColor(g_webcam_image, g_hsv_image, cv::COLOR_BGR2HSV_FULL);
            int channel_selection[] = {1, 0};
            cv::mixChannels(&g_hsv_image, 1, &g_hue_image, 1, channel_selection, 1);

            auto [min_hue, max_hue] = determine_hue_range();
            cv::inRange(g_hue_image, cv::Scalar(min_hue), cv::Scalar(max_hue), g_foreground_mask);

            {
                // apply slight blur to get rid of noise and small details
                cv::medianBlur(g_foreground_mask, g_foreground_mask, 9);
            }

            g_foreground = cv::Mat::zeros(g_webcam_image.size(), CV_8UC3);
            cv::copyTo(
                g_webcam_image,
                g_foreground,
                g_foreground_mask
            );

            std::vector<std::vector<cv::Point>> contours;
            std::vector<cv::Vec4i> hierarchy;

            // https://docs.opencv.org/3.4/d3/dc0/group__imgproc__shape.html#ga17ed9f5d79ae97bd4c7cf18403e1689a
            cv::findContours(
                g_foreground_mask,
                contours,
                hierarchy,
                cv::RETR_CCOMP, // organizes in multi-level list, with external boundaries at the top level
                cv::CHAIN_APPROX_SIMPLE
            );

            if (!contours.empty()) {
                int    idx                   = 0;
                int    largest_component_idx = 0;
                double max_area              = 0;

                // loop through the top-level contours (the iteration sequence is described in the hierarchy vector, and terminates with a negative value)
                // and find the biggest one
                for (; idx >= 0; idx = hierarchy[idx][0]) {
                    const auto& cont = contours[idx];

                    double area = std::fabs(cv::contourArea(cv::Mat(cont)));

                    if (area > max_area) {
                        max_area = area;
                        largest_component_idx = idx;
                    }
                }

                cv::drawContours(
                    g_webcam_image,
                    contours,
                    largest_component_idx,
                    cv::Scalar(0, 0, 255),
                    1, // thickness, or cv::FILLED to fill the entire thing
                    cv::LINE_8,
                    hierarchy
                );

                // find centroid of the contour

                // https://docs.opencv.org/3.4/d3/dc0/group__imgproc__shape.html#ga556a180f43cab22649c23ada36a8a139
                auto moment = cv::moments(
                    contours[largest_component_idx],
                    false
                );
                auto centroid_d = cv::Point2d(
                    moment.m10 / moment.m00,
                    moment.m01 / moment.m00
                );
                auto centroid_i = cv::Point2i(
                    static_cast<int>(centroid_d.x),
                    static_cast<int>(centroid_d.y)
                );

                cv::circle(
                    g_foreground,               // dst
                    centroid_d,                 // center
                    8,                          // radius
                    cv::Scalar(255, 255, 255),  // color
                    -1,                         // thickness (-1 for fill)
                    8,                          // line type
                    0                           // shift
                );

                const auto& largest_contour = contours[largest_component_idx];

                std::vector<double> normalized_distances;
                std::vector<double> distances;
                std::vector<double> angles;

                normalized_distances.reserve(largest_contour.size());
                distances.reserve(largest_contour.size());
                angles.reserve(largest_contour.size());

                double prev_central_angle = -std::numeric_limits<double>::infinity();
                auto   last_pt            = largest_contour[0];

                auto extract = [&](const auto& pt_a, const auto& pt_b) {
                    cv::Point2i d1 = centroid_i - pt_a;
                    cv::Point2i d2 = centroid_i - pt_b;

                    auto delta_angle   = std::atan2(d1.x - d2.x, d1.y - d2.y);
                    auto central_angle = std::atan2(d1.x, d1.y);
                    auto distance      = std::hypot(d1.x, d1.y);

                    // normalize distance across angle; only keep the ones with a monotonically increasing central angle
                    using std::numbers::pi;
                    if ((delta_angle > 0) && (central_angle > prev_central_angle)) {
                        normalized_distances.push_back(distance * 2.0 * pi / delta_angle);
                        angles.push_back(central_angle);
                        distances.push_back(distance);

                        prev_central_angle = central_angle;
                    }
                };

                for (size_t i = 1; i < largest_contour.size(); ++i) {
                    const auto& pt = largest_contour[i];

                    cv::line(g_foreground, centroid_i, pt, cv::Scalar(255,255,255));

                    extract(pt, last_pt);
                    last_pt = pt;
                }

                // fixup for the first/last point
                extract(largest_contour.back(), largest_contour.front());

                // debug -- write and dump the sequence in a CSV file
                if (!std::filesystem::exists("test_sequence.csv")) {
                    std::ofstream out("test_sequence.csv");
                    out << "angle, nd\n";
                    for (size_t i = 0; i < normalized_distances.size(); ++i) {
                        out << angles[i] << ',' << normalized_distances[i] << '\n';
                    }
                }

                // identify clusters in distances
                

                /*
                int min_x = 0;
                int max_x = 0;
                int min_y = 0;
                int max_y = 0;

                for (const auto& pt: largest_contour) {
                    auto delta = centroid_i - pt;
                    min_x = std::min(min_x, delta.x);
                    min_y = std::min(min_y, delta.y);
                    max_x = std::max(max_x, delta.x);
                    max_y = std::max(max_y, delta.y);
                }

                int width  = max_x - min_x;
                int height = max_y - min_y;

                auto rect = cv::Rect(min_x + centroid_i.x, min_y + centroid_i.y, width, height);

                // construct a 1-channel floating point buffer for the ROI
                auto roi_8uc1 = g_foreground_mask(rect).clone();
                cv::Mat roi_32fc1;

                roi_8uc1.convertTo(roi_32fc1, CV_32FC1, 1.0 / 255.0); // normalized to [0..1] range
                cv::imshow("roi", roi_32fc1);
                */
            }

            // ----- rendering -----
            switch (show) {
            case 0: cv::imshow(main_window, g_webcam_image); break;
            case 1: cv::imshow(main_window, g_hsv_image);    break;
            case 2: cv::imshow(main_window, g_hue_image);    break;
            case 3: cv::imshow(main_window, g_foreground);   break;
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

                case 'b':
                case 'B':
                    save_background(g_webcam_image);
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

    save_settings();

    std::cout << "Completed\n";

    return 0;
}