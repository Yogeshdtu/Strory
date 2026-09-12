// 05_fixed_point.cpp
// ============================================================
// PRICES AS INTEGERS. HFT mein price kabhi `double` mein mat rakho:
//   - 0.10 double mein exactly represent NAHI hota -> 3*0.10 != 0.30
//   - `==` price comparisons silently fail
//   - accumulation error badhta rehta hai
//   - float compare/convert x86 pe integer se dheema (aur non-assoc)
//
// Fix: price = (integer) * (fixed scale). Equity ticks = 0.01 -> scale 100.
//   "100.05"  ->  10005   (int64, exact)
//
// Yeh file dikhata:
//   1. float ka rounding bug (measured, exact)
//   2. fixed-point parse ("123.45" -> 12345) bina kisi float ke
//   3. fixed-point format (12345 -> "123.45")
//   4. arithmetic cost: int64 add/compare vs double add/compare (measured)
//   5. notional = price*qty -- scale ka dhyaan (double-scale ho jaata)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 05_fixed_point.cpp -o fp && ./fp
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace ch = std::chrono;
using Clock = ch::steady_clock;
template <class T> static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

// ---- fixed-point price: int64, scale 100 (equity tick 0.01) ----
using Px = std::int64_t;
constexpr std::int64_t kScale = 100;

// parse "PPPP.pp" (exactly 2 fractional digits) -> Px, no float
static Px parse_px(const char* s, std::size_t len) {
    Px whole = 0;
    std::size_t i = 0;
    for (; i < len && s[i] != '.'; ++i) whole = whole * 10 + (s[i] - '0');
    Px frac = 0;
    if (i < len && s[i] == '.') {
        // exactly 2 digits after '.'
        frac = (s[i + 1] - '0') * 10 + (s[i + 2] - '0');
    }
    return whole * kScale + frac;
}

// format Px -> "PPPP.pp" into buf (must hold >= 24). returns length.
static int format_px(Px p, char* buf) {
    const Px whole = p / kScale;
    const Px frac  = p % kScale;
    return std::snprintf(buf, 24, "%lld.%02lld",
                         static_cast<long long>(whole), static_cast<long long>(frac));
}

int main() {
    // ---------------------------------------------------------
    // 1. THE FLOAT BUG -- exact, reproducible
    // ---------------------------------------------------------
    std::puts("=== 1. why not double ===");
    {
        double d = 0.0;
        for (int k = 0; k < 10; ++k) d += 0.1;      // "should" be 1.0
        std::printf("  double: 0.1 added 10x = %.20f   (== 1.0 ? %s)\n",
                    d, (d == 1.0) ? "yes" : "NO");

        Px f = 0;
        for (int k = 0; k < 10; ++k) f += 10;        // 0.10 in scale-100
        char b[24]; format_px(f, b);
        std::printf("  fixed : 0.10 added 10x = %s   (== 1.00 ? %s)\n",
                    b, (f == 100) ? "yes" : "NO");

        // classic: is 100.10 the same price twice?
        const double p1 = 100.1, p2 = 1001.0 / 10.0;
        std::printf("  double: 100.1 vs 1001.0/10.0 equal? %s\n", (p1 == p2) ? "yes" : "NO");
        std::printf("  fixed : parse(\"100.10\") vs parse(\"100.10\") equal? %s\n",
                    (parse_px("100.10", 6) == parse_px("100.10", 6)) ? "yes" : "NO");
    }

    // ---------------------------------------------------------
    // 2 + 3. parse / format round-trip
    // ---------------------------------------------------------
    std::puts("\n=== 2/3. parse + format round-trip ===");
    {
        const char* samples[] = {"0.00", "1.05", "99.99", "100.00", "12345.67"};
        for (const char* s : samples) {
            const Px p = parse_px(s, std::strlen(s));
            char b[24]; format_px(p, b);
            std::printf("  \"%s\" -> %lld -> \"%s\"   %s\n",
                        s, static_cast<long long>(p), b,
                        (std::strcmp(s, b) == 0) ? "ok" : "MISMATCH");
        }
    }

    // ---------------------------------------------------------
    // 4. arithmetic cost -- int64 vs double  (add + compare in a hot loop)
    // ---------------------------------------------------------
    std::puts("\n=== 4. arithmetic cost (measured, this box) ===");
    {
        constexpr std::size_t N = 1u << 24;   // 16M
        std::vector<Px>     ip(N);
        std::vector<double> dp(N);
        for (std::size_t i = 0; i < N; ++i) {
            ip[i] = static_cast<Px>((i * 2654435761u) % 2000000);   // 0..20000.00
            dp[i] = static_cast<double>(ip[i]) / static_cast<double>(kScale);
        }

        auto bench = [](auto f, int reps = 20) {
            double best = 1e300;
            for (int r = 0; r < reps; ++r) {
                auto t0 = Clock::now();
                auto v = f();
                auto t1 = Clock::now();
                keep(v);
                best = std::min(best, static_cast<double>(
                    ch::duration_cast<ch::nanoseconds>(t1 - t0).count()));
            }
            return best;
        };

        // "count how many prices exceed a moving reference" -- add + compare
        const double di = bench([&] {
            std::int64_t c = 0, ref = 500000;
            for (std::size_t i = 0; i < N; ++i) { ref += ip[i] - 499999; if (ip[i] > ref) ++c; }
            return c;
        });
        const double dd = bench([&] {
            std::int64_t c = 0; double ref = 5000.0;
            for (std::size_t i = 0; i < N; ++i) { ref += dp[i] - 4999.99; if (dp[i] > ref) ++c; }
            return c;
        });
        std::printf("  int64  add+compare : %6.3f ns/elem\n", di / static_cast<double>(N));
        std::printf("  double add+compare : %6.3f ns/elem\n", dd / static_cast<double>(N));
        std::puts("  (dono -O2 pe vectorize ho sakte -> farak chhota. Asli faayda");
        std::puts("   CORRECTNESS + deterministic `==` hai, sirf speed nahi.)");
    }

    // ---------------------------------------------------------
    // 5. notional = price * qty -- SCALE ka dhyaan
    // ---------------------------------------------------------
    std::puts("\n=== 5. notional = price * qty (scale discipline) ===");
    {
        const Px  price = parse_px("100.05", 6);   // 10005  (scale 100)
        const std::int64_t qty = 250;
        const std::int64_t notional_scaled = price * qty;   // scale still 100
        char b[24]; format_px(notional_scaled, b);
        std::printf("  100.05 * 250 = %lld (scaled)  = \"%s\"  (i.e. 25012.50)\n",
                    static_cast<long long>(notional_scaled), b);
        std::puts("  Rule: price*qty -> scale WAHI (100). price*price -> scale 10000");
        std::puts("  (dobara /kScale karo). Har multiply ke baad scale track karo.");
    }

    std::puts("\nKya seekha:");
    std::puts(" - double prices = latent correctness bug. int64 fixed-point = exact,");
    std::puts("   `==` deterministic, overflow headroom int64 mein bahut (9.2e18).");
    std::puts(" - parse/format bina float ke -- 2 fractional digits fixed layout.");
    std::puts(" - Speed bonus chhota/variable hai; asli jeet correctness. Pipeline");
    std::puts("   (03_optimized_pipeline.cpp) isi Px type pe chalti.");
    return 0;
}
