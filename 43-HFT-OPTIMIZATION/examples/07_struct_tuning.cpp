// 07_struct_tuning.cpp
// ============================================================
// STRUCT LAYOUT tuning -- teen alag cheezein, sab measured:
//
//   1. PADDING: field order galat -> compiler padding daalta -> struct
//      bada -> kam elements per cache line. Reorder karo (bade fields
//      pehle) -> chhota struct, zero code change.
//
//   2. HOT/COLD FIELDS: ek hot scan sirf 1-2 fields chhuta hai, par
//      fat struct (timestamps, flags, ids) ki poori cache line load hoti.
//      Hot fields ko aage group karo / ya alag array (SoA).
//
//   3. AoS vs SoA: hot field ko apne array mein rakho -> scan sirf
//      us field ke bytes chhuta -> kam memory traffic -> tez.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 07_struct_tuning.cpp -o st && ./st
//   padding dekhne ke liye:  g++ ... -Wpadded 07_struct_tuning.cpp
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace ch = std::chrono;
using Clock = ch::steady_clock;
template <class T> static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

// ---------------------------------------------------------------
// 1. PADDING -- same 3 fields, do orders
// ---------------------------------------------------------------
struct PadBad  { char  side; double price; char  flag; };   // char|pad7|double|char|pad7 = 24
struct PadGood { double price; char side; char flag; };      // double|char|char|pad6      = 16

// ---------------------------------------------------------------
// 2/3. hot scan: har order ke `px`+`qty` aggregate karo (e.g. book pe
// ek pass jo total notional-ish nikaalta). PREDICATE nahi rakha -- pure
// streaming reduction -- taaki teenon versions vectorize hon aur SIRF
// memory layout / traffic compare ho (branch-vs-branchless alag lesson: 36/06).
// ---------------------------------------------------------------
struct FatOrder {                 // realistic "everything" order record -- 1 cache line
    std::int32_t  px;             // <-- the ONLY hot field for this scan
    std::int32_t  qty;
    std::uint64_t id;
    std::uint64_t ts_recv;
    std::uint64_t ts_book;
    std::uint32_t participant;
    std::uint32_t flags;
    std::uint64_t prev, next;     // intrusive list links
    std::uint64_t reserved;       // pad to 64
};

struct SlimOrder {               // only what a price scan needs
    std::int32_t px;
    std::int32_t qty;
};                               // 8 bytes

[[gnu::noinline]]
static std::int64_t scan_fat(const FatOrder* __restrict v, std::size_t n) {
    std::int64_t c = 0;
    for (std::size_t i = 0; i < n; ++i) c += v[i].px + v[i].qty;
    return c;
}
[[gnu::noinline]]
static std::int64_t scan_slim(const SlimOrder* __restrict v, std::size_t n) {
    std::int64_t c = 0;
    for (std::size_t i = 0; i < n; ++i) c += v[i].px + v[i].qty;
    return c;
}
[[gnu::noinline]]
static std::int64_t scan_soa(const std::int32_t* __restrict px,
                             const std::int32_t* __restrict qty, std::size_t n) {
    std::int64_t c = 0;
    for (std::size_t i = 0; i < n; ++i) c += px[i] + qty[i];
    return c;
}

template <class F>
static double bench(F f, std::size_t n, int reps = 40) {
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
    std::printf("=== 1. padding (field order only) ===\n");
    std::printf("  sizeof(PadBad)  = %zu bytes   (char, double, char)\n", sizeof(PadBad));
    std::printf("  sizeof(PadGood) = %zu bytes   (double, char, char)\n", sizeof(PadGood));
    std::printf("  -> %zu%% smaller, zero behaviour change. -Wpadded se warning milti.\n\n",
                100 - (sizeof(PadGood) * 100 / sizeof(PadBad)));

    constexpr std::size_t N = 1u << 22;   // 4M orders
    std::vector<FatOrder>  fat(N);
    std::vector<SlimOrder> slim(N);

    // SoA: dono field-arrays. Ek allocation, qty ko +17 int offset pe (dono
    // ko exactly 2^k bytes apart rakhne se kabhi-kabhi cache-set aliasing
    // hota -- yahan is box pe farak nahi pada, par offset safe-side hai).
    std::vector<std::int32_t> soa(2 * N + 64);
    std::int32_t* px  = soa.data();
    std::int32_t* qty = soa.data() + N + 17;

    std::uint64_t x = 99;
    for (std::size_t i = 0; i < N; ++i) {
        x = x * 6364136223846793005ULL + 1;
        const std::int32_t p = static_cast<std::int32_t>((x >> 40) & 0xFFFF);
        const std::int32_t q = 1 + static_cast<std::int32_t>((x >> 24) & 0xFF);
        fat[i].px = p;  fat[i].qty = q;  fat[i].id = i;  fat[i].ts_recv = x;
        slim[i].px = p; slim[i].qty = q;
        px[i] = p;      qty[i] = q;
    }

    std::printf("=== 2/3. hot aggregate-scan over %zu orders (reads px + qty) ===\n", N);
    std::printf("  sizeof: FatOrder=%zu  SlimOrder=%zu  SoA/elem=%zu\n",
                sizeof(FatOrder), sizeof(SlimOrder), sizeof(std::int32_t) * 2);
    const double tf = bench([&] { return scan_fat (fat.data(),  N); }, N);
    const double ts = bench([&] { return scan_slim(slim.data(), N); }, N);
    const double to = bench([&] { return scan_soa (px, qty, N); }, N);
    std::printf("  AoS fat  (64 B/elem) : %6.3f ns/elem\n", tf);
    std::printf("  AoS slim ( 8 B/elem) : %6.3f ns/elem   (%.1fx vs fat)\n", ts, tf / ts);
    std::printf("  SoA      ( 8 B/elem) : %6.3f ns/elem   (%.1fx vs fat)\n", to, tf / to);

    std::puts("\nKya seekha (measured, is box):");
    std::puts(" - Fat struct pe scan MEMORY-BOUND hai: har element 64 B khinchta");
    std::puts("   jabki kaam ke sirf 8 B. Slim/SoA 8 B/elem -> ~8x kam traffic ->");
    std::puts("   utna hi tez (jab tak L2/L3 se aa raha ho).");
    std::puts(" - Field REORDER (padding kill) free hai -- bas bade fields pehle.");
    std::puts(" - SlimOrder ya SoA: production book mein 'hot' fields (px, qty, next)");
    std::puts("   ko ek cache line mein rakho; ts/flags/strings ko alag 'cold' struct");
    std::puts("   mein (id se link). 39-ORDER-BOOK V3 aur pipeline.hpp yahi karti.");
    std::puts(" - Ulta bhi ho sakta: agar aksar SAARE fields chahiye (random-access,");
    std::puts("   ek order at a time) to AoS behtar -- ek line, ek miss. Measure.");
    return 0;
}
