// 04_erase_remove.cpp
// ============================================================
// Erase-remove idiom -- std::remove doesn't shrink; you must erase()
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 04_erase_remove.cpp -o er && ./er
// ============================================================
//   std::remove(first, last, val):
//     - "removes" by MOVING the kept elements forward, overwriting the removed ones
//     - returns an iterator to the new logical end
//     - container SIZE is UNCHANGED -- the tail is "garbage" (moved-from) but still there
//   Then: container.erase(new_end, container.end())  -> actually shrinks
//
//   C++20: std::erase(container, val)  /  std::erase_if(container, pred)  -- one call, does both.
// ============================================================

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

static void dump(const char* label, const std::vector<int>& v) {
    std::printf("  %-34s size=%zu  [", label, v.size());
    for (std::size_t i = 0; i < v.size(); ++i) std::printf("%s%d", i ? "," : "", v[i]);
    std::printf("]\n");
}

int main() {
    std::printf("=== std::remove alone -- SIZE UNCHANGED ===\n");
    {
        std::vector<int> v{1, 2, 3, 2, 4, 2, 5};
        dump("start", v);
        auto newEnd = std::remove(v.begin(), v.end(), 2);   // move kept elements forward
        std::printf("  after std::remove(2): logical end at index %lld\n", newEnd - v.begin());
        dump("...but v is still", v);                        // size still 7! tail = leftover values
        v.erase(newEnd, v.end());                            // NOW shrink
        dump("after v.erase(newEnd, end)", v);
    }

    std::printf("\n=== the idiom in one line ===\n");
    {
        std::vector<int> v{1, 2, 3, 2, 4, 2, 5};
        v.erase(std::remove(v.begin(), v.end(), 2), v.end());   // classic erase-remove
        dump("erase(remove(...), end)", v);
    }

    std::printf("\n=== remove_if with a predicate ===\n");
    {
        std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        v.erase(std::remove_if(v.begin(), v.end(), [](int x){ return x % 3 == 0; }), v.end());
        dump("removed multiples of 3", v);
    }

    std::printf("\n=== C++20: std::erase / std::erase_if (preferred) ===\n");
    {
        std::vector<int> v{1, 2, 3, 2, 4, 2, 5};
        std::size_t n = std::erase(v, 2);                       // returns count removed
        std::printf("  std::erase(v, 2) removed %zu elements\n", n);
        dump("result", v);

        std::vector<int> w{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        std::erase_if(w, [](int x){ return x > 5; });
        dump("std::erase_if(x > 5)", w);
    }

    std::printf("\n=== unique (also needs erase) -- removes CONSECUTIVE duplicates ===\n");
    {
        std::vector<int> v{1, 1, 2, 2, 2, 3, 1, 1};            // note: 1s at the end are separate group
        v.erase(std::unique(v.begin(), v.end()), v.end());
        dump("unique (consecutive only)", v);                   // [1,2,3,1] -- the trailing 1s survive

        std::vector<int> w{3, 1, 2, 1, 3, 2, 1};
        std::sort(w.begin(), w.end());                          // sort FIRST for full dedup
        w.erase(std::unique(w.begin(), w.end()), w.end());
        dump("sort + unique (full dedup)", w);
    }

    std::printf("\n=== why not just erase in a loop? ===\n");
    {
        // ❌ naive: erase-in-loop is O(n^2) and iterator-invalidation prone
        std::vector<int> v{1, 2, 2, 3, 2, 4};
        for (auto it = v.begin(); it != v.end(); ) {
            if (*it == 2) it = v.erase(it);     // erase returns the next valid iterator (needed!)
            else          ++it;
        }
        dump("erase-in-loop (O(n^2), works but slow)", v);
        std::printf("  -> for big vectors use erase-remove / std::erase (O(n), one shift)\n");
    }

    std::printf(
        "\n"
        "  std::remove / remove_if / unique  -> rearrange, return new end, SIZE unchanged.\n"
        "  Must follow with container.erase(new_end, end()) to actually shrink.\n"
        "  C++20: std::erase(container, val) / std::erase_if(container, pred) -- prefer these.\n");
    return 0;
}
