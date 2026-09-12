// 07_dispatch_comparison.cpp
// ============================================================
// Runtime polymorphism ke 5 tareeke, ek jagah MEASURED. (Folder 16/07 +
// 21/08 ne alag-alag dikhaye; yeh synthesis + homogeneous vs heterogeneous.)
//
//   1. virtual         : vtable indirect call, NOT inlined
//   2. CRTP            : static dispatch, fully inlined
//   3. std::variant    : visit -> jump table, one indirect call, body inlined
//   4. function table   : array<fn-ptr>, indirect call, NOT inlined
//   5. tag + switch     : plain switch on an enum, bodies inlined
//
//   Do workloads:
//     HOMOGENEOUS   — saare elements ek hi concrete type (predictor loves it)
//     HETEROGENEOUS — types interleaved (indirect-call predictor thrashes)
//
// Trade-off:
//  - virtual/fn-table: open set (plugins), par indirect call + no inline
//  - CRTP: fastest, par har concrete type ka apna code (bloat) + closed set
//  - variant/switch: closed set known at compile time, near-CRTP speed,
//    open to "add a case" without touching call sites... sort of
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 07_dispatch_comparison.cpp -o d && ./d
// ============================================================

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <random>
#include <utility>
#include <variant>
#include <vector>

namespace ch = std::chrono;
using Clock = ch::steady_clock;
template <class T> static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

// ---- the operation: 3 "fee" strategies ------------------------
static inline std::int64_t flat_fee (std::int64_t px) { return px + 5; }
static inline std::int64_t pct_fee  (std::int64_t px) { return px + px / 100; }
static inline std::int64_t tier_fee (std::int64_t px) { return px + (px > 1000 ? 20 : 8); }

// 1. virtual
struct IFee { virtual ~IFee() = default; virtual std::int64_t apply(std::int64_t) const = 0; };
struct VFlat : IFee { std::int64_t apply(std::int64_t px) const override { return flat_fee(px); } };
struct VPct  : IFee { std::int64_t apply(std::int64_t px) const override { return pct_fee(px);  } };
struct VTier : IFee { std::int64_t apply(std::int64_t px) const override { return tier_fee(px); } };

// 2. CRTP
template <class D> struct FeeBase { std::int64_t apply(std::int64_t px) const {
    return static_cast<const D*>(this)->do_apply(px); } };
struct CFlat : FeeBase<CFlat> { std::int64_t do_apply(std::int64_t px) const { return flat_fee(px); } };

// 3. variant
struct SFlat { std::int64_t apply(std::int64_t px) const { return flat_fee(px); } };
struct SPct  { std::int64_t apply(std::int64_t px) const { return pct_fee(px);  } };
struct STier { std::int64_t apply(std::int64_t px) const { return tier_fee(px); } };
using FeeVar = std::variant<SFlat, SPct, STier>;

// 4. function table
using FeeFn = std::int64_t (*)(std::int64_t);
static constexpr std::array<FeeFn, 3> FEE_TABLE{ flat_fee, pct_fee, tier_fee };

// 5. tag + switch
enum class Tag : std::uint8_t { Flat, Pct, Tier };
static inline std::int64_t apply_tag(Tag t, std::int64_t px) {
    switch (t) {
        case Tag::Flat: return flat_fee(px);
        case Tag::Pct:  return pct_fee(px);
        case Tag::Tier: return tier_fee(px);
    }
    return px;
}

template <class F>
static double bench(std::size_t n, F f, int reps = 20) {
    double best = 1e300;
    for (int r = 0; r < reps; ++r) {
        auto t0 = Clock::now();
        std::int64_t acc = f();
        auto t1 = Clock::now();
        keep(acc);
        best = std::min(best, static_cast<double>(ch::duration_cast<ch::nanoseconds>(t1 - t0).count()));
    }
    return best / static_cast<double>(n);
}

