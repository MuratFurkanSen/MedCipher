#pragma once
#include "SumMean.h"
// Owner-side reference and eligibility checks; never passed to the evaluator.
struct RelationshipReference { double covariance, correlation, product; bool undefined; };
RelationshipReference relationship_reference(const std::vector<double>& x, const std::vector<double>& y);
struct RelationshipTrace { std::size_t covariance_level{}, correlation_level{}; double covariance_scale{}, correlation_scale{}; };
SumMeanRun measure_relationships(CKKSContext& crypto, const std::vector<double>& x, const std::vector<double>& y, RelationshipTrace* trace = nullptr);
bool relationship_tests(CKKSContext& crypto, const std::string& data_dir);
