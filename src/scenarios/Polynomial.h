#pragma once
#include "../benchmark/Pipeline.h"
#include <string>
#include <vector>
class CKKSContext;
struct PolynomialTrace { std::size_t ciphertext_depth{}, rescale_depth{}, input_level{}, output_level{}, multiplications{}, rescalings{}, modulus_switches{}; double output_scale{}; };
struct PolynomialRun { PipelineResult measurement; PolynomialTrace trace; double max_absolute_error{}, mean_absolute_error{}, max_relative_error{}; bool passed{}; };
void polynomial_reference(const std::vector<double>&, int, std::vector<double>&);
PolynomialRun measure_polynomial(CKKSContext&, const std::vector<double>&, int);
bool polynomial_tests(CKKSContext&, const std::string&);
