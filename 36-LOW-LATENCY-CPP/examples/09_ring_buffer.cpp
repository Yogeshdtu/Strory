// 09_ring_buffer.cpp
// ============================================================
// SPSC ring buffer — the clean final form. (Folder 28/01–02 ne poori
// optimization ladder dikhaya: naive -> +padding -> +cached index. Yahan
// woh final version + push/pop latency distribution + "kab ring galat tool".)
//
//   Design choices, sab yahan:
//    - capacity POWER OF TWO -> index wrap = `& mask` (no `%`, no branch)
//    - head_/tail_ alag cache lines pe (`alignas(64)`) -> no false sharing
//    - producer apne paas consumer ka index CACHE karta -> har push pe
//      consumer ki cache line nahi padhta (yeh ladder ka asli ~1.5x win)
//    - release-store / acquire-load of the two indices -> no CAS, no lock,
//      wait-free in practice; monotonic counters -> no ABA
//
//   Yeh single-thread demo hai (producer aur consumer interleaved) taaki
//   MinGW pe portably chale + latency measure ho. Multi-thread correctness
//   folder 28 mein (TSan + stress). Yahan focus = per-op cost + wrap logic.
//
// Trade-off (lesson 24): ring = bounded, SPSC only (MPMC = Vyukov, folder 28),
// full hone pe producer ko DROP/backpressure decide karna, aur ek "queue" hi
// latency add karti (producer->consumer hop). Direct call sasta hai jab
// decoupling ki zaroorat na ho.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 09_ring_buffer.cpp -o r && ./r
// ============================================================

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <new>
#include <vector>
#include <x86intrin.h>

namespace ch = std::chrono;
template <class T> static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }
static inline std::uint64_t tsc() { _mm_lfence(); auto t = __rdtsc(); _mm_lfence(); return t; }
static double g_tpns = 1.0;

#ifdef __cpp_lib_hardware_interference_size
constexpr std::size_t CL = std::hardware_destructive_interference_size;
#else
constexpr std::size_t CL = 64;
#endif

// ============================================================
//  SpscRing<T, Cap>  (Cap must be a power of two)
// ============================================================
template <class T, std::size_t Cap>
class SpscRing {
    static_assert((Cap & (Cap - 1)) == 0, "Cap must be a power of two");
public:
    bool try_push(const T& v) noexcept {
        const std::size_t h = head_.load(std::memory_order_relaxed);
        if (h - cached_tail_ >= Cap) {                        // maybe full — refresh cache
            cached_tail_ = tail_.load(std::memory_order_acquire);
            if (h - cached_tail_ >= Cap) return false;        // really full
        }
        buf_[h & (Cap - 1)] = v;
        head_.store(h + 1, std::memory_order_release);
        return true;
    }
    bool try_pop(T& out) noexcept {
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        if (t == cached_head_) {                              // maybe empty — refresh cache
            cached_head_ = head_.load(std::memory_order_acquire);
            if (t == cached_head_) return false;              // really empty
        }
        out = buf_[t & (Cap - 1)];
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }

private:
    alignas(CL) std::atomic<std::size_t> head_{0};
    std::size_t cached_tail_{0};                              // producer-private
    alignas(CL) std::atomic<std::size_t> tail_{0};
    std::size_t cached_head_{0};                              // consumer-private
    alignas(CL) std::array<T, Cap> buf_{};
};

int main() {
    {
        auto c0 = ch::steady_clock::now();
        std::uint64_t r0 = tsc();
        volatile std::uint64_t s = 0;
        while (ch::duration_cast<ch::milliseconds>(ch::steady_clock::now() - c0).count() < 120) s = s + 1;
        std::uint64_t r1 = tsc();
        auto c1 = ch::steady_clock::now();
        g_tpns = static_cast<double>(r1 - r0)
               / static_cast<double>(ch::duration_cast<ch::nanoseconds>(c1 - c0).count());
    }
    std::printf("ticks_per_ns = %.4f   sizeof(ring) = %zu\n\n", g_tpns,
                sizeof(SpscRing<std::uint64_t, 1024>));

    constexpr std::size_t CAP = 1024;
    auto ring = std::make_unique<SpscRing<std::uint64_t, CAP>>();
    constexpr int N = 500000;

    std::vector<double> push_lat, pop_lat;
    push_lat.reserve(N); pop_lat.reserve(N);

    // interleave: push a batch, pop a batch — keeps the ring ~half full
    std::uint64_t sink = 0;
    std::uint64_t v = 1;
    for (int round = 0; round < N / 256; ++round) {
        for (int i = 0; i < 256; ++i) {
            std::uint64_t t0 = tsc();
            bool ok = ring->try_push(v++);
            std::uint64_t t1 = tsc();
            keep(ok);
            push_lat.push_back(static_cast<double>(t1 - t0) / g_tpns);
        }
        for (int i = 0; i < 256; ++i) {
            std::uint64_t out = 0;
            std::uint64_t t0 = tsc();
            bool ok = ring->try_pop(out);
            std::uint64_t t1 = tsc();
            keep(ok);
            sink ^= out;
            pop_lat.push_back(static_cast<double>(t1 - t0) / g_tpns);
        }
    }
    keep(sink);

    auto rep = [](const char* tag, std::vector<double>& x) {
        std::sort(x.begin(), x.end());
        auto pc = [&](double p) {
            std::size_t i = static_cast<std::size_t>(p / 100.0 * static_cast<double>(x.size()));
            return x[std::min(i, x.size() - 1)];
        };
        std::printf("  %-10s p50 %5.2f   p99 %5.2f   p99.9 %6.2f   max %7.2f  ns\n",
                    tag, pc(50), pc(99), pc(99.9), x.back());
    };
    rep("try_push", push_lat);
    rep("try_pop", pop_lat);

    std::puts("\nKya seekha:");
    std::puts(" - Steady-state push/pop = a few ns: one relaxed load, `& mask` index,");
    std::puts("   a store, a release-store. No `%`, no branch mispredict, no alloc.");
    std::puts(" - cached_tail_/cached_head_: producer consumer ki atomic ko sirf tab");
    std::puts("   padhta jab uska cached view 'full/empty' kehta — normally kabhi nahi.");
    std::puts("   Yeh folder 28/02 ka ~1.5x win (cross-core cache-line read hataana).");
    std::puts(" - alignas(CL) on head_/tail_/buf_ -> teenon alag cache lines -> zero");
    std::puts("   false sharing between producer and consumer (measured ~3.5-4x in");
    std::puts("   folder 28/07 and ~6-44x in folder 32/04).");
    std::puts(" - Trade-off: bounded (drop/backpressure on full); SPSC only; a queue");
    std::puts("   hop itself costs — use a ring to DECOUPLE, not by reflex.");
    return 0;
}
