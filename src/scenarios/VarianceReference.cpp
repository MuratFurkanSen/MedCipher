#include <vector>
// Separate translation unit ensures each timed iteration executes without LTO.
double plaintext_variance(const std::vector<double>& values) {
    long double sum = 0.0L;
    for (double value : values) sum += static_cast<long double>(value);
    const long double mean = sum / values.size();
    long double squared_deviations = 0.0L;
    for (double value : values) {
        const long double deviation = static_cast<long double>(value) - mean;
        squared_deviations += deviation * deviation;
    }
    return static_cast<double>(squared_deviations / values.size());
}
