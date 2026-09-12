// 03_algorithms_tour.cpp
// ============================================================
// <algorithm> + <numeric> tour -- 30+ algorithms, ek jagah
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 03_algorithms_tour.cpp -o at && ./at
// ============================================================
//   STL algorithms iterators pe kaam karte hain -- container se decoupled.
//   Range: [first, last).  Aksar predicate / lambda lete hain.
//   "Modifying" algos container ka SIZE nahi badalte -- woh sirf elements move/overwrite
//   karte hain; shrink ke liye container.erase() (file 04).
// ============================================================

#include <algorithm>
#include <cstdio>
#include <numeric>
#include <string>
#include <vector>

static void printv(const char* label, const std::vector<int>& v) {
    std::printf("  %-26s [", label);
    for (std::size_t i = 0; i < v.size(); ++i) std::printf("%s%d", i ? "," : "", v[i]);
    std::printf("]\n");
}

int main() {
    std::vector<int> v{5, 3, 8, 1, 9, 2, 7, 3, 6, 4};
    printv("start", v);

    std::printf("\n--- non-modifying ---\n");
    std::printf("  count(3)            = %lld\n", std::count(v.begin(), v.end(), 3));
    std::printf("  count_if(even)      = %lld\n", std::count_if(v.begin(), v.end(), [](int x){ return x % 2 == 0; }));
    std::printf("  all_of(>0)          = %d\n", std::all_of(v.begin(), v.end(), [](int x){ return x > 0; }));
    std::printf("  any_of(>8)          = %d\n", std::any_of(v.begin(), v.end(), [](int x){ return x > 8; }));
    std::printf("  none_of(<0)         = %d\n", std::none_of(v.begin(), v.end(), [](int x){ return x < 0; }));
    std::printf("  find(9) at index    = %lld\n", std::find(v.begin(), v.end(), 9) - v.begin());
    std::printf("  find_if(>7) at index= %lld\n", std::find_if(v.begin(), v.end(), [](int x){ return x > 7; }) - v.begin());
    {
        auto [mn, mx] = std::minmax_element(v.begin(), v.end());
        std::printf("  min=%d max=%d\n", *mn, *mx);
    }
    std::printf("  is_sorted          = %d\n", std::is_sorted(v.begin(), v.end()));
    {
        std::vector<int> a{1,2,3}, b{1,2,4};
        auto [i1, i2] = std::mismatch(a.begin(), a.end(), b.begin());
        std::printf("  mismatch(a,b) at index %lld: %d vs %d\n", i1 - a.begin(), *i1, *i2);
        std::printf("  equal(a,a)         = %d\n", std::equal(a.begin(), a.end(), a.begin()));
    }

    std::printf("\n--- <numeric> ---\n");
    std::printf("  accumulate(sum)    = %d\n", std::accumulate(v.begin(), v.end(), 0));
    std::printf("  accumulate(product)= %lld\n",
                std::accumulate(v.begin(), v.end(), 1LL, [](long long a, int b){ return a * b; }));
    {
        std::vector<int> a{1,2,3,4};
        std::printf("  inner_product(a,a) = %d  (sum of squares)\n",
                    std::inner_product(a.begin(), a.end(), a.begin(), 0));
        std::vector<int> ps(a.size());
        std::partial_sum(a.begin(), a.end(), ps.begin());
        printv("partial_sum([1,2,3,4])", ps);
        std::vector<int> ad(a.size());
        std::adjacent_difference(ps.begin(), ps.end(), ad.begin());
        printv("adjacent_difference(ps)", ad);
        std::vector<int> io(6);
        std::iota(io.begin(), io.end(), 10);
        printv("iota(from 10)", io);
    }

    std::printf("\n--- modifying (size unchanged) ---\n");
    {
        std::vector<int> w = v;
        std::vector<int> squared(w.size());
        std::transform(w.begin(), w.end(), squared.begin(), [](int x){ return x * x; });
        printv("transform(x*x)", squared);

        std::replace(w.begin(), w.end(), 3, 99);
        printv("replace(3 -> 99)", w);

        std::vector<int> filled(5);
        std::fill(filled.begin(), filled.end(), 7);
        printv("fill(7)", filled);

        std::reverse(w.begin(), w.end());
        printv("reverse", w);

        std::rotate(w.begin(), w.begin() + 3, w.end());
        printv("rotate(by 3)", w);
    }

    std::printf("\n--- sorting family ---\n");
    {
        std::vector<int> s = v;
        std::sort(s.begin(), s.end());
        printv("sort", s);
        std::sort(s.begin(), s.end(), std::greater<>{});
        printv("sort(descending)", s);

        std::vector<int> p = v;
        std::nth_element(p.begin(), p.begin() + 4, p.end());        // 5th smallest at index 4
        std::printf("  nth_element -> element[4] = %d (the 5th smallest; partition around it)\n", p[4]);

        std::vector<int> ps2 = v;
        std::partial_sort(ps2.begin(), ps2.begin() + 3, ps2.end()); // 3 smallest, sorted, at front
        printv("partial_sort(top 3)", ps2);

        std::vector<int> pt = v;
        auto mid = std::partition(pt.begin(), pt.end(), [](int x){ return x % 2 == 0; });
        std::printf("  partition(even|odd): evens = %lld\n", mid - pt.begin());
        printv("partition result", pt);
    }

    std::printf("\n--- binary search (on SORTED range) ---\n");
    {
        std::vector<int> s = v;
        std::sort(s.begin(), s.end());
        printv("sorted", s);
        std::printf("  binary_search(7)   = %d\n", std::binary_search(s.begin(), s.end(), 7));
        std::printf("  lower_bound(3) idx = %lld  (first >= 3)\n", std::lower_bound(s.begin(), s.end(), 3) - s.begin());
        std::printf("  upper_bound(3) idx = %lld  (first > 3)\n",  std::upper_bound(s.begin(), s.end(), 3) - s.begin());
        auto [lo, hi] = std::equal_range(s.begin(), s.end(), 3);
        std::printf("  equal_range(3)     = [%lld, %lld)  -> %lld occurrences\n",
                    lo - s.begin(), hi - s.begin(), hi - lo);
    }

    std::printf("\n--- set operations (on SORTED ranges) ---\n");
    {
        std::vector<int> a{1,2,3,4,5}, b{3,4,5,6,7}, out;
        std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(out));
        printv("set_intersection", out);
        out.clear();
        std::set_union(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(out));
        printv("set_union", out);
        out.clear();
        std::set_difference(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(out));
        printv("set_difference(a-b)", out);
    }

    std::printf("\n  30+ algorithms. Rule: hand-written loop likhne se pehle poochho\n"
                "  'kya iske liye ek algorithm hai?' -- aksar hai, aur woh tested + optimized hai.\n");
    return 0;
}
