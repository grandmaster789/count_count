#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <cmath>

namespace {
    // Circular moving average
    std::vector<double> smooth(const std::vector<double>& signal, int window_size) {
        if (signal.empty() || window_size <= 1)
            return signal;

        int n = static_cast<int>(signal.size());
        int half = window_size / 2;
        std::vector<double> result(n);

        for (int i = 0; i < n; ++i) {
            double sum = 0;
            for (int j = -half; j <= half; ++j) {
                int idx = ((i + j) % n + n) % n; // circular wrap
                sum += signal[idx];
            }
            result[i] = sum / window_size;
        }

        return result;
    }
}

TEST_CASE("Smooth - constant signal unchanged", "[smooth]") {
    std::vector<double> signal(20, 5.0);
    auto result = smooth(signal, 5);
    for (size_t i = 0; i < result.size(); ++i)
        REQUIRE(result[i] == Catch::Approx(5.0));
}

TEST_CASE("Smooth - step function produces ramp", "[smooth]") {
    std::vector<double> signal(20, 0.0);
    for (int i = 10; i < 20; ++i) signal[i] = 10.0;

    auto result = smooth(signal, 5);

    // At the transition, values should gradually increase
    REQUIRE(result[8] < result[10]);
    REQUIRE(result[10] < result[12]);
}

TEST_CASE("Smooth - impulse spike reduced", "[smooth]") {
    std::vector<double> signal(20, 0.0);
    signal[10] = 100.0; // single spike

    auto result = smooth(signal, 5);

    // The spike should be reduced to approximately 100/5 = 20
    REQUIRE(result[10] == Catch::Approx(100.0 / 5.0).margin(0.1));
}

TEST_CASE("Smooth - circular wrapping", "[smooth]") {
    std::vector<double> signal(10, 0.0);
    signal[0] = 10.0;
    signal[9] = 10.0;

    auto result = smooth(signal, 3);

    // First and last elements should see each other due to circular wrapping
    REQUIRE(result[0] > 6.0); // sees signal[9], signal[0], signal[1]
    REQUIRE(result[9] > 6.0); // sees signal[8], signal[9], signal[0]
}

TEST_CASE("Smooth - window size 1 returns original", "[smooth]") {
    std::vector<double> signal = { 1, 2, 3, 4, 5 };
    auto result = smooth(signal, 1);
    for (size_t i = 0; i < signal.size(); ++i)
        REQUIRE(result[i] == signal[i]);
}

TEST_CASE("Smooth - synthetic tooth signal preserves count", "[smooth]") {
    // Create a bimodal signal (tooth pattern) with noise spikes
    std::vector<double> signal;
    for (int i = 0; i < 120; ++i) {
        double base = (i % 10 < 5) ? 50.0 : 30.0; // alternating teeth/gaps
        signal.push_back(base);
    }
    // Add noise spike
    signal[25] = 80.0;

    auto smoothed = smooth(signal, 5);

    // Count rising-edge crossings at threshold
    double threshold = 40.0;
    int original_count = 0;
    int smoothed_count = 0;

    for (size_t i = 0; i < signal.size(); ++i) {
        size_t next = (i + 1) % signal.size();
        if (signal[i] < threshold && signal[next] >= threshold) ++original_count;
        if (smoothed[i] < threshold && smoothed[next] >= threshold) ++smoothed_count;
    }

    // Both should detect 12 teeth
    REQUIRE(smoothed_count == 12);
}
