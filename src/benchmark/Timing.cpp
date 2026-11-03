#include "Timing.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
Statistics statistics(std::vector<double> samples) {
    if (samples.empty()) throw std::invalid_argument("No timing samples");
    std::sort(samples.begin(), samples.end());
    const auto n = samples.size();
    const double median = n % 2 ? samples[n / 2] : (samples[n / 2 - 1] + samples[n / 2]) / 2;
    const double mean = std::accumulate(samples.begin(), samples.end(), 0.0) / n;
    double squared = 0;
    for (double value : samples) squared += (value - mean) * (value - mean);
    return {median, mean, std::sqrt(squared / n)};
}
void plaintext_sum_mean(const std::vector<double>& values, double& sum, double& mean) {
    sum = std::accumulate(values.begin(), values.end(), 0.0);
    mean = sum / values.size();
}
