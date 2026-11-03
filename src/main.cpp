#include "crypto/ckks_smoke.h"
#include "benchmark/Benchmark.h"
#include "scenarios/SumMean.h"
#include "scenarios/Variance.h"
#include "scenarios/Relationships.h"
#include <vector>
#include <cmath>
#include "crypto/CKKSContext.h"
#include "data/CSVReader.h"
#include <stdexcept>
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--relationships") {
            CKKSContext crypto(true,true); return relationship_tests(crypto,MEDCIPHER_DATA_DIR) ? 0 : 1;
        }
        if (argc == 2 && std::string(argv[1]) == "--benchmark-strong") {
            if (std::string(MEDCIPHER_BUILD_CONFIG) != "Release") throw std::runtime_error("Release required");
            CKKSContext crypto(true,true); bool pass=true;
            pass=benchmark_sum_mean(crypto,MEDCIPHER_DATA_DIR,std::string(MEDCIPHER_RESULTS_DIR)+"/scenario1_strong.csv") && pass;
            pass=benchmark_variance(crypto,MEDCIPHER_DATA_DIR,std::string(MEDCIPHER_RESULTS_DIR)+"/scenario2_strong.csv") && pass;
            pass=benchmark_relationships(crypto,MEDCIPHER_DATA_DIR,std::string(MEDCIPHER_RESULTS_DIR)+"/scenario3.csv") && pass;
            return pass ? 0 : 1;
        }
        if (argc == 2 && std::string(argv[1]) == "--smoke") return run_ckks_smoke_test() ? 0 : 1;
        if (argc == 2 && (std::string(argv[1]) == "--benchmark" || std::string(argv[1]) == "--benchmark-variance")) {
            if (std::string(MEDCIPHER_BUILD_CONFIG) != "Release") throw std::runtime_error("Benchmark requires a Release build");
            CKKSContext crypto(true);
            const bool variance = std::string(argv[1]) == "--benchmark-variance";
            return (variance ? benchmark_variance(crypto, MEDCIPHER_DATA_DIR, std::string(MEDCIPHER_RESULTS_DIR) + "/scenario2.csv")
                             : benchmark_sum_mean(crypto, MEDCIPHER_DATA_DIR, std::string(MEDCIPHER_RESULTS_DIR) + "/scenario1.csv")) ? 0 : 1;
        }
        if (argc == 2 && std::string(argv[1]) == "--variance-check") {
            CKKSContext crypto(true);
            bool passed = true;
            unsigned index = 0;
            const double expected_variances[] = {124.011636, 103.50260231, 102.9862967344};
            for (unsigned n : {100u, 1000u, 10000u}) {
                const std::string path = std::string(MEDCIPHER_DATA_DIR) + "/patients_" + std::to_string(n) + ".csv";
                const auto values = read_numeric_column(path, "heart_rate");
                if (values.size() != n) throw std::runtime_error("Unexpected record count: " + path);
                const auto check = measure_variance(crypto, values);
                if (std::abs(check.plaintext_variance - expected_variances[index]) > 1e-7 ||
                    std::abs(check.encrypted_variance - expected_variances[index]) > 0.01) passed = false;
                passed = check.passed && passed;
                ++index;
                passed = run_variance(crypto, values, path) && passed;
            }
            // 4097 values cross a chunk boundary: 4096 zeros and one value of 2000.
            std::vector<double> boundary(4097, 0.0); boundary.back() = 2000.0;
            auto b = measure_variance(crypto, boundary);
            const double expected = 4000000.0 * 4096.0 / (4097.0 * 4097.0);
            passed = b.passed && std::abs(b.encrypted_variance - expected) < 0.01 && passed;
            std::vector<double> constant(100, 7.0);
            auto c = measure_variance(crypto, constant);
            passed = c.passed && std::abs(c.encrypted_variance) < 0.01 && passed;
            std::cout << "Boundary and constant fixtures: " << (passed ? "PASS" : "FAIL") << '\n';
            return passed ? 0 : 1;
        }
        if (argc != 1) { std::cerr << "Usage: MedCipher [--smoke|--benchmark|--variance-check|--benchmark-variance]\n"; return 1; }
        bool pass = true;
        CKKSContext crypto(true);
        for (unsigned n : {100u, 1000u, 10000u}) {
            const std::string path = std::string(MEDCIPHER_DATA_DIR) + "/patients_" + std::to_string(n) + ".csv";
            const auto values = read_numeric_column(path, "heart_rate");
            if (values.size() != n) throw std::runtime_error("Unexpected record count in " + path);
            pass = run_sum_mean(crypto, values, path) && pass;
        }
        return pass ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n'; return 1;
    }
}
