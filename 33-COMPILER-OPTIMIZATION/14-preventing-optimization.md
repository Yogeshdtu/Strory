# 14 — Preventing optimization in benchmarks: `volatile`, `DoNotOptimize`, barriers

## Prerequisites
- `06-constant-folding.md` (why the compiler deletes your loop)
- `31-CPU-ARCHITECTURE` / `32-CACHE-MEMORY-PERFORMANCE` (the `keep()` helper used throughout)
- `08_benchmark_barriers.cpp` example

## Yeh topic abhi kyun
Har micro-benchmark ki #1 bug: **compiler ne woh kaam delete kar diya jo tum
naap rahe the.** Result unused → dead code elimination. Loop's output is a
closed form → the loop is replaced by a constant. Loop-invariant call →
hoisted out of the timed region. Tumhe measured `0.00 ns` milta aur tum sochte
ho code infinitely fast hai. Yeh lesson batata exactly kaunse barriers use
karo, kya woh emit karte (ideally: **zero instructions**), aur kahan lagao.

---

## Kya compiler karta tumhare benchmark ko

| Compiler move | Symptom | Fix |
|---|---|---|
| **DCE** — result unused | loop gayab, `0.00 ns` | `DoNotOptimize(result)` |
| **closed-form** — `sum(1..n)` → `n(n+1)/2` | loop gayab even with a sink | `DoNotOptimize(input)` too |
| **LICM / CSE** — pure call, constant args | call runs once, not REPS times | thread a per-iteration `carry`, or `DoNotOptimize` the args each iter |
| **constant-fold** — all inputs `constexpr`/known | whole thing → a `mov` | make an input runtime + opaque |
| **dead-store elimination** — you write, never read | store loop gone | `ClobberMemory()` after the writes |
| **if-conversion** — `if (c) s += x` → `cmov` | branch-prediction demo shows no effect | `#pragma GCC optimize("no-if-conversion")` (folder 31/32) |
| **auto-vectorization** you didn't want (for a scalar baseline) | "scalar" is actually SIMD | `__attribute__((optimize("no-tree-vectorize")))` |
| **whole function inlined + specialized away** | can't even find it in asm | `[[gnu::noinline]]` on the thing under test |

---

## The barrier toolkit

### `DoNotOptimize(x)` — the Google Benchmark idiom

```cpp
template <class T>              // read a value -> force it to be computed
static inline void DoNotOptimize(T const& v) {
    asm volatile("" : : "r,m"(v) : "memory");
}
template <class T>              // read+write -> also force it into memory/register
static inline void DoNotOptimize(T& v) {
    asm volatile("" : "+r,m"(v) : : "memory");
}
```
- `asm volatile("")` — an empty inline-asm the compiler can't reorder or
  remove.
- `"r,m"(v)` input constraint — "this asm reads `v`" → the compiler must
  have `v`'s value ready → **can't DCE the computation that produced it**.
- `"memory"` clobber — "this asm may read/write any memory" → the compiler
  can't hoist prior stores past it.
- **Emits zero machine instructions.** Pure information to the optimizer.

This is exactly the `keep()` used all over folders 31/32.

### `ClobberMemory()` — for writes

```cpp
static inline void ClobberMemory() { asm volatile("" : : : "memory"); }
```
"All memory may have changed." Use **after** a fill/store loop so the
compiler keeps the stores (their "use" is that memory is now observable).

### `volatile` sink — simpler, small cost

```cpp
volatile int sink;
for (...) sink = compute();     // each iteration MUST store to sink
```
- `volatile` = "every access is observable, don't elide/reorder/coalesce".
- Works, but adds **one real store per assignment** — negligible for a big
  loop body, noise for a tiny one. `DoNotOptimize` has no such cost.
- `volatile` on the *accumulator* (`volatile long s; for (i) s += a[i];`) is
  worse — every `+=` becomes load-add-store to memory, distorting the
  measurement. Use `DoNotOptimize(s)` once after the loop instead.

### `benchmark::DoNotOptimize` / `benchmark::ClobberMemory` (the library)
If you use Google Benchmark, these are provided (and portable to MSVC via
`_ReadWriteBarrier` / a volatile trick). Same semantics.

---

## Measured — example `08` (is box, ~2 GHz)

`sum_squares(200M)` in a 5-rep loop:
```
  1. no barrier          :  0.00 ms   <- loop DELETED
  2. volatile sink       : 48.7  ms
  3. DoNotOptimize(result): 48.8  ms   <- correct, zero overhead
  4. DoNotOptimize(in+out): 48.4  ms   <- also opaque input (no closed-form shortcut)
  5. vector fill + ClobberMemory: 0.18 ms/rep   <- stores retained
```
Case 1 vs the rest: `0.00` vs `~48 ms`. Without the barrier you'd conclude
"summing 200M squares is free."

