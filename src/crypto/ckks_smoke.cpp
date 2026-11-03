#include "ckks_smoke.h"

#include <seal/seal.h>

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

    seal::EncryptionParameters params(seal::scheme_type::ckks);
    params.set_poly_modulus_degree(8192);
    params.set_coeff_modulus(seal::CoeffModulus::Create(8192, {60, 40, 40, 60}));
    seal::SEALContext context(params);
    seal::KeyGenerator keys(context);
    seal::PublicKey public_key;
    keys.create_public_key(public_key);
    seal::Encryptor encryptor(context, public_key);
    seal::Decryptor decryptor(context, keys.secret_key());
    seal::CKKSEncoder encoder(context);

    seal::Plaintext left_plain, right_plain;
    encoder.encode(left, std::pow(2.0, 40), left_plain);
    encoder.encode(right, std::pow(2.0, 40), right_plain);
    seal::Ciphertext left_encrypted, right_encrypted;
    encryptor.encrypt(left_plain, left_encrypted);
    encryptor.encrypt(right_plain, right_encrypted);
    const auto sum_encrypted = add_encrypted(context, left_encrypted, right_encrypted);

    seal::Plaintext sum_plain;
    decryptor.decrypt(sum_encrypted, sum_plain);
    std::vector<double> decoded;
    encoder.decode(sum_plain, decoded);
    const double expected = left + right;
    const double error = std::abs(decoded.at(0) - expected);
    const bool passed = error < tolerance;
    std::cout << std::setprecision(12) << "Expected: " << expected << "\nDecrypted: " << decoded[0]
              << "\nAbsolute error: " << error << "\nSmoke test: "
              << (passed ? "PASS" : "FAIL") << '\n';
    return passed;
}
