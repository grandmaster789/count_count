#include "visualization.h"
#include "drawing.h"
#include "types/tooth_anomaly.h"
#include <cmath>
#include <format>
#include <numbers>

namespace {
    // Compute the angular midpoint between two angles, handling the 0/2pi wrap
    double angle_midpoint(double a, double b) {
        double diff = b - a;
        double wrapped = std::fmod(diff, 2.0 * std::numbers::pi);
        if (wrapped < 0.0) wrapped += 2.0 * std::numbers::pi;
        return a + wrapped / 2.0;
    }
}

namespace cc {
    void draw_gear_arrow(
        cc::Image&         output_image,
        const cc::Point2d& gear_center,
        double             gear_radius,
        double             angle,
        const cc::Color3&  color,
        int                thickness
    ) {
        // determine intersection from the center with the circle at radius
        cc::Point2i to(
            static_cast<int>(gear_center.x + 0.95 * gear_radius * std::cos(angle)),
            static_cast<int>(gear_center.y + 0.95 * gear_radius * std::sin(angle))
        );

        cc::Point2i from(
            static_cast<int>(gear_center.x + 0.5 * gear_radius * std::cos(angle)),
            static_cast<int>(gear_center.y + 0.5 * gear_radius * std::sin(angle))
        );

        drawing::draw_arrowed_line(
            output_image,
            from,
            to,
            static_cast<uint8_t>(color.b),
            static_cast<uint8_t>(color.g),
            static_cast<uint8_t>(color.r),
            thickness
        );
    }

    void display_results(
        cc::Point2i                              centroid_i,
        const std::vector<cc::ToothMeasurement>& teeth,
        const std::vector<uint8_t>&              tooth_anomaly_mask,
        cc::Image&                               output_image,
        int                                      speculative_count,
        int                                      direct_count
    ) {
        // Scale font relative to image height so text is readable at any resolution
        double k_FontScale     = std::max(2.0, output_image.rows() / 100.0);
        int    k_FontThickness = std::max(2, static_cast<int>(k_FontScale));

        const uint8_t k_GoodB = 0,   k_GoodG = 255, k_GoodR = 0;   // green
        const uint8_t k_BadB  = 0,   k_BadG  = 0,   k_BadR  = 255; // red
        const uint8_t k_GapB  = 255, k_GapG  = 0,   k_GapR  = 255; // purple
        const uint8_t k_BgB   = 0,   k_BgG   = 0,   k_BgR   = 0;  // black

        // draw the center
        drawing::draw_circle(
            output_image,
            centroid_i,
            8,
            255, 255, 255,
            true
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

        // visualize anomalies
        for (size_t i = 0; i < teeth.size(); ++i) {
            const auto& measurement       = teeth[i];
            const auto& anomaly_detection = tooth_anomaly_mask[i];

            if (anomaly_detection & cc::ToothAnomaly::arc) {
                draw_gear_arrow(
                    output_image,
                    cc::Point2d(centroid_i.x, centroid_i.y),
                    measurement.m_MinDistance,
                    angle_midpoint(measurement.m_StartingAngle, measurement.m_EndingAngle),
                    cc::Color3(255, 255, 127)
                );
            }

            if (anomaly_detection & cc::ToothAnomaly::gap) {
                auto gear_point = [centroid_i, measurement](double angle) {
                    return cc::Point2i(
                        centroid_i.x + static_cast<int>(measurement.m_MinDistance * std::cos(angle)),
                        centroid_i.y + static_cast<int>(measurement.m_MinDistance * std::sin(angle))
                    );
                };

                auto start_angle = measurement.m_StartingAngle;
                auto mid_angle   = angle_midpoint(measurement.m_StartingAngle, measurement.m_EndingAngle);
                auto end_angle   = measurement.m_EndingAngle;

                drawing::draw_line(
                    output_image,
                    gear_point(start_angle),
                    gear_point(mid_angle),
                    k_GapB, k_GapG, k_GapR,
                    3
                );

                drawing::draw_line(
                    output_image,
                    gear_point(mid_angle),
                    gear_point(end_angle),
                    k_GapB, k_GapG, k_GapR,
                    3
                );
            }
        }

        // build the message
        // gap anomaly = missing tooth(s), arc anomaly = damaged tooth(s)
        bool has_anomaly = (num_gap_anomalies > 0 || num_arc_anomalies > 0);
        size_t expected_count = teeth.size() + num_gap_anomalies + num_arc_anomalies;

        // Prefer the temporally-smoothed direct count when available (no anomalies).
        int effective_direct = (direct_count >= 0 && !has_anomaly)
            ? direct_count
            : static_cast<int>(teeth.size());

        // Use the speculative count when it differs meaningfully from the direct count
        // (more than 10% off). The direct count understates when adjacent teeth merge.
        bool use_speculative = speculative_count > 0 &&
            std::abs(speculative_count - effective_direct) > effective_direct / 10;

        std::string message;
        if (has_anomaly) {
            message = std::format("{}/{}", teeth.size(), expected_count);
            if (use_speculative)
                message += std::format(" (~{})", speculative_count);
        } else if (use_speculative) {
            message = std::format("{} (~{})", effective_direct, speculative_count);
        } else {
            message = std::format("{}/{}", effective_direct, effective_direct);
        }

        auto text_size = drawing::measure_text(message, k_FontScale);
        int shadow_offset = std::max(2, static_cast<int>(k_FontScale / 2));
        int padding = shadow_offset * 2;

        cc::Point2i text_pos(
            centroid_i.x - text_size.width / 2,
            centroid_i.y - text_size.height / 2
        );

        // draw a background rectangle for readability
        for (int y = text_pos.y - padding; y < text_pos.y + text_size.height + padding + shadow_offset; ++y)
            for (int x = text_pos.x - padding; x < text_pos.x + text_size.width + padding + shadow_offset; ++x)
                drawing::set_pixel(output_image, x, y, k_BgB, k_BgG, k_BgR);

        // shadow
        drawing::draw_text(
            output_image,
            message,
            cc::Point2i(text_pos.x + shadow_offset, text_pos.y + shadow_offset),
            40, 40, 40,
            k_FontScale,
            k_FontThickness
        );

        uint8_t tb = has_anomaly ? k_BadB : k_GoodB;
        uint8_t tg = has_anomaly ? k_BadG : k_GoodG;
        uint8_t tr = has_anomaly ? k_BadR : k_GoodR;

        drawing::draw_text(
            output_image,
            message,
            text_pos,
            tb, tg, tr,
            k_FontScale,
            k_FontThickness
        );
    }
}
