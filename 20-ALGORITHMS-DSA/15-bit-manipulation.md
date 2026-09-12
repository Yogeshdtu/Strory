# 15 — Bit manipulation for algorithms

## Prerequisites
- Folder 05 file 05 (bitwise operators), folder 19 file 22 (`<bit>`)
- [`14-dynamic-programming.md`](14-dynamic-programming.md) (bitmask DP)

## Yeh topic abhi kyun
Bits se aap ek chhota **set** ek `int` mein rakh sakte ho, aur set operations ek
CPU instruction mein kar sakte ho. Yeh subset enumeration, bitmask DP (TSP,
assignment), aur bit-trick puzzles ka foundation hai. `<bit>` (C++20) ne ye sab
portable aur single-instruction bana diya (folder 19 file 22).

---

## An integer as a set

Bit `i` of a `uint32_t`/`uint64_t` = "element `i` is in the set".

```cpp
std::uint32_t s = 0;                    // empty set
s |=  (1u << i);                        // add element i
s &= ~(1u << i);                        // remove element i
s ^=  (1u << i);                        // toggle element i
bool in = s & (1u << i);               // test element i
s = 0;                                 // clear
s = (1u << n) - 1;                     // full set {0 .. n-1}   (careful: n < 32)

// set ops -- O(1) each:
std::uint32_t uni  = a | b;            // union
std::uint32_t both = a & b;            // intersection
std::uint32_t diff = a & ~b;           // a minus b
std::uint32_t sym  = a ^ b;            // symmetric difference
bool subset = (a & b) == a;            // a is a subset of b
```

`std::bitset<N>` does this for `N > 64` with the same operators, packed.

---

## `<bit>` helpers (C++20) — one instruction each

```cpp
#include <bit>
std::popcount(s);        // set size (POPCNT)
std::countr_zero(s);     // index of the lowest set bit (TZCNT) -- s != 0
std::countl_zero(s);     // leading zeros -> 31 - this = index of highest set bit
std::has_single_bit(s);  // s is a power of two (exactly one element)
std::bit_width(s);       // 1 + floor(log2 s)
std::bit_ceil(x);        // round up to a power of two
```

### Iterate the set bits, lowest first — `O(popcount)`, not `O(width)`
```cpp
for (std::uint32_t x = s; x; x &= x - 1) {
    int i = std::countr_zero(x);   // element i is in the set
    // ... use i ...
}
```
`x & (x - 1)` clears the lowest set bit. Two instructions per element.

---

## Enumerate all subsets

### All `2ⁿ` subsets of `{0 .. n-1}`
```cpp
for (std::uint32_t mask = 0; mask < (1u << n); ++mask) {
    // `mask` is one subset; iterate its members with the loop above
}
```

### All subsets **of a given mask** `s` (submask enumeration) — `O(3ⁿ)` total over all masks
```cpp
for (std::uint32_t sub = s; ; sub = (sub - 1) & s) {
    // `sub` ranges over every subset of s, from s down to 0
    if (sub == 0) break;
}
```
`(sub - 1) & s` jumps to the next-smaller submask. Summed over all `s`, this is
`Σ 2^popcount(s) = 3ⁿ` — the standard bound for "for each set, for each of its
subsets" DP.

---

## Bitmask DP

State = a subset of "things done". `dp[mask]` or `dp[mask][last]`.

### Travelling salesman (held-Karp) — `O(2ⁿ · n²)`
`dp[mask][i]` = shortest path that visits exactly the cities in `mask` and ends
at city `i`.
```
dp[mask | (1<<j)][j] = min over i in mask of  dp[mask][i] + dist[i][j]
answer = min over i of  dp[full][i] + dist[i][0]
```
Feasible for `n ≤ ~20` (`2²⁰ · 20² ≈ 4×10⁸`).

### Assignment / matching
`dp[mask]` = min cost to assign the first `popcount(mask)` workers to the jobs in
`mask`. `O(2ⁿ · n)`.

### "Visit all nodes" shortest path, count Hamiltonian paths, SOS (sum over
subsets) DP — all the same shape.

---

## The classic bit tricks