---

## Where to put the barriers (recipe)

```cpp
auto t0 = clock::now();
for (int r = 0; r < REPS; ++r) {
    DoNotOptimize(input);          // <- input opaque: no const-fold / closed-form
    auto result = thing_under_test(input);
    DoNotOptimize(result);         // <- output escapes: computation can't be DCE'd
}
auto t1 = clock::now();
// ... for a function that WRITES a buffer:
for (int r = 0; r < REPS; ++r) {
    fill_buffer(buf);
    ClobberMemory();               // <- stores are observable
}
```
Also: `[[gnu::noinline]]` on `thing_under_test` if you need it opaque (so it
isn't inlined + specialized away), and `-O2` minimum (a `-O0` benchmark
measures stack traffic — folders 31/32).

For **latency vs throughput** (folder 31 lesson 04): a *carried* dependency
(`x = f(x)`) measures latency; *independent* iterations measure throughput.
Thread `x` through so the loop can't be parallelized/hoisted when you want
latency.

---

## What barriers do NOT do

- They don't make the benchmark **realistic** — you still need a
  representative input, a warm cache (or a deliberately cold one), enough
  iterations to dwarf timer overhead, and multiple runs (median/min — folder
  32 example 01 uses min-of-N).
- They don't stop **the CPU** from doing OoO / prefetch / prediction — that's
  the real hardware you're measuring.
- `DoNotOptimize` on a pointer (`DoNotOptimize(v.data())`) forces the pointer
  to be live, not the buffer contents — use `ClobberMemory()` for contents.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — no barrier at all
`for (i) s += i*i;` with `s` unused → `0.00 ns`. The single most common
benchmark bug.

### Trap 2 — sink only, forgot the input
The loop's result is a closed form of a *known* input → compiler computes it
once. `DoNotOptimize` the input too.

### Trap 3 — `volatile` on the accumulator
`volatile long s;` → every `+=` is a memory round-trip → you measure store
latency, not the computation. Barrier *after* the loop instead.

### Trap 4 — barrier inside the timed loop when measuring throughput
An `asm volatile("" ... "memory")` per iteration can create a scheduling
fence that serializes independent work. For throughput, one `DoNotOptimize`
after the loop (on a carried-forward accumulator) is enough.

### Trap 5 — forgetting `[[gnu::noinline]]`
The function you're timing gets inlined into the loop, specialized on the
constant input, and folded → you time nothing. `noinline` + opaque input.

### Trap 6 — `-O0` "to be safe from the optimizer"
Now you measure stack spills everywhere — worse than useless. `-O2` +
barriers is the only correct combo (folders 31/32 learned this repeatedly).

---

## > **HFT relevance**

> - **Every internal micro-benchmark uses `DoNotOptimize` / `ClobberMemory`**
>   (or Google Benchmark). A "0 ns" result is a bug in the benchmark, not a
>   fast function.
> - **Opaque input + escaped output + `[[gnu::noinline]]` + `-O2`** is the
>   template. Plus min-of-N or median timing on a frequency-noisy box (folder
>   32 example 01), and a note of the machine.
> - **Latency vs throughput** — carry the accumulator for latency, keep
>   iterations independent for throughput (folder 31 lesson 04). Report both
>   for a hot primitive.
> - **The real number comes from a replay** — a micro-benchmark validates a
>   change's *direction*; the P50/P99 on a captured trading session is the
>   decision metric (folder 24 lesson 15, folder 33 lesson 11).
> - **`keep()` in this repo** = `DoNotOptimize`. Same idiom, used in every
>   folder-31/32 example.

---

## Hands-on

