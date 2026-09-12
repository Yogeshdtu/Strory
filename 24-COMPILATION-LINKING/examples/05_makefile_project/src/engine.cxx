#include "engine.hpp"
#include <cstdio>

namespace engine {
int run() {
    const std::string sym = "ACME";
    std::printf("fnv1a(\"%s\")   = %lld\n", sym.c_str(),
                static_cast<long long>(fnv1a(sym)));
    std::printf("reverse(\"%s\") = %s\n", sym.c_str(), reverse(sym).c_str());
    return 0;
}
}  // namespace engine
