#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <numbers>
#include <random>
#include <numeric>

#include "processing/anomalies.h"
#include "types/tooth_measurement.h"
#include "types/tooth_anomaly.h"

using std::numbers::pi;

using namespace cc::processing;
using namespace cc;

ToothMeasurement make_tooth(double start_angle, double end_angle) {
    return ToothMeasurement {
        .m_MinDistance          = 1.0,
        .m_MaxDistance          = 1.1,
        .m_StartingAngle        = start_angle,
        .m_EndingAngle          = end_angle,
        .m_LowHighTransitionIdx = 0,
        .m_HighLowTransitionIdx = 0
    };
}

std::vector<ToothMeasurement> make_uniform_gear(int num_teeth) {
    std::vector<ToothMeasurement> teeth;
    teeth.reserve(num_teeth);

    double tooth_arc = pi / num_teeth;

    for (int i = 0; i < num_teeth; ++i)
        teeth.push_back(
            make_tooth(
                  2 * i      * tooth_arc,
                 (2 * i + 1) * tooth_arc
            )
        );

    return teeth;
}

std::vector<ToothMeasurement> make_normal_gear(
    int    num_teeth,
    double sigma
) {
    auto teeth = make_uniform_gear(num_teeth);

    if (teeth.empty())
        return teeth;

    double mean_arc =
        teeth[0].m_EndingAngle -
        teeth[0].m_StartingAngle;

    std::default_random_engine generator(123); // fixed seed for reproducibility
    std::normal_distribution<> distribution(mean_arc, sigma);

    // redistribute the teeth to make them more normal
    for (auto& measurement : teeth) {
        double arc    = distribution(generator);
        double center = (measurement.m_StartingAngle + measurement.m_EndingAngle) / 2.0;

        measurement.m_StartingAngle = center - arc / 2.0;
        measurement.m_EndingAngle   = center + arc / 2.0;
    }

    return teeth;
}

TEST_CASE("find_anomalies - empty input", "[anomalies]") {
    std::vector<ToothMeasurement> empty_teeth;
    auto result = find_anomalies(empty_teeth);
    REQUIRE(result.empty());
}

TEST_CASE("find_anomalies - single tooth", "[anomalies]") {
    std::vector<ToothMeasurement> single_tooth = {
        {0.0, pi / 6}  // 30 degrees
    };

    auto result = find_anomalies(single_tooth);
    REQUIRE(result.size() == 1);

    // With a single tooth, standard deviation is 0, so no anomalies should be detected
    REQUIRE(result[0] == 0);
}

TEST_CASE("find_anomalies - uniform teeth no anomalies", "[anomalies]") {
    // Create 12 uniform teeth, each 15 degrees with 15-degree gaps
    auto uniform_teeth = make_uniform_gear(12);

    auto result = find_anomalies(uniform_teeth);
    REQUIRE(result.size() == 12);

    // All teeth should be normal (no anomalies)
    for (size_t i = 0; i < result.size(); ++i)
        REQUIRE(result[i] == 0);
}

TEST_CASE("find_anomalies - gap anomaly", "[anomalies]") {
    // Create teeth with one large gap
    auto teeth_with_gap_anomaly = make_uniform_gear(12);
    teeth_with_gap_anomaly.erase(std::begin(teeth_with_gap_anomaly) + 5); // remove one of the teeth

    auto result = find_anomalies(teeth_with_gap_anomaly);
    REQUIRE(result.size() == 11);

    // The tooth before the large gap should be flagged for gap anomaly
    bool gap_anomaly_found = false;

    for (size_t i = 0; i < result.size(); ++i) {
        if (result[i] & cc::gap) {
            gap_anomaly_found = true;
            break;
        }
    }

    REQUIRE(gap_anomaly_found);
}

TEST_CASE("find_anomalies - arc anomaly", "[anomalies]") {
    // Create teeth with one unusually large tooth arc
    auto teeth_with_arc_anomaly = make_uniform_gear(12);

    auto regular_tooth_arc =
        teeth_with_arc_anomaly[0].m_EndingAngle -
        teeth_with_arc_anomaly[0].m_StartingAngle;

    teeth_with_arc_anomaly[5].m_StartingAngle -= 0.5 * regular_tooth_arc;
    teeth_with_arc_anomaly[5].m_EndingAngle   += 0.5 * regular_tooth_arc;

    auto result = find_anomalies(teeth_with_arc_anomaly);
    REQUIRE(result.size() == 12);

    // The large tooth should be flagged for arc anomaly
    REQUIRE((result[5] & cc::arc) != 0);
}

TEST_CASE("find_anomalies - statistical threshold", "[anomalies]") {
    // Create a scenario where we can verify the 3-sigma threshold
    double sigma = 0.1;
    auto teeth_for_threshold_test = make_normal_gear(12, sigma);

    double mean_tooth_arc = std::accumulate(
        std::begin(teeth_for_threshold_test),
        std::end(teeth_for_threshold_test),
        0.0,
        [](double acc, const auto& measurement) {
            return acc + (measurement.m_EndingAngle - measurement.m_StartingAngle);
        }
    ) / static_cast<double>(teeth_for_threshold_test.size());

    // create a disturbance that is moderately difference from the mean
    teeth_for_threshold_test[5].m_StartingAngle -= sigma * mean_tooth_arc;
    teeth_for_threshold_test[5].m_EndingAngle   += sigma * mean_tooth_arc;

    auto result = find_anomalies(teeth_for_threshold_test);
    REQUIRE(result.size() == 12);

    // The moderately different tooth should not be flagged due to the 3-sigma threshold
    REQUIRE((result[5] & cc::arc) == 0);
}