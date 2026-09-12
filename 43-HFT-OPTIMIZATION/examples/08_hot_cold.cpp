// 08_hot_cold.cpp
// ============================================================
// HOT/COLD PATH separation -- ek feed-handler-shaped dispatch loop.
//
//   Hot path  (~99% messages): add / cancel / modify / trade -> book update.
//                              chhota, straight-line, har tick.
//   Cold path (~1% messages):  snapshot-req / gap-recovery / admin / error /
//                              session-reset -> BULKY handlers (log format,
//                              recovery-table walk, buffer clear...).
//
//   Version A: cold handlers INLINE in the switch (hot loop body ke andar)
//   Version B: cold handlers [[gnu::cold]][[gnu::noinline]] + [[unlikely]]
//              -> compiler unhe .text.unlikely (alag, door) mein daalta;
//                 hot loop ka code contiguous + dense rehta.
//
// 36/12 ne CHHOTE cold body pe A~B dikhaya tha (hot loop L1i mein fit).
// Yahan hot path bada + cold handlers bhaari -- dekho ab farak dikhta ya
// nahi. Jo bhi ho, MEASURED number likhenge (Rule 2). Asli trigger:
// `perf stat` mein high "Frontend Bound" / iTLB / L1i-miss (lesson 06).
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 08_hot_cold.cpp -o hc && ./hc
//   asm:  g++ -std=c++20 -O2 -S -masm=intel 08_hot_cold.cpp -o - | c++filt
//         (B mein cold_* functions .text.unlikely section mein -- hot loop se door)
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

constexpr std::size_t kLevels = 256;

struct Book {
    std::int64_t qty[kLevels];
    std::int64_t checksum;
    std::int64_t ewma;
    std::int64_t cold_sink;
};

// ---- HOT: normal book update (chhota, har message) -------------
static inline void hot_update(Book& bk, std::uint8_t type, std::uint32_t a, std::uint32_t b) {
    const std::size_t lvl = a & (kLevels - 1);
    switch (type) {
        case 0: bk.qty[lvl] += b;                 break;   // add
        case 1: bk.qty[lvl] -= (bk.qty[lvl] ? 1 : 0) * static_cast<std::int64_t>(b & 7); break; // cancel
        case 2: bk.qty[lvl]  = static_cast<std::int64_t>(b); break;   // modify
        default: {                                                     // trade
            const std::int64_t fill = bk.qty[lvl] > 0 ? 1 : 0;
            bk.qty[lvl] -= fill * static_cast<std::int64_t>(b & 15);
            if (bk.qty[lvl] < 0) bk.qty[lvl] = 0;
        } break;
    }
    // some straight-line hot arithmetic (checksum + EWMA) -- har message
    const std::uint64_t h = static_cast<std::uint64_t>(bk.checksum) * 1099511628211ULL
                          ^ static_cast<std::uint64_t>(bk.qty[lvl] + type);
    bk.checksum = static_cast<std::int64_t>(h);
    bk.ewma += (bk.qty[lvl] - bk.ewma) >> 6;
}

// ---- COLD: bulky rare handlers --------------------------------
// (A) inline flavour -- forced inline into the hot loop
static inline std::int64_t cold_body_inline(std::uint8_t type, std::uint32_t seed) {
    char buf[96];
    std::int64_t acc = type;
    for (int k = 0; k < 64; ++k) {                     // "format a log line"
        seed = seed * 1664525u + 1013904223u;
        buf[k % 96] = static_cast<char>('0' + (seed & 15));
        acc += buf[k % 96] + (acc << 3);
    }
    std::memset(buf, 0, sizeof buf);                   // "clear scratch"
    acc ^= static_cast<std::int64_t>(buf[0]);
    return acc;
}
// (B) outlined flavour -- one per cold type, cold+noinline
[[gnu::cold, gnu::noinline]] static std::int64_t cold_snapshot(std::uint32_t s) { return cold_body_inline(4, s); }
[[gnu::cold, gnu::noinline]] static std::int64_t cold_recovery(std::uint32_t s) { return cold_body_inline(5, s); }
[[gnu::cold, gnu::noinline]] static std::int64_t cold_admin   (std::uint32_t s) { return cold_body_inline(6, s); }
[[gnu::cold, gnu::noinline]] static std::int64_t cold_error   (std::uint32_t s) { return cold_body_inline(7, s); }
[[gnu::cold, gnu::noinline]] static std::int64_t cold_reset   (std::uint32_t s) { return cold_body_inline(8, s); }

