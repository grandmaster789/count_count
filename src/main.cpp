#include <iostream>
#include "platform/platform.h"
#include "platform/build_date.h"
#include "io/jpg.h"

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio/registry.hpp>

#include <chrono>
#include <format>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <algorithm>

#if CVC_PLATFORM != CVC_PLATFORM_WINDOWS
    #error "Currently only Windows is supported"
#endif

cv::Mat g_foreground;        // BGR
cv::Mat g_foreground_mask;   // grayscale
cv::Mat g_webcam_image;      // BGR

using RGB = std::array<uint8_t, 3>;

RGB g_selected_rgb  = {};
int g_rgb_tolerance = 10; // 0-255 range

//
void save_image(const cv::Mat& img) {
    using cc::io::save_jpg;

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
            // bit of casting to keep it human-readable
            << static_cast<int>(g_selected_rgb[0]) << ' '
            << static_cast<int>(g_selected_rgb[1]) << ' '
            << static_cast<int>(g_selected_rgb[2]) << ' '
            << g_rgb_tolerance << '\n';
}

void load_settings(int camera_id = 0) {
    // if a resolution was selected before, use that
    if (std::filesystem::exists("count_count.cfg")) {
        std::ifstream cfg("count_count.cfg");
        int width         = 0;
        int height        = 0;

        cfg >> width >> height
            >> g_selected_rgb[0]
            >> g_selected_rgb[1]
            >> g_selected_rgb[2]
            >> g_rgb_tolerance;

        g_webcam_resolution = { width, height };

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

struct SensitivityBarHandler {
    int m_ToleranceRange = 0;

    void on_track_sensitivity_changed(int new_value) {
        m_ToleranceRange = new_value;
    }
};

static void track_sensitivity_changed(
    int   new_value,
    void* userdata
) {
    static_cast<SensitivityBarHandler*>(userdata)->on_track_sensitivity_changed(new_value);
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
        g_selected_rgb[0] = g_webcam_image.at<cv::Vec3b>(y, x)[0];
        g_selected_rgb[1] = g_webcam_image.at<cv::Vec3b>(y, x)[1];
        g_selected_rgb[2] = g_webcam_image.at<cv::Vec3b>(y, x)[2];

        std::cout << "RGB is #"
            << std::hex
            << std::setw(2)
            << std::setfill('0')
            << static_cast<int>(g_selected_rgb[0])
            << static_cast<int>(g_selected_rgb[1])
            << static_cast<int>(g_selected_rgb[2])
            << '\n';
        break;

    default:
        break;
    };
}

struct ColorRange {
    RGB m_MinRGB = { 0x00, 0x00, 0x00 };
    RGB m_MaxRGB = { 0xFF, 0xFF, 0xFF };
};

ColorRange determine_color_range(int tolerance_range) {
    ColorRange result;

    for (int i = 0; i < 3; ++i) {
        int lower = g_selected_rgb[i] - (tolerance_range / 2);
        int upper = g_selected_rgb[i] + (tolerance_range / 2);

        if (lower < 0)
            lower = 0;

        if (upper > 255)
            upper = 255;

        result.m_MinRGB[i] = static_cast<uint8_t>(lower);
        result.m_MaxRGB[i] = static_cast<uint8_t>(upper);
    }

    return result;
}

void initialize_image_buffers() {
    if (g_foreground.empty())
        g_foreground.create(g_webcam_image.size(), CV_8UC3);
    if (g_foreground_mask.empty())
        g_foreground_mask.create(g_webcam_image.size(), CV_8UC1);
}

void determine_foreground(int tolerance_range) {
    auto [min_rgb, max_rgb] = determine_color_range(tolerance_range);
    cv::inRange(
        g_webcam_image,
        min_rgb,
        max_rgb,
        g_foreground_mask
    );

    // apply slight blur to get rid of noise and small details
    cv::medianBlur(g_foreground_mask, g_foreground_mask, 9);

    g_foreground = cv::Mat::zeros(g_webcam_image.size(), CV_8UC3);
    cv::copyTo(
        g_webcam_image,
        g_foreground,
        g_foreground_mask
    );
}

cv::Point_<int> find_centroid(
    const std::vector<std::vector<cv::Point>>& contours,
    int                                        largest_component_idx,
    bool                                       do_visualization = false
) {
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

    if (do_visualization)
        cv::circle(
            g_foreground,               // dst
            centroid_d,                 // center
            8,                          // radius
            cv::Scalar(255, 255, 255),  // color
            -1,                         // thickness (-1 for fill)
            8,                          // line type
            0                           // shift
        );

    return centroid_i;
}

int main(int, char* argv[]) {
    constexpr static const bool use_live_video = false;

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
            static_image = cc::io::load_jpg((data_path / "test_broken_tooth_001.jpg"));
        }

        SensitivityBarHandler trackbar_handler;

        cv::String main_window = "Webcam Video Stream";
        cv::namedWindow(main_window, cv::WINDOW_KEEPRATIO | cv::WINDOW_AUTOSIZE); // create a resizeable window
        cv::createTrackbar("Sensitivity", main_window, nullptr, 255, track_sensitivity_changed, &trackbar_handler);
        cv::setTrackbarPos("Sensitivity", main_window, g_rgb_tolerance);
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
            initialize_image_buffers();

            // ----- video processing -----
            determine_foreground(trackbar_handler.m_ToleranceRange);

            std::vector<std::vector<cv::Point>> contours;
            std::vector<cv::Vec4i> hierarchy;

            // https://docs.opencv.org/3.4/d3/dc0/group__imgproc__shape.html#ga17ed9f5d79ae97bd4c7cf18403e1689a
            cv::findContours(
                g_foreground_mask,
                contours,
                hierarchy,
                cv::RETR_CCOMP, // organizes in a multi-level list, with external boundaries at the top level
                cv::CHAIN_APPROX_SIMPLE
            );

            if (!contours.empty()) {
                int    idx                   = 0;
                int    largest_component_idx = 0;
                double max_area              = 0;

                // loop through the top-level contours (the iteration sequence is described in the hierarchy vector
                // and terminates with a negative value) and find the biggest one.
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
                auto centroid_i = find_centroid(
                    contours,
                    largest_component_idx,
                    true
                );

                const auto& largest_contour = contours[largest_component_idx];

                // loop over the largest contour, collect 'similar' distances to the center point
                std::vector<double> distances;
                for (const auto& pt : largest_contour) {
                    auto distance = std::hypot(
                        pt.x - centroid_i.x,
                        pt.y - centroid_i.y
                    );

                    distances.push_back(distance);
                }

                // find the largest and smallest distances to the center, use half that as a threshold
                auto min_max            = std::minmax_element(distances.begin(), distances.end());
                auto distance_threshold = (*min_max.first + *min_max.second) / 2.0;

                std::vector<uint8_t> tooth_mask(largest_contour.size(), 0);

                for (size_t i = 0; i < largest_contour.size(); ++i)
                    tooth_mask[i] = (distances[i] < distance_threshold) ? 1 : 0;

                // Here we figure out how often the threshold is crossed to determine a tooth count
                // -- only count the 'rising' edges
                int tooth_count = 0;
                for (size_t i = 0; i < tooth_mask.size() - 1; ++i)
                    if (!tooth_mask[i] && tooth_mask[i + 1])
                        ++tooth_count;

                // fixup for the starting point
                if (!tooth_mask.back() && tooth_mask.front())
                    ++tooth_count;

                // and display the result in-image at the center of the gear
                {
                    constexpr int    k_FontFace      = cv::FONT_HERSHEY_SIMPLEX;
                    constexpr double k_FontScale     = 1.0;
                    constexpr int    k_FontThickness = 3;
                    const cv::Scalar k_TextColor     = cv::Scalar(255, 255, 255);
                    const cv::Scalar k_TextBgColor   = cv::Scalar(0, 0, 0);
                    constexpr int    k_LineThickness = 2;
                    constexpr int    k_LineType      = cv::LINE_AA;

                    auto tooth_count_half_str = std::to_string(tooth_count);

                    auto text_size = cv::getTextSize(
                        tooth_count_half_str,
                        k_FontFace,
                        k_FontScale,
                        k_FontThickness,
                        nullptr
                    );

                    // simple shadow
                    cv::putText(
                        g_webcam_image,
                        tooth_count_half_str,
                        centroid_i - cv::Point2i(text_size.width / 2, text_size.height / 2) + cv::Point2i(2, 2),
                        k_FontFace,
                        k_FontScale,
                        k_TextBgColor,
                        k_LineThickness,
                        k_LineType,
                        false       // when drawing in an image with bottom left origin, this should be true
                    );

                    cv::putText(
                        g_webcam_image,
                        tooth_count_half_str,
                        centroid_i - cv::Point2i(text_size.width / 2, text_size.height / 2),
                        k_FontFace,
                        k_FontScale,
                        k_TextColor,
                        k_LineThickness,
                        k_LineType,
                        false       // when drawing in an image with bottom left origin, this should be true
                    );
                }
            }

            // ----- rendering -----
            switch (show) {
            case 0: cv::imshow(main_window, g_webcam_image); break;
            case 1: cv::imshow(main_window, g_foreground);   break;
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
                    if (++show > 1)
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