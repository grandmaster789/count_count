#include "visualization.h"
#include "types/tooth_anomaly.h"
#include <numbers>

namespace cc {
    void draw_gear_arrow(
        cv::Mat&     output_image,
        const cv::Point2d& gear_center,
        double             gear_radius,
        double             angle,
        const cv::Scalar&  color,
        int                thickness
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
}