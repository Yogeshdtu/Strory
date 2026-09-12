// 03_object_pool.cpp
// ============================================================
// OBJECT pool — raw memory pool + object lifetime. Do design choices,
// dono measured:
//
//   A. CONSTRUCT: acquire = placement-new a fresh T in a free slot ;
//      release = ~T() + slot wapas   -> har acquire pe constructor cost
//   B. RECYCLE:  slots mein T pehle se constructed rehte ; acquire = slot do,
//      caller ek sasta reset() karta ; release = slot wapas
//      -> zero constructor cost, par object "stale" fields ke saath aata
//
//   T = ek chhota Order jaisa struct jiska ctor ek 16-int array zero karta
//   (yani real work — trivial nahi).
//
// Trade-off: (A) clean semantics; (B) faster steady-state. HFT aksar (B) —
// fixed schema, fields har use pe overwrite, ya ek sasta reset().
// Dono variants ZERO heap allocation karte steady state mein.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 03_object_pool.cpp -o o && ./o
// ============================================================

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
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
    std::printf("  %-26s p50 %5.1f   p99 %6.1f   p99.9 %7.1f   max %8.1f  ns\n",
                tag, pc(50), pc(99), pc(99.9), ns.back());
}

// ---- the pooled object -----------------------------------------
struct Order {
    std::uint64_t id{};
    std::int64_t  price{};
    std::uint32_t qty{};
    std::uint32_t side{};
    std::array<std::int32_t, 16> scratch{};        // ctor zero-inits -> real work
    void reset(std::uint64_t i, std::int64_t px, std::uint32_t q) noexcept {
        id = i; price = px; qty = q; side = 0;
        scratch.fill(0);
    }
};

// ============================================================
//  ObjectPool<T, Cap> — fixed capacity, free-list = array stack.
//  Zero heap allocation after construction.
// ============================================================
template <class T, std::size_t Cap>
class ObjectPool {
public:
    ObjectPool() {
        for (std::size_t i = 0; i < Cap; ++i) free_[top_++] = &storage_[i];
    }

    template <class... Args>
    T* acquire_construct(Args&&... args) {
        if (top_ == 0) return nullptr;
        Slot* s = free_[--top_];
        return ::new (s->mem) T(std::forward<Args>(args)...);
    }
    void release_destroy(T* p) noexcept {
        p->~T();
        free_[top_++] = reinterpret_cast<Slot*>(p);
    }

    T* acquire_recycle() noexcept {
        if (top_ == 0) return nullptr;
        Slot* s = free_[--top_];
        return std::launder(reinterpret_cast<T*>(s->mem));
    }
    void release_recycle(T* p) noexcept { free_[top_++] = reinterpret_cast<Slot*>(p); }

    void construct_all() { for (auto& s : storage_) ::new (s.mem) T(); }
    void destroy_all() noexcept {
        for (auto& s : storage_) std::launder(reinterpret_cast<T*>(s.mem))->~T();
    }

private:
    struct alignas(T) Slot { std::byte mem[sizeof(T)]; };
    std::array<Slot, Cap>  storage_{};
    std::array<Slot*, Cap> free_{};
    std::size_t top_ = 0;
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
    std::printf("ticks_per_ns = %.4f   sizeof(Order) = %zu\n\n", g_tpns, sizeof(Order));

    constexpr int N = 200000;
    constexpr std::size_t CAP = 2048;
    std::mt19937 rng(2024);

    // --- baseline: new/delete Order -------------------------------
    {
        std::vector<double> lat; lat.reserve(N);
        std::vector<Order*> live; live.reserve(CAP);
        for (int i = 0; i < N; ++i) {
            if (live.size() >= CAP / 2) {
                std::size_t k = rng() % live.size();
                delete live[k]; live[k] = live.back(); live.pop_back();
            }
            std::uint64_t t0 = tsc();
            auto* p = new Order();
            std::uint64_t t1 = tsc();
            p->id = static_cast<std::uint64_t>(i); keep(p); live.push_back(p);
            lat.push_back(static_cast<double>(t1 - t0) / g_tpns);
        }
        for (auto* p : live) delete p;
        report("new Order()", lat);
    }

    // --- strategy A: pool + construct-on-acquire -----------------
    {
        auto pool = std::make_unique<ObjectPool<Order, CAP>>();
        std::vector<double> lat; lat.reserve(N);
        std::vector<Order*> live; live.reserve(CAP);
        for (int i = 0; i < N; ++i) {
            if (live.size() >= CAP / 2) {
                std::size_t k = rng() % live.size();
                pool->release_destroy(live[k]); live[k] = live.back(); live.pop_back();
            }
            std::uint64_t t0 = tsc();
            Order* p = pool->acquire_construct();
            std::uint64_t t1 = tsc();
            p->id = static_cast<std::uint64_t>(i); keep(p); live.push_back(p);
            lat.push_back(static_cast<double>(t1 - t0) / g_tpns);
        }
        for (auto* p : live) pool->release_destroy(p);
        report("pool + construct", lat);
    }

    // --- strategy B: pool + recycle (pre-constructed) ------------
    {
        auto pool = std::make_unique<ObjectPool<Order, CAP>>();
        pool->construct_all();
        std::vector<double> lat; lat.reserve(N);
        std::vector<Order*> live; live.reserve(CAP);
        for (int i = 0; i < N; ++i) {
            if (live.size() >= CAP / 2) {
                std::size_t k = rng() % live.size();
                pool->release_recycle(live[k]); live[k] = live.back(); live.pop_back();
            }
            std::uint64_t t0 = tsc();
            Order* p = pool->acquire_recycle();
            p->reset(static_cast<std::uint64_t>(i), 100, 1);      // cheap explicit reset, timed
            std::uint64_t t1 = tsc();
            keep(p); live.push_back(p);
            lat.push_back(static_cast<double>(t1 - t0) / g_tpns);
        }
        for (auto* p : live) pool->release_recycle(p);
        pool->destroy_all();
        report("pool + recycle + reset", lat);
    }

    std::puts("\nKya seekha:");
    std::puts(" - new Order(): free-list fast path + ctor; tail = allocator internals.");
    std::puts(" - pool + construct: koi allocator tail nahi, par har acquire pe ctor");
    std::puts("   (16-int array zero) — visible cost.");
    std::puts(" - pool + recycle: slot pehle se constructed; acquire ~free, phir ek");
    std::puts("   chhota reset() jo tum control karte. Aam taur pe sabse flat + fast.");
    std::puts(" - Trade-off: recycle mein object 'stale' fields ke saath aata — har");
    std::puts("   field overwrite karo ya reset() do, warna bug.");
    return 0;
}
