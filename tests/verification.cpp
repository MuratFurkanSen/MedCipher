#include "benchmark/Timing.h"
#include "data/CSVReader.h"
#include <cmath>
#include <iostream>
#include <numeric>
#include <string>
int main() {
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
