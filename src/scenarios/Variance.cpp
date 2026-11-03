#include "Variance.h"
#include "../crypto/CKKSContext.h"
#include "../crypto/Evaluation.h"
#include "../benchmark/Pipeline.h"
#include "../benchmark/Timing.h"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>

double plaintext_variance(const std::vector<double>& values);
namespace {
seal::Ciphertext evaluate(const seal::SEALContext& context, const seal::GaloisKeys& galois,
                          const seal::RelinKeys& relin, const std::vector<seal::Ciphertext>& chunks,
                          std::size_t slots, std::size_t count) {
    seal::Evaluator evaluator(context);
    seal::Ciphertext total, squared_total;
    bool first = true;
    for (const auto& chunk : chunks) {
        auto partial = chunk;
        seal::Ciphertext squared;
        evaluator.square(chunk, squared);
        evaluator.relinearize_inplace(squared, relin);
        evaluator.rescale_to_next_inplace(squared);
        for (std::size_t step = 1; step < slots; step *= 2) {
            seal::Ciphertext rotated;
            evaluator.rotate_vector(partial, static_cast<int>(step), galois, rotated);
            evaluator.add_inplace(partial, rotated);
            evaluator.rotate_vector(squared, static_cast<int>(step), galois, rotated);
            evaluator.add_inplace(squared, rotated);
        }
        if (first) { total = std::move(partial); squared_total = std::move(squared); first = false; }
        else { evaluator.add_inplace(total, partial); evaluator.add_inplace(squared_total, squared); }
    }
    seal::CKKSEncoder encoder(context);
    auto mean=multiply_constant(context,evaluator,encoder,total,1.0/static_cast<double>(count),total.scale());
    evaluator.square_inplace(mean);
    evaluator.relinearize_inplace(mean,relin);
    evaluator.rescale_to_next_inplace(mean);
    squared_total=multiply_constant(context,evaluator,encoder,squared_total,1.0/static_cast<double>(count),mean.scale());
    evaluator.sub_inplace(squared_total, mean);
    return squared_total;
}
}
VarianceRun measure_variance(CKKSContext& crypto, const std::vector<double>& values) {
    if (values.empty()) throw std::runtime_error("Cannot calculate variance of an empty dataset");
    VarianceRun run;
    constexpr int batch = 1000;
    double reference = 0;
    const double plaintext_time = measure_ms([&] { for (int i=0; i<batch; ++i) reference = plaintext_variance(values); }) / batch;
    auto pipeline = measure_pipeline(crypto, values, [&](const std::vector<seal::Ciphertext>& chunks) {
        std::vector<seal::Ciphertext> result;
        result.push_back(evaluate(crypto.context(), crypto.galois_keys(), crypto.relin_keys(), chunks, crypto.slots(), values.size()));
        return result;
    });
    run.times = pipeline.times; run.times[0] = plaintext_time;
    run.input_ciphertext_bytes = pipeline.input_ciphertext_bytes;
    run.plaintext_variance = reference; run.encrypted_variance = pipeline.decoded.at(0);
    constexpr double tolerance = 0.01;
    run.passed = std::isfinite(run.encrypted_variance) && run.encrypted_variance >= -tolerance && std::abs(run.encrypted_variance - reference) <= tolerance;
    return run;
}
bool run_variance(CKKSContext& crypto, const std::vector<double>& values, const std::string& name) {
    auto result = measure_variance(crypto, values);
    const double error = std::abs(result.encrypted_variance - result.plaintext_variance);
    std::cout << std::setprecision(12) << name << ": plaintext variance " << result.plaintext_variance
              << ", encrypted variance " << result.encrypted_variance << ", absolute error " << error
              << ", relative error " << (result.plaintext_variance == 0 ? error : error / std::abs(result.plaintext_variance))
              << ", " << (result.passed ? "PASS" : "FAIL") << '\n';
    return result.passed;
}