```bash
./build.ps1 fast 33-COMPILER-OPTIMIZATION/examples/08_benchmark_barriers.cpp
#   case 1 = 0.00 ms  ;  cases 2-4 = ~48 ms  ;  case 5 = 0.18 ms/rep

# see the deletion:
./build.ps1 asm 33-COMPILER-OPTIMIZATION/examples/08_benchmark_barriers.cpp | head -40
#   the no-barrier case has no loop; the DoNotOptimize case has the imul/add loop

# confirm the barrier emits nothing:
echo 'void k(long& x){asm volatile("":"+r,m"(x)::);} long f(){long s=0;for(int i=0;i<9;++i)s+=i;k(s);return s;}' \
  | g++ -O2 -S -masm=intel -xc++ - -o - | grep -A6 'f():'
#   -> the loop is folded to a constant (9 known), but with a runtime/opaque input
#      the loop stays and the asm("") produces ZERO instructions
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "my function does 200M sums in 0 ns" | the loop was deleted — add `DoNotOptimize` |
| "a `volatile` result variable is enough" | works, but adds a store/iter; also opaque the input |
| "`volatile` accumulator" | measures memory round-trips, not compute |
| "barrier per iteration" | for throughput, one after the loop; per-iter can serialize |
| "`-O0` avoids optimizer tricks" | measures stack traffic; `-O2` + barriers |
| "`DoNotOptimize(ptr)` keeps the buffer" | keeps the pointer live; `ClobberMemory()` for contents |

---

## Exercises

1. `double t0=now(); double x=0; for(i<N) x += std::sin(i*0.001); double
   t1=now(); print((t1-t0)/N);` — is benchmark mein kya-kya galat, sab fix
   karo.

   <details><summary>Answer</summary>

   (1) **`x` is never used** → the whole loop is dead → likely `0 ns`. Add
   `DoNotOptimize(x)` after the loop. (2) **`i*0.001` and the loop bound are
   compile-time known** → with `-ffast-math` or aggressive folding the
   compiler could precompute; even without, `sin` is `const`-attributed so
   the compiler *could* hoist/CSE. Make the step opaque:
   `DoNotOptimize(step)` where `double step` is read from argv or a
   `volatile`. (3) **No warm-up** — first iterations include page faults /
   cache-cold `libm`. Do a throwaway pass. (4) **One run** — frequency noise;
   do min-of-N or median. (5) **`-O` not stated** — must be `-O2`+. (6)
   Depending on flags `sin` may or may not be vectorized (libmvec) — decide
   and state which you're measuring. Fixed: warm-up, opaque `step`,
   `DoNotOptimize(x)`, `-O2`, median of 10, note the machine.
   </details>

2. Tum ek `memcpy`-like `mycopy(dst, src, n)` ko benchmark kar rahe ho. Bina
   kisi barrier ke bhi loop delete nahi hua — kyun? Aur kaunsa barrier phir
   bhi chahiye aur kyun?

   <details><summary>Answer</summary>

   The loop isn't deleted because it **writes to memory** (`dst[i] = src[i]`)
   — those stores are a side effect the compiler must preserve *if* `dst`
   could be observed later. But it can still: (a) **hoist the copy out of
   your REPS loop** if `dst`/`src` don't change between reps (the result is
   the same each time) → you time one copy, not REPS; (b) **replace your loop
   with a call to the libc `memcpy`** (often faster — that's fine, but know
   it); (c) if `dst` is a local that's never read after, **DCE the whole
   thing** as a dead store. Fix: `ClobberMemory()` after each `mycopy` call
   (makes `dst` "observed" each rep so it can't hoist), and/or
   `DoNotOptimize(dst[some_index])` to force a read, plus perturb `src` or an
   offset per rep so reps aren't identical. Also `[[gnu::noinline]]` on
   `mycopy` so it isn't turned into `memcpy` if you're specifically
   benchmarking *your* implementation.
   </details>

3. `keep(acc)` (this repo's `DoNotOptimize`) ek carried loop ke **andar** har
   iteration call karne se kya hota, aur throughput benchmark mein yeh
   problem kyun?

   <details><summary>Answer</summary>

   Calling `keep(acc)` every iteration forces `acc` to be materialized in a
   register/memory *at that point* every time, and the `"memory"` clobber
   acts as a scheduling fence. For a **latency** benchmark (carried chain
   `acc = f(acc)`) that's fine and often intended — it pins the dependency.
   For a **throughput** benchmark where iterations are supposed to be
   *independent* (`out[i] = f(in[i])`), a per-iteration `keep` serializes
   them: the compiler/CPU can no longer overlap iteration `i+1`'s work with
   `i`'s, because the fence says "everything before me must be done". You'd
   measure a latency-bound number instead of the throughput you wanted. Fix:
   accumulate into a cheap running value (e.g. `sink ^= out[i]`) and call
   `keep(sink)` **once after** the loop; keep the loop body free of barriers.
   (Folder 31 example 02 hit exactly this — an array + `keep(v[j])` per iter
   forced memory ops and broke the ILP demo; the fix was named scalars +
   one `keep` at the end.)
   </details>

---

## Interview questions

1. 4 ways the compiler can invalidate a naive micro-benchmark.
2. `DoNotOptimize(x)` — how the `asm volatile` + constraints + `"memory"`
   clobber work, and why it emits zero instructions.
3. `ClobberMemory()` — when you need it (writes).
4. `volatile` sink vs `DoNotOptimize` — the cost difference.
5. Why `volatile` on the accumulator is wrong.
6. Latency vs throughput benchmark — carried vs independent iterations.
7. Barrier placement — why not inside a throughput loop.

---

## Next
→ [`15-reading-optimized-output.md`](15-reading-optimized-output.md)
