// 04_aos_vs_soa.cpp
// ============================================================
// AoS vs SoA -- struct layout aur cache (measured)
// ============================================================
//   BENCHMARK -> -O2 ZAROORI.
//   g++ -std=c++20 -O2 04_aos_vs_soa.cpp -o aos && ./aos
// ============================================================
// Yeh folder 09 ke 07_aos_vs_soa.cpp ka struct-focused version:
// dikhta hai ki struct ka SIZE (padding samet) AoS scan ki speed
// ko kaise affect karta hai, aur SoA field-subset access pe kyun jeet-ta hai.
// ============================================================

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

constexpr std::size_t N = 4'000'000;
constexpr int REPS = 30;

// AoS: ek "quote" -- 8 fields. Sirf `bid` chahiye to bhi poora struct cache mein aata hai.
struct Quote {
    std::int64_t ts;
    std::int32_t bid;
    std::int32_t ask;
    std::int32_t bidSize;
    std::int32_t askSize;
    std::int16_t venue;
    std::int16_t flags;
    std::int32_t seq;
};   // 32 bytes

// ============================================================
//  KERNELS -- [[gnu::noipa]] kyun?
// ============================================================
// Pehle yeh loops seedha main() mein the: `for (r < REPS) { m = 0; scan; maxAoS = m; }`.
// GCC 16.2 -O2 ne dekha ki sirf AAKHRI rep ka result use hota hai aur scan ka koi
// side effect nahi -> pehle 29 reps DEAD CODE maan ke hata diye. "30 reps" asal mein
// 1 rep naap rahe the (~8 ms). noipa = compiler is function ke andar jhaank ke
// "pure hai, result same hai" wala faisla nahi kar sakta -> har call sach mein chalti hai.
// Sabak: benchmark ka number physically possible hai ya nahi, hamesha check karo.
[[gnu::noipa]] std::int32_t maxBidAoS(const std::vector<Quote>& aos) {
    std::int32_t m = 0;
    for (const Quote& q : aos) if (q.bid > m) m = q.bid;   // stride 32 B -- 28/32 bekaar
    return m;
}

[[gnu::noipa]] std::int32_t maxBidSoA(const std::vector<std::int32_t>& bid) {
    std::int32_t m = 0;
    for (std::int32_t b : bid) if (b > m) m = b;            // contiguous -- poori line kaam ki
    return m;
}

int main() {
    std::vector<Quote> aos(N);
    for (std::size_t i = 0; i < N; ++i) {
        auto v = static_cast<std::int32_t>(i & 0xFF);
        aos[i] = { static_cast<std::int64_t>(i), v, v + 1, 100, 100, 1, 0, static_cast<std::int32_t>(i) };
    }

    // SoA: har field ka apna contiguous array
    struct SoA {
        std::vector<std::int64_t> ts;
        std::vector<std::int32_t> bid, ask, bidSize, askSize, seq;
        std::vector<std::int16_t> venue, flags;
    } soa;
    soa.ts.resize(N); soa.bid.resize(N); soa.ask.resize(N);
    soa.bidSize.assign(N, 100); soa.askSize.assign(N, 100);
    soa.venue.assign(N, 1); soa.flags.assign(N, 0); soa.seq.resize(N);
    for (std::size_t i = 0; i < N; ++i) {
        soa.ts[i] = static_cast<std::int64_t>(i);
        soa.bid[i] = static_cast<std::int32_t>(i & 0xFF);
        soa.ask[i] = soa.bid[i] + 1;
        soa.seq[i] = static_cast<std::int32_t>(i);
    }

    auto ms = [](auto a, auto b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };

    std::cout << "sizeof(Quote) = " << sizeof(Quote) << " bytes ("
              << 64 / sizeof(Quote) << " per cache line)\n";
    std::cout << "N = " << N << ", REPS = " << REPS << "\n\n";

    // ---- TASK: "best bid" ka rolling max nikaalo -- sirf `bid` field chahiye ----
    std::int32_t maxAoS = 0, maxSoA = 0;

    auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < REPS; ++r) maxAoS = maxBidAoS(aos);
    auto t1 = std::chrono::steady_clock::now();

    for (int r = 0; r < REPS; ++r) maxSoA = maxBidSoA(soa.bid);
    auto t2 = std::chrono::steady_clock::now();

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "max(bid) -- ONE field of 8:\n";
    std::cout << "  AoS (vector<Quote>, stride 32 B) : " << std::setw(8) << ms(t0, t1) << " ms\n";
    std::cout << "  SoA (vector<int32>, contiguous)  : " << std::setw(8) << ms(t1, t2) << " ms\n";
    if (ms(t1, t2) > 0.0)
        std::cout << "  SoA " << std::setprecision(2) << (ms(t0, t1) / ms(t1, t2))
                  << "x faster" << std::setprecision(1) << "\n";
    std::cout << "  (results " << (maxAoS == maxSoA ? "match" : "DIFFER") << ")\n";

    std::cout <<
        "\n"
        "  * Struct chhota rakho (padding hataake -- 03_struct_optimization.cpp)\n"
        "    -> AoS scan bhi thoda tez (zyada records per line).\n"
        "  * Par field-SUBSET access pe SoA hamesha jeet-ta hai: har cache line\n"
        "    ka 100% kaam ka data, aur SIMD vectorize aasan.\n"
        "  * Poore quote ki zaroorat (ek quote process karna) -> AoS behtar.\n"
        "  * HFT order books / market-data analytics = SoA-style. Lesson 07, folder 39.\n";

    return 0;
}
