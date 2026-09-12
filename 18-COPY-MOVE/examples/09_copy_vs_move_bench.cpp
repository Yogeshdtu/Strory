// 09_copy_vs_move_bench.cpp
// ============================================================
// Copy vs Move -- cost comparison (MEASURED)
// ============================================================
//   BENCHMARK -- -O2:
//     g++ -std=c++20 -O2 09_copy_vs_move_bench.cpp -o cmb && ./cmb
// ============================================================
//   COPY : O(n) -- naya buffer alloc + poora data memcpy
//   MOVE : O(1) -- 3 fields (ptr, size, capacity) transfer + source null
//   Bada payload -> bada farq. Chhota POD -> move == copy (dono ek register move).
// ============================================================

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using Clock = std::chrono::steady_clock;
static std::uint64_t g_sink = 0;

template <class F>
static double time_ms(F&& f) {
    auto t0 = Clock::now();
    f();
    auto t1 = Clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

int main() {
    // ---------- 1. copy vs move a big vector<string>, single shot ----------
    {
        const std::size_t N = 1'000'000;                 // 1M strings
        std::printf("=== copy vs move  std::vector<std::string> of %zu x 50-char ===\n", N);

        std::vector<std::string> a;
        a.reserve(N);
        for (std::size_t i = 0; i < N; ++i) a.emplace_back(50, 'x');   // each > SSO -> heap

        // COPY: N string copies (each: alloc + memcpy 50 bytes) + one vector buffer
        std::vector<std::string> copyDst;
        double c = time_ms([&] { copyDst = a; });
        g_sink += copyDst.size();

        // MOVE: steal a's 3 pointers. O(1). a becomes empty.
        std::vector<std::string> moveDst;
        double m = time_ms([&] { moveDst = std::move(a); });
        g_sink += moveDst.size();

        std::printf("  copy  (auto v = a)           : %9.3f ms\n", c);
        std::printf("  move  (auto v = std::move(a)): %9.6f ms\n", m);
        std::printf("  a.size() after move          : %zu   (moved-from vector -> empty)\n", a.size());
        std::printf("  ratio copy/move              : %.0fx\n\n", m > 0 ? c / m : c * 1e6);
    }

    // ---------- 2. push_back lvalue (copy) vs rvalue (move) into a vector ----------
    {
        const int COUNT = 200000;
        std::printf("=== push_back: copy an lvalue vs move an rvalue (COUNT=%d) ===\n", COUNT);
        const std::string proto(120, 'y');               // 120 chars -> heap-backed

        double copyT = time_ms([&] {
            std::vector<std::string> v; v.reserve(COUNT);
            for (int i = 0; i < COUNT; ++i) v.push_back(proto);          // COPY (proto is an lvalue)
            g_sink += v.size();
        });
        double moveT = time_ms([&] {
            std::vector<std::string> v; v.reserve(COUNT);
            for (int i = 0; i < COUNT; ++i) { std::string s = proto; v.push_back(std::move(s)); }  // MOVE
            g_sink += v.size();
        });
        // NOTE: the move loop still builds `s = proto` (one copy) then moves -> so it's
        // "copy + move" vs "copy". The delta is the extra alloc/free the plain-copy path
        // avoids by... actually both do one alloc per element. The point below is clearer.
        std::printf("  push_back(proto)            [copy]        : %9.3f ms\n", copyT);
        std::printf("  string s=proto; push_back(move(s)) [copy+move]: %9.3f ms\n", moveT);
        std::printf("  -> move itself is ~free; the alloc dominates. See #3 for the clean case.\n\n");
    }

    // ---------- 3. the clean case: return a big object by value ----------
    {
        std::printf("=== returning a big std::vector<int> by value (move, not copy) ===\n");
        const std::size_t SZ = 5'000'000;

        auto makeVec = [SZ] {
            std::vector<int> v(SZ, 7);
            return v;                                     // NRVO / move out -- NOT a copy
        };

        double t = time_ms([&] {
            for (int i = 0; i < 20; ++i) {
                std::vector<int> got = makeVec();         // 20 x (build 5M ints + move out)
                g_sink += got.size();
            }
        });
        std::printf("  20 x makeVec() (5M ints each)  : %8.3f ms   (~%.2f ms/call)\n", t, t / 20.0);
        std::printf("  If return were a COPY: +1 alloc + 20MB memcpy PER CALL on top.\n");
    }

    std::printf("\n  (sink %llu)\n", static_cast<unsigned long long>(g_sink));
    std::printf(
        "\n"
        "  Move = O(1) pointer steal; copy = O(n) alloc + memcpy.\n"
        "  #1: copying 1M strings vs moving = huge ratio (move ~microseconds).\n"
        "  Function return, vector realloc, container insert of rvalues -> all use move,\n"
        "  so a big object leaves a function 'for free'. Chhote POD: move == copy.\n");
    return 0;
}
