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

        cv::arrowedLine(
            output_image,
            from,
            to,
            color,
            thickness
        );
    }

    void display_results(
        cv::Point2i                              centroid_i,
        const std::vector<cc::ToothMeasurement>& teeth,
        const std::vector<uint8_t>&              tooth_anomaly_mask,
        cv::Mat&                                 output_image
    ) {
        constexpr int    k_FontFace      = cv::FONT_HERSHEY_SIMPLEX;
        constexpr double k_FontScale     = 1.0;
        constexpr int    k_FontThickness = 3;
        const cv::Scalar k_GoodTextColor = cv::Scalar(0,   255, 0); // green  (BGR)
        const cv::Scalar k_BadTextColor  = cv::Scalar(0,   0, 255); // red    (BGR)
        const cv::Scalar k_GapColor      = cv::Scalar(255, 0, 255); // purple (BGR)
        const cv::Scalar k_TextBgColor   = cv::Scalar(0, 0, 0);
        constexpr int    k_LineThickness = 2;
        constexpr int    k_LineType      = cv::LINE_AA;

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

        // see if we have any anomalies
        size_t num_arc_anomalies = 0;
        size_t num_gap_anomalies = 0;

        for (auto anomaly : tooth_anomaly_mask) {
            if (anomaly & cc::ToothAnomaly::gap)
                ++num_gap_anomalies;
            if (anomaly & cc::ToothAnomaly::arc)
                ++num_arc_anomalies;
        }

        // visualize anomalies using arrows from the center towards the missing tooth
        // and lines along the gap areas. Gap areas are rare, so if we see them we don't draw the arc anomalies
        {
            for (size_t i = 0; i < teeth.size(); ++i) {
                const auto& measurement       = teeth[i];
                const auto& anomaly_detection = tooth_anomaly_mask[i];

                if (num_gap_anomalies == 0 &&
                    anomaly_detection & cc::ToothAnomaly::arc
                ) {
                    draw_gear_arrow(
                        output_image,
                        centroid_i,
                        measurement.m_MinDistance,
                        (measurement.m_EndingAngle + measurement.m_StartingAngle) / 2.0,
                        cv::Scalar(255, 255, 127)
                    );
                }

                if (anomaly_detection & cc::ToothAnomaly::gap) {
                    auto gear_point = [centroid_i, measurement](double angle) {
                        return centroid_i + cv::Point2i(
                            static_cast<int>(measurement.m_MinDistance * std::cos(angle)),
                            static_cast<int>(measurement.m_MinDistance * std::sin(angle))
                        );
                    };

                    auto start_angle = measurement.m_StartingAngle;
                    auto mid_angle   = (measurement.m_EndingAngle + measurement.m_StartingAngle) / 2.0;
                    auto end_angle   = measurement.m_EndingAngle;

                    cv::line(
                        output_image,
                        gear_point(start_angle),
                        gear_point(mid_angle),
                        k_GapColor,
                        3 // line thickness
                    );

                    cv::line(
                        output_image,
                        gear_point(mid_angle),
                        gear_point(end_angle),
                        k_GapColor,
                        3 // line thickness
                    );
                }
            }
        }

        // when we have no anomalies, the number of teeth is equal to the number of measurements
        // - use green text to indicate that everything is ok
        auto tooth_count_str = std::to_string(teeth.size());

        auto message = tooth_count_str + '/' + tooth_count_str;

        if (num_arc_anomalies > 0)
            message = tooth_count_str + '/' + std::to_string(teeth.size() + num_arc_anomalies);

        if (num_gap_anomalies > 0)
            message = "?";

        auto text_size = cv::getTextSize(
            message,
            k_FontFace,
            k_FontScale,
            k_FontThickness,
            nullptr
        );

        // simple shadow
        cv::putText(
            output_image,
            message,
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
            message,
            centroid_i - cv::Point2i(text_size.width / 2, text_size.height / 2),
            k_FontFace,
            k_FontScale,
            (num_arc_anomalies == 0 ? k_GoodTextColor : k_BadTextColor),
            k_LineThickness,
            k_LineType,
            false       // when drawing in an image with bottom left origin, this should be true
        );
    }
}