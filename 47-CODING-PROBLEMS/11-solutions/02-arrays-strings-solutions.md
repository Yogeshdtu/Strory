# 02 — Arrays & strings: worked solutions

Poore code un problems ke liye jinme in-place trick ya index-as-hash hai. Baaki
`02-arrays-strings-problems.md` ke `<details>` blocks mein.

---

## B1 — Product of array except self (no division, O(1) extra)

```cpp
#include <vector>
std::vector<long long> product_except_self(const std::vector<int>& a) {
    const size_t n = a.size();
    std::vector<long long> out(n, 1);
    long long prefix = 1;
    for (size_t i = 0; i < n; ++i) { out[i] = prefix; prefix *= a[i]; }   // out[i] = prod of a[0..i-1]
    long long suffix = 1;
    for (size_t i = n; i-- > 0; ) { out[i] *= suffix; suffix *= a[i]; }   // *= prod of a[i+1..]
    return out;
}
// {1,2,3,4} -> {24,12,8,6}
```

Output array `O(1)` extra ke count mein nahi aata (problem convention). Zeros
apne aap handle: ek zero → sirf uska slot non-zero; do zero → sab zero.

---

## B12 — Subarray sum equals k (prefix + hash map)

```cpp
#include <unordered_map>
#include <vector>
int subarray_sum(const std::vector<int>& a, int k) {
    std::unordered_map<long long, int> count{{0, 1}};   // empty prefix seen once
    long long pre = 0;
    int ans = 0;
    for (int x : a) {
        pre += x;
        if (auto it = count.find(pre - k); it != count.end()) ans += it->second;
        ++count[pre];
    }
    return ans;
}
```

`sum(i..j) == pre[j] - pre[i-1] == k` → har `pre[j]` pe count karo kitne pehle
`pre` `== pre[j] - k` the. `count[0] = 1` taaki `i = 0` waale subarrays count
hon. Sliding window yahan **kaam nahi karta** — negatives se window monotonic
nahi.

---

## C3 — First missing positive (O(n) time, O(1) space)

```cpp
#include <vector>
int first_missing_positive(std::vector<int>& a) {
    const int n = static_cast<int>(a.size());
    for (int i = 0; i < n; ++i)
        // a[i] ko apni "ghar" position a[i]-1 pe le jao, jab tak in-range + misplaced
        while (a[i] >= 1 && a[i] <= n && a[a[i] - 1] != a[i])
            std::swap(a[i], a[a[i] - 1]);
    for (int i = 0; i < n; ++i)
        if (a[i] != i + 1) return i + 1;   // pehla khaali ghar
    return n + 1;                          // 1..n sab present
}
// {3,4,-1,1} -> 2 ; {7,8,9,11,12} -> 1
```

`while` loop total `O(n)` amortized — har `swap` ek value ko permanent sahi jagah
daalta, so at most `n` swaps overall. Array khud hash table ban gaya. Values
`> n` ya `<= 0` ko ignore — answer `1..n+1` range mein hi ho sakta.

---

## C1 — Trapping rain water (two-pointer, O(1) space)

```cpp
#include <vector>
long long trap(const std::vector<int>& h) {
    if (h.empty()) return 0;
    size_t l = 0, r = h.size() - 1;
    int left_max = h[l], right_max = h[r];
    long long water = 0;
    while (l < r) {
        if (left_max <= right_max) {           // left side ka answer decided
            ++l;
            left_max = std::max(left_max, h[l]);
            water += left_max - h[l];
        } else {
            --r;
            right_max = std::max(right_max, h[r]);
            water += right_max - h[r];
        }
    }
    return water;
}
// {0,1,0,2,1,0,1,3,2,1,2,1} -> 6
```

Insight: jis taraf ka running max chhota hai, us index pe paani ki height *usi*
max se decide hoti (doosri taraf usse bada max guaranteed). Isliye chhoti-max
side ka pointer safely move kar sakte.

---

## C10 — KMP substring search (O(n + m))

```cpp
#include <string>
#include <vector>
std::vector<int> build_lps(const std::string& p) {
    std::vector<int> lps(p.size(), 0);
    int len = 0;
    for (size_t i = 1; i < p.size(); ) {
        if (p[i] == p[len]) lps[i++] = ++len;
        else if (len) len = lps[len - 1];       // fall back within the pattern
        else lps[i++] = 0;
    }
    return lps;
}
int kmp_find(const std::string& text, const std::string& pat) {
    if (pat.empty()) return 0;
    const auto lps = build_lps(pat);
    for (size_t i = 0, j = 0; i < text.size(); ) {
        if (text[i] == pat[j]) {
            ++i; ++j;
            if (j == pat.size()) return static_cast<int>(i - j);
        } else if (j) j = lps[j - 1];           // skip, don't rewind i
        else ++i;
    }
    return -1;
}
```

`lps[k]` = pattern ke `p[0..k]` ka longest proper prefix jo suffix bhi hai.
Mismatch pe `i` (text index) kabhi peeche nahi jaata — sirf `j` pattern mein
backtrack karta. Total `O(n + m)`.
