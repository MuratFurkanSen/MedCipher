#pragma once
#include "../scenarios/SumMean.h"

inline bool benchmark_run_acceptable(const SumMeanRun& run, bool relationships) {
    return run.passed && (!relationships || run.status == "PASS");
}
