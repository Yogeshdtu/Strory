// 04_exception_cost.cpp
// ============================================================
// "Zero-cost exceptions" ko MEASURE karo:
//   1. Happy path (kuch throw nahi) — try/catch hone ka overhead ≈ 0
//   2. Throw path — ek throw+catch ki asli keemat (nanoseconds mein)
//   3. Realistic 0.1% error rate — kya throw tab bhi theek hai?
// Return-code version se side-by-side comparison.
// ============================================================
//   BENCHMARK => -O2 ZAROORI:
//   g++ -std=c++20 -O2 04_exception_cost.cpp -o ec && ./ec
//   (-O0 pe numbers bekaar — sab kuch 10x dheema)
// ============================================================

#include <cstdio>
#include <cstdint>
#include <vector>
#include <chrono>
#include <stdexcept>

using Clock = std::chrono::steady_clock;
static double ms_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// ---- "kaam": negative input = error ----
struct BadValue : std::runtime_error {
    long v;
    explicit BadValue(long x) : std::runtime_error("bad value"), v(x) {}
};

// exception style
static inline std::int64_t process_throw(std::int64_t x) {
    if (x < 0) throw BadValue(x);
    return x * 2 + 1;
}
// return-code style ([[nodiscard]] taaki caller check bhoole na)
[[nodiscard]] static inline bool process_code(std::int64_t x, std::int64_t& out) {
    if (x < 0) return false;
    out = x * 2 + 1;
    return true;
}

// data banao: `bad_every` = kitne mein se 1 negative (0 => koi negative nahi)
static std::vector<std::int64_t> make_data(std::size_t n, std::size_t bad_every) {
    std::vector<std::int64_t> d(n);
    std::uint64_t s = 0x9E3779B97F4A7C15ull;
    for (std::size_t i = 0; i < n; ++i) {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;          // xorshift
        std::int64_t val = static_cast<std::int64_t>(s % 1000);
        if (bad_every && (i % bad_every == 0)) val = -val - 1;   // negative
        d[i] = val;
    }
    return d;
}

