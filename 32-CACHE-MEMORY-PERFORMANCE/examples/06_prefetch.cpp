// 06_prefetch.cpp
// ============================================================
// Manual software prefetch: `__builtin_prefetch(addr, rw, locality)`.
//
// HW prefetcher SIRF predictable patterns pakadta (monotonic stride ek
// page ke andar). INDIRECT / gather access -- `sum += table[idx[i]]` jahan
// idx[] random hai -- woh andha rehta.
//
// Do scenario -- aur dono ka nateeja ⚠️ Rule 2 hai (measured, expectation se ulta):
//   S1: light gather (1 int per lookup). Loads independent -> OoO engine
//       KHUD ~10 misses in-flight rakhta (MLP) -> pehle se ~9 ns/lookup ->
//       SW prefetch sirf ~1.1x deta (marginal).
//   S2: heavy per-lookup (64-B bucket scan + dependent mix). Socha tha ROB
//       bhar jaane se MLP girega aur prefetch jeetega. ULTA hua: bucket ke
//       16 independent loads memory system ko waise hi saturate kar dete
//       (~3 ns/lookup, S1 se TEZ!), aur upar se SW prefetch daalna LFB/
//       bandwidth ke liye ladta -> ~3x DHEEMA.
//
// Sabak: bade OoO core pe SW prefetch "free speedup" NAHI hai -- aksar
// marginal, kabhi harmful. Woh ek scalpel hai (neeche kab).
// D (prefetch distance) sweep bhi -- sweet spot measure karke.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 06_prefetch.cpp -o pf && ./pf
// ============================================================

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

using Clock = std::chrono::steady_clock;

template <class T>
static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

constexpr std::size_t TABLE = 64u << 20;        // 64 Mi ints * 4B = 256 MiB
constexpr std::size_t NIDX  = 4u << 20;         // 4 Mi lookups
constexpr int         REPS  = 10;

static inline std::uint64_t mix(std::uint64_t h) {
    h ^= h >> 33; h *= 0xff51afd7ed558ccdull;
    h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ull;
    h ^= h >> 33;
    return h;
}

// ---- S1: light gather ----
template <int D>
static std::uint64_t g1(const std::vector<std::int32_t>& tab,
                        const std::vector<std::uint32_t>& idx) {
    std::uint64_t s = 0;
    const std::size_t n = idx.size();
    for (std::size_t i = 0; i < n; ++i) {
        if constexpr (D > 0) if (i + D < n) __builtin_prefetch(&tab[idx[i + D]], 0, 0);
        s += static_cast<std::uint64_t>(tab[idx[i]]);
    }
    return s;
}

// ---- S2: heavy gather -- 64-B bucket scan + dependent mix ----
template <int D>
static std::uint64_t g2(const std::vector<std::int32_t>& tab,
                        const std::vector<std::uint32_t>& idx) {
    std::uint64_t s = 0;
    const std::size_t n = idx.size();
    for (std::size_t i = 0; i < n; ++i) {
        if constexpr (D > 0) if (i + D < n) __builtin_prefetch(&tab[(idx[i + D] & ~15u)], 0, 0);
        const std::size_t base = idx[i] & ~static_cast<std::uint32_t>(15);  // line-aligned
        std::uint64_t acc = 0;
        for (int k = 0; k < 16; ++k) acc += static_cast<std::uint64_t>(tab[base + static_cast<std::size_t>(k)]);
        s = mix(s ^ acc);                       // dependent on previous iter
    }
    return s;
}

int main() {
    std::vector<std::int32_t> tab(TABLE);
    for (std::size_t i = 0; i < TABLE; ++i) tab[i] = static_cast<std::int32_t>(i * 2654435761u);

    std::vector<std::uint32_t> idx(NIDX);
    std::mt19937 rng(2024);
    std::uniform_int_distribution<std::uint32_t> d(0, TABLE - 1);
    for (auto& v : idx) v = d(rng);

    auto bench = [&](auto fn) {
        std::uint64_t acc = fn(tab, idx);        // warm
        keep(acc);
        auto t0 = Clock::now();
        for (int r = 0; r < REPS; ++r) acc += fn(tab, idx);
        auto t1 = Clock::now();
        keep(acc);
        return std::chrono::duration<double>(t1 - t0).count() * 1e9
             / (static_cast<double>(NIDX) * REPS);
    };

    std::puts("=== S1: light gather  s += table[idx[i]]   (256 MiB table) ===");
    const double p1_0  = bench([](const auto& t, const auto& x){ return g1<0>(t, x); });
    const double p1_16 = bench([](const auto& t, const auto& x){ return g1<16>(t, x); });
    const double p1_32 = bench([](const auto& t, const auto& x){ return g1<32>(t, x); });
    const double p1_64 = bench([](const auto& t, const auto& x){ return g1<64>(t, x); });
    std::printf("  no prefetch : %6.2f ns/lookup\n", p1_0);
    std::printf("  D=16 : %6.2f  (%.2fx)   D=32 : %6.2f  (%.2fx)   D=64 : %6.2f  (%.2fx)\n",
                p1_16, p1_0 / p1_16, p1_32, p1_0 / p1_32, p1_64, p1_0 / p1_64);

    std::puts("\n=== S2: heavy gather  16-int bucket scan + dependent mix() ===");
    const double p2_0  = bench([](const auto& t, const auto& x){ return g2<0>(t, x); });
    const double p2_16 = bench([](const auto& t, const auto& x){ return g2<16>(t, x); });
    const double p2_32 = bench([](const auto& t, const auto& x){ return g2<32>(t, x); });
    const double p2_64 = bench([](const auto& t, const auto& x){ return g2<64>(t, x); });
    std::printf("  no prefetch : %6.2f ns/lookup\n", p2_0);
    std::printf("  D=16 : %6.2f  (%.2fx)   D=32 : %6.2f  (%.2fx)   D=64 : %6.2f  (%.2fx)\n",
                p2_16, p2_0 / p2_16, p2_32, p2_0 / p2_32, p2_64, p2_0 / p2_64);

    std::puts("\nKya hua (measured, Rule 2):");
    std::puts(" - S1: address idx[i] se -> HW prefetcher andha. PAR loads ek doosre");
    std::puts("   pe depend nahi -> OoO engine ~10 misses ek saath in-flight rakhta");
    std::puts("   (MLP) -> effective ~9 ns (true DRAM ~80 ns se bahut kam). SW");
    std::puts("   prefetch bas thoda aur overlap -> ~1.1x. Marginal.");
    std::puts(" - S2: bucket ke 16 sequential loads + inter-iter mix chain. Umeed");
    std::puts("   thi MLP giregi -> prefetch jeetega. ULTA: 16 independent loads");
    std::puts("   memory ko waise hi bhar dete (~3 ns/lookup), aur SW prefetch");
    std::puts("   requests LFB/bandwidth ke liye demand loads se ladte -> ~3x SLOW.");
    std::puts(" - Sabak: SW prefetch bada OoO core pe 'free' nahi. Woh tab jeetta:");
    std::puts("   (a) in-order / chhota-OoO core (embedded, purane), (b) verified");
    std::puts("   MLP-deficit (perf: low mem-parallelism), (c) HW prefetcher ki");
    std::puts("   blind-spot cross karni ho (page boundary, non-monotonic). Warna");
    std::puts("   pehle contiguous/SoA/blocking try karo.");
    std::puts(" - D chhota -> line time pe nahi aati; D bahut bada -> use se pehle");
    std::puts("   evict / pollution. Hamesha measure.");
    return 0;
}
