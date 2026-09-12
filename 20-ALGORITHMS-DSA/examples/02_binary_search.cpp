// 02_binary_search.cpp
// ============================================================
// Binary search -- the canonical version, lower_bound / upper_bound,
// the classic off-by-one traps, and "binary search the answer"
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 02_binary_search.cpp -o bs && ./bs
// ============================================================
//   - contains(x)        -> yes/no
//   - lowerBound(x)      -> first index with a[i] >= x   (insertion point)
//   - upperBound(x)      -> first index with a[i] >  x
//   - all use the half-open [lo, hi) invariant -> no off-by-one
//   - "binary search on the answer": monotone predicate -> first true
// ============================================================

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

// ---- classic membership: half-open [lo, hi) ----
static bool contains(const std::vector<int>& a, int x) {
    std::size_t lo = 0, hi = a.size();          // search space is [lo, hi)
    while (lo < hi) {
        std::size_t mid = lo + (hi - lo) / 2;   // NOT (lo+hi)/2 -> that can overflow
        if (a[mid] == x)      return true;
        else if (a[mid] < x)  lo = mid + 1;     // discard [lo, mid]
        else                  hi = mid;         // discard [mid, hi)
    }
    return false;
}

// ---- first index with a[i] >= x  (std::lower_bound) ----
static std::size_t lowerBound(const std::vector<int>& a, int x) {
    std::size_t lo = 0, hi = a.size();
    while (lo < hi) {
        std::size_t mid = lo + (hi - lo) / 2;
        if (a[mid] < x) lo = mid + 1;
        else            hi = mid;
    }
    return lo;                                   // in [0, a.size()]
}

// ---- first index with a[i] > x  (std::upper_bound) ----
static std::size_t upperBound(const std::vector<int>& a, int x) {
    std::size_t lo = 0, hi = a.size();
    while (lo < hi) {
        std::size_t mid = lo + (hi - lo) / 2;
        if (a[mid] <= x) lo = mid + 1;
        else             hi = mid;
    }
    return lo;
}

// ---- binary search on a MONOTONE predicate: smallest n with n*n >= target ----
static long long isqrtCeil(long long target) {
    long long lo = 0, hi = 3'000'000'000LL;     // hi*hi must not overflow: 3e9^2 ~ 9e18 < 9.22e18 (LLONG_MAX)
    while (lo < hi) {
        long long mid = lo + (hi - lo) / 2;
        if (mid * mid >= target) hi = mid;       // predicate true -> answer is <= mid
        else                     lo = mid + 1;   // predicate false -> answer is > mid
    }
    return lo;
}

int main() {
    std::vector<int> a{1, 3, 3, 3, 5, 8, 8, 13, 21};
    //  index:        0  1  2  3  4  5  6   7   8

    std::printf("=== 1. membership ===\n");
    for (int x : {3, 4, 21, 0, 100})
        std::printf("  contains(%3d) = %s\n", x, contains(a, x) ? "yes" : "no");

    std::printf("\n=== 2. lower_bound / upper_bound (and count) ===\n");
    for (int x : {3, 8, 4, 1, 21, 25}) {
        std::size_t lb = lowerBound(a, x), ub = upperBound(a, x);
        std::size_t stdlb = static_cast<std::size_t>(std::lower_bound(a.begin(), a.end(), x) - a.begin());
        std::size_t stdub = static_cast<std::size_t>(std::upper_bound(a.begin(), a.end(), x) - a.begin());
        std::printf("  x=%2d : lower=%zu upper=%zu count=%zu   (std: %zu / %zu)%s\n",
                    x, lb, ub, ub - lb, stdlb, stdub,
                    (lb == stdlb && ub == stdub) ? "" : "   <-- MISMATCH");
    }

    std::printf("\n=== 3. sorted insert position ===\n");
    {
        std::vector<int> v{10, 20, 30, 40};
        for (int x : {25, 5, 50, 20}) {
            auto pos = std::lower_bound(v.begin(), v.end(), x);
            std::printf("  insert %2d at index %td to stay sorted\n", x, pos - v.begin());
        }
    }

    std::printf("\n=== 4. binary search on the answer (ceil sqrt) ===\n");
    for (long long t : {0LL, 1LL, 2LL, 15LL, 16LL, 17LL, 1'000'000LL, 999'999'999'999LL}) {
        long long n = isqrtCeil(t);
        std::printf("  isqrtCeil(%13lld) = %8lld   (check: %lld*%lld=%lld >= t, (%lld)^2=%lld < t? %s)\n",
                    t, n, n, n, n * n, n - 1, (n - 1) * (n - 1),
                    (n == 0 || (n - 1) * (n - 1) < t) ? "ok" : "BAD");
    }

    std::printf("\n=== 5. the classic traps ===\n");
    std::printf(
        "  1. mid = (lo + hi) / 2  -> integer overflow for large indices.\n"
        "     use  mid = lo + (hi - lo) / 2.\n"
        "  2. `while (lo <= hi)` with `hi = a.size()` -> reads a[a.size()] (OOB).\n"
        "     use half-open [lo, hi) with `while (lo < hi)` and `hi = mid` (not mid-1).\n"
        "  3. `lo = mid` (instead of mid + 1) when the low half is discarded\n"
        "     -> infinite loop when hi - lo == 1.\n"
        "  4. binary search on UNSORTED data -> silently wrong answer, not a crash.\n"
        "  5. predicate for 'binary search the answer' MUST be monotone\n"
        "     (false...false true...true), else the result is meaningless.\n");
    return 0;
}
