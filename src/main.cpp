#include "crypto/ckks_smoke.h"
#include "crypto/CKKSContext.h"
#include "benchmark/Benchmark.h"
#include "scenarios/ScenarioChecks.h"
#include "scenarios/Relationships.h"
#include "scenarios/WeightedScoring.h"
#include "scenarios/Polynomial.h"
#include <iostream>
#include <string>
#include <stdexcept>

int main(int argc, char** argv) {
    const char* usage="Usage: MedCipher <--smoke|--scenario1|--scenario2|--scenario3|--scenario4|--scenario5|--benchmark>\n";
    if(argc!=2) { std::cerr<<usage; return 1; }
    const std::string mode=argv[1];
    try {
        if(mode=="--smoke") return run_ckks_smoke_test()?0:1;
        if(mode=="--scenario1" || mode=="--scenario2") {
            CKKSContext crypto(true);
            return (mode=="--scenario1"?scenario1_tests(crypto,MEDCIPHER_DATA_DIR):scenario2_tests(crypto,MEDCIPHER_DATA_DIR))?0:1;
        }
        if(mode=="--scenario3" || mode=="--scenario5") {
            CKKSContext crypto(true,true);
            return (mode=="--scenario3"?relationship_tests(crypto,MEDCIPHER_DATA_DIR):polynomial_tests(crypto,MEDCIPHER_DATA_DIR))?0:1;
        }
        if(mode=="--scenario4") { CKKSContext crypto; return scoring_tests(crypto,MEDCIPHER_DATA_DIR)?0:1; }
        if(mode=="--benchmark") {
            if(std::string(MEDCIPHER_BUILD_CONFIG)!="Release") throw std::runtime_error("Benchmark requires a Release build");
            return benchmark_all(MEDCIPHER_DATA_DIR,MEDCIPHER_RESULTS_DIR)?0:1;
        }
        std::cerr<<usage; return 1;
    } catch(const std::exception& e) { std::cerr<<"Error: "<<e.what()<<'\n'; return 1; }
}
