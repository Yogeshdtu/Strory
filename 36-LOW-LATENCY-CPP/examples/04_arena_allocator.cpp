// 04_arena_allocator.cpp
// ============================================================
// BUMP / ARENA allocator — sabse tez allocation jo hoti hai:
//   allocate(n) = pointer ko align karo + n aage badhao  (~2-3 instructions)
//   reset()     = pointer ko shuru pe wapas               (1 instruction)
//   free(p)     = HAI HI NAHI  <-- yeh trade-off hai
//
// Pattern: ek "scope" ke liye jitni memory chahiye ek arena se lo, scope
// khatam -> `reset()`. HFT mein: per-message ya per-tick arena. Parse ke
// dauran jitne temp objects bane, sab arena mein; message process hone ke
// baad ek reset, sab gaya.
//
// Trade-off:
//  - individual free nahi -> lifetimes arena ke scope se bandhe
//  - non-trivial destructors chalane padte to alag se track karo
//  - overflow pe kya? (fallback arena / assert / drop)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 04_arena_allocator.cpp -o ar && ./ar
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
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
    std::printf("  %-22s p50 %5.1f   p99 %6.1f   p99.9 %7.1f   max %8.1f  ns\n",
                tag, pc(50), pc(99), pc(99.9), ns.back());
}

// ============================================================
//  Arena — monotonic bump allocator over a fixed buffer
// ============================================================
class Arena {
public:
    Arena(std::byte* buf, std::size_t cap) noexcept : begin_(buf), cur_(buf), end_(buf + cap) {}

    void* allocate(std::size_t n, std::size_t align = alignof(std::max_align_t)) noexcept {
        std::uintptr_t p = reinterpret_cast<std::uintptr_t>(cur_);
        std::uintptr_t aligned = (p + (align - 1)) & ~(align - 1);
        std::byte* out = reinterpret_cast<std::byte*>(aligned);
        if (out + n > end_) return nullptr;                 // OVERFLOW — caller handles
        cur_ = out + n;
        return out;
    }
    template <class T, class... Args>
    T* make(Args&&... args) noexcept {
        void* m = allocate(sizeof(T), alignof(T));
        return m ? ::new (m) T(std::forward<Args>(args)...) : nullptr;
    }
    void reset() noexcept { cur_ = begin_; }                // <-- the whole point
    std::size_t used() const noexcept { return static_cast<std::size_t>(cur_ - begin_); }

private:
    std::byte* begin_;
    std::byte* cur_;
    std::byte* end_;
};

// a "parsed message" — a few small heap-ish objects per parse
struct Field { int tag; long val; };

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
    constexpr int FIELDS = 12;                              // temp objects per message

    // ---- baseline: new/delete per temp object per message -------
    {
        std::vector<double> lat; lat.reserve(MSGS);
        for (int m = 0; m < MSGS; ++m) {
            std::uint64_t t0 = tsc();
            std::vector<Field*> tmp;
            tmp.reserve(FIELDS);
            for (int f = 0; f < FIELDS; ++f) tmp.push_back(new Field{f, m + f});
            long acc = 0;
            for (auto* p : tmp) acc += p->val;
            for (auto* p : tmp) delete p;
            std::uint64_t t1 = tsc();
            keep(acc);
            lat.push_back(static_cast<double>(t1 - t0) / g_tpns);
        }
        report("new/delete per msg", lat);
    }

    // ---- arena: allocate temps, one reset per message ----------
    {
        alignas(64) static std::byte buf[1 << 16];           // 64 KB scratch
        Arena arena(buf, sizeof buf);
        std::vector<double> lat; lat.reserve(MSGS);
        for (int m = 0; m < MSGS; ++m) {
            std::uint64_t t0 = tsc();
            Field* tmp[FIELDS];
            for (int f = 0; f < FIELDS; ++f) tmp[f] = arena.make<Field>(f, m + f);
            long acc = 0;
            for (auto* p : tmp) acc += p->val;
            arena.reset();                                   // <-- frees ALL temps, 1 instruction
            std::uint64_t t1 = tsc();
            keep(acc);
            lat.push_back(static_cast<double>(t1 - t0) / g_tpns);
        }
        report("arena + reset per msg", lat);
    }

    std::puts("\nKya seekha:");
    std::puts(" - Arena allocation = align + bump; reset = ek store. Poora per-message");
    std::puts("   temp-object lifecycle allocator ke free-list se poori tarah bahar.");
    std::puts(" - p99.9 aur max lagbhag p50 ke barabar — koi allocator internals nahi.");
    std::puts(" - Trade-off: (1) individual free nahi — sab temps ka lifetime =");
    std::puts("   message scope; (2) non-trivial dtors khud track karo; (3) arena");
    std::puts("   overflow (bada message) pe fallback / assert / drop policy chahiye.");
    std::puts(" - PMR ke saath: std::pmr::monotonic_buffer_resource yahi hai (example 05).");
    return 0;
}
