#include <iostream>
#include "platform/platform.h"
#include "platform/build_date.h"
#include "io/jpg.h"
#include "io/data_location.h"
#include "types/tooth_measurement.h"
#include "types/tooth_anomaly.h"
#include "types/rgb.h"
#include "types/color_range.h"
#include "types/resolution.h"
#include "types/settings.h"
#include "math/statistics.h"

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio/registry.hpp>

#include <chrono>
#include <format>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <algorithm>
#include <numeric>

#include <execution>

#if CVC_PLATFORM != CVC_PLATFORM_WINDOWS
    #error "Currently only Windows is supported"
#endif

cv::Mat g_foreground;      // BGR
cv::Mat g_foreground_mask; // grayscale
cv::Mat g_source_image;    // BGR

cc::Settings g_Settings;

static constexpr const size_t k_MinimumToothCount = 8;

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

// the openCV api doesn't provide a method to actually query supported resolutions
// so we'll just check a couple common ones
bool check_resolution(int camera_id, const cc::Resolution& res) {
    cv::VideoCapture cap(camera_id);
    cap.set(cv::CAP_PROP_FRAME_WIDTH, res.m_Width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, res.m_Height);

    int actual_width  = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int actual_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));

    return
        (actual_width == res.m_Width) &&
        (actual_height == res.m_Height);
}

void save_settings() {
    std::ofstream cfg("count_count.cfg");
    cfg << g_Settings;
}

void load_settings(int camera_id = cc::FIRST_CAMERA) {
    // if a resolution was selected before, use that
    if (std::filesystem::exists("count_count.cfg")) {
        std::ifstream cfg("count_count.cfg");

        cfg >> g_Settings;

        return;
    }

    // no settings available, provide some defaults
    std::vector<cc::Resolution> resolutions = {
        cc::Resolution { 3840, 2160 }, // 4k
        cc::Resolution { 1920, 1080},  // 1080p
        cc::Resolution { 1280, 720 },  // 720p
        cc::Resolution { 640,  480 }   // 480p
    };

    size_t selected_resolution = 0;

    for (; selected_resolution < resolutions.size(); ++selected_resolution) {
        if (check_resolution(camera_id, resolutions[selected_resolution]))
            break;
    }

    g_Settings.m_SourceResolution = resolutions[selected_resolution];
}

struct SensitivityBarHandler {
    void on_track_sensitivity_changed(int new_value) {
        g_Settings.m_ForegroundColorTolerance = new_value;
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

        g_Settings.m_ForegroundColor[0] = g_source_image.at<cv::Vec3b>(y, x)[0];
        g_Settings.m_ForegroundColor[1] = g_source_image.at<cv::Vec3b>(y, x)[1];
        g_Settings.m_ForegroundColor[2] = g_source_image.at<cv::Vec3b>(y, x)[2];

        std::cout << "RGB is #"
            << std::hex
            << std::setw(2)
            << std::setfill('0')
            << static_cast<int>(g_Settings.m_ForegroundColor[0])
            << static_cast<int>(g_Settings.m_ForegroundColor[1])
            << static_cast<int>(g_Settings.m_ForegroundColor[2])
            << '\n';
        break;

    default:
        break;
    };
}

void initialize_image_buffers() {
    if (g_foreground.empty())
        g_foreground.create(g_source_image.size(), CV_8UC3);
    if (g_foreground_mask.empty())
        g_foreground_mask.create(g_source_image.size(), CV_8UC1);
}

void determine_foreground(
        const cc::RGB& selected_color,
        int            tolerance_range,
        const cv::Mat& source_image,
              cv::Mat& foreground_mask,
              cv::Mat& foreground
) {
    foreground_mask = cv::Mat::zeros(source_image.size(), CV_8UC1);

    auto [min_rgb, max_rgb] = cc::determine_color_range(selected_color, tolerance_range);
    cv::inRange(
        source_image,
        min_rgb,
        max_rgb,
        foreground_mask
    );

    // apply slight blur to get rid of noise and small details
    cv::medianBlur(foreground_mask, foreground_mask, 9);

    foreground = cv::Mat::zeros(source_image.size(), CV_8UC3);
    cv::copyTo(
        source_image,
        foreground,
        foreground_mask
    );
}

