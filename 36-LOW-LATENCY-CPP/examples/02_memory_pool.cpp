// 02_memory_pool.cpp
// ============================================================
// FIXED-SIZE memory pool — build it, benchmark its LATENCY DISTRIBUTION
// vs `new`/`delete`. (Folder 14/07 ne ise banaya; yahan hot-path lens:
// p50/p99/p99.9/max, aur trade-offs.)
//
//   Pool: ek baar bada arena allocate karo, N fixed slots, free-list
//   (har free slot ke andar next ka pointer). allocate/deallocate = O(1),
//   no syscall, no lock (per-thread), no branch mispredict.
//
//   Trade-offs:
//    - ek hi block size; capacity FIXED (full -> nullptr, tumhe handle karna)
//    - thread-safe nahi -> per-thread pool
//    - freed memory OS ko wapas nahi jaati (design hai, bug nahi)
//    - random free order -> slots cache mein bikhar sakte (yahan measure)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 02_memory_pool.cpp -o p && ./p
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <random>
#include <vector>
#include <x86intrin.h>

namespace ch = std::chrono;
template <class T> static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }
static inline std::uint64_t tsc() { _mm_lfence(); auto t = __rdtsc(); _mm_lfence(); return t; }
static double g_tpns = 1.0;

static void report(const char* tag, std::vector<double>& ns) {
    std::sort(ns.begin(), ns.end());
    auto pc = [&](double p) {
        std::size_t i = static_cast<std::size_t>(p / 100.0 * static_cast<double>(ns.size()));
        return ns[std::min(i, ns.size() - 1)];
    };
    std::printf("  %-24s p50 %5.1f   p99 %6.1f   p99.9 %7.1f   max %8.1f  ns\n",
                tag, pc(50), pc(99), pc(99.9), ns.back());
}

// ============================================================
//  FixedPool — O(1) alloc/free, intrusive free list
// ============================================================
class FixedPool {
public:
    FixedPool(std::size_t block, std::size_t count)
        : block_(std::max(block, sizeof(void*))),
          count_(count),
          arena_(static_cast<std::byte*>(::operator new(block_ * count_,
                                                        std::align_val_t{alignof(std::max_align_t)}))) {
        free_ = nullptr;
        for (std::size_t i = count_; i-- > 0;) {
            void* slot = arena_ + i * block_;
            std::memcpy(slot, &free_, sizeof free_);       // *(void**)slot = free_
            free_ = slot;
        }
    }
    ~FixedPool() { ::operator delete(arena_, std::align_val_t{alignof(std::max_align_t)}); }
    FixedPool(const FixedPool&) = delete;
    FixedPool& operator=(const FixedPool&) = delete;

    void* allocate() noexcept {
        if (!free_) return nullptr;                        // FULL — caller must handle
        void* p = free_;
        std::memcpy(&free_, p, sizeof free_);              // free_ = *(void**)p
        return p;
    }
    void deallocate(void* p) noexcept {
        if (!p) return;
        std::memcpy(p, &free_, sizeof free_);
        free_ = p;
    }

private:
    std::size_t block_, count_;
    std::byte* arena_;
    void* free_;
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
    std::printf("ticks_per_ns = %.4f\n\n", g_tpns);

    constexpr int N = 200000;
    constexpr std::size_t BLK = 64;
    constexpr std::size_t CAP = 4096;
    std::mt19937 rng(777);

    constexpr std::size_t TARGET = CAP / 2;                 // steady-state live count

    // ---- baseline: new/delete, steady state = free one + alloc one --
    {
        std::vector<double> lat; lat.reserve(N);
        std::vector<std::byte*> live; live.reserve(TARGET + 1);
        for (int i = 0; i < N; ++i) {
            if (live.size() >= TARGET) {
                std::size_t k = rng() % live.size();
                delete[] live[k]; live[k] = live.back(); live.pop_back();
            }
            std::uint64_t t0 = tsc();
            auto* p = new std::byte[BLK];
            std::uint64_t t1 = tsc();
            p[0] = std::byte{1}; keep(p); live.push_back(p);
            lat.push_back(static_cast<double>(t1 - t0) / g_tpns);
        }
        for (auto* p : live) delete[] p;
        report("new[64] (churn)", lat);
    }

    // ---- FixedPool, same access pattern -------------------------
    {
        FixedPool pool(BLK, CAP);
        std::vector<double> lat; lat.reserve(N);
        std::vector<void*> live; live.reserve(TARGET + 1);
        for (int i = 0; i < N; ++i) {
            if (live.size() >= TARGET) {
                std::size_t k = rng() % live.size();
                pool.deallocate(live[k]); live[k] = live.back(); live.pop_back();
            }
            std::uint64_t t0 = tsc();
            void* p = pool.allocate();
            std::uint64_t t1 = tsc();
            if (!p) { report("POOL FULL (unexpected)", lat); return 1; }
            *static_cast<std::byte*>(p) = std::byte{1}; keep(p); live.push_back(p);
            lat.push_back(static_cast<double>(t1 - t0) / g_tpns);
        }
        report("FixedPool::allocate", lat);
    }

    std::puts("\nKya seekha:");
    std::puts(" - Pool ka p50 ~ new ke barabar (dono free-list ka head lete),");
    std::puts("   PAR p99.9/max naatakiya kam — koi mmap, coalesce, size-class,");
    std::puts("   ya arena lock nahi. Tail FLAT ho jaati.");
    std::puts(" - Yeh HFT ka core pattern: Order/Event objects ek per-thread");
    std::puts("   FixedPool se, capacity worst-case ke liye size ki hui.");
    std::puts(" - Trade-off: pool FULL hone pe kya? (drop / backpressure / assert).");
    std::puts("   Capacity ki sizing ek design decision hai, guess nahi.");
    return 0;
}
