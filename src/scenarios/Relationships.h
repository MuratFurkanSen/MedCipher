#pragma once
#include "SumMean.h"
// Owner-side reference and eligibility checks; never passed to the evaluator.
struct RelationshipReference { double covariance, correlation, product; bool undefined; };
RelationshipReference relationship_reference(const std::vector<double>& x, const std::vector<double>& y);
SumMeanRun measure_relationships(CKKSContext& crypto, const std::vector<double>& x, const std::vector<double>& y);
bool relationship_tests(CKKSContext& crypto, const std::string& data_dir);
