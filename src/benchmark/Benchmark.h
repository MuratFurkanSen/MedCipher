#pragma once
#include <string>
class CKKSContext;
bool benchmark_sum_mean(CKKSContext& crypto, const std::string& data_dir, const std::string& output_path);

bool benchmark_variance(CKKSContext& crypto, const std::string& data_dir, const std::string& output_path);

bool benchmark_relationships(CKKSContext&,const std::string&,const std::string&);

bool benchmark_scoring(CKKSContext&,const std::string&,const std::string&);

bool benchmark_polynomial(CKKSContext&,const std::string&,const std::string&);
