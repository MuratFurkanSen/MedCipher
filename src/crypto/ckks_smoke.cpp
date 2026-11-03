#include "ckks_smoke.h"

#include "CKKSContext.h"

#include <cmath>
#include <iostream>
#include <iomanip>
#include <vector>

namespace {
// Evaluation receives only ciphertexts and public context, never a secret key.
seal::Ciphertext add_encrypted(const seal::SEALContext& context,
                               const seal::Ciphertext& left,
                               const seal::Ciphertext& right) {
    seal::Evaluator evaluator(context);
    seal::Ciphertext sum;
    evaluator.add(left, right, sum);
    return sum;
}
}

bool run_ckks_smoke_test() {
    constexpr double left = 1.25;
    constexpr double right = 2.5;
    constexpr double tolerance = 1e-5;

    CKKSContext crypto;
    const auto left_encrypted = crypto.encrypt({left});
    const auto right_encrypted = crypto.encrypt({right});
    const auto sum_encrypted = add_encrypted(crypto.context(), left_encrypted, right_encrypted);
    const double result = crypto.decrypt_first(sum_encrypted);
    const double expected = left + right;
    const double error = std::abs(result - expected);
    const bool passed = error < tolerance;
    std::cout << std::setprecision(12) << "Expected: " << expected << "\nDecrypted: " << result
              << "\nAbsolute error: " << error << "\nSmoke test: "
              << (passed ? "PASS" : "FAIL") << '\n';
    return passed;
}
