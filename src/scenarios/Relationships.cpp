#include "Relationships.h"
#include "../crypto/CKKSContext.h"
#include "../crypto/Evaluation.h"
#include "../benchmark/Pipeline.h"
#include "../benchmark/Timing.h"
#include "../data/CSVReader.h"
#include <cmath>
#include <iostream>
#include <iomanip>
#include <stdexcept>

namespace {
// Fixed public domain [4096,16384], chosen before reading any dataset statistics.
// Degree 8 binomial series in u=v/10240-1; |u| <= 0.6.
std::vector<seal::Ciphertext> evaluate(const seal::SEALContext& context,
 const seal::GaloisKeys& galois, const seal::RelinKeys& relin,
 const std::vector<seal::Ciphertext>& inputs, std::size_t chunks, std::size_t slots, std::size_t count, double scale, bool correlation_requested, RelationshipTrace& trace) {
    seal::Evaluator e(context); seal::CKKSEncoder encoder(context);
    auto verify=[&](const seal::Ciphertext& value,std::size_t level){
        if(context.get_context_data(value.parms_id())->chain_index()!=level || !std::isfinite(value.scale()) || std::abs(std::log2(value.scale()/scale))>0.1)
            throw std::runtime_error("Unexpected relationship ciphertext level/scale");
    };
    for(const auto& input:inputs)verify(input,9);
    auto product=[&](seal::Ciphertext a,seal::Ciphertext b) {
        auto ia=context.get_context_data(a.parms_id())->chain_index(), ib=context.get_context_data(b.parms_id())->chain_index();
        if(ia>ib)e.mod_switch_to_inplace(a,b.parms_id()); else if(ib>ia)e.mod_switch_to_inplace(b,a.parms_id());
        seal::Ciphertext r; e.multiply(a,b,r);e.relinearize_inplace(r,relin);e.rescale_to_next_inplace(r);return r;
    };
    auto sum_slots=[&](seal::Ciphertext a) {
        for(std::size_t k=1;k<slots;k*=2){seal::Ciphertext r;e.rotate_vector(a,int(k),galois,r);e.add_inplace(a,r);}return a;
    };
    seal::Ciphertext sx,sy,sxx,syy,sxy;
    for(std::size_t i=0;i<chunks;++i){
        auto x=inputs[i],y=inputs[chunks+i];
        auto xx=sum_slots(product(x,x)), yy=sum_slots(product(y,y)), xy=sum_slots(product(x,y));
        x=sum_slots(x);y=sum_slots(y);
        if(i==0){sx=x;sy=y;sxx=xx;syy=yy;sxy=xy;}
        else{e.add_inplace(sx,x);e.add_inplace(sy,y);e.add_inplace(sxx,xx);e.add_inplace(syy,yy);e.add_inplace(sxy,xy);}
    }
    auto mx=multiply_constant(context,e,encoder,sx,1.0/count,scale);
    auto my=multiply_constant(context,e,encoder,sy,1.0/count,scale);
    auto mx2=product(mx,mx),my2=product(my,my),mxy=product(mx,my);
    auto vx=multiply_constant(context,e,encoder,sxx,1.0/count,mx2.scale());
    auto vy=multiply_constant(context,e,encoder,syy,1.0/count,my2.scale());
    auto cov=multiply_constant(context,e,encoder,sxy,1.0/count,mxy.scale());
    e.sub_inplace(vx,mx2);e.sub_inplace(vy,my2);e.sub_inplace(cov,mxy);
    verify(cov,7);trace.covariance_level=7;trace.covariance_scale=cov.scale();
    if(!correlation_requested) return {cov};
    auto v=product(vx,vy);
    auto u=multiply_constant(context,e,encoder,v,1.0/10240.0,scale);
    seal::Plaintext minus_one;encoder.encode(-1.0,u.parms_id(),u.scale(),minus_one);e.add_plain_inplace(u,minus_one);
    std::vector<seal::Ciphertext> powers(9);powers[1]=u;
    for(int k=2;k<=8;++k)powers[k]=product(powers[k/2],powers[k-k/2]);
    // All weighted powers finish at one common level and scale.
    seal::Ciphertext polynomial;double coefficient=1.0;
    for(int k=1;k<=8;++k){
        coefficient *= -(2.0*k-1)/(2.0*k);
        e.mod_switch_to_inplace(powers[k],powers[8].parms_id());
        auto term=multiply_constant(context,e,encoder,powers[k],coefficient/std::sqrt(10240.0),scale);
        if(k==1)polynomial=term;else e.add_inplace(polynomial,term);
    }
    seal::Plaintext constant;encoder.encode(1.0/std::sqrt(10240.0),polynomial.parms_id(),scale,constant);e.add_plain_inplace(polynomial,constant);
    auto correlation=product(cov,polynomial);
    verify(correlation,0);trace.correlation_level=0;trace.correlation_scale=correlation.scale();
    return {cov,correlation};
}
}
SumMeanRun measure_relationships(CKKSContext& crypto,const std::vector<double>& x,const std::vector<double>& y, RelationshipTrace* observed){
    RelationshipReference ref{};SumMeanRun run;
    run.times[0]=measure_ms([&]{for(int i=0;i<1000;++i)ref=relationship_reference(x,y);})/1000;
    const bool supported=!ref.undefined && ref.product>=4096 && ref.product<=16384;
    run.status=ref.undefined ? "UNDEFINED" : supported ? "PASS" : "UNSUPPORTED";
    // Owner packs two independent columns; padding never enters the reference statistics.
    const auto chunks=(x.size()+crypto.slots()-1)/crypto.slots();
    std::vector<double> packed=x;packed.resize(chunks*crypto.slots(),0);packed.insert(packed.end(),y.begin(),y.end());
    const auto& context=crypto.context();const auto& galois=crypto.galois_keys();const auto& relin=crypto.relin_keys();
    auto slots=crypto.slots(),count=x.size();double scale=crypto.initial_scale();
    RelationshipTrace trace;
    auto p=measure_pipeline(crypto,packed,[&context,&galois,&relin,chunks,slots,count,scale,supported,&trace](const auto& encrypted){return evaluate(context,galois,relin,encrypted,chunks,slots,count,scale,supported,trace);});
    auto t=run.times[0];run.times=p.times;run.times[0]=t;run.input_ciphertext_bytes=p.input_ciphertext_bytes;
    run.plaintext_sum=ref.covariance;run.plaintext_mean=ref.correlation;run.encrypted_sum=p.decoded[0];run.encrypted_mean=supported ? p.decoded[1] : NAN;
    run.passed=std::isfinite(run.encrypted_sum)&&std::abs(run.encrypted_sum-ref.covariance)<=0.01&&(!supported || (std::isfinite(run.encrypted_mean)&&std::abs(run.encrypted_mean-ref.correlation)<=0.01));
    if(observed)*observed=trace;
    return run;
}
bool relationship_tests(CKKSContext& crypto,const std::string& dir){
    bool pass=true;
    auto check=[&](const std::vector<double>& x,const std::vector<double>& y){
        RelationshipTrace trace;auto r=measure_relationships(crypto,x,y,&trace);
        std::cout<<std::setprecision(14)<<x.size()<<" covariance="<<r.encrypted_sum<<" reference="<<r.plaintext_sum<<" correlation="<<r.encrypted_mean<<" reference="<<r.plaintext_mean<<" status="<<r.status<<" correctness="<<r.passed<<" levels="<<trace.covariance_level<<","<<trace.correlation_level<<" scales="<<trace.covariance_scale<<","<<trace.correlation_scale<<'\n';pass &= r.passed;return r;
    };
    const double expected_cov[]{9.72960910,2.6150262845,1.849911868511};
    const double expected_rho[]{0.081544057300277573,0.021921640724986342,0.015326059603436212};
    int index=0;
    for(int n: std::vector<int>{1000,10000,100000}){
        auto path=dir+"/patients_"+std::to_string(n/1000)+"K.csv";
        auto x=read_numeric_column(path,"heart_rate"),y=read_numeric_column(path,"systolic_bp");
        if(x.size()!=static_cast<std::size_t>(n)||y.size()!=x.size())throw std::runtime_error("Unexpected final dataset count");
        auto r=check(x,y);
        pass &= r.status=="PASS" && std::abs(r.plaintext_sum-expected_cov[index])<1e-9 && std::abs(r.plaintext_mean-expected_rho[index])<1e-12; ++index;
    }
    for(double v:{4096.0,10240.0,16384.0}){
        double a=std::pow(v,0.25);if(v==16384)a=std::nextafter(a,0.0);
        for(int sign:{-1,1}){auto r=check({-a,-a,a,a},{-sign*a,-sign*a,sign*a,sign*a});pass &=r.status=="PASS" && std::abs(r.encrypted_sum-sign*a*a)<=0.01 && std::abs(r.encrypted_mean-sign)<=0.01;}
    }
    for(int n:{4097,8191,8192,8193}){std::vector<double>x(n),y(n);for(int i=0;i<n;++i){x[i]=i%2?10:-10;y[i]=x[i];}auto r=check(x,y);pass &=r.status=="PASS" && std::abs(r.encrypted_mean-1)<=0.01;}
    for(double a:{0.0,1.0,20.0}){auto r=measure_relationships(crypto,{-a,a},{-a,a});pass &= r.status==(a==0 ? "UNDEFINED" : "UNSUPPORTED") && std::isnan(r.encrypted_mean) && r.passed;}
    auto zero=measure_relationships(crypto,{1,1},{-10,10});pass &= zero.status=="UNDEFINED" && zero.passed && std::isnan(zero.encrypted_mean);
    std::cout<<"Relationship fixtures: "<<(pass?"PASS":"FAIL")<<'\n';return pass;
}
