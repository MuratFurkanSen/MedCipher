#include "WeightedScoring.h"
void scoring_reference(const ScoreColumns& c, std::vector<double>& out) {
    for (std::size_t i=0; i<out.size(); ++i)
        out[i]=static_cast<double>((25.L*c[0][i]+30.L*c[1][i]+30.L*c[2][i]+15.L*c[3][i])/100.L);
}
