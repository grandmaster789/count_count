#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "math/statistics.h"

#include <algorithm>
#include <vector>

namespace {
    // Old midpoint method for comparison
    double midpoint_threshold(const std::vector<double>& distances) {
        auto [min_it, max_it] = std::minmax_element(distances.begin(), distances.end());
        return (*min_it + *max_it) / 2.0;
    }
}

TEST_CASE("Percentile threshold - bimodal distribution", "[threshold]") {
    // Distances near 30 (gaps) and 50 (teeth)
    std::vector<double> distances = {
        29, 30, 31, 30, 29, 31,  // gaps
        49, 50, 51, 50, 49, 51   // teeth
    };
    double threshold = cc::math::percentile_threshold(distances);
    REQUIRE(threshold > 30);
    REQUIRE(threshold < 50);
}

TEST_CASE("Percentile threshold - with extreme outliers", "[threshold]") {
    std::vector<double> distances = {
        5,  // extreme low outlier
        29, 30, 31, 30, 29, 31,
        49, 50, 51, 50, 49, 51,
        80  // extreme high outlier
    };
    double threshold = cc::math::percentile_threshold(distances);
    // Should still be between clusters, not pulled by outliers
    REQUIRE(threshold > 28);
    REQUIRE(threshold < 52);
}

TEST_CASE("Percentile threshold - uniform distribution returns midpoint", "[threshold]") {
    std::vector<double> distances;
    for (int i = 0; i <= 100; ++i)
        distances.push_back(static_cast<double>(i));

    double threshold = cc::math::percentile_threshold(distances);
    REQUIRE(threshold == Catch::Approx(50.0).margin(2.0));
}

TEST_CASE("Percentile threshold - single value", "[threshold]") {
    std::vector<double> distances = { 42.0 };
    REQUIRE(cc::math::percentile_threshold(distances) == 42.0);
}

// ---------------------------------------------------------------------------
// otsu_threshold
// ---------------------------------------------------------------------------

TEST_CASE("otsu_threshold - bimodal distribution lands between clusters", "[threshold]") {
    // 50 points near 30 (gaps, values 29–31) + 50 points near 50 (teeth, values 49–51)
    std::vector<double> distances;
    for (int i = 0; i < 50; ++i) distances.push_back(29.0 + (i % 3));
    for (int i = 0; i < 50; ++i) distances.push_back(49.0 + (i % 3));

    double t = cc::math::otsu_threshold(distances);
    REQUIRE(t > 31.0); // above the top of the lower cluster (31)
    REQUIRE(t < 49.0); // below the bottom of the upper cluster (49)
}

TEST_CASE("otsu_threshold - separates shallow bimodal better than midpoint", "[threshold]") {
    // Simulate shallow-toothed gear: most points are near root (~200, values 200–204),
    // a minority are at tooth tips (~225–228). Otsu should place threshold above the
    // root cluster and below the tip cluster.
    std::vector<double> distances;
    for (int i = 0; i < 100; ++i) distances.push_back(200.0 + (i % 5));  // root cluster: 200–204
    for (int i = 0; i <  40; ++i) distances.push_back(225.0 + (i % 4));  // tip cluster:  225–228

    double t = cc::math::otsu_threshold(distances);
    REQUIRE(t > 204.0); // above the top of the root cluster (204)
    REQUIRE(t < 225.0); // below the bottom of the tip cluster (225)
}

TEST_CASE("otsu_threshold - single value returns that value", "[threshold]") {
    REQUIRE(cc::math::otsu_threshold(std::vector<double>{42.0}) == Catch::Approx(42.0));
}

TEST_CASE("otsu_threshold - uniform distribution returns midpoint", "[threshold]") {
    std::vector<double> distances;
    for (int i = 0; i <= 100; ++i)
        distances.push_back(static_cast<double>(i));

    double t = cc::math::otsu_threshold(distances);
    REQUIRE(t > 30.0);
    REQUIRE(t < 70.0);
}

TEST_CASE("Percentile vs midpoint with outliers", "[threshold]") {
    // Bimodal distribution with injected outliers
    std::vector<double> distances = {
        30, 30, 30, 30, 30,   // cluster 1
        50, 50, 50, 50, 50,   // cluster 2
        5,                     // outlier low
        95                     // outlier high
    };

    double pct = cc::math::percentile_threshold(distances);
    double mid = midpoint_threshold(distances);

    // Midpoint is (5 + 95) / 2 = 50, which is right on cluster 2
    // Percentile should be between the two clusters
    REQUIRE(pct > 28);
    REQUIRE(pct < 52);

    // Midpoint is pulled by outliers
    REQUIRE(mid == Catch::Approx(50.0));
}
