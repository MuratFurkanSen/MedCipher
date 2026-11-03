#include "ScenarioChecks.h"
#include "SumMean.h"
#include "Variance.h"
#include "../data/CSVReader.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

bool scenario1_tests(CKKSContext& crypto, const char* dir) {
    bool pass=true;
    for(unsigned n:{1000u,10000u,100000u}) {
        auto path=std::string(dir)+"/patients_"+std::to_string(n/1000)+"K.csv";
        auto values=read_numeric_column(path,"heart_rate");
        if(values.size()!=n) throw std::runtime_error("Unexpected record count in "+path);
        pass=run_sum_mean(crypto,values,path) && pass;
    }
    for (std::size_t n : {4095u, 4096u, 4097u}) {
        std::vector<double> values(n, 1.0);
        values.back() = 3.0;
        const double expected_sum = static_cast<double>(n) + 2.0;
        auto result = measure_sum_mean(crypto, values);
        pass = result.passed && std::abs(result.encrypted_sum - expected_sum) <= 0.01
            && std::abs(result.encrypted_mean - expected_sum / n) <= 0.0001 && pass;
    }
    return pass;
}
bool scenario2_tests(CKKSContext& crypto, const char* dir) {
    bool passed=true; unsigned index=0;
    const double expected_variances[]{99.86710236,100.5622800479,102.184034656279};
    for(unsigned n:{1000u,10000u,100000u}) {
        auto path=std::string(dir)+"/patients_"+std::to_string(n/1000)+"K.csv";
        auto values=read_numeric_column(path,"heart_rate");
        if(values.size()!=n) throw std::runtime_error("Unexpected record count: "+path);
        auto check=measure_variance(crypto,values);
        if(std::abs(check.plaintext_variance-expected_variances[index])>1e-7 || std::abs(check.encrypted_variance-expected_variances[index])>0.01) passed=false;
        passed=check.passed && passed; ++index;
        passed=run_variance(crypto,values,path) && passed;
    }
    std::vector<double> boundary(4097,0.0); boundary.back()=2000.0;
    auto b=measure_variance(crypto,boundary);
    const double expected=4000000.0*4096.0/(4097.0*4097.0);
    passed=b.passed && std::abs(b.encrypted_variance-expected)<0.01 && passed;
    std::vector<double> constant(100,7.0); auto c=measure_variance(crypto,constant);
    passed=c.passed && std::abs(c.encrypted_variance)<0.01 && passed;
    std::cout<<"Boundary and constant fixtures: "<<(passed?"PASS":"FAIL")<<'\n';
    return passed;
}
