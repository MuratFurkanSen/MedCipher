#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <vector>
class CKKSContext;
// Milliseconds: plaintext, encoding, encryption, evaluation, decryption, decoding.
struct SumMeanRun {
    std::array<double, 6> times{};
    double plaintext_sum{}, plaintext_mean{}, encrypted_sum{}, encrypted_mean{};
    std::size_t input_ciphertext_bytes{};
    bool passed{};
    std::string status = "PASS";
};
SumMeanRun measure_sum_mean(CKKSContext& crypto, const std::vector<double>& values);
bool run_sum_mean(CKKSContext& crypto, const std::vector<double>& values, const std::string& path);
