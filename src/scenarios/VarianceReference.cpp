#include <vector>
// Separate translation unit ensures each timed iteration executes without LTO.
double plaintext_variance(const std::vector<double>& values) {
    double sum = 0.0, squares = 0.0;
    for (double value : values) { sum += value; squares += value * value; }
    double mean = sum / values.size();
    return squares / values.size() - mean * mean;
}