int main() {
    constexpr std::size_t N = 1u << 22;                       // 4M
    std::mt19937 rng(11);

    std::vector<std::int64_t> px(N);
    for (auto& v : px) v = 100 + (rng() % 4000);

    // type selector per element: HOMO (all 0) vs HETERO (random 0..2)
    std::vector<std::uint8_t> homo(N, 0), hetero(N);
    for (auto& v : hetero) v = static_cast<std::uint8_t>(rng() % 3);

    auto make_virtual = [](std::uint8_t k) -> std::unique_ptr<IFee> {
        if (k == 0) return std::make_unique<VFlat>();
        if (k == 1) return std::make_unique<VPct>();
        return std::make_unique<VTier>();
    };

    for (const auto& [name, sel] : { std::pair{"HOMOGENEOUS", std::cref(homo)},
                                     std::pair{"HETEROGENEOUS", std::cref(hetero)} }) {
        std::printf("=== %s ===\n", name);
        const auto& s = sel.get();

        // build the polymorphic arrays for this selector
        std::vector<std::unique_ptr<IFee>> vobjs(N);
        for (std::size_t i = 0; i < N; ++i) vobjs[i] = make_virtual(s[i]);
        std::vector<FeeVar> vvars(N);
        for (std::size_t i = 0; i < N; ++i)
            vvars[i] = s[i] == 0 ? FeeVar{SFlat{}} : s[i] == 1 ? FeeVar{SPct{}} : FeeVar{STier{}};
        std::vector<Tag> tags(N);
        for (std::size_t i = 0; i < N; ++i)
            tags[i] = s[i] == 0 ? Tag::Flat : s[i] == 1 ? Tag::Pct : Tag::Tier;

        std::printf("  virtual        : %6.3f ns/call\n", bench(N, [&]{
            std::int64_t a = 0;
            for (std::size_t i = 0; i < N; ++i) { a += vobjs[i]->apply(px[i]); }
            return a; }));
        std::printf("  variant+visit  : %6.3f ns/call\n", bench(N, [&]{
            std::int64_t a = 0;
            for (std::size_t i = 0; i < N; ++i) {
                a += std::visit([&](const auto& fee){ return fee.apply(px[i]); }, vvars[i]);
            }
            return a; }));
        std::printf("  fn-table       : %6.3f ns/call\n", bench(N, [&]{
            std::int64_t a = 0;
            for (std::size_t i = 0; i < N; ++i) { a += FEE_TABLE[s[i]](px[i]); }
            return a; }));
        std::printf("  tag+switch     : %6.3f ns/call\n", bench(N, [&]{
            std::int64_t a = 0;
            for (std::size_t i = 0; i < N; ++i) { a += apply_tag(tags[i], px[i]); }
            return a; }));
        std::printf("  CRTP (mono)    : %6.3f ns/call   (single type only — reference floor)\n", bench(N, [&]{
            CFlat fee;
            std::int64_t a = 0;
            for (std::size_t i = 0; i < N; ++i) { a += fee.apply(px[i]); }
            return a; }));
        std::puts("");
    }

    std::puts("Kya seekha:");
    std::puts(" - HOMOGENEOUS: indirect-call predictor har call same target dekhta ->");
    std::puts("   virtual/fn-table bhi theek chalte, par body inline nahi -> CRTP/switch");
    std::puts("   phir bhi aage (inlined arithmetic + vectorize scope).");
    std::puts(" - HETEROGENEOUS: virtual/fn-table ka indirect call MISPREDICT karta har");
    std::puts("   type-change pe -> saaf slowdown. variant/switch bhi ek jump-table");
    std::puts("   hit karte par body inlined -> kam nuksaan.");
    std::puts(" - CRTP = floor (single type). Multi-type chahiye -> variant/switch best");
    std::puts("   balance (closed set). virtual = tab jab set genuinely open ho (plugins).");
    return 0;
}
