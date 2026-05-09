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