std::tuple<cv::Point2d, cv::Point2f, cv::Point2i> find_centroid(
    const std::vector<std::vector<cv::Point>>& contours,
    int                                        largest_component_idx
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

    auto centroid_f = cv::Point2f(
        static_cast<float>(centroid_d.x),
        static_cast<float>(centroid_d.y)
    );

    auto centroid_i = cv::Point2i(
        static_cast<int>(centroid_d.x),
        static_cast<int>(centroid_d.y)
    );

    return std::make_tuple(
        centroid_d,
        centroid_f,
        centroid_i
    );
}

double arc_length(
    double starting_radians,
    double ending_radians
) {
    while (starting_radians > ending_radians)
        ending_radians += 2.0 * std::numbers::pi;

    return ending_radians - starting_radians;
}

std::vector<uint8_t>& find_anomalies(
    const std::vector<cc::ToothMeasurement>& teeth,
          std::vector<uint8_t>&              tooth_anomaly_mask
) {
    // apply some statistics:
    // - find the mean distance to the next tooth
    // - establish variance for both tooth arcs and gaps (unbiased sample variance)
    // - whenever the measurement exceeds the variance plus tolerance, present a signal
    std::vector<double> tooth_arc_gaps_to_next;
    std::vector<double> tooth_arcs;

    for (size_t i = 0; i < teeth.size(); ++i) {
        const auto &current = teeth[i];
        const auto &next = teeth[(i + 1) % teeth.size()];;

        tooth_arcs.push_back(
            arc_length(current.m_StartingAngle, current.m_EndingAngle)
        );

        tooth_arc_gaps_to_next.push_back(
            arc_length(current.m_EndingAngle, next.m_StartingAngle)
        );
    }

    const double tooth_arc_mean   = cc::math::calculate_mean(tooth_arcs);
    const double tooth_arc_stddev = cc::math::calculate_standard_deviation(tooth_arcs);

    const double tooth_gap_mean   = cc::math::calculate_mean(tooth_arc_gaps_to_next);
    const double tooth_gap_stddev = cc::math::calculate_standard_deviation(tooth_arc_gaps_to_next);

    // strong anomalies several times the distance of the stddev from the mean,
    // weak anomalies between stddev and strong anomaly threshold
    // regular observations are less than the stddev
    //
    // let's focus on finding strong anomalies first
    const double tooth_gap_anomaly_strong = 3.0 * tooth_gap_stddev;
    const double tooth_arc_anomaly_strong = 3.0 * tooth_arc_stddev;

    for (size_t i = 0; i < teeth.size(); ++i) {
        double tooth_gap = tooth_arc_gaps_to_next[i];
        double tooth_arc = tooth_arcs[i];

        double tooth_arc_anomaly = abs(tooth_arc - tooth_arc_mean);
        double tooth_gap_anomaly = abs(tooth_gap - tooth_gap_mean);

        if (tooth_gap_anomaly > tooth_gap_anomaly_strong)
            tooth_anomaly_mask[i] |= cc::gap;

        if (tooth_arc_anomaly > tooth_arc_anomaly_strong)
            tooth_anomaly_mask[i] |= cc::arc;
    }

    return tooth_anomaly_mask;
}

void draw_gear_arrow(
          cv::Mat&     output_image,
    const cv::Point2d& gear_center,
    double             gear_radius,
    double             angle,
    const cv::Scalar&  color     = cv::Scalar(255, 255, 127),
    int                thickness = 3
) {
    // determine intersection from the center with the circle at radius
    cv::Point2d to(
        gear_center.x + 0.95 * gear_radius * std::cos(angle),
        gear_center.y + 0.95 * gear_radius * std::sin(angle)
    );

    cv::Point2d from(
        gear_center.x + 0.5 * gear_radius * std::cos(angle),
        gear_center.y + 0.5 * gear_radius * std::sin(angle)
    );

    cv::Point2d left(
        gear_center.x + 0.9 * gear_radius * std::cos(angle - std::numbers::pi / 40.0),
        gear_center.y + 0.9 * gear_radius * std::sin(angle - std::numbers::pi / 40.0)
    );

    cv::Point2d right(
        gear_center.x + 0.9 * gear_radius * std::cos(angle + std::numbers::pi / 40.0),
        gear_center.y + 0.9 * gear_radius * std::sin(angle + std::numbers::pi / 40.0)
    );

    cv::line(output_image, from,  to, color, thickness);
    cv::line(output_image, left,  to, color, thickness);
    cv::line(output_image, right, to, color, thickness);
}

