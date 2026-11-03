#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <vector>
class CKKSContext;
struct VarianceRun {
    std::array<double, 6> times{};
    double plaintext_variance{}, encrypted_variance{};
    std::size_t input_ciphertext_bytes{};
    bool passed{};
};
VarianceRun measure_variance(CKKSContext& crypto, const std::vector<double>& values);
bool run_variance(CKKSContext& crypto, const std::vector<double>& values, const std::string& name);
