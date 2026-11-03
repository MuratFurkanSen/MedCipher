#include "Polynomial.h"
void polynomial_reference(const std::vector<double>& z, int degree, std::vector<double>& out) {
    for(std::size_t i=0;i<z.size();++i) {
        long double v=0;
        for(int k=degree;k>=0;--k) v=v*static_cast<long double>(z[i])+1.0L/(1u<<k);
        out[i]=static_cast<double>(v);
    }
}
