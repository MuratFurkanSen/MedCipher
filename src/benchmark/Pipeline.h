#pragma once
#include <array>
#include <functional>
#include <vector>
#include <seal/seal.h>
class CKKSContext;
// Six stage times: plaintext is supplied by the caller; the remaining stages are measured here.
struct PipelineResult {
    std::array<double, 6> times{};
    std::vector<double> decoded;
    std::size_t input_ciphertext_bytes{};
};
using EncryptedEvaluation = std::function<std::vector<seal::Ciphertext>(const std::vector<seal::Ciphertext>&)>;
PipelineResult measure_pipeline(CKKSContext& crypto, const std::vector<double>& values, const EncryptedEvaluation& evaluate);
