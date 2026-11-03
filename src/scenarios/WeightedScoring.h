#pragma once
#include <array>
#include <string>
#include <vector>
#include "../benchmark/Pipeline.h"
class CKKSContext;
using ScoreColumns = std::array<std::vector<double>, 4>;
struct ScoringRun { PipelineResult measurement; double max_absolute_error{}, mean_absolute_error{}, max_relative_error{}; bool passed{}; };
ScoreColumns load_score_columns(const std::string& path);
void scoring_reference(const ScoreColumns&, std::vector<double>&);
ScoringRun measure_scoring(CKKSContext&, const ScoreColumns&);
bool scoring_tests(CKKSContext&, const std::string&);
