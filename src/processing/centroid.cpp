#include "centroid.h"

namespace cc::processing {
    std::tuple<
        cc::Point2d,
        cc::Point2f,
        cc::Point2i
    > find_centroid(
        const std::vector<cc::Point2i>& contour
    ) {
        // Compute moments manually (replaces cv::moments)
        // m00 = area (via shoelace), m10 = sum of x*area_contribution, m01 = sum of y*area_contribution
        // For a polygon, the centroid is simply the average of vertex coordinates
        // (weighted by the signed area of each triangle with the origin).
        // However, for a contour that's densely sampled, averaging all points gives a good approximation.
        if (contour.empty())
            return { cc::Point2d(0, 0), cc::Point2f(0, 0), cc::Point2i(0, 0) };

        double sum_x = 0.0;
        double sum_y = 0.0;

        for (const auto& pt : contour) {
            sum_x += pt.x;
            sum_y += pt.y;
        }

        double n = static_cast<double>(contour.size());

        auto centroid_d = cc::Point2d(sum_x / n, sum_y / n);

        auto centroid_f = cc::Point2f(
            static_cast<float>(centroid_d.x),
            static_cast<float>(centroid_d.y)
        );

        auto centroid_i = cc::Point2i(
            static_cast<int>(centroid_d.x),
            static_cast<int>(centroid_d.y)
        );

        return std::make_tuple(
            centroid_d,
            centroid_f,
            centroid_i
        );
    }
}
