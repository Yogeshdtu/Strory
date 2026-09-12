// 11_container_benchmark.cpp
// ============================================================
// Container comparison suite -- vector / deque / list / set / unordered_set
// ============================================================
//   BENCHMARK -- -O2:
//     g++ -std=c++20 -O2 11_container_benchmark.cpp -o cb && ./cb
// ============================================================
//   Measures, for N elements:
//     A) sequential iteration (sum)
//     B) random lookup / membership
//     C) push_back / insert-at-end
//   Takeaway: contiguity (vector) wins iteration + lookup-by-index by a lot;
//   node containers (list/set) lose to cache misses; unordered_set wins membership.
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <list>
#include <numeric>
#include <random>
#include <set>
#include <unordered_set>
#include <vector>

using Clock = std::chrono::steady_clock;
static std::uint64_t g_sink = 0;

template <class F>
static double ms(F&& f) {
    auto t0 = Clock::now();
    f();
    auto t1 = Clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

int main() {
    const int N = 1'000'000;
    std::mt19937_64 rng{42};

    std::vector<int> src(static_cast<std::size_t>(N));
    std::iota(src.begin(), src.end(), 0);
    std::shuffle(src.begin(), src.end(), rng);

    std::vector<int>        vec(src.begin(), src.end());
    std::deque<int>         deq(src.begin(), src.end());
    std::list<int>          lst(src.begin(), src.end());
    std::set<int>           st (src.begin(), src.end());
    std::unordered_set<int> ust(src.begin(), src.end());

    std::printf("N = %d\n\n", N);
    std::printf("%-22s %12s %12s %12s\n", "container", "iterate(ms)", "push_back(ms)", "lookup(ms)");
    std::printf("%-22s %12s %12s %12s\n", "----------", "-----------", "-------------", "----------");

    // ---- A) sequential iteration ----
    auto iterate = [&](auto& c) {
        return ms([&]{ std::uint64_t s = 0; for (int x : c) s += static_cast<std::uint64_t>(x); g_sink += s; });
    };

    // ---- C) build via push_back / insert ----
    auto buildVec = ms([&]{ std::vector<int> v; v.reserve(static_cast<std::size_t>(N)); for (int x : src) v.push_back(x); g_sink += v.size(); });
    auto buildDeq = ms([&]{ std::deque<int>  d; for (int x : src) d.push_back(x); g_sink += d.size(); });
    auto buildLst = ms([&]{ std::list<int>   l; for (int x : src) l.push_back(x); g_sink += l.size(); });
    auto buildSet = ms([&]{ std::set<int>    s; for (int x : src) s.insert(x);   g_sink += s.size(); });
    auto buildUst = ms([&]{ std::unordered_set<int> u; u.reserve(static_cast<std::size_t>(N)); for (int x : src) u.insert(x); g_sink += u.size(); });

    // ---- B) membership lookup ----
    std::vector<int> q = src;
    std::shuffle(q.begin(), q.end(), rng);
    // set / unordered_set: query ALL N values
    auto lookupSet = ms([&]{ std::uint64_t hits = 0; for (int x : q) hits += st.count(x);  g_sink += hits; });
    auto lookupUst = ms([&]{ std::uint64_t hits = 0; for (int x : q) hits += ust.count(x); g_sink += hits; });
    // vector linear find: only a SMALL sample (it's O(n) per lookup -> N*N is minutes)
    const int SAMPLE = 2000;
    auto lookupVecSample = ms([&]{
        std::uint64_t hits = 0;
        for (int i = 0; i < SAMPLE; ++i) hits += (std::find(vec.begin(), vec.end(), q[static_cast<std::size_t>(i)]) != vec.end());
        g_sink += hits;
    });
    double lookupVecPer = lookupVecSample / SAMPLE;   // ms per lookup

    std::printf("%-22s %12.2f %12.2f %12s\n", "vector",         iterate(vec), buildVec, "(see below)");
    std::printf("%-22s %12.2f %12.2f %12s\n", "deque",          iterate(deq), buildDeq, "-");
    std::printf("%-22s %12.2f %12.2f %12s\n", "list",           iterate(lst), buildLst, "-");
    std::printf("%-22s %12.2f %12.2f %12.2f\n", "set (RB-tree)", iterate(st),  buildSet, lookupSet);
    std::printf("%-22s %12.2f %12.2f %12.2f\n", "unordered_set", iterate(ust), buildUst, lookupUst);
    std::printf("\n  vector std::find (linear): %.4f ms/lookup  (~%.0f ms for all %d -- O(n) each, don't)\n",
                lookupVecPer, lookupVecPer * N, N);

    std::printf("\n  (sink %llu)\n", static_cast<unsigned long long>(g_sink));
    std::printf(
        "\n"
        "  Typical results:\n"
        "  - iterate: vector fastest (contiguous, prefetch-friendly). list/set slowest\n"
        "    (each element a separate heap node -> cache miss per step).\n"
        "  - build: vector (reserved) fastest. set/unordered_set pay for node alloc + rebalance/hash.\n"
        "  - membership: unordered_set ~ set << vector-linear-find.\n"
        "  DEFAULT to std::vector. Reach for a node/hash container only when its\n"
        "  specific property (ordering, O(1) membership, stable iterators) is REQUIRED.\n");
    return 0;
}
