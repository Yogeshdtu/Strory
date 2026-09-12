# 08 — Loop patterns

## Prerequisites
- [`03-for-loop.md`](03-for-loop.md), [`04-range-based-for.md`](04-range-based-for.md), [`06-break-continue.md`](06-break-continue.md)

## Yeh topic abhi kyun
90% loops in mein se kisi ek shakl ke hote hain: **jama karo** (accumulator),
**dhoondho** (search), **chhaano** (filter), **sabse bada/chhota** (min/max),
**do-taraf se** (two-pointer). In patterns ko pehchaanne se aap loop jaldi aur
bina bug ke likhte ho — aur pata chalta hai ki kab ek STL algorithm loop se
behtar hai.

---

## Pattern 1 — ACCUMULATOR (fold / reduce)

Ek running result, har element se update.

```cpp
long long sum = 0;                        // identity value (jod ke liye 0)
for (const auto& x : v) sum += x;

long long product = 1;                    // guna ke liye 1
for (const auto& x : v) product *= x;

int count = 0;                            // ginti
for (const auto& x : v) if (x > threshold) ++count;

std::string joined;
for (const auto& s : parts) joined += s + ",";
```

### Branchless counting (folder 06 file 08 se)
```cpp
int count = 0;
for (const auto& x : v) count += (x > threshold);   // bool 0/1 -- no branch
```

### STL
```cpp
long long sum = std::accumulate(v.begin(), v.end(), 0LL);
long long sum = std::reduce(v.begin(), v.end(), 0LL);          // C++17, parallel-friendly
auto count   = std::count_if(v.begin(), v.end(), [](int x){ return x > t; });
```

⚠️ **Accumulator type ka dhyaan:** `std::accumulate(v.begin(), v.end(), 0)` — `0`
`int` hai → sum `int` mein hoga → overflow. `0LL` do.

---

## Pattern 2 — SEARCH (linear)

Pehla element jo condition satisfy kare — aur turant `break`.

```cpp
int foundIndex = -1;
for (std::size_t i = 0; i < v.size(); ++i) {
    if (v[i] == target) { foundIndex = static_cast<int>(i); break; }
}

// ya bool
bool exists = false;
for (const auto& x : v) if (x == target) { exists = true; break; }
```

### Better return type — `std::optional`
```cpp
std::optional<std::size_t> findIndex(const std::vector<int>& v, int target) {
    for (std::size_t i = 0; i < v.size(); ++i)
        if (v[i] == target) return i;
    return std::nullopt;
}
```

### STL
```cpp
auto it = std::find(v.begin(), v.end(), target);
if (it != v.end()) { /* index = it - v.begin() */ }

auto it2 = std::find_if(v.begin(), v.end(), [](int x){ return x > 100; });
bool any = std::any_of(v.begin(), v.end(), pred);
bool all = std::all_of(v.begin(), v.end(), pred);
```

**Sorted data pe:** linear search O(n) mat karo — `std::binary_search` /
`std::lower_bound` O(log n).

---

## Pattern 3 — FILTER (select subset)

```cpp
std::vector<int> evens;
for (const auto& x : v)
    if (x % 2 == 0)
        evens.push_back(x);

// count pehle se pata ho to reserve karo -- reallocation bachao
evens.reserve(v.size());
```

### Transform + filter
```cpp
std::vector<int> result;
for (const auto& x : v)
    if (x > 0)
        result.push_back(x * x);
```

### STL / ranges
```cpp
std::copy_if(v.begin(), v.end(), std::back_inserter(evens),
             [](int x){ return x % 2 == 0; });

// C++20 ranges -- lazy, no intermediate vector
auto evens = v | std::views::filter([](int x){ return x % 2 == 0; });
for (int x : evens) { ... }
```

---

## Pattern 4 — MIN / MAX (aur unka index)

```cpp
// ⚠️ v khali na ho -- warna v[0] UB
int maxVal = v[0];
for (std::size_t i = 1; i < v.size(); ++i)
    if (v[i] > maxVal) maxVal = v[i];

// max + uska index
std::size_t maxIdx = 0;
for (std::size_t i = 1; i < v.size(); ++i)
    if (v[i] > v[maxIdx]) maxIdx = i;

// min aur max ek hi pass mein
int lo = v[0], hi = v[0];
for (const auto& x : v) { lo = std::min(lo, x); hi = std::max(hi, x); }
```

### STL
```cpp
auto it = std::max_element(v.begin(), v.end());       // iterator
int hi  = *std::max_element(v.begin(), v.end());
auto [mn, mx] = std::minmax_element(v.begin(), v.end());   // dono ek call mein
```

⚠️ Khali container: `max_element` `end()` deta hai — deref se pehle check.

---

## Pattern 5 — TWO-POINTER

Do index, aksar dono siron se, ek doosre ki taraf. Sorted arrays, palindromes,
in-place partition, "pair with sum" — sab yahin.

### Palindrome check
```cpp
bool isPalindrome(const std::string& s) {
    std::size_t i = 0, j = s.size();
    while (i < j) {
        --j;
        if (s[i] != s[j]) return false;
        ++i;
    }
    return true;
}
```

