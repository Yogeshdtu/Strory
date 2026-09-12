// silent_a.cxx — "Config" struct ka ek version (3 fields)
// ============================================================
// DOOSRI TU (silent_b.cxx) mein SAME NAME "Config" ka ALAG layout hai.
// Yeh bhi ODR violation hai (class ki do alag definitions), par linker
// ise pakadta NAHI -> silent. `-flto -Wodr` ya different sizes se hi
// pata chalta hai. Runtime pe garbage / crash.
// ============================================================
#include <cstdio>

struct Config {
    int  rate;
    int  depth;
    bool verbose;
};

inline int describe(const Config& c) { return c.rate + c.depth; }   // body A

void run_a() {
    Config c{100, 5, true};
    std::printf("A: sizeof(Config)=%zu describe=%d\n", sizeof(Config), describe(c));
}
