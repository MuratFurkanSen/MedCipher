#include "WeightedScoring.h"
#include "../crypto/CKKSContext.h"
#include "../crypto/Evaluation.h"
#include "../benchmark/Timing.h"
#include "../data/CSVReader.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

ScoreColumns load_score_columns(const std::string& path) {
    return {read_numeric_column(path,"heart_rate"), read_numeric_column(path,"systolic_bp"), read_numeric_column(path,"glucose"), read_numeric_column(path,"bmi")};
}
ScoringRun measure_scoring(CKKSContext& crypto, const ScoreColumns& c) {
    const auto n=c[0].size(), slots=crypto.slots();
    if (!n) throw std::invalid_argument("Empty scoring input");
    for (const auto& col:c) {
        if (col.size()!=n) throw std::invalid_argument("Scoring column length mismatch");
        for (double v:col) if (!std::isfinite(v)) throw std::invalid_argument("Nonfinite scoring input");
    }
    std::vector<double> reference(n);
    auto plain_time=measure_ms([&]{for(int i=0;i<1000;++i) scoring_reference(c,reference);})/1000;
    const auto chunks=(n+slots-1)/slots;
    std::vector<double> packed; packed.reserve(4*chunks*slots);
    for(const auto& col:c) { packed.insert(packed.end(),col.begin(),col.end()); packed.resize(packed.size()+chunks*slots-n,0); }
    const auto& context=crypto.context(); const auto scale=crypto.initial_scale();
    auto p=measure_pipeline(crypto,packed,[&context,chunks,scale](const auto& encrypted){
        seal::Evaluator eval(context); seal::CKKSEncoder encoder(context);
        const double weights[]{0.25,0.30,0.30,0.15};
        std::vector<seal::Ciphertext> result; result.reserve(chunks);
        for(std::size_t i=0;i<chunks;++i) {
            auto sum=multiply_constant(context,eval,encoder,encrypted[i],weights[0],scale);
            for(std::size_t j=1;j<4;++j) {
                auto term=multiply_constant(context,eval,encoder,encrypted[j*chunks+i],weights[j],scale);
                if(term.parms_id()!=sum.parms_id() || term.scale()!=sum.scale()) throw std::runtime_error("Scoring scale or level mismatch");
                eval.add_inplace(sum,term);
            }
            result.push_back(std::move(sum));
        }
        return result;
    },true);
    if (p.output_ciphertext_count!=chunks || p.decoded.size()!=chunks*slots) throw std::runtime_error("Incorrect scoring output size");
    p.times[0]=plain_time; p.decoded.resize(n);
    ScoringRun r; r.measurement=std::move(p); r.passed=r.measurement.decoded.size()==n;
    for(std::size_t i=0;i<n;++i) {
        double e=std::abs(r.measurement.decoded[i]-reference[i]);
        if(!std::isfinite(e) || e>1e-5) r.passed=false;
        r.max_absolute_error=std::max(r.max_absolute_error,e); r.mean_absolute_error+=e/n;
        r.max_relative_error=std::max(r.max_relative_error, reference[i]==0 ? e : e/std::abs(reference[i]));
    }
    return r;
}
bool scoring_tests(CKKSContext& crypto,const std::string& dir) {
    bool pass=true;
    for(unsigned n:{1000u,10000u,100000u}) {
        auto c=load_score_columns(dir+"/patients_"+std::to_string(n/1000)+"K.csv");
        if(c[0].size()!=n) throw std::runtime_error("Incorrect scoring dataset size");
        auto r=measure_scoring(crypto,c); pass &= r.passed;
        std::cout<<n<<" records: "<<(r.passed?"PASS":"FAIL")<<" max="<<r.max_absolute_error<<" mean="<<r.mean_absolute_error<<" relative="<<r.max_relative_error<<'\n';
    }
    for(std::size_t n:{1u,4095u,4096u,4097u,8193u}) {
        ScoreColumns c; for(auto& v:c) v.resize(n);
        for(std::size_t i=0;i<n;++i) { c[0][i]=double(int(i%11)-5); c[1][i]=double(int(i%7)-3); c[2][i]=double(int(i%5)-2); c[3][i]=double(int(i%3)-1); }
        auto r=measure_scoring(crypto,c); pass &= r.passed;
        std::cout<<"fixture "<<n<<": "<<(r.passed?"PASS":"FAIL")<<'\n';
    }
    for(int j=0;j<4;++j) { ScoreColumns c; for(auto& v:c)v={0,0,0}; c[j]={-100,0,100}; auto r=measure_scoring(crypto,c); const double w[]{0.25,0.30,0.30,0.15};
        pass &= r.passed && r.measurement.decoded.size()==3 && std::abs(r.measurement.decoded[0]+100*w[j])<=1e-5 && std::abs(r.measurement.decoded[1])<=1e-5 && std::abs(r.measurement.decoded[2]-100*w[j])<=1e-5; }
    return pass;
}
