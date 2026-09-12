// 08_std_function_cost.cpp
// ============================================================
// `std::function` ki cost aur alternatives — MEASURED.
// (Folder 19/08 ne ise chhua; yahan hot-path lens + a hand-rolled
// non-owning function_ref.)
//
//   Callable ko store/pass karne ke tareeke:
//     1. template param            — inlined, zero overhead, par header-only
//                                     + har callable ka apna instantiation
//     2. raw function pointer       — one indirect call, no alloc, no inline
//     3. std::function             — type-erased: one indirect call,
//                                     + SMALL-buffer (SBO) ya HEAP alloc for
//                                       a big capture (the tail landmine)
//     4. function_ref (non-owning)  — {void* ctx, fn(void*, Args...)} — 2 words,
//                                     no alloc EVER, one indirect call.
//                                     Callable ki lifetime caller sambhaale.
//
// Trade-off: template = fastest but viral; function_ref = great for
// "pass a callback down one level"; std::function = only when you must OWN
// a heterogeneous callable with unknown lifetime (and then watch the capture size).
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 08_std_function_cost.cpp -o f && ./f
// ============================================================

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <type_traits>
#include <vector>

namespace ch = std::chrono;
using Clock = ch::steady_clock;
template <class T> static inline void keep(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }

// ---- a minimal non-owning function_ref (like C++26 std::function_ref) ----
template <class Sig> class function_ref;
template <class R, class... Args>
class function_ref<R(Args...)> {
public:
    template <class F>
    function_ref(F&& f) noexcept
        : ctx_(const_cast<void*>(static_cast<const void*>(std::addressof(f)))),
          call_([](void* c, Args... a) -> R {
              return (*static_cast<std::remove_reference_t<F>*>(c))(std::forward<Args>(a)...);
          }) {}
    R operator()(Args... a) const { return call_(ctx_, std::forward<Args>(a)...); }
private:
    void* ctx_;
    R (*call_)(void*, Args...);
};

// ---- work: a running hash the callable folds into ----
static inline std::uint64_t mix(std::uint64_t h, std::uint64_t x) {
    h ^= x; h *= 0x100000001b3ULL; h ^= h >> 29; return h;
}

template <class Callable>
[[gnu::noinline]] static std::uint64_t run_templated(const std::uint64_t* in, std::size_t n, Callable cb) {
    std::uint64_t h = 0;
    for (std::size_t i = 0; i < n; ++i) h = cb(h, in[i]);
    return h;
}
using RawFn = std::uint64_t (*)(std::uint64_t, std::uint64_t);
[[gnu::noinline]] static std::uint64_t run_rawfn(const std::uint64_t* in, std::size_t n, RawFn cb) {
    std::uint64_t h = 0;
    for (std::size_t i = 0; i < n; ++i) h = cb(h, in[i]);
    return h;
}
[[gnu::noinline]] static std::uint64_t run_stdfn(const std::uint64_t* in, std::size_t n,
                                                 const std::function<std::uint64_t(std::uint64_t, std::uint64_t)>& cb) {
    std::uint64_t h = 0;
    for (std::size_t i = 0; i < n; ++i) h = cb(h, in[i]);
    return h;
}
[[gnu::noinline]] static std::uint64_t run_fnref(const std::uint64_t* in, std::size_t n,
                                                 function_ref<std::uint64_t(std::uint64_t, std::uint64_t)> cb) {
    std::uint64_t h = 0;
    for (std::size_t i = 0; i < n; ++i) h = cb(h, in[i]);
    return h;
}

template <class F>
static double bench(std::size_t n, F f, int reps = 25) {
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

// a global counter so we can prove whether a std::function heap-allocated
static std::size_t g_new_bytes = 0;
void* operator new(std::size_t n) { g_new_bytes += n; return std::malloc(n); }
void  operator delete(void* p) noexcept { std::free(p); }
void  operator delete(void* p, std::size_t) noexcept { std::free(p); }

int main() {
    constexpr std::size_t N = 1u << 22;
    std::vector<std::uint64_t> in(N);
    for (std::size_t i = 0; i < N; ++i) in[i] = i * 2654435761u + 1;

    auto lam = [](std::uint64_t h, std::uint64_t x) { return mix(h, x); };

    // a big capture -> forces std::function onto the heap
    std::array<std::uint64_t, 8> big{};
    big.fill(3);
    auto fat_lam = [big](std::uint64_t h, std::uint64_t x) { return mix(h, x + big[0]); };

    std::printf("templated (inlined) : %6.3f ns/call\n",
                bench(N, [&]{ return run_templated(in.data(), N, lam); }));
    std::printf("raw fn pointer      : %6.3f ns/call\n",
                bench(N, [&]{ return run_rawfn(in.data(), N, +[](std::uint64_t h, std::uint64_t x){ return mix(h,x); }); }));

    g_new_bytes = 0;
    std::function<std::uint64_t(std::uint64_t, std::uint64_t)> sf_small = lam;
    std::printf("std::function (small): %6.3f ns/call   [ctor heap bytes: %zu]\n",
                bench(N, [&]{ return run_stdfn(in.data(), N, sf_small); }), g_new_bytes);

    g_new_bytes = 0;
    std::function<std::uint64_t(std::uint64_t, std::uint64_t)> sf_fat = fat_lam;
    std::printf("std::function (fat)  : %6.3f ns/call   [ctor heap bytes: %zu  <- SBO overflow]\n",
                bench(N, [&]{ return run_stdfn(in.data(), N, sf_fat); }), g_new_bytes);

    std::printf("function_ref         : %6.3f ns/call   (2 words, no alloc ever)\n",
                bench(N, [&]{ return run_fnref(in.data(), N, lam); }));

    std::puts("\nKya seekha (measured, is box):");
    std::puts(" - templated / raw-fn-ptr / function_ref ~1.5 ns/call — BARABAR yahan,");
    std::puts("   kyunki `h` ek carried dependency hai (LATENCY-bound loop) -> OoO");
    std::puts("   call/indirect overhead ko dep-chain ke peeche chhupa deta. Ek");
    std::puts("   THROUGHPUT loop (independent items) mein templated aage nikal jaata");
    std::puts("   (inline -> vectorize). Rule 2: kya bound hai woh maayne rakhta.");
    std::puts(" - std::function ~3.1 ns (~2x) — uska call path (type-erased indirect +");
    std::puts("   invoker) itna heavy ki dep-chain bhi poora nahi chhupa paata.");
    std::puts(" - fat capture (64 B) -> std::function ctor mein HEAP alloc (bytes > 0)");
    std::puts("   -> construction spike + extra indirection + a cache miss per call.");
    std::puts("   Small callable -> SBO (0 bytes) par phir bhi type-erased call.");
    std::puts(" - Hot path pe: template ya function_ref (2 words, no alloc EVER).");
    std::puts("   std::function tabhi jab ownership + unknown lifetime zaroori ho —");
    std::puts("   aur tab capture size pe nazar rakho.");
    return 0;
}
