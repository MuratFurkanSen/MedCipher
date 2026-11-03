#include "Benchmark.h"
#include "Timing.h"
#include "../crypto/CKKSContext.h"
#include "../data/CSVReader.h"
#include "../scenarios/SumMean.h"
#include "../scenarios/Variance.h"
#include "../scenarios/Relationships.h"
#include <seal/util/config.h>
#include <sys/utsname.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::string csv(const std::string& value) {
    std::string out = "\"";
    for (char c : value) { if (c == '"') out += '"'; out += c; }
    return out + '"';
}
std::string cpu_model() {
#ifdef __APPLE__
    FILE* pipe = popen("/usr/sbin/sysctl -n machdep.cpu.brand_string", "r");
    if (!pipe) return "unknown";
    char buffer[512]{};
    std::string result = fgets(buffer, sizeof(buffer), pipe) ? buffer : "unknown";
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
    return result;
#else
    return "unknown";
#endif
}
double relative(double error, double reference) { return reference == 0 ? error : error / std::abs(reference); }
}
static bool benchmark(CKKSContext& crypto, const std::string& data_dir, const std::string& output_path, int scenario) {
    const bool variance = scenario == 1;
    const bool relationships = scenario == 2;
    constexpr int warmups = 5, repetitions = 30;
    struct utsname system{};
    const bool have_uname = uname(&system) == 0;
    const std::string os = have_uname ? std::string(system.sysname) + " " + system.release : "unknown";
    const std::string arch = have_uname ? system.machine : "unknown";
    const auto cpu = cpu_model();
    std::ofstream out(output_path);
    if (!out) throw std::runtime_error("Cannot write " + output_path);
    out << "scenario,dataset,column,records,poly_modulus_degree,coeff_modulus_bits,scale,slots,seal_version,build,compiler,os,architecture,cpu,warmups,runs,metric,unit,median,mean,population_stddev,input_ciphertext_bytes,raw_input_bytes,storage_ratio," << (relationships ? "plaintext_covariance,encrypted_covariance,plaintext_correlation,encrypted_correlation,max_covariance_abs_error,max_covariance_rel_error,max_correlation_abs_error,max_correlation_rel_error,status\n" : variance ? "plaintext_variance,encrypted_variance,max_variance_abs_error,max_variance_rel_error,status\n" : "plaintext_sum,encrypted_sum,plaintext_mean,encrypted_mean,max_sum_abs_error,max_sum_rel_error,max_mean_abs_error,max_mean_rel_error,status\n");
    out << std::setprecision(17);
    std::vector<double> second_column;
    auto measure = [&](CKKSContext& ckks, const std::vector<double>& input) {
        if (relationships) return measure_relationships(ckks, input, second_column);
        if (!variance) return measure_sum_mean(ckks, input);
        const auto v = measure_variance(ckks, input);
        SumMeanRun common;
        common.times = v.times; common.input_ciphertext_bytes = v.input_ciphertext_bytes;
        common.plaintext_sum = v.plaintext_variance; common.encrypted_sum = v.encrypted_variance;
        common.passed = v.passed;
        return common;
    };
    bool all_passed = true;
    for (unsigned n : {100u, 1000u, 10000u}) {
        const std::string name = "patients_" + std::to_string(n) + ".csv";
        const auto values = read_numeric_column(data_dir + "/" + name, "heart_rate");
        if (values.size() != n) throw std::runtime_error("Unexpected record count: " + name);
        if (relationships) second_column = read_numeric_column(data_dir + "/" + name, "systolic_bp");
        for (int i = 0; i < warmups; ++i) {
            if (!measure(crypto, values).passed) throw std::runtime_error("Warmup correctness failure: " + name);
        }
        std::array<std::vector<double>, 9> samples;
        SumMeanRun last;
        double max_sum_error = 0, max_mean_error = 0;
        bool passed = true;
        for (int i = 0; i < repetitions; ++i) {
            last = measure(crypto, values);
            passed = passed && last.passed;
            for (std::size_t j = 0; j < last.times.size(); ++j) samples[j].push_back(last.times[j]);
            const double total = std::accumulate(last.times.begin() + 1, last.times.end(), 0.0);
            samples[6].push_back(total);
            if (last.times[0] <= 0) throw std::runtime_error("Plaintext timing below clock resolution: " + name);
            samples[7].push_back(last.times[3] / last.times[0]);
            samples[8].push_back(last.times[2] + last.times[4]);
            max_sum_error = std::max(max_sum_error, std::abs(last.encrypted_sum - last.plaintext_sum));
            max_mean_error = std::isnan(last.encrypted_mean) ? NAN : std::max(max_mean_error, std::abs(last.encrypted_mean - last.plaintext_mean));
        }
        all_passed = all_passed && passed;
        const std::array<const char*, 9> names = {"plaintext", "encoding", "encryption", "homomorphic_computation", "decryption", "decoding", "encrypted_total", "computation_slowdown", "encryption_decryption"};
        const std::size_t raw_bytes = values.size() * sizeof(double) * (relationships ? 2 : 1);
        for (std::size_t j = 0; j < samples.size(); ++j) {
            auto stat = statistics(samples[j]);
            out << (relationships ? "statistical_relationships," : variance ? "statistical_dispersion," : "basic_aggregation,") << csv(name) << (relationships ? ",heart_rate+systolic_bp," : ",heart_rate,") << n << (crypto.slots()==8192 ? ",16384,\"60;35;35;35;35;35;35;35;35;35;60\",2^35," : ",8192,\"60;40;40;60\",2^40,") << crypto.slots()
                << ',' << csv(SEAL_VERSION) << ',' << csv(MEDCIPHER_BUILD_CONFIG) << ',' << csv(MEDCIPHER_COMPILER)
                << ',' << csv(os) << ',' << csv(arch) << ',' << csv(cpu) << ',' << warmups << ',' << repetitions
                << ',' << names[j] << ',' << (j == 7 ? "ratio" : "ms") << ',' << stat.median << ',' << stat.mean << ',' << stat.stddev
                << ',' << last.input_ciphertext_bytes << ',' << raw_bytes << ',' << static_cast<double>(last.input_ciphertext_bytes) / raw_bytes
                << ',' << last.plaintext_sum << ',' << last.encrypted_sum;
            if (!variance) out << ',' << last.plaintext_mean << ',' << last.encrypted_mean;
            out << ',' << max_sum_error << ',' << relative(max_sum_error, last.plaintext_sum);
            if (!variance) out << ',' << max_mean_error << ',' << relative(max_mean_error, last.plaintext_mean);
            out << ',' << (passed ? last.status : "FAIL") << '\n';
        }
        std::cout << name << ": " << (passed ? last.status : "FAIL")
                  << ", median encrypted total " << statistics(samples[6]).median << " ms, median evaluation " << statistics(samples[3]).median
                  << " ms, input ciphertext " << last.input_ciphertext_bytes << " bytes\n";
    }
    out.close();
    if (!out) throw std::runtime_error("Failed writing " + output_path);
    return all_passed;
}

bool benchmark_sum_mean(CKKSContext& crypto, const std::string& data_dir, const std::string& output_path) {
    return benchmark(crypto, data_dir, output_path, false);
}
bool benchmark_variance(CKKSContext& crypto, const std::string& data_dir, const std::string& output_path) {
    return benchmark(crypto, data_dir, output_path, true);
}

bool benchmark_relationships(CKKSContext& crypto, const std::string& data_dir, const std::string& output_path) { return benchmark(crypto,data_dir,output_path,2); }
