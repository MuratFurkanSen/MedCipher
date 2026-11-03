#include "CKKSContext.h"
seal::EncryptionParameters CKKSContext::parameters(bool relationships) {
    seal::EncryptionParameters params(seal::scheme_type::ckks);
    params.set_poly_modulus_degree(relationships ? 16384 : 8192);
    params.set_coeff_modulus(seal::CoeffModulus::Create(relationships ? 16384 : 8192, relationships ? std::vector<int>{60,35,35,35,35,35,35,35,35,35,60} : std::vector<int>{60,40,40,60}));
    return params;
}
CKKSContext::CKKSContext(bool rotations, bool relationships)
    : scale_(relationships ? 34359738368.0 : scale), context_(parameters(relationships)), keys_(context_), encoder_(context_) {
    keys_.create_public_key(public_key_);
    if (rotations) {
        keys_.create_relin_keys(relin_);
        std::vector<int> steps;
        for (std::size_t step = 1; step < slots(); step *= 2) steps.push_back(static_cast<int>(step));
        keys_.create_galois_keys(steps, galois_);
    }
}
std::size_t CKKSContext::slots() const { return encoder_.slot_count(); }
seal::Plaintext CKKSContext::encode(const std::vector<double>& values) {
    seal::Plaintext plain;
    encoder_.encode(values, scale_, plain);
    return plain;
}
seal::Ciphertext CKKSContext::encrypt_plain(const seal::Plaintext& plain) {
    seal::Ciphertext encrypted;
    seal::Encryptor(context_, public_key_).encrypt(plain, encrypted);
    return encrypted;
}
seal::Plaintext CKKSContext::decrypt(const seal::Ciphertext& value) {
    seal::Plaintext plain;
    seal::Decryptor(context_, keys_.secret_key()).decrypt(value, plain);
    return plain;
}
std::vector<double> CKKSContext::decode(const seal::Plaintext& plain) {
    std::vector<double> decoded;
    encoder_.decode(plain, decoded);
    return decoded;
}
double CKKSContext::decode_first(const seal::Plaintext& plain) {
    return decode(plain).at(0);
}
seal::Ciphertext CKKSContext::encrypt(const std::vector<double>& values) {
    return encrypt_plain(encode(values));
}
double CKKSContext::decrypt_first(const seal::Ciphertext& value) {
    return decode_first(decrypt(value));
}
