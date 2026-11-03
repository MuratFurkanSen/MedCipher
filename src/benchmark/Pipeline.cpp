#include "Pipeline.h"
#include "Timing.h"
#include "../crypto/CKKSContext.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>
PipelineResult measure_pipeline(CKKSContext& crypto, const std::vector<double>& values, const EncryptedEvaluation& evaluate, bool decode_all_slots) {
    if (values.empty()) throw std::runtime_error("Cannot process an empty dataset");
    PipelineResult run;
    const auto slots = crypto.slots();
    std::vector<std::vector<double>> padded;
    for (std::size_t offset = 0; offset < values.size(); offset += slots) {
        const auto end = std::min(offset + slots, values.size());
        padded.emplace_back(values.begin() + offset, values.begin() + end);
        padded.back().resize(slots, 0.0);
    }
    std::vector<seal::Plaintext> plaintexts;
    plaintexts.reserve(padded.size());
    run.times[1] = measure_ms([&] { for (const auto& chunk : padded) plaintexts.push_back(crypto.encode(chunk)); });
    std::vector<seal::Ciphertext> chunks;
    chunks.reserve(padded.size());
    run.times[2] = measure_ms([&] { for (const auto& plain : plaintexts) chunks.push_back(crypto.encrypt_plain(plain)); });
    for (const auto& chunk : chunks) {
        std::ostringstream stream(std::ios::binary);
        run.input_ciphertext_bytes += static_cast<std::size_t>(chunk.save(stream, seal::compr_mode_type::none));
    }
    std::vector<seal::Ciphertext> results;
    run.times[3] = measure_ms([&] { results = evaluate(chunks); });
    std::vector<seal::Plaintext> decrypted;
    decrypted.reserve(results.size());
    run.times[4] = measure_ms([&] { for (const auto& result : results) decrypted.push_back(crypto.decrypt(result)); });
    run.decoded.reserve(results.size() * (decode_all_slots ? slots : 1));
    run.times[5] = measure_ms([&] {
        for (const auto& plain : decrypted) {
            if (decode_all_slots) {
                auto decoded = crypto.decode(plain);
                run.decoded.insert(run.decoded.end(), decoded.begin(), decoded.end());
            } else run.decoded.push_back(crypto.decode_first(plain));
        }
    });
    if (decode_all_slots) {
        run.output_ciphertext_count = results.size();
        for (const auto& result : results) {
            std::ostringstream stream(std::ios::binary);
            run.output_ciphertext_bytes += static_cast<std::size_t>(result.save(stream, seal::compr_mode_type::none));
        }
    }
    return run;
}