void display_results(
    size_t                                   tooth_count,
    cv::Point2i                              centroid_i,
    const std::vector<cc::ToothMeasurement>& teeth,
    const std::vector<uint8_t>&              tooth_anomaly_mask,
    cv::Mat&                                 output_image
) {
    constexpr int    k_FontFace      = cv::FONT_HERSHEY_SIMPLEX;
    constexpr double k_FontScale     = 1.0;
    constexpr int    k_FontThickness = 3;
    const cv::Scalar k_TextColor     = cv::Scalar(255, 255, 255);
    const cv::Scalar k_TextBgColor   = cv::Scalar(0, 0, 0);
    constexpr int    k_LineThickness = 2;
    constexpr int    k_LineType      = cv::LINE_AA;

    auto tooth_count_half_str = std::to_string(tooth_count);

    // draw the center
    cv::circle(
        output_image,               // dst
        centroid_i,                 // center
        8,                          // radius
        cv::Scalar(255, 255, 255),  // color
        -1,                         // thickness (-1 for fill)
        8,                          // line type
        0                           // shift
    );

    // visualize anomalies using a simple line from the center towards the tooth
    {
        for (size_t i = 0; i < teeth.size(); ++i) {
            const auto& measurement       = teeth[i];
            const auto& anomaly_detection = tooth_anomaly_mask[i];

            if (anomaly_detection & cc::ToothAnomaly::arc) {
                draw_gear_arrow(
                    output_image,
                    centroid_i,
                    measurement.m_MinDistance,
                    (measurement.m_EndingAngle + measurement.m_StartingAngle) / 2.0,
                    cv::Scalar(255, 255, 127)
                );
            }
        }
    }

    auto text_size = cv::getTextSize(
        tooth_count_half_str,
        k_FontFace,
        k_FontScale,
        k_FontThickness,
        nullptr
    );

    // simple shadow
    cv::putText(
        output_image,
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
        output_image,
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

int main(int, char* argv[]) {
    bool use_live_video = false;

    namespace fs = std::filesystem;

    std::cout << "Starting Counting...\n";
    std::cout << "Running on: " << cc::ePlatform::current << '\n';
    std::cout << "Built "       << cc::get_days_since_build() << " days ago\n";

    fs::path exe_path(argv[0]);
    auto data_path = cc::find_data_folder(exe_path);
    std::cout << "Exe path:  " << exe_path.string() << '\n';
    std::cout << "Data path: " << data_path.string() << '\n';

    {
        load_settings();

        std::cout << "Selected resolution: " << g_Settings.m_SourceResolution << '\n';

        cv::Mat          static_image;
        cv::VideoCapture webcam;
        cv::Mat          output_image;

        if (!use_live_video)
            static_image = cc::io::load_jpg((data_path / "test_broken_tooth_002.jpg"));

        SensitivityBarHandler trackbar_handler;

        cv::String main_window = "Webcam Video Stream";
        cv::namedWindow(main_window, cv::WINDOW_KEEPRATIO | cv::WINDOW_AUTOSIZE); // create a resizeable window
        cv::createTrackbar("Sensitivity", main_window, nullptr, 255, track_sensitivity_changed, &trackbar_handler);
        cv::setTrackbarPos("Sensitivity", main_window, g_Settings.m_ForegroundColorTolerance);
        cv::setMouseCallback(main_window, main_window_mouse_event);

        int show = 0;

        // press 'q' or escape to terminate the loop
        bool done = false;
        while (!done) {
            // ----- video input -----
            if (use_live_video) {
                if (!webcam.isOpened()) {
                    webcam = cv::VideoCapture(0);

                    if (!webcam.isOpened()) {
                        std::cerr << "Cannot open webcam\n";
                        return -1;
                    }

                    webcam.set(cv::CAP_PROP_FRAME_WIDTH,  g_Settings.m_SourceResolution.m_Width);
                    webcam.set(cv::CAP_PROP_FRAME_HEIGHT, g_Settings.m_SourceResolution.m_Height);
                }

                webcam >> g_source_image; // live video
            }
            else
                g_source_image = static_image.clone(); // single image

            if (g_source_image.empty()) {
                std::cerr << "Failed to retrieve image from webcam; exiting application\n";
                break;
            }

            // if the other images haven't been initialized yet, do so now
            initialize_image_buffers();
            output_image = g_source_image.clone();

            // ----- video processing -----
            determine_foreground(
                g_Settings.m_ForegroundColor,
                g_Settings.m_ForegroundColorTolerance,
                g_source_image,
                g_foreground_mask,
                g_foreground
            );

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
                    output_image,
                    contours,
                    largest_component_idx,
                    cv::Scalar(0, 0, 255),
                    1, // thickness, or cv::FILLED to fill the entire thing
                    cv::LINE_8,
                    hierarchy
                );

                // find centroid of the contour
                auto [centroid_d, centroid_f, centroid_i] = find_centroid(
                    contours,
                    largest_component_idx
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
                // -- only count the 'rising' edges to establish a count
                // -- also figure out some tooth measurements
                int tooth_count = 0;

                std::vector<cc::ToothMeasurement> teeth;

                // find the first (low) position where the mask changes from low to high
                auto find_tooth_start = [](const auto& mask) -> std::optional<size_t> {
                    if (mask.empty())
                        return std::nullopt;

                    for (size_t i = 0; i < mask.size(); ++i)
                        if (!mask[i] && mask[(i + 1) % mask.size()])
                            return i;

                    // super edge case where there is just one change, and it's right at the end of the list
                    if (!mask.back() && mask.front())
                        return mask.size() - 1;

                    return std::nullopt;
                };

                auto first_tooth = find_tooth_start(tooth_mask);
                if (!first_tooth)
                    continue;

                size_t starting_idx = *first_tooth;

                // from the first position, iterate over the entire set and collect measurements during traversal
                for (size_t i = starting_idx; i < starting_idx + tooth_mask.size(); ++i) {
                    uint8_t current_mask_value = tooth_mask[i % tooth_mask.size()];
                    uint8_t next_mask_value    = tooth_mask[(i + 1) % tooth_mask.size()];

                    // count rising edges as the start of a tooth
                    // the algorithm starts at a position where this is the case
                    if (!current_mask_value && next_mask_value) {
                        ++tooth_count;

                        cc::ToothMeasurement new_measurement;
                        new_measurement.m_StartMaskIdx  = i % (tooth_mask.size() - 1);
                        new_measurement.m_ToothIdx      = tooth_count;
                        new_measurement.m_StartingAngle = std::atan2f(
                            largest_contour[new_measurement.m_StartMaskIdx].y - centroid_f.y,
                            largest_contour[new_measurement.m_StartMaskIdx].x - centroid_f.x
                        );

                        teeth.push_back(new_measurement);
                    }

                    if (current_mask_value && !next_mask_value) {
                        // complete measurements from the last measurement
                        auto& measurement = teeth.back();

                        measurement.m_EndMaskIdx = i % (tooth_mask.size() - 1);
                        measurement.m_EndingAngle = std::atan2f(
                            largest_contour[measurement.m_EndMaskIdx].y - centroid_f.y,
                            largest_contour[measurement.m_EndMaskIdx].x - centroid_f.x
                        );

                        // because of the wrapping structure, this is actually challenging for std::minmax_element...
                        measurement.m_MinDistance = std::numeric_limits<double>::max();
                        measurement.m_MaxDistance = -std::numeric_limits<double>::max();

                        for (size_t j = measurement.m_StartMaskIdx; j <= measurement.m_EndMaskIdx; ++j, j %= tooth_mask.size()) {
                            if (distances[j] < measurement.m_MinDistance)
                                measurement.m_MinDistance = distances[j];

                            if (distances[j] > measurement.m_MaxDistance)
                                measurement.m_MaxDistance = distances[j];
                        }
                    }
                }

                // early exit -- if we have found less than 8 teeth, it's probably not a gear that we found
                if (tooth_count >= k_MinimumToothCount) {
                    auto tooth_anomaly_mask = cc::create_anomaly_mask(teeth.size());

                    tooth_anomaly_mask = find_anomalies(teeth, tooth_anomaly_mask);

                    // and display the result in-image at the center of the gear
                    display_results(
                        tooth_count,
                        centroid_i,
                        teeth,
                        tooth_anomaly_mask,
                        output_image
                    );
                }
            }

            // ----- rendering -----
            switch (show) {
            case 0: cv::imshow(main_window,  output_image); break;
            case 1: cv::imshow(main_window, g_foreground);  break;
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
                    save_image(g_source_image);
                    break;

                case ' ': // space bar
                    //cycle through shown images
                    if (++show > 1)
                        show = 0;
                    break;

                case 13: // enter
                    use_live_video = !use_live_video;

                    if (!use_live_video)
                        static_image = g_source_image.clone(); // store the last live image as the static image

                    break;

                case -1: // timeout
                    break;

            default:
                std::cout << "Pressed: " << key << '\n';
                break;
            }

            // if the window closed, we're done
            if (cv::getWindowProperty(main_window, cv::WND_PROP_VISIBLE) < 1)
                done = true;
        }
    }

    save_settings();

    std::cout << "Completed\n";

    return 0;
}
