# 13 — `<numeric>`: accumulate, reduce, scans, inner_product

## Prerequisites
- [`09-algorithms-part1.md`](09-algorithms-part1.md)
- Folder 03 (integer overflow, floating point), folder 05 (operators)

## Yeh topic abhi kyun
`<numeric>` chhota header hai par HFT-relevant: sums, dot products, prefix sums,
differences — market data aggregation aur signal computation ka bread and butter.
Aur ismein ek classic **overflow trap** hai (`accumulate` ka init type return type
decide karta) aur ek **parallelism** angle (`reduce` vs `accumulate`).

`#include <numeric>` (not `<algorithm>`).

---

## `std::accumulate` — left fold

```cpp
std::vector<int> v{1, 2, 3, 4, 5};

int  s  = std::accumulate(v.begin(), v.end(), 0);            // 15   -- sum, init 0
long s2 = std::accumulate(v.begin(), v.end(), 0L);           // init 0L -> accumulates in long
double avg = std::accumulate(v.begin(), v.end(), 0.0) / v.size();

int prod = std::accumulate(v.begin(), v.end(), 1, std::multiplies<>{});   // 120
std::string joined = std::accumulate(next(w.begin()), w.end(), w.front(),
                                     [](std::string a, const std::string& b){ return a + "," + b; });
```

**The init value's type IS the accumulator type and the return type.**
- `std::accumulate(v.begin(), v.end(), 0)` with `v` a `std::vector<double>` →
  accumulates in **`int`**, truncating every step. Use `0.0`.
- `std::accumulate(bigVec.begin(), bigVec.end(), 0)` where the sum exceeds
  `INT_MAX` → **signed overflow (UB)**. Use `0L` / `0LL` / `std::int64_t{0}`.
- Sequential, left-to-right: `((((init op v0) op v1) op v2) …)`. Deterministic
  floating-point order.

## `std::reduce` (C++17) — fold without an order guarantee → parallelizable

```cpp
int s = std::reduce(v.begin(), v.end());                     // init defaults to T{} (0 for int)
int s2= std::reduce(v.begin(), v.end(), 0);
int s3= std::reduce(std::execution::par, v.begin(), v.end(), 0);   // <execution> -- may use threads/SIMD

double d = std::reduce(v.begin(), v.end(), 0.0, std::plus<>{});
```

