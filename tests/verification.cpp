#include "benchmark/Timing.h"
#include "benchmark/Acceptance.h"
#include "data/CSVReader.h"
#include <cmath>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

double plaintext_variance(const std::vector<double>& values);

int main() {
    SumMeanRun relationship;
    relationship.passed = true;
    for (const char* status : {"UNSUPPORTED", "UNDEFINED"}) {
        relationship.status = status;
        if (benchmark_run_acceptable(relationship, true)) return 1;
    }
    relationship.status = "PASS";
    if (!benchmark_run_acceptable(relationship, true)) return 1;
    relationship.passed = false;
    if (benchmark_run_acceptable(relationship, true)) return 1;

    if (plaintext_variance({1e12, 1e12, 1e12}) != 0.0 ||
        std::abs(plaintext_variance({1e12 - 0.25, 1e12, 1e12 + 0.25, 1e12}) - 0.03125) > 1e-12 ||
        std::abs(plaintext_variance({2, 4, 4, 4, 5, 5, 7, 9}) - 4.0) > 1e-12) {
        std::cerr << "Variance reference failed\n"; return 1;
    }
    const auto stats = statistics({1.0, 2.0, 3.0, 4.0});
    if (stats.median != 2.5 || stats.mean != 2.5 || std::abs(stats.stddev - std::sqrt(1.25)) > 1e-12) {
        std::cerr << "Statistics calculation failed\n"; return 1;
    }
    const unsigned counts[] = {1000, 10000, 100000};
    // Independently obtained by summing the one-decimal CSV values as integer tenths.
    const long expected_tenths[] = {723042, 7256039, 72636189};
    for (int i = 0; i < 3; ++i) {
        const auto values = read_numeric_column(std::string(MEDCIPHER_DATA_DIR) + "/patients_" + std::to_string(counts[i]/1000) + "K.csv", "heart_rate");
        const double sum = std::accumulate(values.begin(), values.end(), 0.0);
        if (values.size() != counts[i] || std::abs(sum - expected_tenths[i] / 10.0) > 1e-6) {
            std::cerr << "Dataset reference failed\n"; return 1;
        }
    }
    return 0;
}