int main() {
    std::printf("%-34s %14s %14s\n", "benchmark", "total", "per-iter");
    std::puts("-----------------------------------------------------------------");

    // ========================================================
    //  BENCH 1 — HAPPY PATH (0 errors). try/catch overhead?
    // ========================================================
    {
        const std::size_t N = 20'000'000;
        auto data = make_data(N, 0);              // koi negative nahi
        std::uint64_t sink = 0;
        std::size_t errs = 0;

        auto t0 = Clock::now();
        for (std::size_t i = 0; i < N; ++i) {
            try {
                sink += static_cast<std::uint64_t>(process_throw(data[i]));
            } catch (const BadValue&) { ++errs; }
        }
        double e_ms = ms_since(t0);

        auto t1 = Clock::now();
        for (std::size_t i = 0; i < N; ++i) {
            std::int64_t out;
            if (process_code(data[i], out)) sink += static_cast<std::uint64_t>(out);
            else ++errs;
        }
        double c_ms = ms_since(t1);

        std::printf("%-34s %11.2f ms %9.3f ns\n", "1. happy: try/catch (0 throws)",
                    e_ms, e_ms * 1e6 / static_cast<double>(N));
        std::printf("%-34s %11.2f ms %9.3f ns\n", "1. happy: return-code",
                    c_ms, c_ms * 1e6 / static_cast<double>(N));
        std::printf("   -> ratio %.2fx   (~1.0 = zero-cost model: try region free jab throw nahi hota)\n",
                    e_ms / c_ms);
        std::printf("   [sink=%llu errs=%zu]\n\n",
                    static_cast<unsigned long long>(sink), errs);
    }

    // ========================================================
    //  BENCH 2 — THROW PATH (har iteration error). Ek throw+catch = ?
    // ========================================================
    {
        const std::size_t N = 200'000;           // chhota: har iter throw = mehnga
        auto data = make_data(N, 1);             // sab negative
        std::uint64_t sink = 0;
        std::size_t errs = 0;

        auto t0 = Clock::now();
        for (std::size_t i = 0; i < N; ++i) {
            try { sink += static_cast<std::uint64_t>(process_throw(data[i])); }
            catch (const BadValue& b) { errs += static_cast<std::size_t>(b.v < 0); }
        }
        double e_ms = ms_since(t0);

        auto t1 = Clock::now();
        for (std::size_t i = 0; i < N; ++i) {
            std::int64_t out;
            if (process_code(data[i], out)) sink += static_cast<std::uint64_t>(out);
            else ++errs;
        }
        double c_ms = ms_since(t1);

        std::printf("%-34s %11.2f ms %9.1f ns\n", "2. throw: every iteration",
                    e_ms, e_ms * 1e6 / static_cast<double>(N));
        std::printf("%-34s %11.2f ms %9.3f ns\n", "2. return-code: every iteration",
                    c_ms, c_ms * 1e6 / static_cast<double>(N));
        std::printf("   -> ek throw+catch ~= %.0f ns; return-code ~= %.2f ns; ratio ~%.0fx\n",
                    e_ms * 1e6 / static_cast<double>(N),
                    c_ms * 1e6 / static_cast<double>(N),
                    (e_ms / c_ms));
        std::printf("   [sink=%llu errs=%zu]\n\n",
                    static_cast<unsigned long long>(sink), errs);
    }

    // ========================================================
    //  BENCH 3 — REALISTIC 0.1% error rate
    // ========================================================
    {
        const std::size_t N = 20'000'000;
        auto data = make_data(N, 1000);          // 0.1% negative
        std::uint64_t sink = 0;
        std::size_t errs = 0;

        auto t0 = Clock::now();
        for (std::size_t i = 0; i < N; ++i) {
            try { sink += static_cast<std::uint64_t>(process_throw(data[i])); }
            catch (const BadValue&) { ++errs; }
        }
        double e_ms = ms_since(t0);

        auto t1 = Clock::now();
        for (std::size_t i = 0; i < N; ++i) {
            std::int64_t out;
            if (process_code(data[i], out)) sink += static_cast<std::uint64_t>(out);
            else ++errs;
        }
        double c_ms = ms_since(t1);

        std::printf("%-34s %11.2f ms %9.3f ns\n", "3. 0.1% errors: try/catch",
                    e_ms, e_ms * 1e6 / static_cast<double>(N));
        std::printf("%-34s %11.2f ms %9.3f ns\n", "3. 0.1% errors: return-code",
                    c_ms, c_ms * 1e6 / static_cast<double>(N));
        std::printf("   -> ratio %.2fx   (throw abhi bhi theek hai jab error VAKAI durlabh ho)\n",
                    e_ms / c_ms);
        std::printf("   [sink=%llu errs=%zu]\n\n",
                    static_cast<unsigned long long>(sink), errs);
    }

    std::puts("Nateeja (is machine pe, GCC 15 -O2):");
    std::puts(" - Happy path: try/catch vs return-code = ~1.0x. try region MUFT jab");
    std::puts("   throw nahi hota (Itanium zero-cost / table-based EH).");
    std::puts(" - Ek throw+catch ~= 6000+ ns. Ek return-code check ~= 1.5 ns. Yaani");
    std::puts("   throw ~4000x mehnga. Unwinder .eh_frame tables padhta hai, dtors");
    std::puts("   chalata hai, RTTI se handler match karta hai — sab cold code, aur");
    std::puts("   pehli baar page/cache miss bhi.");
    std::puts(" - 0.1% error rate pe try/catch abhi bhi theek (~5x, ~8 ns/iter).");
    std::puts("   Fail rate jitna zyada, throw utna hi zeher.");
    std::puts(" - Rule: exceptions 'exceptional' ke liye. Hot loop mein EXPECTED");
    std::puts("   failure (parse miss, order reject, lookup miss) => return-code / expected.");
    return 0;
}
