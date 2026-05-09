#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "math/statistics.h"

using namespace cc::math;

TEST_CASE("calculate_variance - single element returns 0", "[statistics]") {
    std::vector<double> v = { 5.0 };
    REQUIRE(calculate_variance(v) == 0.0);
}

TEST_CASE("calculate_variance - Bessel corrected (N-1)", "[statistics]") {
    // For values {2, 4, 4, 4, 5, 5, 7, 9}
    // Mean = 5.0
    // Sum of squared deviations = (2-5)^2 + (4-5)^2*3 + (5-5)^2*2 + (7-5)^2 + (9-5)^2
    //                           = 9 + 3 + 0 + 4 + 16 = 32
    // Population variance = 32/8 = 4.0
    // Sample variance (Bessel) = 32/7 = 4.571428...
    std::vector<double> v = { 2, 4, 4, 4, 5, 5, 7, 9 };
    double var = calculate_variance(v);
    REQUIRE(var == Catch::Approx(32.0 / 7.0));
}

TEST_CASE("calculate_variance - two identical elements returns 0", "[statistics]") {
    std::vector<double> v = { 3.0, 3.0 };
    REQUIRE(calculate_variance(v) == 0.0);
}

TEST_CASE("calculate_stddev - matches expected", "[statistics]") {
    std::vector<double> v = { 2, 4, 4, 4, 5, 5, 7, 9 };
    double stddev = calculate_standard_deviation(v);
    REQUIRE(stddev == Catch::Approx(std::sqrt(32.0 / 7.0)));
}

TEST_CASE("calculate_mean - correct average", "[statistics]") {
    std::vector<double> v = { 1, 2, 3, 4, 5 };
    REQUIRE(calculate_mean(v) == Catch::Approx(3.0));
}

TEST_CASE("calculate_stddev - single element returns 0", "[statistics]") {
    std::vector<double> v = { 42.0 };
    REQUIRE(calculate_standard_deviation(v) == 0.0);
}
