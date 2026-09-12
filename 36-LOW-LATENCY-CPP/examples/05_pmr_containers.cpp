// 05_pmr_containers.cpp
// ============================================================
// std::pmr — STL containers ko ek custom memory_resource do, bina apna
// allocator likhe. Hot-path use:
//
//   std::pmr::monotonic_buffer_resource on a STACK buffer
//     -> std::pmr::vector / unordered_map / string ki saari allocation
//        us buffer se -> steady state mein ZERO global `new`
//
//   std::pmr::null_memory_resource()
//     -> "yahan koi allocation nahi honi chahiye" ka RUNTIME assert
//        (allocate() -> std::bad_alloc throw)
//
// Trade-off:
//  - monotonic = no per-element free (arena jaisa); container clear pe
//    memory reclaim nahi hoti jab tak resource release/rewind na ho
//  - pmr containers ka type `std::pmr::vector<T>` hai (alag from std::vector<T>);
//    ek virtual call per allocation (usually inlined/cheap vs the alloc itself)
//  - buffer overflow -> upstream resource (yahan null -> throw; ya new_delete)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 05_pmr_containers.cpp -o pmr && ./pmr
// ============================================================

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory_resource>
#include <vector>
#include <x86intrin.h>

namespace ch = std::chrono;
template <class T> static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }
static inline std::uint64_t tsc() { _mm_lfence(); auto t = __rdtsc(); _mm_lfence(); return t; }
static double g_tpns = 1.0;

// ---- a counting resource: how many times did we hit the heap? ----
class CountingResource : public std::pmr::memory_resource {
public:
    explicit CountingResource(std::pmr::memory_resource* up) : up_(up) {}
    std::size_t allocs() const { return allocs_; }
private:
    void* do_allocate(std::size_t b, std::size_t a) override { ++allocs_; return up_->allocate(b, a); }
    void do_deallocate(void* p, std::size_t b, std::size_t a) override { up_->deallocate(p, b, a); }
    bool do_is_equal(const std::pmr::memory_resource& o) const noexcept override { return this == &o; }
    std::pmr::memory_resource* up_;
    std::size_t allocs_ = 0;
};

static void report(const char* tag, std::vector<double>& ns) {
    std::sort(ns.begin(), ns.end());
    auto pc = [&](double p) {
        std::size_t i = static_cast<std::size_t>(p / 100.0 * static_cast<double>(ns.size()));
        return ns[std::min(i, ns.size() - 1)];
    };
    std::printf("  %-30s p50 %5.1f   p99 %6.1f   p99.9 %7.1f   max %8.1f  ns\n",
                tag, pc(50), pc(99), pc(99.9), ns.back());
}

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

    constexpr int MSGS = 200000;
    constexpr int PER_MSG = 24;                              // ints appended per message

    // ---- baseline: std::vector (global new/delete) -------------
    {
        CountingResource cr(std::pmr::new_delete_resource());
        (void)cr;
        std::size_t heap_hits = 0;
        std::vector<double> lat; lat.reserve(MSGS);
        for (int m = 0; m < MSGS; ++m) {
            std::uint64_t t0 = tsc();
            std::vector<int> v;                              // fresh -> will malloc on first push
            for (int i = 0; i < PER_MSG; ++i) v.push_back(m + i);
            long acc = 0; for (int x : v) acc += x;
            std::uint64_t t1 = tsc();
            keep(acc);
            if (v.capacity()) ++heap_hits;
            lat.push_back(static_cast<double>(t1 - t0) / g_tpns);
        }
        report("std::vector<int> (new/delete)", lat);
        std::printf("      -> ~%zu messages each hit the heap (vector growth)\n\n", heap_hits);
    }

    // ---- pmr::vector on a stack monotonic buffer ---------------
    {
        alignas(64) std::array<std::byte, 1 << 14> buf{};    // 16 KB on the stack
        std::pmr::monotonic_buffer_resource mono(buf.data(), buf.size(),
                                                 std::pmr::null_memory_resource());
        CountingResource cr(&mono);
        std::vector<double> lat; lat.reserve(MSGS);
        for (int m = 0; m < MSGS; ++m) {
            std::uint64_t t0 = tsc();
            std::pmr::vector<int> v(&cr);
            v.reserve(PER_MSG);                              // one alloc from `mono` (bump)
            for (int i = 0; i < PER_MSG; ++i) v.push_back(m + i);
            long acc = 0; for (int x : v) acc += x;
            std::uint64_t t1 = tsc();
            keep(acc);
            mono.release();                                  // rewind the arena -> reuse buf
            lat.push_back(static_cast<double>(t1 - t0) / g_tpns);
        }
        report("pmr::vector on stack buffer", lat);
        std::printf("      -> %zu allocations total, all from the stack buffer, 0 global new\n\n",
                    cr.allocs());
    }

    // ---- null_memory_resource as a no-alloc guard --------------
    {
        alignas(64) std::array<std::byte, 256> tiny{};
        std::pmr::monotonic_buffer_resource mono(tiny.data(), tiny.size(),
                                                 std::pmr::null_memory_resource());
        std::pmr::vector<int> v(&mono);
        bool threw = false;
        try {
            for (int i = 0; i < 100000; ++i) v.push_back(i);  // will overflow 256 B -> null -> throw
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        std::printf("null_memory_resource guard: overflow -> std::bad_alloc thrown = %s\n",
                    threw ? "yes (good — caught a hidden allocation)" : "NO");
    }

    std::puts("\nKya seekha:");
    std::puts(" - pmr::vector + monotonic_buffer_resource on a stack buffer = STL");
    std::puts("   ergonomics + arena performance: 0 global `new` steady state, flat tail.");
    std::puts(" - `mono.release()` per message = arena reset (buffer reuse).");
    std::puts(" - null_memory_resource upstream = a runtime tripwire: any allocation");
    std::puts("   you didn't expect -> std::bad_alloc, caught in test/CI.");
    std::puts(" - Trade-off: monotonic = no per-element free; the pmr indirection is");
    std::puts("   one (usually inlined) virtual call per allocation, dwarfed by the alloc.");
    return 0;
}
