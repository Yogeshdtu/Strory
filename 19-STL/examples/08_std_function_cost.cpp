// 08_std_function_cost.cpp
// ============================================================
// std::function vs function pointer vs direct lambda vs template -- MEASURED
// ============================================================
//   BENCHMARK -- -O2:
//     g++ -std=c++20 -O2 08_std_function_cost.cpp -o sfc && ./sfc
// ============================================================
//   direct call / templated callable : compiler INLINES -> ~free
//   function pointer                  : indirect call, no inline -> a few ns
//   std::function                     : type erasure -> indirect call, NO inline,
//                                        + possible HEAP allocation if the callable
//                                        (capture) is bigger than the small-buffer (~16 B)
// ============================================================

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <new>

using Clock = std::chrono::steady_clock;

namespace { long g_allocs = 0; }
void* operator new(std::size_t n)      { ++g_allocs; void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc{}; return p; }
void  operator delete(void* p) noexcept { std::free(p); }
void  operator delete(void* p, std::size_t) noexcept { std::free(p); }

int add_free(int a, int b) { return a + b; }

template <class F>
static std::int64_t run_templated(F f, long iters) {          // callable is a TEMPLATE param -> inlines
    std::int64_t acc = 0;
    for (long i = 0; i < iters; ++i) acc += f(static_cast<int>(i & 0xffff), 3);
    return acc;
}

static std::int64_t run_fnptr(int (*f)(int, int), long iters) {
    std::int64_t acc = 0;
    for (long i = 0; i < iters; ++i) acc += f(static_cast<int>(i & 0xffff), 3);
    return acc;
}

static std::int64_t run_stdfunc(const std::function<int(int, int)>& f, long iters) {
    std::int64_t acc = 0;
    for (long i = 0; i < iters; ++i) acc += f(static_cast<int>(i & 0xffff), 3);
    return acc;
}

int main() {
    const long N = 50'000'000;
    volatile std::int64_t sink = 0;

    auto ns_per = [&](const char* label, auto&& call) {
        auto t0 = Clock::now();
        sink += call();
        auto t1 = Clock::now();
        double ns = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
        std::printf("  %-40s %6.2f ns/call\n", label, ns / static_cast<double>(N));
    };

    auto lambda = [](int a, int b) { return a + b; };

    std::printf("=== call overhead (%ld calls of a+b) ===\n\n", N);

    ns_per("templated callable (lambda)  [inlines]", [&]{ return run_templated(lambda, N); });
    ns_per("templated callable (fn ptr)", [&]{ return run_templated(&add_free, N); });
    ns_per("raw function pointer", [&]{ return run_fnptr(&add_free, N); });
    ns_per("std::function<int(int,int)> (small)", [&]{
        std::function<int(int,int)> f = lambda;                 // tiny capture -> small-buffer, no alloc
        return run_stdfunc(f, N);
    });

    // std::function holding a non-trivial / large closure -> heap allocation
    {
        std::string tag(40, 'x');                   // a std::string capture: > SBO, non-trivially-copyable
        long before = g_allocs;
        std::function<int(int,int)> f = [tag](int a, int b) {
            return a + b + static_cast<int>(tag.size());   // reads the capture -> can't be optimized away
        };
        std::printf("\n  std::function holding a std::string closure -> %ld heap allocation(s) on construction\n",
                    g_allocs - before);
        ns_per("std::function (std::string capture)", [&]{ return run_stdfunc(f, N); });
    }

    std::printf("\n  (sink %lld, total allocs %ld)\n", static_cast<long long>(sink), g_allocs);
    std::printf(
        "\n"
        "  templated callable  : compiler inlines -> ~0 ns (just the add).\n"
        "  function pointer    : indirect call, no inline -> ~1-3 ns.\n"
        "  std::function       : type-erased indirect call + no inline -> ~2-5 ns;\n"
        "                        big capture -> a heap allocation on construction.\n"
        "  Hot path: template the callable, ya function pointer / a tag. std::function\n"
        "  cold paths / config / callbacks-that-are-rare ke liye theek.\n");
    return 0;
}