- The op must be **associative and commutative** — `reduce` may regroup and
  reorder (that's what lets it parallelize / vectorize).
- Floating-point `+` is *not* associative → `reduce` and `accumulate` can give
  **slightly different** sums. For bitwise-reproducible results (common
  requirement in finance) stick to `accumulate`, or a fixed-order Kahan sum.
- `std::reduce` without a policy is still often faster than `accumulate` because
  the compiler may vectorize the un-ordered reduction.

## `std::transform_reduce` — map then fold (the dot-product shape)

```cpp
// sum of a[i]*b[i]:
double dot = std::transform_reduce(a.begin(), a.end(), b.begin(), 0.0);

// sum of f(x):
double e = std::transform_reduce(v.begin(), v.end(), 0.0, std::plus<>{},
                                 [](double x){ return x * x; });

double dot2 = std::transform_reduce(std::execution::par_unseq,
                                    a.begin(), a.end(), b.begin(), 0.0);
```

`std::inner_product` is the old sequential version:
```cpp
double dot = std::inner_product(a.begin(), a.end(), b.begin(), 0.0);          // Σ a[i]*b[i]
double r   = std::inner_product(a.begin(), a.end(), b.begin(), 0.0,
                                std::plus<>{}, [](double x, double y){ return x - y; });  // custom ops
```

## Scans — running totals

```cpp
std::vector<int> in{1, 2, 3, 4}, out(4);

std::partial_sum(in.begin(), in.end(), out.begin());          // {1, 3, 6, 10}  -- INCLUSIVE prefix sum
std::inclusive_scan(in.begin(), in.end(), out.begin());       // C++17: same, but parallelizable (assoc op)
std::exclusive_scan(in.begin(), in.end(), out.begin(), 0);    // {0, 1, 3, 6}   -- each excludes self, needs init
std::adjacent_difference(in.begin(), in.end(), out.begin());  // {1, 1, 1, 1}   -- out[0]=in[0], out[i]=in[i]-in[i-1]
```

- `partial_sum` / `inclusive_scan` — cumulative volume, cumulative P&L,
  cumulative depth in an order book.
- `adjacent_difference` — inverse of `partial_sum`; tick-to-tick price changes,
  inter-arrival times from timestamps.
- `transform_inclusive_scan` / `transform_exclusive_scan` — map each element
  before scanning.

## `std::iota` and friends

```cpp
std::vector<int> idx(10);
std::iota(idx.begin(), idx.end(), 0);          // 0..9  -- e.g. an index vector for an index-sort (file 11)

std::gcd(a, b);   std::lcm(a, b);              // C++17
std::midpoint(a, b);                           // C++20 -- (a+b)/2 with no overflow
std::lerp(a, b, t);                             // C++20 -- a + t*(b-a), correctly rounded
```

---

## Andar kya hota hai

- `std::accumulate` is a plain loop: `T acc = init; for (; first != last; ++first)
  acc = acc + *first; return acc;`. The type of `acc` is `decltype(init)` — hence
  the overflow/truncation traps. Because the order is fixed, the compiler can
  **not** vectorize a floating-point sum here (reassociation would change the
  result) unless you pass `-ffast-math`.
- `std::reduce` is allowed to compute a tree of partial sums → the compiler
  emits SIMD (add 4/8 lanes in parallel, combine at the end), and `std::execution
  ::par` can split across threads. This is why `reduce` ≥ `accumulate` on speed
  but can differ in the last FP bits.
- `std::transform_reduce` fuses the map and the fold into one pass — no temporary
  container, and it vectorizes (`vfmadd` for a dot product).
- `partial_sum` is inherently sequential (each output depends on the previous);
  `inclusive_scan` uses a work-efficient parallel-scan algorithm when given a
  policy.

> **HFT relevance:** `std::transform_reduce` / `std::inner_product` over
> **contiguous** `std::vector`s compile to tight FMA-vectorized loops — the right
> tool for VWAP, weighted mid, moving-average dot products, correlation sums.
> `adjacent_difference` turns a timestamp column into inter-arrival gaps in one
> pass; `inclusive_scan` builds cumulative depth. The **reproducibility** point
> is real: risk/PnL numbers that must match across runs and machines use
> `accumulate` (fixed order) or an explicit compensated sum — `reduce` /
> `-ffast-math` can change the low bits. Watch the accumulator type: summing a
> day of `int` quantities in an `int` overflows silently — use `std::int64_t`.

---

## Hands-on

```bash
./build.ps1 19-STL/examples/03_algorithms_tour.cpp
```

It runs `accumulate`, `inner_product`, `partial_sum`, `adjacent_difference`,
`iota`. Add: sum a `std::vector<double>` with init `0` vs `0.0` and print both
(watch the `int` truncation); a `transform_reduce` VWAP = Σ(px·qty) / Σ(qty).

---

## ⚠️ Traps

### Trap 1 — wrong init type → truncation
```cpp
std::vector<double> v{1.5, 2.5, 3.5};
double s = std::accumulate(v.begin(), v.end(), 0);   // ⚠️ int accumulator -> 1+2+3 = 6 (0.0 -> 7.5)
```

### Trap 2 — int accumulator overflow
```cpp
std::accumulate(millionBigInts.begin(), millionBigInts.end(), 0);   // ⚠️ signed overflow UB. init 0LL
```

### Trap 3 — `reduce` on non-associative op expecting `accumulate`'s result
```cpp
double a = std::accumulate(v.begin(), v.end(), 0.0);
double b = std::reduce(v.begin(), v.end(), 0.0);     // ⚠️ may differ in the last bits (FP + is not associative)
```

### Trap 4 — `reduce` with a non-commutative op
```cpp
std::reduce(w.begin(), w.end(), std::string{}, [](auto a, auto b){ return a + b; });   // ⚠️ string concat isn't commutative -> scrambled order
```

### Trap 5 — `exclusive_scan` forgetting the init argument
```cpp
std::exclusive_scan(in.begin(), in.end(), out.begin());   // ❌ needs an init value: (..., out.begin(), 0)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`accumulate`'s init is just the starting value" | It also fixes the accumulator **type** and the return type |
| "`accumulate` and `reduce` always give the same number" | Same for integers; FP can differ (reduce reorders) |
| "`reduce` is just a faster `accumulate`" | Only valid for associative + commutative ops; result order is unspecified |
| "`partial_sum` is `adjacent_difference`" | They're inverses: `partial_sum` cumulates, `adjacent_difference` deltas |
| "`<numeric>` is in `<algorithm>`" | Separate header — `#include <numeric>` |

---

## Exercises

1. **VWAP:** `std::vector<double> px, qty;` (same length). Compute volume-weighted
   average price in one pass each way.

   <details><summary>Answer</summary>

   `double num = std::transform_reduce(px.begin(), px.end(), qty.begin(), 0.0);
   double den = std::reduce(qty.begin(), qty.end(), 0.0); double vwap = den ? num
   / den : 0.0;` — or two `std::inner_product` / `accumulate` calls for a fixed
   order.
   </details>

2. **Overflow fix:** `std::vector<std::int32_t> qtys` with ~2 million entries
   around 5000 each. `std::accumulate(qtys.begin(), qtys.end(), 0)` — what's
   wrong and the fix?

   <details><summary>Answer</summary>

   Total ≈ 10¹⁰ > `INT32_MAX` (~2.1×10⁹) → signed overflow, UB. Fix:
   `std::accumulate(qtys.begin(), qtys.end(), std::int64_t{0})`.
   </details>

3. **Deltas:** `std::vector<std::int64_t> ts` of nanosecond timestamps. Produce
   inter-arrival gaps and then the max gap.

   <details><summary>Answer</summary>

   `std::vector<std::int64_t> gap(ts.size());
   std::adjacent_difference(ts.begin(), ts.end(), gap.begin());` — `gap[0] ==
   ts[0]`, so `auto mx = *std::max_element(gap.begin() + 1, gap.end());`.
   </details>

4. **Cumulative depth:** `std::vector<std::uint64_t> levelQty` (top of book
   downward). Produce cumulative quantity available down to each level.

   <details><summary>Answer</summary>

   `std::vector<std::uint64_t> cum(levelQty.size());
   std::inclusive_scan(levelQty.begin(), levelQty.end(), cum.begin());` (or
   `std::partial_sum`). `cum[k]` = total qty in levels `0..k`.
   </details>

5. **reduce vs accumulate:** for a risk report that must be byte-identical across
   two machines, which do you use for a sum of `double` P&L, and why not the
   other?

   <details><summary>Answer</summary>

   `std::accumulate` (or an explicit Kahan/compensated sum). `std::reduce` may
   regroup the additions, and FP `+` is not associative, so different hardware /
   thread counts can yield a different last-bit result — not acceptable for a
   reproducible report.
   </details>

---

## Interview questions

1. `std::accumulate` ka init argument type kyun matter karta (2 traps)?
2. `std::reduce` vs `std::accumulate` — kab result alag, kyun?
3. `std::reduce` ke op pe kya constraints (assoc + commutative)?
4. `partial_sum` vs `adjacent_difference` — rishta kya?
5. `std::transform_reduce` dot product ke liye kyun accha (fused, vectorizes)?
6. FP sum ki reproducibility — kaunsa algorithm, kyun?

---

## Next
→ [`14-ranges.md`](14-ranges.md)
