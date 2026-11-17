#include "SumMean.h"
#include "../crypto/CKKSContext.h"
#include "../crypto/Evaluation.h"
#include "../benchmark/Timing.h"
#include "../benchmark/Pipeline.h"
#include <seal/seal.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
// Only public evaluation material and ciphertexts enter this function.
std::pair<seal::Ciphertext, seal::Ciphertext> evaluate(
    const seal::SEALContext& context,
    const seal::GaloisKeys& galois,
    const std::vector<seal::Ciphertext>& chunks,
    std::size_t slots,
    std::size_t count) {

    seal::Evaluator evaluator(context);
    seal::Ciphertext total;
    bool first = true;
    for (auto partial : chunks) {
        for (std::size_t step = 1; step < slots; step *= 2) {
            seal::Ciphertext rotated;
            evaluator.rotate_vector(partial, static_cast<int>(step), galois, rotated);
            evaluator.add_inplace(partial, rotated);
        }
        if (first) { total = std::move(partial); first = false; }
        else evaluator.add_inplace(total, partial);
    }
    seal::CKKSEncoder encoder(context);
    auto mean=multiply_constant(context,evaluator,encoder,total,1.0/static_cast<double>(count),total.scale());
    return {total, mean};
}
}
SumMeanRun measure_sum_mean(CKKSContext& crypto, const std::vector<double>& values) {
    if (values.empty()) throw std::runtime_error("Cannot calculate mean of an empty dataset");
    SumMeanRun run;
    constexpr int plaintext_batch = 1000;
    const double plaintext_time = measure_ms([&] {
        for (int i = 0; i < plaintext_batch; ++i)
            plaintext_sum_mean(values, run.plaintext_sum, run.plaintext_mean);
    }) / plaintext_batch;
    auto pipeline = measure_pipeline(crypto, values, [&](const std::vector<seal::Ciphertext>& chunks) {
        auto [sum, mean] = evaluate(crypto.context(), crypto.galois_keys(), chunks, crypto.slots(), values.size());
        return std::vector<seal::Ciphertext>{std::move(sum), std::move(mean)};
    });
    run.times = pipeline.times;
    run.times[0] = plaintext_time;
    run.input_ciphertext_bytes = pipeline.input_ciphertext_bytes;
    run.encrypted_sum = pipeline.decoded.at(0);
    run.encrypted_mean = pipeline.decoded.at(1);
    run.passed = std::isfinite(run.encrypted_sum) && std::isfinite(run.encrypted_mean)
        && std::abs(run.encrypted_sum - run.plaintext_sum) <= 0.01
        && std::abs(run.encrypted_mean - run.plaintext_mean) <= 0.0001;
    return run;
}
bool run_sum_mean(CKKSContext& crypto, const std::vector<double>& values, const std::string& path) {
    const auto run = measure_sum_mean(crypto, values);
    const double sum_error = std::abs(run.encrypted_sum - run.plaintext_sum);
    const double mean_error = std::abs(run.encrypted_mean - run.plaintext_mean);
    auto relative = [](double error, double reference) { return reference == 0 ? error : error / std::abs(reference); };
    std::cout << std::setprecision(12) << "Dataset: " << std::filesystem::path(path).filename().string()
              << "\nColumn: heart_rate\nRecords: " << values.size()
              << "\n\nPlaintext sum: " << run.plaintext_sum << "\nEncrypted sum: " << run.encrypted_sum
              << "\nAbsolute error: " << sum_error << "\nRelative error: " << relative(sum_error, run.plaintext_sum)
              << "\n\nPlaintext mean: " << run.plaintext_mean << "\nEncrypted mean: " << run.encrypted_mean
              << "\nAbsolute error: " << mean_error << "\nRelative error: " << relative(mean_error, run.plaintext_mean)
              << "\n\nResult: " << (run.passed ? "PASS" : "FAIL") << "\n\n";
    return run.passed;
}
