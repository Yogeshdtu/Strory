# 09 — "Optimize this code" problems

## Prerequisites
- `32-CACHE-MEMORY-PERFORMANCE/` (locality, AoS/SoA, false sharing)
- `33-COMPILER-OPTIMIZATION/`, `35-PROFILING-BENCHMARKING/`
- `43-HFT-OPTIMIZATION/` (the v0→v3 methodology — measure, profile, one change,
  re-benchmark)

## Yeh file kya hai
20 problems. Har ek mein: **slow code + ek target** (ratio ya absolute ns).
Kaam: bottleneck naam do, ek change batao, aur bolo *kya badla aur kyun*.

**Rule 1 (CLAUDE.md):** number bina chalaye mat likho. Yahan diye targets is repo
ke folders 32/43 ke **measured** results se aaye hain (AMD Ryzen 7 4700U, GCC 15,
unpinned) — tumhare box pe alag honge. **`./build.ps1 fast file.cpp`** (`-O2`)
se measure karo; `-O0` benchmark bekaar hai.

**Rule 2:** agar measurement expectation se ulta aaye — chhupao mat, wahi
seekho (e.g. #12 SW-prefetch aksar **slower**).

Poora before/after code + numbers → [`11-solutions/09-optimization-solutions.md`](11-solutions/09-optimization-solutions.md).

---

## Part A — Memory / layout

### 1. Column-major traversal
```cpp
long sum = 0;
for (int c = 0; c < N; ++c)
    for (int r = 0; r < N; ++r)
        sum += m[r][c];          // stride = N, cache line waste
```
**Target: big — `examples/09_optimize_row_vs_col.cpp` pe is box pe measured
~45–70×** (`N = 4096`, `int64`).
<details><summary>Fix</summary>

Loop order swap → row-major (`for r { for c { sum += m[r][c]; } }`). Stride 1 →
har cache line ke 8 `long` use hote (pehle 1). **Rule 2:** ratio "~7×" se bada
kyun — teen effects stack hote: (1) cache lines ~8× traffic, (2) col stride
`4096·8` = 32 KiB → har access alag page → 4K-page TLB thrash (folder 32 `11`),
(3) row-major auto-vectorize hota, col-major nahi. Folder 32 example `02` ka
"~7×" ek pure pointer-chase ka ns/line tha — yeh full nested loop hai.
</details>

### 2. Vector of pointers → contiguous
```cpp
std::vector<Order*> book;        // har Order alag new
long total_qty = 0;
for (auto* o : book) total_qty += o->qty;
```
**Target: ~2–3× (AoS)**, aur SoA se **~2–2.4× aur** agar sirf ek field chahiye.
<details><summary>Fix</summary>

`std::vector<Order>` (pointer-chase gaya, contiguous scan). Agar loop sirf `qty`
padhta → SoA: `std::vector<int64_t> qty;` alag — folder 32 example `05` T1/T2:
SoA **~2.0–2.4×** (bandwidth + SIMD). Caveat: agar **poora record** randomly
touch hota → AoS ~3× jeetta (example `05` T3). Access pattern pehle dekho.
</details>

### 3. map lookups in a hot loop
```cpp
std::map<int,int> px_to_qty;      // node per entry, O(log n) pointer chase
for (int t : ticks) acc += px_to_qty[t];
```
**Target: map→sorted vector ~2–3×; vector→direct-index array ~2× aur.**
<details><summary>Fix</summary>

(a) sorted `std::vector<std::pair<int,int>>` + `lower_bound` — ~2 cache lines vs
`log n` scattered node misses (folder 32 `08`: **2–10×**). (b) agar tick range
bounded (e.g. price ±2048 around a reference) → `std::array<int,4096>` indexed by
`tick - base` — ek load, zero branch. HFT order book yehi karta (folder 39).
</details>

### 4. std::endl in a logging loop
```cpp
for (auto& ev : events) log << ev.id << " " << ev.px << std::endl;
```
**Target: ~10–30×** on the logging cost.
<details><summary>Fix</summary>

`std::endl` = `'\n'` **+ flush**. Flush = `write()` syscall har line. `'\n'`
use karo, end pe ek `std::flush`. Better: format into a buffer, ek `write`.
Best (HFT): hot thread ek POD ko SPSC ring pe daale (~20–40 ns), background
thread formats+writes (folder 45 `10`).
</details>

### 5. String concat in a loop
```cpp
std::string out;
for (auto& tok : toks) out = out + tok + ",";   // temp + realloc har baar
```
**Target: ~5× + zero reallocations.**
<details><summary>Fix</summary>

`out.reserve(estimated_total)` once; `out += tok; out += ',';` (no temporary,
amortized-`O(1)` append). `out = out + x` ek naya string banata har iteration
(`O(n²)` total). Ya `std::format_to(std::back_inserter(out), …)`.
</details>

### 6. pow() for small integer powers
```cpp
for (...) result += std::pow(x[i], 2) + std::pow(y[i], 0.5);
```
**Target: ~10×+.**
<details><summary>Fix</summary>

`std::pow(x, 2)` → `x*x`. `std::pow(y, 0.5)` → `std::sqrt(y)`. `pow` ek general
`exp(y*log(x))` routine hai (~50–100 cycles); `mulsd`/`sqrtsd` few cycles.
`-ffast-math` compiler ko yeh khud karne deta par side-effects. Constant integer
exponent → khud likho.
</details>

### 7. Division by a runtime constant
```cpp
for (int i = 0; i < n; ++i) out[i] = a[i] / divisor;   // divisor loop-invariant
```
**Target: const divisor ~13×; power-of-two ~17×; invariant recip-mul ~7×**
(folder 43 `10`, measured).
<details><summary>Fix</summary>

Compile-time constant divisor → compiler magic-number multiply karta (`~0.31
ns/elem` vs `~4.1 ns` div). Power of two → `>> k` (`~0.25 ns`). Runtime-but-loop-
invariant → khud ek reciprocal precompute karo: `R = (1u<<32)/divisor + 1;
out[i] = (uint64_t)a[i]*R >> 32;` (`~0.59 ns`, ~7×). `%` bhi utna hi mehnga —
power-of-two pe `& (d-1)`.
</details>

### 8. Branch on a data-dependent condition (the classic)
```cpp
long s = 0;
for (int v : data) if (v >= 128) s += v;     // data unsorted → ~50% mispredict
```
**Target: ~3–6× (sort first, ya branchless).**
<details><summary>Fix</summary>

Unsorted → branch predictor ~50% galat → har miss ~15–20 cycles. (a) `data` ko
pehle sort karo → predictor ~100% → ~6× (classic StackOverflow demo). (b)
branchless: `s += v & -(long)(v >= 128);` ya `s += (v >= 128) * v;` — compiler
`CMOV`/`SETcc`. Sirf tab jab sort ki cost amortize ho.
</details>

---

## Part B — Abstraction cost

### 9. shared_ptr through the call stack
```cpp
void on_tick(std::shared_ptr<Book> b);   // 5 layers deep, all by value
```
**Target: kill the atomic refcount traffic — ~2×+ and a tighter p99.**
<details><summary>Fix</summary>

By-value `shared_ptr` = `fetch_add`/`fetch_sub` (atomic, `LOCK`-prefixed, cache-
line bounce) har copy pe. Pass `const std::shared_ptr<Book>&` ya (better) `Book*`
/ `Book&` — ownership hot path pe transfer nahi ho raha, sirf use. Owner ko ek
jagah rakho. Tail latency: `delete` non-deterministic ho jab last ref drops in
the hot path.
</details>

### 10. std::function in the inner loop
```cpp
std::function<double(const Tick&)> signal;   // called per tick, no inline
```
**Target: ~2–4× on the call, enables inlining of the body.**
<details><summary>Fix</summary>

`std::function` = type-erased indirect call + possible heap + no inline. Make the
caller a template: `template<class Signal> void run(Signal&& sig)` — direct call,
inlinable. Ya function pointer agar signature fixed. Ya `if constexpr` dispatch
over a small enum of known signals.
</details>

### 11. new/delete per message
```cpp
auto* order = new Order(parse(msg));   // ... later ... delete order;
```
**Target: ~5–20× on the alloc, and no tail spikes.**
<details><summary>Fix</summary>

`malloc`/`free` = lock (or per-thread arena), free-list walk, possible `mmap`.
Object pool (`ObjectPool<Order, N>`, intrusive free list) → `acquire`/`release`
= few instructions, zero syscalls, deterministic. Folder 36 / `46-INTERVIEW-PREP/
examples/05_object_pool.cpp`.
</details>

### 12. SW prefetch — the Rule 2 trap
"Ye lookup loop slow hai, `__builtin_prefetch` daal do."
```cpp
for (int i = 0; i < n; ++i) { __builtin_prefetch(&bucket[h[i+8]]); use(bucket[h[i]]); }
```
**Target: often ~1.15× best case — and up to 3× SLOWER.**
<details><summary>Fix / lesson</summary>

Folder 32 `06` (measured): light body → **~1.15×**; heavy body (64-B bucket scan +
dependent mix) → **~0.33× (3× slower)** — prefetch requests LFB/bandwidth ko
demand loads se compete karte. Modern OoO core apne aap kaafi MLP nikaalta.
**Measure before and after.** Real win: fix the data structure (fewer misses),
not paper over it.
</details>

### 13. Big struct captured by value in a lambda
```cpp
for (...) { std::for_each(v.begin(), v.end(), [cfg](auto& x){ ... }); }  // cfg copied each iter
```
**Target: ~2×** (depends on `sizeof(cfg)`).
<details><summary>Fix</summary>

Capture by reference (`[&cfg]`) ya lambda ko loop ke bahar hoist karo. By-value
capture har baar poora `cfg` copy karta (ctor + possibly heap). Careful: `[&]`
+ async → dangling; yahan synchronous to fine.
</details>

### 14. 12-byte copy via a loop
```cpp
for (int i = 0; i < 12; ++i) dst[i] = src[i];   // byte loop
```
**Target: ~3×.**
<details><summary>Fix</summary>

`std::memcpy(dst, src, 12);` — compiler ek/do word moves mein karta (`mov` 8 +
`mov` 4). Ya agar POD struct hai → `*(Msg*)dst = *(const Msg*)src;` (struct
assignment). `std::bit_cast<Msg>(buf)` for the typed read. Byte loop 12 iterations
+ bounds.
</details>

---

## Part C — Aliasing / atomics / misc

### 15. size()/end() recomputed in the loop
```cpp
for (size_t i = 0; i < v.size(); ++i) out.push_back(f(v[i]));  // out aliases? size() reload
```
**Target: ~1.5–2×.**
<details><summary>Fix</summary>

Compiler `v.size()` ko har iteration reload karta agar prove nahi kar sakta ki
loop body use nahi badalta (aliasing). `const size_t n = v.size();` hoist karo;
`out` ko `reserve` karo (`push_back` ki branch + realloc-check hatao). Ya
`__restrict` pointers / range-`for` (iterator cached). Measure — often modest.
</details>

### 16. Virtual call resolved to one type
```cpp
struct Handler { virtual void on(const Msg&) = 0; };
for (auto& m : msgs) h->on(m);     // h is always ConcreteHandler at runtime
```
**Target: ~2×.**
<details><summary>Fix</summary>

Agar concrete type call site pe pata hai → template / `static_cast` + non-virtual
call. Ya `final` on the class/method + LTO → compiler devirtualizes (indirect
call → direct → inline). Vtable load + indirect call defeats inlining and can
mispredict. CRTP for a vtable-free base.
</details>

### 17. seq_cst config read every iteration
```cpp
for (...) { if (enabled.load()) do_work();  }   // enabled: atomic<bool>, default seq_cst
```
**Target: remove the fences.**
<details><summary>Fix</summary>

Default `load()` = `seq_cst` (on x86 a plain `mov` for loads actually — but the
compiler can't reorder around it, killing optimizations; on ARM it's a real
fence). `enabled.load(std::memory_order_relaxed)` — a flag that doesn't guard
other memory needs no ordering. Better: hoist the load out of the loop if it
can't change mid-batch.
</details>

### 18. vector<bool> in a hot loop
```cpp
std::vector<bool> seen(n);
for (...) if (!seen[x]) { seen[x] = true; ... }   // proxy object, bit ops
```
**Target: ~2–4×.**
<details><summary>Fix</summary>

`std::vector<bool>` = bit-packed, `operator[]` returns a **proxy** (mask/shift per
access, no raw `bool&`). `std::vector<char> seen(n)` — one byte, direct, SIMD-
friendly. Bit-packing sirf tab jeetta jab `n` huge aur memory/bandwidth bound
aur access sequential. For sparse marks → `std::unordered_set` / a stamp array.
</details>

### 19. False sharing between two counters
```cpp
struct { std::atomic<long> a, b; } ctr;   // same 64-B line
// thread 1: ctr.a.fetch_add(1);   thread 2: ctr.b.fetch_add(1);
```
**Target: ~5–10× on the contended case.**
<details><summary>Fix</summary>

Dono atomics ek line pe → har `fetch_add` doosre core ki line invalidate karta
(coherence ping-pong) even though `a` and `b` are logically independent.
`struct alignas(64) Padded { std::atomic<long> v; char pad[56]; }; Padded a, b;`.
Folder 32; `08-lock-free-problems.md` #14/#15.
</details>

### 20. Denormal floats silently 100× slow
```cpp
for (...) { y[i] = a*y[i-1] + x[i]; }   // IIR filter, y decays into denormal range
```
**Target: ~10–100× on the affected region.**
<details><summary>Fix</summary>

Jab values `~1e-308` ke neeche jaate (denormals/subnormals) → many CPUs ek
microcode assist leta → per-op 10–100× slower, non-obvious. Fix: flush-to-zero +
denormals-are-zero (`_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON)` /
`-ffast-math`), ya add a tiny "anti-denormal" DC offset, ya periodically flush
small state to 0. Audio/DSP aur decaying-average signals mein classic.
</details>

---

## The process (har problem pe)

1. **Measure** the baseline — `-O2`, realistic data, stable number (folder 43 `02`).
2. **Profile** — kahan ja raha time? (`perf`, folder 35). Guess mat karo.
3. **One change.** 
4. **Re-benchmark.** Ratio nikaalo.
5. **Explain** kya badla aur kyun — cache misses? branches? syscalls? fences?
6. Agar faayda < noise → revert. Agar ulta → Rule 2, seekho.

## Next
→ [`10-hft-problems.md`](10-hft-problems.md)