| Trick | Effect |
|---|---|
| `x & (x - 1)` | clear the lowest set bit |
| `x & -x` | isolate the lowest set bit (a mask with just that bit) |
| `x \| (x + 1)` | set the lowest **clear** bit |
| `x & (x + 1)` | clear the lowest run of trailing 1s |
| `(x ^ (x >> 31)) - (x >> 31)` | abs(x) for 32-bit (branchless) |
| `x ^ y` then swap without temp | `a^=b; b^=a; a^=b;` (cute, not faster than `std::swap`) |
| `x >> 1` / `x << 1` | ÷2 / ×2 for **unsigned** (signed right shift is impl-defined pre-C++20, arithmetic since) |
| `n & (n - 1) == 0` | `n` is a power of two (or 0) — prefer `std::has_single_bit` |
| XOR of `1..n` and `1..n \ {k}` | find the missing number in `O(n)`, `O(1)` |
| XOR all elements | find the single non-duplicated element (rest appear twice) |
| Gray code `i ^ (i >> 1)` | successive values differ in exactly one bit |

**Use `<bit>` names** (`std::popcount`, `std::countr_zero`, `std::has_single_bit`,
`std::rotl`) instead of hand-rolled hacks — they state intent and the compiler
reliably emits the single instruction (folder 19 file 22).

---

## Andar kya hota hai

- Set-as-int operations (`|`, `&`, `^`, `~`) are 1 cycle each and branchless — an
  intersection of two 64-element sets is one `and`. A `std::set`/`std::bitset<64>`
  equivalent is far more code.
- `std::popcount` → `POPCNT` (~3-cycle latency, 1/cycle throughput);
  `std::countr_zero` → `TZCNT`; both replace a loop. Without the ISA extension the
  compiler emits a short branchless sequence.
- The set-bit iteration `for (x = s; x; x &= x - 1)` does `popcount(s)` iterations
  — proportional to the number of elements, not the word width. `blsr` (`x &=
  x-1`) and `tzcnt` are both 1 cycle.
- Bitmask DP over `dp[1 << n]` is a **contiguous array** indexed by an integer —
  cache-friendly, and the transitions are bit ops. The `2ⁿ` blowup is the limit,
  not memory layout: `n = 20` → a 1M-entry table (fits in L2/L3); `n = 25` → 32M
  entries (borderline); `n = 30` → 1G (no).
- Submask enumeration `(sub - 1) & s` is 2 instructions per step; the `3ⁿ` total
  is why "for each subset, for each of its subsets" is only feasible for `n ≤ ~20`.

> **HFT relevance:** bitmasks are the fast way to carry **small fixed sets** of
> flags/state — an order's status flags, which venues are up, which risk checks
> passed, which slots in a pool are free. A **free-list as a bitmask**:
> `allocate` = `std::countr_zero(free_mask)`, `free` = set the bit — `O(1)`, two
> instructions, no data structure (folder 19 file 22). `std::popcount(a ^ b)` =
> Hamming distance for fingerprint/dedup checks. `std::bitset<N>` for larger
> fixed flag sets (subscription masks, symbol universe membership). Bitmask DP
> itself is offline (TSP-style execution-order optimization for small `n`), not
> hot-path.

---

## Hands-on

```bash
./build.ps1 fast 19-STL/examples/... # (folder 19 file 22 covers the <bit> functions)
```
```cpp
// bitmask.cpp
#include <bit>
#include <cstdio>
#include <cstdint>
int main() {
    std::uint32_t s = 0b1011010;
    std::printf("size=%d  low=%d  high=%d\n",
        std::popcount(s), std::countr_zero(s), 31 - std::countl_zero(s));
    for (std::uint32_t x = s; x; x &= x - 1) std::printf("bit %d\n", std::countr_zero(x));
    // all subsets of {0,1,2}
    for (std::uint32_t m = 0; m < 8; ++m) { std::printf("{ ");
        for (std::uint32_t x = m; x; x &= x - 1) std::printf("%d ", std::countr_zero(x));
        std::printf("}\n"); }
}
```
```bash
g++ -std=c++20 -O2 -Wall bitmask.cpp -o bm && ./bm
```

Implement: "single number" (XOR), "missing number" (XOR `0..n`), submask
enumeration, and a bitmask-DP assignment problem for `n = 12`.

---

## ⚠️ Traps

