#include "crypto/ckks_smoke.h"

#include <exception>
#include <iostream>

int main() {
    try {
        return run_ckks_smoke_test() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "CKKS smoke test failed: " << error.what() << '\n';
        return 1;
    }
}
