// 02_arrays_strings_kata.cpp
// ============================================================
// Folder 47 file 02 ke in-place / index-as-hash problems, assertions ke saath.
// product-except-self · subarray-sum-k · first-missing-positive · trap-rain-water
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g -O0 02_arrays_strings_kata.cpp -o t && ./t
// ============================================================

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <vector>

// ---- B1: product of array except self, no division, O(1) extra --------
static std::vector<long long> product_except_self(const std::vector<int>& a) {
    const std::size_t n = a.size();
    std::vector<long long> out(n, 1);
    long long prefix = 1;
    for (std::size_t i = 0; i < n; ++i) { out[i] = prefix; prefix *= a[i]; }
    long long suffix = 1;
    for (std::size_t i = n; i-- > 0; ) { out[i] *= suffix; suffix *= a[i]; }
    return out;
}

// ---- B12: count subarrays with sum == k (prefix + hashmap) -----------
static int subarray_sum(const std::vector<int>& a, int k) {
    std::unordered_map<long long, int> count{{0, 1}};
    long long pre = 0;
    int ans = 0;
    for (int x : a) {
        pre += x;
        if (auto it = count.find(pre - k); it != count.end()) ans += it->second;
        ++count[pre];
    }
    return ans;
}

// ---- C3: first missing positive, O(n) time, O(1) space --------------
static int first_missing_positive(std::vector<int>& a) {
    const std::size_t n = a.size();
    auto in_range = [n](int v) { return v >= 1 && static_cast<std::size_t>(v) <= n; };
    for (std::size_t i = 0; i < n; ++i)
        while (in_range(a[i])) {
            const std::size_t home = static_cast<std::size_t>(a[i] - 1);
            if (a[home] == a[i]) break;                 // already placed
            std::swap(a[i], a[home]);
        }
    for (std::size_t i = 0; i < n; ++i)
        if (a[i] != static_cast<int>(i) + 1) return static_cast<int>(i) + 1;
    return static_cast<int>(n) + 1;
}

// ---- C1: trapping rain water, two-pointer, O(1) space ---------------
static long long trap(const std::vector<int>& h) {
    if (h.empty()) return 0;
    std::size_t l = 0, r = h.size() - 1;
    int left_max = h[l], right_max = h[r];
    long long water = 0;
    while (l < r) {
        if (left_max <= right_max) {
            ++l;
            if (h[l] > left_max) left_max = h[l];
            water += left_max - h[l];
        } else {
            --r;
            if (h[r] > right_max) right_max = h[r];
            water += right_max - h[r];
        }
    }
    return water;
}

int main() {
    // product_except_self
    assert((product_except_self({1, 2, 3, 4}) == std::vector<long long>{24, 12, 8, 6}));
    assert((product_except_self({-1, 1, 0, -3, 3}) == std::vector<long long>{0, 0, 9, 0, 0}));

    // subarray_sum
    assert(subarray_sum({1, 1, 1}, 2) == 2);
    assert(subarray_sum({1, 2, 3}, 3) == 2);              // {1,2} and {3}
    assert(subarray_sum({1, -1, 1, -1}, 0) == 4);         // negatives -> window won't work

    // first_missing_positive
    { std::vector<int> v{1, 2, 0};            assert(first_missing_positive(v) == 3); }
    { std::vector<int> v{3, 4, -1, 1};        assert(first_missing_positive(v) == 2); }
    { std::vector<int> v{7, 8, 9, 11, 12};    assert(first_missing_positive(v) == 1); }
    { std::vector<int> v{1, 2, 3, 4};         assert(first_missing_positive(v) == 5); }

    // trap
    assert(trap({0, 1, 0, 2, 1, 0, 1, 3, 2, 1, 2, 1}) == 6);
    assert(trap({4, 2, 0, 3, 2, 5}) == 9);
    assert(trap({}) == 0);
    assert(trap({3, 3, 3}) == 0);

    std::puts("02_arrays_strings_kata: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - product_except_self: prefix pass fills out[i], suffix pass multiplies
//     in the running suffix. Zeros fall out for free.
//   - subarray_sum: sum(i..j) = pre[j] - pre[i-1]; count how many earlier
//     prefixes equal pre[j]-k. count[0]=1 seeds subarrays starting at 0.
//     Sliding window FAILS here (negatives break monotonicity).
//   - first_missing_positive: put value v at index v-1 by cyclic swaps.
//     Amortized O(n) — each swap permanently places one value.
//   - trap: the side with the smaller running max decides its own water
//     level (a taller max is guaranteed on the other side).
// ============================================================
