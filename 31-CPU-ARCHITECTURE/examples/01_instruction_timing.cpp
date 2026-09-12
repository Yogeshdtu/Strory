// 01_instruction_timing.cpp
// ============================================================
// Ek instruction ki "cost" ek number nahi -- latency (result ready hone
// mein kitne cycles) aur throughput (per cycle kitne shuru ho sakte)
// alag hain. Dependency chain se latency, independent ops se throughput.
// ADD vs MUL vs integer DIV compare.
//
// ⚠️ CLAUDE.md Rule 2 note: bina optimization-barrier ke `-O2` in loops ko
// closed-form solve / DCE kar deta tha (ADD chain -> 0.000 ns). `keep()`
// (inline-asm barrier, Google-Benchmark ke DoNotOptimize jaisa) har
// iteration pe value ko register mein "materialize" karwa deta -> asli chain.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 01_instruction_timing.cpp -o instime && ./instime
//   (-O2 REQUIRED -- -O0 pe loop overhead sab kuch chhupa deta)
// ============================================================

#include <cstdint>
#include <cstdio>
#include <chrono>

using Clock = std::chrono::steady_clock;
static constexpr std::uint64_t N = 400'000'000;
static volatile std::uint64_t g_sink;

// optimization barrier: compiler ko lagta `v` change/observe hui -> na DCE,
// na reassociate, na close-form. Zero instructions emit hoti.
template <class T>
static inline void keep(T& v) { asm volatile("" : "+r"(v) : : ); }

// ---- LATENCY: har op pichhle ke result pe depend -> serialize ----
template <class Step>
static double latency_ns(Step step) {
    std::uint64_t x = 1;
    auto t0 = Clock::now();
    for (std::uint64_t i = 0; i < N; ++i) { x = step(x, i); keep(x); }
    auto t1 = Clock::now();
    g_sink = x;
    return std::chrono::duration<double>(t1 - t0).count() * 1e9 / static_cast<double>(N);
}

// ---- THROUGHPUT: 4 independent chains -> CPU inko parallel chala sakta ----
template <class Step>
static double throughput_ns(Step step) {
    std::uint64_t a = 1, b = 2, c = 3, d = 4;
    auto t0 = Clock::now();
    for (std::uint64_t i = 0; i < N / 4; ++i) {
        a = step(a, i); b = step(b, i); c = step(c, i); d = step(d, i);
        keep(a); keep(b); keep(c); keep(d);
    }
    auto t1 = Clock::now();
    g_sink = a ^ b ^ c ^ d;
    return std::chrono::duration<double>(t1 - t0).count() * 1e9 / static_cast<double>(N);
}

int main() {
    std::printf("Per-op time (ns). LATENCY = dependent chain, THROUGHPUT = 4 parallel chains.\n");
    std::printf("Cycle count ~ ns * your GHz (is box ~2 GHz power-save; HFT box locked ~3-5 GHz).\n\n");

    // ADD: latency 1 cycle, throughput ~4/cycle -> throughput ~4x faster
    auto add = [](std::uint64_t x, std::uint64_t i) { return x + i + 1; };
    // MUL (imul r64): latency ~3 cycles, throughput ~1/cycle
    auto mul = [](std::uint64_t x, std::uint64_t i) { return x * (i | 1u); };
    // OR-shift: cheap ALU, similar to add
    auto ors = [](std::uint64_t x, std::uint64_t i) { return (x ^ i) + ((x << 1) | 1u); };
    // integer DIV (div r64): latency ~20-40 cycles, NOT pipelined well
    auto dvi = [](std::uint64_t x, std::uint64_t i) { return x / ((i % 7u) + 3u) + 1u; };

    struct { const char* name; double lat; double thr; } rows[] = {
        { "ADD (x = x + i + 1)",        latency_ns(add), throughput_ns(add) },
        { "OR/SHIFT mix",              latency_ns(ors), throughput_ns(ors) },
        { "MUL (x = x * (i|1))",       latency_ns(mul), throughput_ns(mul) },
        { "DIV (x = x / ((i%7)+3))",   latency_ns(dvi), throughput_ns(dvi) },
    };

    std::printf("  %-26s  %11s  %13s  %s\n", "op", "latency", "throughput", "lat/thr");
    for (auto& r : rows)
        std::printf("  %-26s  %8.3f ns  %10.3f ns  %6.1fx\n",
                    r.name, r.lat, r.thr, r.lat / r.thr);

    std::puts("\nKya seekha:");
    std::puts(" - ADD/OR: latency ~= throughput * (chhota factor). ALU ports bahut hain,");
    std::puts("   1-cycle latency -> dependent chain bhi ~1 cyc/op, independent ~3-4x tez.");
    std::puts(" - MUL: latency ~2-3x throughput -- ek multiplier port, ~3-cycle latency,");
    std::puts("   par har cycle naya mul shuru ho sakta (pipelined). Chain latency-bound.");
    std::puts(" - DIV: latency ~= throughput, aur dono bade -- divider pipelined NAHI");
    std::puts("   (~20-40 cyc), agla div pichhla khatam hone tak wait. Isi liye HFT hot");
    std::puts("   path pe division avoid: reciprocal-multiply, shift (power-of-2), table.");
    std::puts(" - Design rule: latency-critical dependency chain mein sasti short-latency");
    std::puts("   instructions rakho; independent work interleave karo taaki OoO engine");
    std::puts("   ILP nikaal sake (example 02).");
    std::puts(" - Reference: Agner Fog ki instruction tables (agner.org) / uops.info --");
    std::puts("   har x86 instruction ki latency + throughput + kaunse ports.");
    return 0;
}
