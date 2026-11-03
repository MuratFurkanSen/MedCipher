#pragma once
#include <seal/seal.h>
#include <vector>

// Owns the keys and common CKKS objects for the current experiment.
class CKKSContext {
public:
    explicit CKKSContext(bool rotations = false, bool relationships = false);
    std::size_t slots() const;
    double initial_scale() const { return scale_; }
    seal::Plaintext encode(const std::vector<double>& values);
    seal::Ciphertext encrypt_plain(const seal::Plaintext& plain);
    seal::Plaintext decrypt(const seal::Ciphertext& value);
    std::vector<double> decode(const seal::Plaintext& plain);
    double decode_first(const seal::Plaintext& plain);
    seal::Ciphertext encrypt(const std::vector<double>& values);
    double decrypt_first(const seal::Ciphertext& value);
    const seal::SEALContext& context() const { return context_; }
    const seal::RelinKeys& relin_keys() const { return relin_; }
    const seal::GaloisKeys& galois_keys() const { return galois_; }
    static constexpr double scale = 1099511627776.0; // 2^40
private:
    static seal::EncryptionParameters parameters(bool relationships);
    double scale_;
    seal::SEALContext context_;
    seal::KeyGenerator keys_;
    seal::PublicKey public_key_;
    seal::GaloisKeys galois_;
    seal::RelinKeys relin_;
    seal::CKKSEncoder encoder_;
};
