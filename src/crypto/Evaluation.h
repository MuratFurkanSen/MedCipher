#pragma once
#include <seal/seal.h>
// Public-only arithmetic. Choose the encoding scale so rescaling gives target_scale.
inline seal::Ciphertext multiply_constant(const seal::SEALContext& context, seal::Evaluator& evaluator,
    seal::CKKSEncoder& encoder, const seal::Ciphertext& value, double constant, double target_scale) {
    const auto& primes = context.get_context_data(value.parms_id())->parms().coeff_modulus();
    seal::Plaintext plain;
    encoder.encode(constant, value.parms_id(), target_scale * static_cast<double>(primes.back().value()) / value.scale(), plain);
    seal::Ciphertext result;
    evaluator.multiply_plain(value, plain, result);
    evaluator.rescale_to_next_inplace(result);
    result.scale() = target_scale; // Only floating-point rounding of the scale expression above.
    return result;
}