[[gnu::noinline]]
static std::int64_t run_inline(const std::uint8_t* ty, const std::uint32_t* a,
                               const std::uint32_t* b, std::size_t n) {
    Book bk{};
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint8_t t = ty[i];
        if (t < 4) {
            hot_update(bk, t, a[i], b[i]);
        } else {
            switch (t) {                               // cold -- INLINE, in the hot loop
                case 4: bk.cold_sink += cold_body_inline(4, a[i]); break;
                case 5: bk.cold_sink += cold_body_inline(5, a[i]); break;
                case 6: bk.cold_sink += cold_body_inline(6, a[i]); break;
                case 7: bk.cold_sink += cold_body_inline(7, a[i]); break;
                default: bk.cold_sink += cold_body_inline(8, a[i]); break;
            }
        }
    }
    return bk.checksum ^ bk.ewma ^ bk.cold_sink;
}

[[gnu::noinline]]
static std::int64_t run_outlined(const std::uint8_t* ty, const std::uint32_t* a,
                                 const std::uint32_t* b, std::size_t n) {
    Book bk{};
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint8_t t = ty[i];
        if (t < 4) [[likely]] {
            hot_update(bk, t, a[i], b[i]);
        } else [[unlikely]] {
            switch (t) {                               // cold -- OUT-OF-LINE calls
                case 4:  bk.cold_sink += cold_snapshot(a[i]); break;
                case 5:  bk.cold_sink += cold_recovery(a[i]); break;
                case 6:  bk.cold_sink += cold_admin   (a[i]); break;
                case 7:  bk.cold_sink += cold_error   (a[i]); break;
                default: bk.cold_sink += cold_reset   (a[i]); break;
            }
        }
    }
    return bk.checksum ^ bk.ewma ^ bk.cold_sink;
}

template <class F>
static double bench(F f, std::size_t n, int reps = 30) {
    double best = 1e300;
    for (int r = 0; r < reps; ++r) {
        auto t0 = Clock::now();
        auto v = f();
        auto t1 = Clock::now();
        keep(v);
        best = std::min(best, static_cast<double>(ch::duration_cast<ch::nanoseconds>(t1 - t0).count()));
    }
    return best / static_cast<double>(n);
}

int main() {
    constexpr std::size_t N = 8u << 20;    // 8M messages
    std::vector<std::uint8_t>  ty(N);
    std::vector<std::uint32_t> a(N), b(N);
    std::uint64_t x = 12345;
    std::size_t cold = 0;
    for (std::size_t i = 0; i < N; ++i) {
        x = x * 6364136223846793005ULL + 1;
        const std::uint32_t r = static_cast<std::uint32_t>(x >> 33);
        // ~1% cold (types 4..8), rest hot (0..3)
        if ((r % 100u) == 0u) { ty[i] = static_cast<std::uint8_t>(4 + (r % 5u)); ++cold; }
        else                  { ty[i] = static_cast<std::uint8_t>(r & 3u); }
        a[i] = r;
        b[i] = static_cast<std::uint32_t>(x >> 20);
    }
    std::printf("messages: %zu   cold: %zu (%.2f%%)\n\n",
                N, cold, 100.0 * static_cast<double>(cold) / static_cast<double>(N));

    const double ti = bench([&] { return run_inline  (ty.data(), a.data(), b.data(), N); }, N);
    const double to = bench([&] { return run_outlined(ty.data(), a.data(), b.data(), N); }, N);

    std::printf("A cold INLINE in hot loop        : %6.3f ns/msg\n", ti);
    std::printf("B cold [[gnu::cold]] + [[unlikely]]: %6.3f ns/msg   (%.1f%% %s)\n",
                to, 100.0 * (ti - to) / ti, (to < ti) ? "faster" : "slower/same");

    std::puts("\nKya seekha (measured, is box):");
    if (to < ti * 0.97) {
        std::puts(" - B tez: cold handlers .text.unlikely mein chale gaye -> hot loop");
        std::puts("   ka code contiguous + chhota -> frontend (L1i/uop-cache/fetch)");
        std::puts("   ko kam kaam -> hot path faster.");
    } else {
        std::puts(" - A ~ B is box pe (Rule 2). Hot loop ka code itna bada nahi hua");
        std::puts("   ki L1i/uop-cache se nikle. 36/12 ne bhi yahi dekha tha.");
        std::puts(" - Concept sahi hai: BADE hot paths mein (poora feed handler +");
        std::puts("   strategy, hazaaron instructions) jahan `perf stat` 'Frontend");
        std::puts("   Bound' high dikhaye -- wahan hot/cold split + PGO/BOLT (lesson");
        std::puts("   06) 10-30% dete. Chhote demo pe farak chhupa nahi.");
    }
    std::puts(" - Attribute hamesha lagao (cost 0): intent document hota + bade");
    std::puts("   binary mein compiler ko layout hint milta. `./build.ps1 asm` se");
    std::puts("   confirm karo: B mein cold_* loop ke code se door hai.");
    return 0;
}
