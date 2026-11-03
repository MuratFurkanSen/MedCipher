#include "Polynomial.h"
#include "../crypto/CKKSContext.h"
#include "../crypto/Evaluation.h"
#include "../benchmark/Timing.h"
#include "../data/CSVReader.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

PolynomialRun measure_polynomial(CKKSContext& crypto,const std::vector<double>& glucose,int degree) {
    if(degree!=2 && degree!=3 && degree!=5) throw std::invalid_argument("Unsupported polynomial degree");
    if(glucose.empty()) throw std::invalid_argument("Empty polynomial input");
    std::vector<double> z; z.reserve(glucose.size());
    for(double g:glucose) { if(!std::isfinite(g)||g<65||g>250) throw std::invalid_argument("Glucose outside [65,250]"); z.push_back((g-157.5)/92.5); }
    std::vector<double> ref(z.size());
    double t=measure_ms([&]{for(int i=0;i<1000;++i) polynomial_reference(z,degree,ref);})/1000;
    const auto& ctx=crypto.context(); const auto& keys=crypto.relin_keys(); auto scale=crypto.initial_scale();
    PolynomialTrace trace;
    auto p=measure_pipeline(crypto,z,[&ctx,&keys,scale,degree,&trace](const auto& inputs){
        seal::Evaluator e(ctx); seal::CKKSEncoder encoder(ctx); std::vector<seal::Ciphertext> out;
        auto level=[&](const seal::Ciphertext& c){return ctx.get_context_data(c.parms_id())->chain_index();};
        for(const auto& x:inputs) {
            auto input_level=level(x);
            if(input_level!=9 || std::abs(std::log2(x.scale()/scale))>0.1) throw std::runtime_error("Invalid polynomial input level or scale");
            auto product=[&](seal::Ciphertext a,seal::Ciphertext b) {
                if(level(a)>level(b)) {e.mod_switch_to_inplace(a,b.parms_id()); ++trace.modulus_switches;}
                else if(level(b)>level(a)) {e.mod_switch_to_inplace(b,a.parms_id()); ++trace.modulus_switches;}
                seal::Ciphertext r; e.multiply(a,b,r); e.relinearize_inplace(r,keys); e.rescale_to_next_inplace(r); ++trace.multiplications; ++trace.rescalings; return r;
            };
            std::vector<seal::Ciphertext> powers(degree+1); powers[1]=x; powers[2]=product(x,x);
            if(degree>=3) powers[3]=product(powers[2],x);
            if(degree==5) {powers[4]=product(powers[2],powers[2]); powers[5]=product(powers[3],powers[2]);}
            auto target=level(powers[degree]); seal::Ciphertext sum;
            for(int k=1;k<=degree;++k) {
                if(level(powers[k])>target) {e.mod_switch_to_inplace(powers[k],powers[degree].parms_id()); ++trace.modulus_switches;}
                auto term=multiply_constant(ctx,e,encoder,powers[k],1.0/(1u<<k),scale); ++trace.rescalings;
                if(k==1) sum=std::move(term); else { if(sum.parms_id()!=term.parms_id()||sum.scale()!=term.scale()) throw std::runtime_error("Polynomial terms differ in scale or level"); e.add_inplace(sum,term); }
            }
            seal::Plaintext one; encoder.encode(1.0,sum.parms_id(),sum.scale(),one); e.add_plain_inplace(sum,one);
            const std::size_t depth=degree==2?1:degree==3?2:3;
            if(level(sum)!=input_level-depth-1 || std::abs(std::log2(sum.scale()/scale))>0.1) throw std::runtime_error("Invalid polynomial output level or scale");
            trace.input_level=input_level; trace.output_level=level(sum); trace.rescale_depth=input_level-level(sum); trace.ciphertext_depth=depth; trace.output_scale=sum.scale();
            out.push_back(std::move(sum));
        }
        return out;
    },true);
    const auto chunks=(z.size()+crypto.slots()-1)/crypto.slots();
    if(p.output_ciphertext_count!=chunks || p.decoded.size()!=chunks*crypto.slots()) throw std::runtime_error("Invalid polynomial output size");
    p.times[0]=t; p.decoded.resize(z.size()); PolynomialRun r; r.measurement=std::move(p); r.trace=trace; r.passed=true;
    for(std::size_t i=0;i<z.size();++i) {double error=std::abs(r.measurement.decoded[i]-ref[i]); if(!std::isfinite(error)||error>1e-5) r.passed=false; r.max_absolute_error=std::max(r.max_absolute_error,error); r.mean_absolute_error+=error/z.size(); r.max_relative_error=std::max(r.max_relative_error,ref[i]==0?error:error/std::abs(ref[i]));}
    return r;
}
bool polynomial_tests(CKKSContext& crypto,const std::string& dir) {
    bool pass=true;
    for(int d:{2,3,5}) {
        for(unsigned n:{1000u,10000u,100000u}) {auto v=read_numeric_column(dir+"/patients_"+std::to_string(n/1000)+"K.csv","glucose"); if(v.size()!=n) throw std::runtime_error("Dataset count mismatch"); auto r=measure_polynomial(crypto,v,d); pass &= r.passed; std::cout<<"P"<<d<<" "<<n<<" max="<<r.max_absolute_error<<" "<<(r.passed?"PASS":"FAIL")<<'\n';}
        auto r=measure_polynomial(crypto,{65,157.5,250},d); const double lo=d==2?0.75:d==3?0.625:0.65625; const double hi=d==2?1.75:d==3?1.875:1.96875;
        pass &= r.passed && std::abs(r.measurement.decoded[0]-lo)<1e-5 && std::abs(r.measurement.decoded[1]-1)<1e-5 && std::abs(r.measurement.decoded[2]-hi)<1e-5;
        for(std::size_t n:{1u,4097u,8191u,8192u,8193u}) {std::vector<double> v(n); for(std::size_t i=0;i<n;++i) v[i]=65.0+185.0*(i%101)/100.0; auto x=measure_polynomial(crypto,v,d); pass &= x.passed && x.measurement.decoded.size()==n;}
    }
    for(double x:{64.9,250.1,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {bool rejected=false; try {measure_polynomial(crypto,{x},2);} catch(const std::invalid_argument&) {rejected=true;} pass &= rejected;}
    bool rejected=false; try {measure_polynomial(crypto,{},2);} catch(const std::invalid_argument&) {rejected=true;} return pass && rejected;
}
