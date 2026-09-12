// 08_compile_time_dispatch.cpp
// ============================================================
// Virtual dispatch vs template (compile-time) dispatch --
// MEASURED, and a note on what to look for in the assembly.
//   virtual   : indirect call through a vtable, NOT inlined
//   template  : concrete type known -> inlined -> just the work
//   std::variant + visit : jump-table dispatch, one indirect call
// ============================================================
//   BENCH: g++ -std=c++20 -O2 08_compile_time_dispatch.cpp -o cd && ./cd
//   ASM  : g++ -std=c++20 -O2 -S -masm=intel 08_compile_time_dispatch.cpp -o - | c++filt | less
// ============================================================

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <variant>
#include <vector>

using Clock = std::chrono::steady_clock;

// ---- strategy: three ways to add a "fee" to a price ----
struct FlatFee     { std::int64_t apply(std::int64_t px) const { return px + 5; } };
struct PercentFee  { std::int64_t apply(std::int64_t px) const { return px + px / 100; } };
struct TieredFee   { std::int64_t apply(std::int64_t px) const { return px + (px > 1000 ? 20 : 8); } };

// ---- (A) virtual ----
struct IFee { virtual ~IFee() = default; virtual std::int64_t apply(std::int64_t px) const = 0; };
struct VFlat    : IFee { std::int64_t apply(std::int64_t px) const override { return px + 5; } };
struct VPercent : IFee { std::int64_t apply(std::int64_t px) const override { return px + px / 100; } };

// ---- (B) template on the strategy type ----
template <class Fee>
std::int64_t runTemplated(const Fee& fee, long iters) {
    std::int64_t acc = 0;
    for (long i = 0; i < iters; ++i) acc += fee.apply(1000 + (i & 0x3ff));
    return acc;
}

// ---- (C) std::variant + std::visit ----
using FeeVar = std::variant<FlatFee, PercentFee, TieredFee>;
std::int64_t runVariant(const FeeVar& fee, long iters) {
    std::int64_t acc = 0;
    for (long i = 0; i < iters; ++i)
        acc += std::visit([&](const auto& f){ return f.apply(1000 + (i & 0x3ff)); }, fee);
    return acc;
}

// ---- (A) driver ----
std::int64_t runVirtual(const IFee& fee, long iters) {
    std::int64_t acc = 0;
    for (long i = 0; i < iters; ++i) acc += fee.apply(1000 + (i & 0x3ff));   // indirect, not inlined
    return acc;
}

int main() {
    const long N = 200'000'000;
    volatile std::int64_t sink = 0;

    auto bench = [&](const char* label, auto&& fn) {
        auto t0 = Clock::now();
        sink += fn();
        auto t1 = Clock::now();
        double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / static_cast<double>(N);
        std::printf("  %-34s %6.2f ns/call\n", label, ns);
    };

    std::printf("=== dispatch cost (%ld calls of fee.apply) ===\n\n", N);

    VPercent vp;
    bench("virtual (vtable indirect call)", [&]{ return runVirtual(vp, N); });

    PercentFee tp;
    bench("template (concrete type, inlined)", [&]{ return runTemplated(tp, N); });

    FeeVar var = PercentFee{};
    bench("std::variant + std::visit", [&]{ return runVariant(var, N); });

    std::printf("  (sink %lld)\n", static_cast<long long>(sink));
    std::printf(
        "\n"
        "  Typical shape:\n"
        "   template : ~0.3-1 ns   (the compiler sees PercentFee::apply -> inlines it;\n"
        "                            often the whole loop vectorizes)\n"
        "   variant  : ~2-6 ns     (std::visit builds a jump table -> one predictable\n"
        "                            indirect call; the callee can still inline INSIDE visit)\n"
        "   virtual  : ~3-8 ns     (load vptr, load slot, indirect call -- NOT inlined,\n"
        "                            and it blocks loop vectorization)\n"
        "\n"
        "  In the assembly (`./build.ps1 asm`): the template version has NO `call` in the\n"
        "  hot loop (just add/idiv); the virtual version has `mov rax,[reg]; call [rax+off]`.\n");
    return 0;
}