### Trap 1 — `1 << i` overflow / signedness
```cpp
int m = 1 << 31;         // ⚠️ 1 is int; 1 << 31 is UB (overflows int). 1u << 31, or 1ull << 63
```

### Trap 2 — `(1u << n) - 1` for `n == 32`
```cpp
std::uint32_t full = (1u << 32) - 1;   // ⚠️ shift >= width is UB. Use ~0u, or 64-bit, or guard n < 32
```

### Trap 3 — `countr_zero(0)` / `countl_zero(0)`
```cpp
int i = std::countr_zero(mask);   // ⚠️ mask == 0 -> returns the width (32/64), not "none". Guard mask != 0
```

### Trap 4 — hand-rolled popcount / ctz instead of `<bit>`
```cpp
int c = 0; while (x) { c += x & 1; x >>= 1; }   // ⚠️ O(width). std::popcount(x) -> one instruction
```

### Trap 5 — bitmask DP for `n` too large
```cpp
// dp[1 << n]: n=20 ~ 1M (ok), n=25 ~ 32M (borderline), n=30 ~ 1G entries (no). Check 2^n * state fits.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`1 << 31` is fine" | `1` is `int` → UB. Use `1u`/`1ull` |
| "Iterating set bits is `O(width)`" | `for (x=s; x; x &= x-1)` is `O(popcount)` |
| "`std::popcount` loops over bits" | One `POPCNT` instruction on modern CPUs |
| "`countr_zero(0)` returns -1" | Returns the bit width — guard against zero |
| "Bitmask DP scales to any `n`" | `2ⁿ` blows up — feasible only for `n ≤ ~20–22` |

---

## Exercises

1. **Single number:** every element of `v` appears twice except one. Find it in
   `O(n)` time, `O(1)` space.

   <details><summary>Answer</summary>

   `int r = 0; for (int x : v) r ^= x; return r;` — pairs cancel (`a ^ a = 0`),
   leaving the unique element.
   </details>

2. **Missing number:** `v` contains `n` distinct values from `0..n` (one
   missing). Find it, `O(n)`, `O(1)`.

   <details><summary>Answer</summary>

   `int r = 0; for (int i = 0; i <= n; ++i) r ^= i; for (int x : v) r ^= x;
   return r;` — XOR of the full range against XOR of the array leaves the missing
   value. (Or `n(n+1)/2 - sum`.)
   </details>

3. **Subset sum via submasks:** count subsets of `{a, b, c, d}` whose sum is
   `T`, by iterating all `2⁴` masks.

   <details><summary>Answer</summary>

   `for (m = 0; m < 16; ++m) { int s = 0; for (x = m; x; x &= x-1) s +=
   val[countr_zero(x)]; if (s == T) ++count; }` — `O(2ⁿ · n)`.
   </details>

4. **Bitmask DP:** `n = 4` jobs, cost matrix `cost[worker][job]`. Min total cost
   to assign each worker a distinct job. State and transition?

   <details><summary>Answer</summary>

   `dp[mask]` = min cost to assign workers `0 .. popcount(mask)-1` to the jobs in
   `mask`. `w = popcount(mask)`; `dp[mask | (1<<j)] = min(dp[mask | (1<<j)],
   dp[mask] + cost[w][j])` for each `j` not in `mask`. Answer `dp[(1<<n)-1]`.
   `O(2ⁿ · n)`.
   </details>

5. **Free-list bitmask:** `uint64_t free_mask` (1 = slot free). Claim the lowest
   free slot; free slot `k`.

   <details><summary>Answer</summary>

   Claim: `if (!free_mask) return -1; int i = std::countr_zero(free_mask);
   free_mask &= free_mask - 1; return i;`. Free: `free_mask |= (1ull << k);`.
   Both `O(1)`, ~2 instructions.
   </details>

---

## Interview questions

1. `int` ko set ki tarah — add/remove/test/union/intersection kaise?
2. Set bits iterate karne ka `O(popcount)` tareeka?
3. `1 << i` ke signedness / overflow traps?
4. Submask enumeration `(sub-1) & s` — total complexity over all masks (`3ⁿ`)?
5. Bitmask DP (TSP / assignment) — state kya, `n` ki limit kyun?
6. `<bit>` functions hand-rolled hacks se kyun better?

---

## Next
→ [`16-string-algorithms.md`](16-string-algorithms.md)
