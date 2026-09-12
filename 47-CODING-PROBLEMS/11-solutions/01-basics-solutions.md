# 01 — Basics: worked solutions

Poore code un problems ke liye jahan ek trap chhupa hai (overflow, order, closed
form). Baaki `01-basics-problems.md` ke `<details>` blocks mein.

---

## A3 — Reverse an integer (overflow-safe)

```cpp
#include <climits>

int reverse_int(int n) {
    int rev = 0;
    while (n != 0) {
        int digit = n % 10;              // n negative ho to digit bhi negative — theek
        // Check BEFORE the multiply. Post-check = signed overflow UB.
        if (rev > INT_MAX / 10 || (rev == INT_MAX / 10 && digit > 7))  return 0;
        if (rev < INT_MIN / 10 || (rev == INT_MIN / 10 && digit < -8)) return 0;
        rev = rev * 10 + digit;
        n /= 10;
    }
    return rev;
}
```

**Kyun `7` aur `-8`:** `INT_MAX = 2147483647` → last digit `7`. `INT_MIN =
-2147483648` → last digit `-8`. `n % 10` C++ mein sign-of-dividend follow karta,
isliye negative `n` pe `digit` negative — koi alag `abs` handling nahi chahiye,
aur `INT_MIN` ko negate karne ka UB bhi nahi.

---

## A7 — Factorial with overflow detection

```cpp
#include <cstdint>
#include <optional>

std::optional<std::uint64_t> factorial(unsigned n) {
    std::uint64_t r = 1;
    for (unsigned i = 2; i <= n; ++i) {
        if (r > UINT64_MAX / i) return std::nullopt;   // r*i would overflow
        r *= i;
    }
    return r;
}
// factorial(20) = 2432902008176640000 ; factorial(21) -> nullopt
```

`20!` `uint64_t` mein sabse bada exact factorial. Guard `r > MAX / i` — division
`i != 0` (loop `i >= 2`). `n = 0, 1` → loop skip, `r = 1`.

---

## B8 — nCr without overflow

```cpp
#include <cstdint>
#include <algorithm>

std::uint64_t nCr(int n, int r) {
    if (r < 0 || r > n) return 0;
    r = std::min(r, n - r);                          // C(n,r) == C(n,n-r)
    std::uint64_t result = 1;
    for (int i = 1; i <= r; ++i)
        result = result * static_cast<std::uint64_t>(n - r + i)
                        / static_cast<std::uint64_t>(i);   // multiply THEN divide
    return result;
}
// nCr(52, 5) = 2598960
```

**Order matters:** har step pe `result` = `C(n-r+i-1, i-1)`, ek integer. `result *
(n-r+i)` pehle karo, **phir** `/ i` — `i` consecutive integers ka product hamesha
`i!` se divisible, isliye `/ i` exact. Ulta (`/ i` pehle) → truncation → galat
answer. Har intermediate `C(n-r+i, i)` — final result se chhota, so no overflow
jab tak answer khud `uint64_t` mein fits.

---

## B11 — Overflow-safe midpoint

```cpp
// Bug (2006, JDK binarySearch): (lo + hi) can overflow int.
int mid_bad(int lo, int hi)  { return (lo + hi) / 2; }

// Fix: never forms lo+hi.
int mid_ok(int lo, int hi)   { return lo + (hi - lo) / 2; }   // rounds toward lo

// C++20:
#include <numeric>
int mid_std(int lo, int hi)  { return std::midpoint(lo, hi); } // handles signs, rounds toward lo
```

`lo = 1'500'000'000, hi = 2'000'000'000` → `lo + hi` overflows (`> INT_MAX`), UB.
`lo + (hi - lo)/2` = `1'750'000'000`, fine. `hi - lo` non-negative jab `lo <= hi`
(binary search invariant), so `(hi - lo)/2` well-defined.

---

## C4 — Float sum order (measured)

```cpp
#include <cstdio>
#include <vector>

int main() {
    std::vector<float> v(10'000'000, 0.1f);
    v[0] = 1e7f;                              // ek bada value

    float fwd = 0.0f;
    for (float x : v) fwd += x;               // bada pehle -> chhote swallow

    float bwd = 0.0f;
    for (size_t i = v.size(); i-- > 0; ) bwd += v[i];  // chhote pehle

    // Kahan summation
    float sum = 0.0f, c = 0.0f;
    for (float x : v) { float y = x - c; float t = sum + y; c = (t - sum) - y; sum = t; }

    std::printf("forward = %.3f\nbackward= %.3f\nkahan   = %.3f\n", fwd, bwd, sum);
    // Typical: forward loses the most (~9999999.0), kahan closest to 10999999.0
}
```

Float add associative nahi. `bigSum + 0.1f` jab `bigSum` bada → `0.1f` round off.
**Lesson:** accumulator ko `double` rakho even for `float` data, ya Kahan. Kabhi
"sum is just a loop" mat maano jab precision matter karti hai.