### Pair with target sum (sorted array)
```cpp
// v sorted hai
std::size_t i = 0, j = v.size();
while (i + 1 < j) {                 // careful with unsigned
    --j;
    long long s = v[i] + v[j];
    if (s == target) return {i, j};
    if (s < target)  ++i;          // chhota hai -> left pointer aage
    else             ;             // bada hai -> right pehle hi ghata (--j upar)
    ++i;                            // (yeh sketch hai -- exercise mein theek karo)
}
```

### In-place: remove elements matching pred (stable)
```cpp
std::size_t write = 0;
for (std::size_t read = 0; read < v.size(); ++read)
    if (!shouldRemove(v[read]))
        v[write++] = v[read];
v.resize(write);
// == std::erase_if(v, shouldRemove)  (C++20)
```

Yeh **read/write two-pointer** hai — `std::remove` / `std::unique` andar se aisa
hi karte hain.

---

## Pattern 6 — SLIDING WINDOW (intro)

Contiguous sub-range pe running stat, window ko slide karke O(n):

```cpp
// size-k window ka max sum
long long windowSum = 0;
for (std::size_t i = 0; i < k; ++i) windowSum += v[i];
long long best = windowSum;
for (std::size_t i = k; i < v.size(); ++i) {
    windowSum += v[i] - v[i - k];      // naya add, purana remove
    best = std::max(best, windowSum);
}
```

Naive hota O(n·k); sliding window O(n). (Market data mein "last N ticks ka VWAP /
moving average" isi tarah.)

---

## Loop ya STL algorithm?

| Use STL algorithm jab | Use raw loop jab |
|---|---|
| Standard operation hai (`find`, `count`, `sort`, `accumulate`) | Custom multi-step logic har element pe |
| Intent naam se saaf ho jaaye (`any_of` > loop+flag+break) | Loop body mein I/O, side effects, early complex exit |
| Ranges se compose karna ho (`filter | transform | take`) | Performance-critical + aapko exact codegen chahiye |

**Default:** agar ek `<algorithm>` function fit baithta hai — use it. Woh
off-by-one aur empty-container bugs pehle se handle karta hai, aur intent
declare karta hai. Raw loop tab jab logic custom ho. (Folder 19 mein poora
`<algorithm>`.)

> **HFT relevance:** Hot path mein aksar **raw loops** — exact control chahiye
> (branchless, prefetch, no allocation, SIMD-friendly layout). Par correctness-
> critical non-hot code mein STL algorithms — kam bugs. Aur `std::reduce` /
> `std::transform_reduce` (C++17) execution-policy ke saath vectorize/parallelize
> ho sakte hain. Patterns pehchaano — phir decide raw vs algorithm.

---

## Hands-on

`examples/02_range_based.cpp` (accumulator), `examples/03_nested_patterns.cpp`
(nested), aur ek chhota program likho jisme saare 5 patterns ek `std::vector<int>`
pe:

```bash
./build.ps1 07-LOOPS/examples/02_range_based.cpp
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::accumulate(b, e, 0)` int overflow safe" | `0` `int` → sum `int`. `0LL` do |
| "min/max loop `v[0]` se shuru — khali pe bhi theek" | Khali container → `v[0]` UB. Pehle check |
| "Linear search sorted array pe theek" | O(n) — `binary_search` / `lower_bound` O(log n) |
| "Filter mein `push_back` — bas chalega" | `reserve()` karo agar count pata ho |
| "STL algorithm hamesha slow (extra abstraction)" | `-O2` pe raw loop jaisa; aksar behtar vectorized |

---

## Exercises

1. **Accumulator:** `std::vector<int>` ka sum, product, aur "kitne > 50" — teenon
   ek hi loop mein. Phir `std::accumulate` / `std::count_if` se.

2. **Overflow:** `std::vector<int> v(100, 100'000'000);` ka `std::accumulate(b, e, 0)`
   kya deta hai? Ab `0LL` se? Explain.

3. **Search → optional:** `firstNegative(const std::vector<int>&)` → `std::optional<size_t>`.
   Test: koi negative nahi, pehla element negative, aakhri negative.

4. **Filter + transform:** `v` ke positive elements ke squares ka naya vector.
   Raw loop se, phir `std::views::filter | std::views::transform` se.

5. **Min + max ek pass:** `std::pair<int,int> minMax(const std::vector<int>&)`.
   Khali pe kya karoge (throw? optional?). `std::minmax_element` se compare.

6. **Two-pointer palindrome:** `isPalindrome(std::string_view)` — unsigned index
   ke trap se bacho. Test: `""`, `"a"`, `"aba"`, `"abba"`, `"abc"`.

7. **Sliding window:** `maxSubarraySum(const std::vector<int>&, int k)` — size-`k`
   contiguous window ka max sum, O(n). Naive O(n·k) se answer match karo.

8. **Raw vs STL benchmark:** `sum` raw loop vs `std::accumulate` vs `std::reduce`,
   10M ints, `-O2`. Time likho — farq hai?

---

## Interview questions

1. Accumulator pattern kya hai? Identity value ka kya matlab (sum, product, min)?
2. `std::accumulate` ke initial value ka type kyun matter karta hai?
3. Linear search vs binary search — kab kaunsa?
4. Two-pointer technique — 2 problems jahan yeh O(n²) → O(n) karta hai?
5. Sliding window O(n·k) ko O(n) kaise banata hai?
6. Loop vs `<algorithm>` — decision kaise loge? HFT hot path mein?
7. Filter loop mein `reserve()` kyun?

---

## Next
→ [`09-loop-performance.md`](09-loop-performance.md)
