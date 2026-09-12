# 09 — "Optimize this" : worked solutions

Before/after + the mechanism. Numbers = is repo ke folders 32/43 ke **measured**
results (AMD Ryzen 7 4700U, GCC 15, `-O2`, unpinned). **Apne box pe khud
measure karo** — `./build.ps1 fast file.cpp`.

---

## 1 — Column-major → row-major

```cpp
// SLOW: stride = N, ek cache line se 1 element use
for (int c = 0; c < N; ++c)
    for (int r = 0; r < N; ++r) sum += m[r * N + c];

// FAST: stride = 1, ek line se 8 (long) / 16 (int) elements
for (int r = 0; r < N; ++r)
    for (int c = 0; c < N; ++c) sum += m[r * N + c];
```

**Mechanism:** row-major storage mein `m[r][c]` aur `m[r][c+1]` adjacent. Column
walk har access pe nayi 64-B line touch karta → 8× zyada DRAM traffic +
prefetcher confused.

**Measured — `examples/09_optimize_row_vs_col.cpp`, this box, `-O2`, `N=4096`
`int64`:** row-major ~7–11 ms, col-major ~480–495 ms → **~45–70×** (run-to-run
varies, row-major time is the noisy one). Bigger than folder 32 `02`'s "~7×"
(that was ns/line for a pure pointer-chase) because three effects stack: (1)
cache lines ~8× traffic, (2) col stride `4096·8` = 32 KiB → a different page
every access → 4K-page TLB thrash (folder 32 `11`), (3) row-major
auto-vectorizes, col-major can't. **Rule 2: report what you measure, not the
textbook figure.**

---

## 7 — Division by a constant / invariant divisor

```cpp
// A: var / var                         ~4.1  ns/elem   (idiv)         baseline
for (i) out[i] = a[i] / b[i];

// B: compile-time constant divisor     ~0.31 ns/elem   (~13×)
for (i) out[i] = a[i] / 1000;           // compiler emits magic-number multiply

// C: power-of-two constant             ~0.25 ns/elem   (~17×)
for (i) out[i] = a[i] >> 10;            // == / 1024 for unsigned / non-negative

// D: runtime-but-loop-invariant divisor  ~0.59 ns/elem  (~7×)
const uint64_t R = (uint64_t(1) << 32) / divisor + 1;   // hoist once
for (i) out[i] = uint32_t((uint64_t(a[i]) * R) >> 32);
```

**Mechanism (folder 43 `10`, measured):** `idiv` ~20–40 cycles, not pipelined.
A constant divisor → the compiler replaces it with a multiply-by-reciprocal +
shift. A runtime invariant → do that reciprocal trick yourself, once, outside the
loop. `%` is the same cost — power-of-two modulus → `& (d-1)`.

---

## 8 — Branch misprediction (sorted vs unsorted)

```cpp
// data has ~50% of values >= 128, in random order
long s = 0;
for (int v : data) if (v >= 128) s += v;     // ~50% mispredict, ~15-20 cyc each

// Fix A: sort first  -> predictor ~100% right  -> ~6× on the loop
std::sort(data.begin(), data.end());

// Fix B: branchless
for (int v : data) s += v & -static_cast<long>(v >= 128);   // mask: 0 or all-ones
```

**Mechanism:** an unpredictable data-dependent branch flushes the pipeline on
every miss. Sorting makes the branch predictable (long runs of taken/not-taken).
Branchless replaces control flow with a `SETcc` + `AND` — no misprediction, but
does the work unconditionally. **Only** sort if the sort cost amortizes; the
classic demo is the pure loop speedup.

---

## 2 — Vector of pointers → contiguous / SoA

```cpp
// SLOW: pointer chase, each Order a separate allocation
std::vector<Order*> book;
for (auto* o : book) total_qty += o->qty;

// BETTER: contiguous — the scan is prefetched
std::vector<Order> book;
for (const auto& o : book) total_qty += o.qty;

// BEST (if the loop only needs qty): SoA
std::vector<int64_t> qty;
for (int64_t q : qty) total_qty += q;
```

**Measured (folder 32 `05`, `09`):** AoS-contiguous vs pointer-vector ≈ **2–3×**;
SoA on top ≈ **2.0–2.4×** more when only one field is read (bandwidth +
auto-vectorization). **Caveat:** if the loop touches the *whole* record randomly,
AoS wins ~3× (SoA then does one miss per field). Profile the access pattern
first.

---

## 19 — False sharing between two counters

```cpp
// SLOW: a and b share one 64-B line -> coherence ping-pong across cores
struct { std::atomic<long> a, b; } ctr;

// FAST: force them onto separate lines
struct alignas(64) Padded { std::atomic<long> v; char pad[56]; };
Padded a, b;
```

**Mechanism:** even though thread 1 only touches `a` and thread 2 only `b`, if
they're in the same cache line every `fetch_add` on one core invalidates the
line in the other core's cache. `alignas(64)` + padding → independent lines →
each core keeps its line in M state. **Expect ~5–10×** on the contended case
(4+ threads). `perf c2c` (folder 35) points right at it.

---

## 12 — SW prefetch: the Rule 2 result

```cpp
// "just add a prefetch"
for (int i = 0; i + 8 < n; ++i) {
    __builtin_prefetch(&bucket[hash[i + 8]], 0, 0);
    consume(bucket[hash[i]]);
}
```

**Measured (folder 32 `06`):**
- light loop body → **~1.15×** (marginal).
- heavy body (64-B bucket scan + dependent `mix()`) → **~0.33× — 3× SLOWER**.

The prefetch requests compete with demand loads for line-fill buffers and
bandwidth; a modern out-of-order core already extracts enough memory-level
parallelism. **Lesson (Rule 2):** don't paper over misses — measure, and fix the
*data structure* (fewer misses) instead. When you show this in an interview, show
the number that surprised you.
