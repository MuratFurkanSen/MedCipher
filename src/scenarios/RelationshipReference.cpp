#include "Relationships.h"
#include <cmath>
#include <stdexcept>
RelationshipReference relationship_reference(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.empty() || x.size()!=y.size()) throw std::invalid_argument("Paired inputs must have equal nonzero length");
    for(std::size_t i=0;i<x.size();++i) if(!std::isfinite(x[i]) || !std::isfinite(y[i])) throw std::invalid_argument("Nonfinite relationship input");
    long double mx=0,my=0;
    for (std::size_t i=0;i<x.size();++i) { mx+=x[i]; my+=y[i]; }
    mx/=x.size(); my/=x.size();
    long double vx=0,vy=0,c=0;
    for (std::size_t i=0;i<x.size();++i) { auto a=x[i]-mx,b=y[i]-my; vx+=a*a; vy+=b*b; c+=a*b; }
    vx/=x.size();vy/=x.size();c/=x.size();
    bool zero=vx==0 || vy==0;
    return {double(c), zero ? NAN : double(c/std::sqrt(vx*vy)), double(vx*vy),zero};
}
