#pragma once
#include <chrono>
#include <vector>

using Clock = std::chrono::steady_clock;
template<class Function> double measure_ms(Function&& function) {
    const auto start = Clock::now();
    function();
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
struct Statistics { double median, mean, stddev; };
Statistics statistics(std::vector<double> samples);
// Kept out of the timing caller's translation unit so each batch iteration executes.
void plaintext_sum_mean(const std::vector<double>& values, double& sum, double& mean);
