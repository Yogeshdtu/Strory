// silent_b.cxx — SAME name "Config", ALAG layout (extra field + reordered)
// aur `describe` ki ALAG body. Dono TUs link ho jaati hain (koi error nahi).
// ============================================================
#include <cstdio>

struct Config {
    long rate;          // int -> long (alag size/offset!)
    long depth;
    long extra;         // b ke paas ek extra field
    bool verbose;
};

inline int describe(const Config& c) { return static_cast<int>(c.rate * c.depth); }  // body B

void run_b() {
    Config c{100, 5, 0, true};
    std::printf("B: sizeof(Config)=%zu describe=%d\n", sizeof(Config),
                describe(c));
}
