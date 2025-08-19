#include "contours.h"

#include "centroid.h"
#include "count_teeth.h"
#include "anomalies.h"

#include "gui/visualization.h"

#include "types/tooth_measurement.h"

namespace cc::processing {
    std::optional<
        std::tuple<
            int,
            std::vector<ToothMeasurement>,
            cv::Point2i
        >
    > process_contours(
        const std::vector<std::vector<cv::Point>>& all_contours,
        const std::vector<cv::Vec4i>&              hierarchy,
              cv::Mat&                             output_image
    ) {
        int    idx                   = 0;
        int    largest_component_idx = 0;
        double max_area              = 0;

        // loop through the top-level contours (the iteration sequence is described in the hierarchy vector
        // and terminates with a negative value) and find the biggest one.
        for (; idx >= 0; idx = hierarchy[idx][0]) {
            const auto& cont = all_contours[idx];

            double area = std::fabs(cv::contourArea(cv::Mat(cont)));

            if (area > max_area) {
                max_area = area;
                largest_component_idx = idx;
            }
        }

        cv::drawContours(
            output_image,
            all_contours,
            largest_component_idx,
            cv::Scalar(0, 0, 255),
            1, // thickness, or cv::FILLED to fill the entire thing
            cv::LINE_8,
            hierarchy
        );

        // find centroid of the contour
        auto [centroid_d, centroid_f, centroid_i] = find_centroid(
            all_contours,
            largest_component_idx
        );

        const auto& largest_contour = all_contours[largest_component_idx];

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

        std::vector<ToothMeasurement> teeth;

        auto first_tooth = find_tooth_start(tooth_mask);
        if (!first_tooth)
            return std::nullopt;

        tooth_count = count_teeth(
            *first_tooth,
            tooth_mask,
            teeth,
            largest_contour,
            distances,
            centroid_f
        );

        return std::make_tuple(
            tooth_count,
            teeth,
            centroid_i
        );
    }
}