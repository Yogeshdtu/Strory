// 05_ranges_demo.cpp
// ============================================================
// C++20 <ranges> -- views, pipelines, lazy evaluation
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 05_ranges_demo.cpp -o rd && ./rd
// ============================================================
//   Views = lazy, non-owning adaptors over a range. Compose with `|`.
//   Nothing computed until you iterate. No intermediate containers.
//     data | views::filter(pred) | views::transform(f) | views::take(n)
// ============================================================

#include <algorithm>
#include <cstdio>
#include <ranges>
#include <string>
#include <vector>

namespace rv = std::ranges::views;

int main() {
    std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

    std::printf("=== 1. filter | transform | take (lazy pipeline) ===\n");
    {
        auto pipeline = v
                      | rv::filter([](int x){ return x % 2 == 0; })   // evens: 2 4 6 8 10 12
                      | rv::transform([](int x){ return x * x; })     // squares: 4 16 36 64 100 144
                      | rv::take(3);                                   // first 3: 4 16 36
        std::printf("  evens -> squared -> first 3:  ");
        for (int x : pipeline) std::printf("%d ", x);                  // computed HERE, element by element
        std::printf("\n  (no intermediate vectors allocated)\n");
    }

    std::printf("\n=== 2. more views ===\n");
    {
        std::printf("  drop(8)        : ");   for (int x : v | rv::drop(8))            std::printf("%d ", x); std::printf("\n");
        std::printf("  reverse        : ");   for (int x : v | rv::reverse)            std::printf("%d ", x); std::printf("\n");
        std::printf("  take_while(<5) : ");   for (int x : v | rv::take_while([](int x){ return x < 5; })) std::printf("%d ", x); std::printf("\n");
        std::printf("  drop_while(<5) : ");   for (int x : v | rv::drop_while([](int x){ return x < 5; })) std::printf("%d ", x); std::printf("\n");
        std::printf("  iota(100,105)  : ");   for (int x : rv::iota(100, 105))         std::printf("%d ", x); std::printf("\n");
    }

    std::printf("\n=== 3. lazy = only computes what's consumed ===\n");
    {
        int calls = 0;
        auto expensive = [&calls](int x) { ++calls; return x * 10; };
        auto view = v | rv::transform(expensive) | rv::take(3);
        std::printf("  built the view; transform calls so far: %d\n", calls);   // 0 -- nothing ran
        int sum = 0;
        for (int x : view) sum += x;                                            // NOW it runs, 3 times
        std::printf("  after consuming take(3): calls = %d, sum = %d\n", calls, sum);
    }

    std::printf("\n=== 4. ranges algorithms (no .begin()/.end()) ===\n");
    {
        std::vector<int> w{5, 2, 8, 1, 9, 3};
        std::ranges::sort(w);                                    // whole container, no iterators
        std::printf("  ranges::sort         : "); for (int x : w) std::printf("%d ", x); std::printf("\n");
        auto it = std::ranges::find(w, 8);
        std::printf("  ranges::find(8) idx  : %lld\n", it - w.begin());
        std::printf("  ranges::count_if even: %lld\n", std::ranges::count_if(w, [](int x){ return x % 2 == 0; }));
    }

    std::printf("\n=== 5. materialize a view into a container ===\n");
    {
        std::vector<int> result;
        for (int x : v | rv::filter([](int x){ return x > 6; }) | rv::transform([](int x){ return -x; }))
            result.push_back(x);
        std::printf("  filtered/transformed -> vector: ");
        for (int x : result) std::printf("%d ", x);
        std::printf("\n  (C++23: std::ranges::to<std::vector>() does this in one call)\n");
    }

    std::printf(
        "\n"
        "  Views: lazy, non-owning, composable with `|`. Zero intermediate allocations.\n"
        "  Pipeline pehle padhne mein saaf, aur -O2 pe aksar ek fused loop ban jaata.\n"
        "  Watch-out: dangling (view over a temporary), aur kuch views single-pass hote.\n");
    return 0;
}
