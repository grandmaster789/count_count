#include "anomalies.h"

#include "math/statistics.h"
#include "math/angles.h"

#include "types/tooth_anomaly.h"

namespace cc::processing {
    std::vector<uint8_t> find_anomalies(
        const std::vector<cc::ToothMeasurement>& teeth
    ) {
        using cc::math::arc_length;
        using cc::math::calculate_mean;
        using cc::math::calculate_standard_deviation;

        auto tooth_anomaly_mask = cc::create_anomaly_mask(teeth.size());

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
                arc_length(
                    current.m_StartingAngle,
                    current.m_EndingAngle
                )
            );

            tooth_arc_gaps_to_next.push_back(
                arc_length(
                    current.m_EndingAngle,
                    next.m_StartingAngle
                )
            );
        }

        const double tooth_arc_mean   = calculate_mean(tooth_arcs);
        const double tooth_arc_stddev = calculate_standard_deviation(tooth_arcs);

        const double tooth_gap_mean   = calculate_mean(tooth_arc_gaps_to_next);
        const double tooth_gap_stddev = calculate_standard_deviation(tooth_arc_gaps_to_next);

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
}