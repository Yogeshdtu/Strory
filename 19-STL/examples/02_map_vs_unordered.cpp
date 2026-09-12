// 02_map_vs_unordered.cpp
// ============================================================
// std::map vs std::unordered_map vs sorted vector -- lookup benchmark
// ============================================================
//   BENCHMARK -- -O2:
//     g++ -std=c++20 -O2 02_map_vs_unordered.cpp -o mvu && ./mvu
// ============================================================
//   std::map            : red-black tree. O(log n) lookup. Node-per-element -> POINTER CHASING,
//                         cache-hostile. Ordered iteration. Stable iterators.
//   std::unordered_map  : hash table (buckets + chained nodes). O(1) avg lookup, O(n) worst.
//                         Still node-per-element -> a pointer chase per bucket. No order.
//   sorted std::vector  : contiguous. O(log n) via binary_search. Best cache behaviour.
//                         Insert = O(n). Good for build-once, query-many.
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <map>
#include <random>
#include <unordered_map>
#include <vector>

using Clock = std::chrono::steady_clock;

int main() {
    const int N = 200000;

    std::mt19937_64 rng{12345};
    std::vector<std::uint64_t> keys(static_cast<std::size_t>(N));
    for (auto& k : keys) k = rng();

    // build all three
    std::map<std::uint64_t, int>           om;
    std::unordered_map<std::uint64_t, int>  um;
    um.reserve(static_cast<std::size_t>(N));                 // avoid rehashing during build
    std::vector<std::pair<std::uint64_t,int>> sv;
    sv.reserve(static_cast<std::size_t>(N));

    for (int i = 0; i < N; ++i) {
        om.emplace(keys[static_cast<std::size_t>(i)], i);
        um.emplace(keys[static_cast<std::size_t>(i)], i);
        sv.emplace_back(keys[static_cast<std::size_t>(i)], i);
    }
    std::sort(sv.begin(), sv.end());

    // lookup workload: query every key once, in shuffled order
    std::vector<std::uint64_t> queries = keys;
    std::shuffle(queries.begin(), queries.end(), rng);

    volatile std::uint64_t sink = 0;
    auto ns_per = [&](const char* label, auto&& lookup) {
        auto t0 = Clock::now();
        for (auto q : queries) sink += lookup(q);
        auto t1 = Clock::now();
        double ns = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
        std::printf("  %-22s %7.1f ns/lookup\n", label, ns / static_cast<double>(N));
    };

    std::printf("N = %d keys, %d shuffled lookups each:\n\n", N, N);

    ns_per("std::map", [&](std::uint64_t q) {
        auto it = om.find(q);
        return it == om.end() ? 0 : static_cast<std::uint64_t>(it->second);
    });
    ns_per("std::unordered_map", [&](std::uint64_t q) {
        auto it = um.find(q);
        return it == um.end() ? 0 : static_cast<std::uint64_t>(it->second);
    });
    ns_per("sorted vector (bsearch)", [&](std::uint64_t q) {
        auto it = std::lower_bound(sv.begin(), sv.end(), q,
                                   [](const auto& p, std::uint64_t v){ return p.first < v; });
        return (it != sv.end() && it->first == q) ? static_cast<std::uint64_t>(it->second) : 0;
    });

    std::printf("\n  unordered_map load_factor = %.2f, bucket_count = %zu\n",
                um.load_factor(), um.bucket_count());
    std::printf("  (sink %llu)\n", static_cast<unsigned long long>(sink));

    std::printf(
        "\n"
        "  Typical: unordered_map fastest (O(1) avg), sorted-vector close (cache-friendly,\n"
        "  fewer pointer chases), std::map slowest (log n + node pointer chasing per level).\n"
        "  std::map choose karo jab ORDERED iteration / range queries chahiye, ya stable\n"
        "  iterators. Warna unordered_map (custom hash + reserve). Query-heavy read-only\n"
        "  dataset -> sorted vector (folder 20, HFT order books -- folder 39).\n");
    return 0;
}
