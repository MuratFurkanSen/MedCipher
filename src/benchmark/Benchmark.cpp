#include "Benchmark.h"
#include "Timing.h"
#include "../crypto/CKKSContext.h"
#include "../data/CSVReader.h"
#include "../scenarios/SumMean.h"
#include "../scenarios/Variance.h"
#include "../scenarios/Relationships.h"
#include "../scenarios/WeightedScoring.h"
#include "../scenarios/Polynomial.h"
#include <seal/util/config.h>
#include <sys/utsname.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <unistd.h>
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
static bool benchmark(CKKSContext& crypto, const std::string& data_dir, const std::string& output_path, int scenario, int degree=0, bool append=false) {
    const bool polynomial = scenario == 4;
    const bool scoring = scenario == 3;
    const bool variance = scenario == 1;
    const bool relationships = scenario == 2;
    constexpr int warmups = 5, repetitions = 30;
    struct utsname system{};
    const bool have_uname = uname(&system) == 0;
    const std::string os = have_uname ? std::string(system.sysname) + " " + system.release : "unknown";
    const std::string arch = have_uname ? system.machine : "unknown";
    const auto cpu = cpu_model();
    namespace fs = std::filesystem;
    fs::path destination(output_path);
    if (destination.filename().empty() || !fs::is_directory(destination.parent_path())) throw std::runtime_error("Invalid output destination: " + output_path);
    if (scoring && fs::exists(destination)) throw std::runtime_error("Refusing to overwrite scoring results: " + output_path);
    for (unsigned n : std::vector<unsigned>{1000,10000,100000}) {
        auto name = "patients_" + std::to_string(n/1000) + "K.csv";
        auto path = data_dir + "/" + name;
        auto values = read_numeric_column(path, polynomial ? "glucose" : "heart_rate");
        if (values.size()!=n) throw std::runtime_error("Unexpected record count: " + name);
        if (relationships && read_numeric_column(path,"systolic_bp").size()!=n) throw std::runtime_error("Invalid paired dataset");
        if (scoring) for (const auto& c : load_score_columns(path)) if (c.size()!=n) throw std::runtime_error("Invalid scoring dataset");
    }
    std::ostringstream out;
    if (append) { std::ifstream old(output_path); if (!old) throw std::runtime_error("Cannot read existing results"); out << old.rdbuf(); }
    if (!append) out << "scenario,dataset,column,records,poly_modulus_degree,coeff_modulus_bits,scale,slots,seal_version,build,compiler,os,architecture,cpu,warmups,runs,metric,unit,median,mean,population_stddev,input_ciphertext_bytes,raw_input_bytes,storage_ratio," << (polynomial ? "max_absolute_error,mean_absolute_error,max_relative_error,output_ciphertext_count,output_ciphertext_bytes,degree,coefficients,absolute_tolerance,ct_multiplicative_depth,rescale_depth,input_chain_index,output_chain_index,ct_multiplications,rescale_operations,modulus_switch_operations,output_scale,status\n" : scoring ? "max_absolute_error,mean_absolute_error,max_relative_error,output_ciphertext_count,output_ciphertext_bytes,weights,absolute_tolerance,status\n" : relationships ? "plaintext_covariance,encrypted_covariance,plaintext_correlation,encrypted_correlation,max_covariance_abs_error,max_covariance_rel_error,max_correlation_abs_error,max_correlation_rel_error,status,variance_product,covariance_chain_index,correlation_chain_index,covariance_scale,correlation_scale\n" : variance ? "plaintext_variance,encrypted_variance,max_variance_abs_error,max_variance_rel_error,status\n" : "plaintext_sum,encrypted_sum,plaintext_mean,encrypted_mean,max_sum_abs_error,max_sum_rel_error,max_mean_abs_error,max_mean_rel_error,status\n");
    out << std::setprecision(17);
    std::vector<double> second_column;
    ScoreColumns score_columns;
    ScoringRun score_result;
    PolynomialRun polynomial_result;
    RelationshipTrace relationship_trace;
    auto measure = [&](CKKSContext& ckks, const std::vector<double>& input) {
        if (polynomial) {
            polynomial_result=measure_polynomial(ckks,input,degree);
            score_result.measurement=std::move(polynomial_result.measurement);
            score_result.max_absolute_error=polynomial_result.max_absolute_error;
            score_result.mean_absolute_error=polynomial_result.mean_absolute_error;
            score_result.max_relative_error=polynomial_result.max_relative_error;
            score_result.passed=polynomial_result.passed;
            SumMeanRun common;common.times=score_result.measurement.times;
            common.input_ciphertext_bytes=score_result.measurement.input_ciphertext_bytes;
            common.passed=score_result.passed;return common;
        }
        if (scoring) {
            score_result=measure_scoring(ckks,score_columns);
            SumMeanRun common;common.times=score_result.measurement.times;
            common.input_ciphertext_bytes=score_result.measurement.input_ciphertext_bytes;
            common.passed=score_result.passed;return common;
        }
        if (relationships) return measure_relationships(ckks, input, second_column, &relationship_trace);
        if (!variance) return measure_sum_mean(ckks, input);
        const auto v = measure_variance(ckks, input);
        SumMeanRun common;
        common.times = v.times; common.input_ciphertext_bytes = v.input_ciphertext_bytes;
        common.plaintext_sum = v.plaintext_variance; common.encrypted_sum = v.encrypted_variance;
        common.passed = v.passed;
        return common;
    };
    bool all_passed = true;
    for (unsigned n : std::vector<unsigned>{1000,10000,100000}) {
        const std::string name = "patients_" + std::to_string(n/1000) + "K.csv";
        const auto values = read_numeric_column(data_dir + "/" + name, polynomial ? "glucose" : "heart_rate");
        if (values.size() != n) throw std::runtime_error("Unexpected record count: " + name);
        if (relationships) second_column = read_numeric_column(data_dir + "/" + name, "systolic_bp");
        if (scoring) score_columns=load_score_columns(data_dir + "/" + name);
        for (int i = 0; i < warmups; ++i) {
            if (!measure(crypto, values).passed) throw std::runtime_error("Warmup correctness failure: " + name);
        }
        std::array<std::vector<double>, 9> samples;
        SumMeanRun last;
        double max_sum_error = 0, max_mean_error = 0;
        double max_score_error=0,mean_score_error=0,max_score_relative=0;
        bool passed = true;
        for (int i = 0; i < repetitions; ++i) {
            last = measure(crypto, values);
            passed = passed && last.passed;
            if(scoring || polynomial){
                max_score_error=std::max(max_score_error,score_result.max_absolute_error);
                mean_score_error+=score_result.mean_absolute_error/repetitions;
                max_score_relative=std::max(max_score_relative,score_result.max_relative_error);
            }
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
        const std::size_t raw_bytes = values.size() * sizeof(double) * (scoring ? 4 : relationships ? 2 : 1);
        for (std::size_t j = 0; j < samples.size(); ++j) {
            auto stat = statistics(samples[j]);
            out << (polynomial ? "polynomial_evaluation," : scoring ? "weighted_scoring," : relationships ? "statistical_relationships," : variance ? "statistical_dispersion," : "basic_aggregation,") << csv(name) << (polynomial ? ",glucose," : scoring ? ",heart_rate+systolic_bp+glucose+bmi," : relationships ? ",heart_rate+systolic_bp," : ",heart_rate,") << n << (crypto.slots()==8192 ? ",16384,\"60;35;35;35;35;35;35;35;35;35;60\",2^35," : ",8192,\"60;40;40;60\",2^40,") << crypto.slots()
                << ',' << csv(SEAL_VERSION) << ',' << csv(MEDCIPHER_BUILD_CONFIG) << ',' << csv(MEDCIPHER_COMPILER)
                << ',' << csv(os) << ',' << csv(arch) << ',' << csv(cpu) << ',' << warmups << ',' << repetitions
                << ',' << names[j] << ',' << (j == 7 ? "ratio" : "ms") << ',' << stat.median << ',' << stat.mean << ',' << stat.stddev
                << ',' << last.input_ciphertext_bytes << ',' << raw_bytes << ',' << static_cast<double>(last.input_ciphertext_bytes) / raw_bytes
                ;
            if(scoring || polynomial){
                out<<','<<max_score_error<<','<<mean_score_error<<','<<max_score_relative
                   <<','<<score_result.measurement.output_ciphertext_count<<','<<score_result.measurement.output_ciphertext_bytes
                   ;
                if(polynomial){
                    const auto& t=polynomial_result.trace;
                    const char* coefficients=degree==2?"1;0.5;0.25":degree==3?"1;0.5;0.25;0.125":"1;0.5;0.25;0.125;0.0625;0.03125";
                    out<<','<<degree<<','<<coefficients<<",0.00001,"<<t.ciphertext_depth<<','<<t.rescale_depth
                       <<','<<t.input_level<<','<<t.output_level<<','<<t.multiplications<<','<<t.rescalings
                       <<','<<t.modulus_switches<<','<<t.output_scale<<','<<(passed?"PASS":"FAIL")<<'\n';
                } else
                out<<",0.25;0.30;0.30;0.15,0.00001,"<<(passed?"PASS":"FAIL")<<'\n';
                continue;
            }
            out << ',' << last.plaintext_sum << ',' << last.encrypted_sum;
            if (!variance) out << ',' << last.plaintext_mean << ',' << last.encrypted_mean;
            out << ',' << max_sum_error << ',' << relative(max_sum_error, last.plaintext_sum);
            if (!variance) out << ',' << max_mean_error << ',' << relative(max_mean_error, last.plaintext_mean);
            out << ',' << (passed ? last.status : "FAIL");
            if(relationships) out<<','<<relationship_reference(values,second_column).product<<','<<relationship_trace.covariance_level<<','<<relationship_trace.correlation_level<<','<<relationship_trace.covariance_scale<<','<<relationship_trace.correlation_scale;
            out << '\n';
        }
        if(polynomial) std::cout<<"degree "<<degree<<" ";
        std::cout << name << ": " << (passed ? last.status : "FAIL")
                  << ", median encrypted total " << statistics(samples[6]).median << " ms, median evaluation " << statistics(samples[3]).median
                  << " ms, input ciphertext " << last.input_ciphertext_bytes << " bytes\n";
    }
    if (!all_passed || !out) return false;
    auto pattern = (destination.parent_path() / ("." + destination.filename().string() + ".tmp.XXXXXX")).string();
    std::vector<char> name(pattern.begin(),pattern.end()); name.push_back('\0');
    int fd=mkstemp(name.data());
    if(fd<0) throw std::runtime_error("Cannot create temporary results file");
    auto contents=out.str(); std::size_t done=0;
    while(done<contents.size()) { auto n=write(fd,contents.data()+done,contents.size()-done); if(n<=0) { close(fd); unlink(name.data()); throw std::runtime_error("Cannot write results"); } done+=static_cast<std::size_t>(n); }
    if(close(fd)!=0) { unlink(name.data()); throw std::runtime_error("Cannot close results"); }
    if(scoring) { if(link(name.data(),output_path.c_str())!=0) { unlink(name.data()); throw std::runtime_error("Cannot publish new scoring results"); } unlink(name.data()); }
    else if(rename(name.data(),output_path.c_str())!=0) { unlink(name.data()); throw std::runtime_error("Cannot publish results"); }
    return true;
}

bool benchmark_sum_mean(CKKSContext& crypto, const std::string& data_dir, const std::string& output_path) {
    return benchmark(crypto, data_dir, output_path, false);
}
bool benchmark_variance(CKKSContext& crypto, const std::string& data_dir, const std::string& output_path) {
    return benchmark(crypto, data_dir, output_path, true);
}

bool benchmark_relationships(CKKSContext& crypto, const std::string& data_dir, const std::string& output_path) { return benchmark(crypto,data_dir,output_path,2); }

bool benchmark_scoring(CKKSContext& crypto,const std::string& data_dir,const std::string& output_path){return benchmark(crypto,data_dir,output_path,3);}

bool benchmark_polynomial(CKKSContext& crypto,const std::string& data_dir,const std::string& output_path){
    namespace fs=std::filesystem;
    fs::path dest(output_path);
    if(dest.filename().empty() || !fs::is_directory(dest.parent_path()) || fs::exists(dest))
        throw std::runtime_error("Invalid or existing polynomial results destination: " + output_path);
    std::string pattern=(dest.parent_path()/("."+dest.filename().string()+".staging.XXXXXX")).string();
    std::vector<char> temp(pattern.begin(),pattern.end()); temp.push_back('\0');
    int fd=mkstemp(temp.data()); if(fd<0) throw std::runtime_error("Cannot create polynomial staging file");
    close(fd);
    try {
        for(int degree:{2,3,5}) if(!benchmark(crypto,data_dir,temp.data(),4,degree,degree!=2)) { unlink(temp.data()); return false; }
        if(link(temp.data(),output_path.c_str())!=0) throw std::runtime_error("Cannot publish polynomial results");
        unlink(temp.data()); return true;
    } catch(...) { unlink(temp.data()); throw; }
}



bool benchmark_all(const std::string& data_dir, const std::string& results_dir) {
    namespace fs=std::filesystem;
    fs::path dir(results_dir);
    if(!fs::is_directory(dir)) throw std::runtime_error("Results directory does not exist");
    const std::vector<std::string> names={"scenario1.csv","scenario2.csv","scenario3.csv","scenario4.csv","scenario5.csv"};
    for(const auto& name:names) if(fs::exists(dir/name)) throw std::runtime_error("Results already exist: "+(dir/name).string());
    auto pattern=(dir/".medcipher-benchmark.XXXXXX").string();
    std::vector<char> tmp(pattern.begin(),pattern.end()); tmp.push_back('\0');
    if(!mkdtemp(tmp.data())) throw std::runtime_error("Cannot create benchmark staging directory");
    fs::path stage(tmp.data()); std::vector<fs::path> published;
    try {
        { CKKSContext crypto(true); if(!benchmark_sum_mean(crypto,data_dir,(stage/names[0]).string())) {fs::remove_all(stage);return false;} }
        { CKKSContext crypto(true); if(!benchmark_variance(crypto,data_dir,(stage/names[1]).string())) {fs::remove_all(stage);return false;} }
        { CKKSContext crypto(true,true); if(!benchmark_relationships(crypto,data_dir,(stage/names[2]).string())) {fs::remove_all(stage);return false;} }
        { CKKSContext crypto; if(!benchmark_scoring(crypto,data_dir,(stage/names[3]).string())) {fs::remove_all(stage);return false;} }
        { CKKSContext crypto(true,true); if(!benchmark_polynomial(crypto,data_dir,(stage/names[4]).string())) {fs::remove_all(stage);return false;} }
        for(const auto& name:names) {
            auto dest=dir/name;
            if(link((stage/name).c_str(),dest.c_str())!=0) throw std::runtime_error("Cannot publish results: "+dest.string());
            published.push_back(dest);
        }
        fs::remove_all(stage); return true;
    } catch(...) {
        for(const auto& path:published) {std::error_code error; fs::remove(path,error);}
        std::error_code error; fs::remove_all(stage,error); throw;
    }
}
