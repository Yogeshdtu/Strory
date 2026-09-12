# 16 — Exercises: compiler optimization

## Prerequisites
- Poora folder 33 (`01`–`15`)

## Kaise use karein
- **Part A** — predict the codegen / the measured effect, then verify
  (`./build.ps1 asm` / `fast`, or godbolt).
- **Part B** — find the bug: each snippet has a compiler-interaction bug (a
  deleted benchmark, a blocked optimization, an unsafe flag).
- **Part C** — reasoning: given an asm/`-fopt-info`/`perf` observation, explain.
- **Part D** — challenge: build + measure, **real numbers** (CLAUDE.md Rule 2),
  and if the compiler/hardware defeats the expectation (like examples `03`
  reduction, `04` inlined-away, `06` carried-loop), that's a finding — write it.

> Is repo ka box AMD Zen 2 ~2 GHz (throttled). **Ratios port, absolutes
> nahi.** `-O2` mandatory; `-O0` benchmarks bekaar (lesson 14).

---

## Part A — Predict

### A1
```cpp
long f() { long s = 0; for (int i = 0; i < 100000000; ++i) s += i * i; return 0; }
```
`-O2` pe `f()` ki asm kya hogi (roughly), aur agar `return s;` kar dein to?

<details><summary>Answer</summary>

`return 0;` — `s` is never used → the whole loop is **dead-code eliminated**
→ `f` compiles to `xor eax, eax ; ret` (return 0). `return s;` — now `s`
escapes, but the bound (1e8) and body are compile-time known → GCC computes
the **closed form** of `sum(i²)` for a constant `n` → `f` becomes `mov rax,
<the constant> ; ret`, still no loop. To actually time the loop you need
`DoNotOptimize` on an **opaque input** (a runtime `n`) *and* on the result
(lesson 14, example `08` cases 1 & 4).
</details>

### A2
Example `03`: MAP loop scalar vs auto-vectorized, plain `-O2`. Expected
speedup? And a float REDUCE (`s += a[i]`) — same expectation?

<details><summary>Answer</summary>

**MAP**: ~**3.5×** on this box (SSE2, 4 floats/vector; measured 0.52 →
0.15 ns/elem). Independent `out[i]` → safe vectorization, no reassociation
needed, plain `-O2` does it. `-march=native` → 8-wide → bigger. **REDUCE**:
**no speedup at plain `-O2`** (measured ~0.72 ns/elem = scalar) — `s += a[i]`
is a float sum; partial-sum vectorization reorders the additions → changes
rounding → the compiler won't do it without `-ffast-math` /
`-fassociative-math` / `#pragma omp simd reduction`. With `-ffast-math`:
**~4×** (verified). PREFIX (`out[i] = out[i-1] + a[i]`): never — true
loop-carried dependency.
</details>

### A3
`void f(float* a, const float* b) { for (int i=0;i<N;++i) a[i] = b[i]*(*b); }`
as a `[[gnu::noinline]]` function vs with `__restrict` on both pointers.
Codegen + ~speedup?

<details><summary>Answer</summary>

Without `__restrict`: `a` could alias `b`, so `*b` (a single scalar) is
**reloaded every iteration** and the loop stays scalar (`movss`/`mulss`) —
the compiler can't prove `a[i] = ...` didn't change `*b`. With `__restrict`
on `a` and `b`: `*b` is hoisted to a register once, and the loop vectorizes
(`mulps`). Measured shape (example `04`, which uses the same idiom): **~3.5×**
(0.43 → 0.12 ns/elem). Note: it *must* be `noinline` (or cross-TU) — inlined,
the compiler sees `f(vec_a.data(), vec_b.data())` and proves non-aliasing
itself, so the gap disappears (that's the example-`04` lesson).
</details>

### A4
`-flto` add karne se example `06` ke throughput loop pe kya, aur uske
*carried* version pe kya (jo pehle wali file mein thi)?

<details><summary>Answer</summary>

**Throughput loop** (`out[i] = hot_transform(in[i] ^ bump)`): LTO inlines
`hot_transform` (defined in `mathx.cxx`) into `main`'s loop → the per-element
`call`/`ret` is gone and the 5-op hash is scheduled with the surrounding
code → measured **1.75 → 0.76 ns/elem (~2.3×)**. **Carried version** (`acc =
hot_transform(acc ^ i)`): **~no difference** — the loop is latency-bound on
the hash's critical path (~5 dependent ops), and the ~2-cycle `call`/`ret`
overhead overlaps with the OoO engine's execution of the previous iteration's
hash. LTO's cross-TU inlining helps where the call is on the *throughput*
path, not where it's hidden behind a dependency chain. (Rule 2 nuance,
lesson 10.)
</details>

### A5
Example `05`: `[[unlikely]]` on a ~1/1000 branch. Measured speedup? What
actually changed?

<details><summary>Answer</summary>

**~1.3×** (measured 0.56 → 0.42 ns/elem) — **small**, because the hardware
branch predictor already predicts a 1/1000 branch ~perfectly without any
hint. What changed (visible in `./build.ps1 asm`): the `slow_path` call moved
**out of the loop body** to a cold region, reached by a rarely-taken forward
`jmp`; the hot path is straight-line fall-through. That layout compaction
matters (I-cache) in a *big* function with many such rare checks; in this
micro-loop it's ~1.3× / noise. The hint changes **layout**, not prediction.
PGO measures the real bias and does this (and more) automatically.
</details>

### A6
`-march=native -O3` on a hot pricing kernel: loop 2× faster, whole process
throughput 5% *slower*, `perf` shows core frequency ~200 MHz lower. Why?

<details><summary>Answer</summary>

The kernel vectorized to **wide SIMD (AVX-512, or heavy 256-bit)** → the
core hit the **AVX frequency offset** (folder 31 lesson 13): running wide
vector instructions forces a lower clock for that core, so *every other*
piece of code on it — parse, book update, network path — runs ~200 MHz
slower. The kernel won 2× locally; the rest of the process lost more. Fix:
`-mprefer-vector-width=256` (keep useful AVX-512 features, cap width → no
downclock), or `-march=x86-64-v3` for that TU, or run the wide-SIMD work on
a dedicated core with no latency-critical code. Always benchmark end-to-end
throughput + P99, not the loop alone (lesson 12).
</details>

---

## Part B — Find the bug

### B1
```cpp
double bench_sqrt(double x) {
    auto t0 = clock();
    for (int i = 0; i < 100'000'000; ++i) x = std::sqrt(x + 1.0);
    auto t1 = clock();
    return double(t1 - t0);
}
```

<details><summary>Answer</summary>

`x` is carried and *returned via the timing*, so it's not fully dead — but
the caller almost certainly ignores `bench_sqrt`'s return value or only uses
the time, so the compiler may still DCE the `x` chain if it can't observe
`x`. Worse: `sqrt` is `const`-attributed; with a **constant** starting `x`
the compiler could fold the whole chain. And there's no warm-up, one run,
`-O` unstated. Fix: pass `x` in from `argv` / a `volatile` (opaque input),
`DoNotOptimize(x)` after the loop, warm-up pass, min-of-N timing, `-O2`,
state the machine. Also decide: are you measuring scalar `sqrtsd` (~15-20
cyc latency, carried → latency-bound) or do you want throughput (independent
`sqrt`s)? Thread the design accordingly (lesson 14).
</details>

### B2
```cpp
// "make the hot loop faster"
#pragma GCC optimize("Ofast")
float energy(const float* v, int n) {
    float e = 0;
    for (int i = 0; i < n; ++i) { if (std::isnan(v[i])) return -1.0f; e += v[i]*v[i]; }
    return e;
}
```

<details><summary>Answer</summary>

`#pragma GCC optimize("Ofast")` = `-O3 -ffast-math`, which includes
**`-ffinite-math-only`** → the compiler assumes no value is ever `NaN` →
`std::isnan(v[i])` is folded to **`false`** → the `return -1.0f` guard is
**deleted**. If a `NaN` reaches this function (bad feed value, an earlier
`0/0`), it now silently propagates: `e += NaN*NaN` → `e` becomes `NaN` →
returned as a valid-looking energy. Fix: remove the pragma. If you need the
reduction to vectorize, do it safely — write N named accumulators in the
source (visible reassociation, plain `-O2` vectorizes it), or `#pragma omp
simd reduction(+:e)` on just that loop — and keep the `isnan` guard compiled
under normal IEEE semantics (lesson 13).
</details>

### B3
```cpp
uint32_t parse_be32(const uint8_t* p) {
    return __builtin_bswap32(*(const uint32_t*)p);
}
```

<details><summary>Answer</summary>

`*(const uint32_t*)p` is **strict-aliasing UB** (reading bytes as a
`uint32_t` lvalue when the object isn't one) — at `-O2` the compiler may
reorder/elide surrounding byte accesses assuming this `uint32_t` read
doesn't alias the `uint8_t` buffer → wrong results in a larger parse
function. It's also potentially **misaligned** (`p` need not be 4-aligned).
Fix: `uint32_t v; std::memcpy(&v, p, 4); return __builtin_bswap32(v);` —
well-defined, alignment-safe, and `-O2` compiles it to a single `mov` (or
`movbe` for load+swap in one). This is the standard wire-parse idiom
(lesson 9, folder 38).
</details>

### B4
```cpp
// kernel, called from another .cpp, no -flto
void saxpy(float* y, const float* x, float a, int n) {
    for (int i = 0; i < n; ++i) y[i] += a * x[i];
}
```

<details><summary>Answer</summary>

Two blockers, both fixable: (1) **Aliasing** — `y` and `x` are plain
pointers; the compiler must assume `y` might alias `x` → conservative,
possibly no vectorization / `a*x[i]` reloads. Fix: `float* __restrict y,
const float* __restrict x`. (2) **Cross-TU, no LTO** — the caller can't
inline `saxpy`, so it stays an opaque `call` and the compiler never sees
that the real arguments are distinct allocations. Fix: `-flto` (so it inlines
across the boundary and proves non-aliasing), or move `saxpy` to a header as
`inline`. Do both: `__restrict` on the API *and* `-flto` on the release
build. Verify with `-fopt-info-vec` that the loop reports "vectorized".
</details>

### B5
```cpp
struct Handler { virtual void on(const Msg&) = 0; };
// ... vector<unique_ptr<Handler>> hs;  filled from a factory with 5 concrete types
for (auto& h : hs)
    for (auto& m : batch)
        h->on(m);                 // hot: batch is large, hs has ~5 entries
```

<details><summary>Answer</summary>

The `h->on(m)` in the inner loop is a **virtual call that won't be
devirtualized** (5 concrete types, heterogeneous, and unless LTO enumerates
them + one dominates, the compiler keeps the indirect `call [vtbl+slot]`).
Per inner iteration: load vptr, load slot, indirect call, and `on()` is
**not inlined** → no const-fold / vectorization of the message-processing
body. The BTB target is stable *within* the `h` loop (same `h` for all of
`batch`), so prediction is fine — the real cost is the non-inlining and the
2 dependent loads. Better: the loop nesting is already good (dispatch once
per `h`, not per `m`); to remove the indirect call, hoist it —
`auto* fn = &h->on; ...` won't help (still a call). Real fixes: a tag
`switch(h->type())` calling each concrete `on` (inlinable), or `variant` +
`visit`, or `-flto` + PGO for speculative devirt on the common type, or
group `batch` handling so each concrete `on` runs a monomorphic inner loop
(folder 32 lesson 10, lesson 7).
</details>

### B6
```cpp
constexpr int SIZE = 1024;
int scan(const std::vector<int>& v) {           // v.size() may differ from SIZE!
    int s = 0;
    for (int i = 0; i < SIZE; ++i) s += v[i];    // -D_GLIBCXX_ASSERTIONS in this build
    return s;
}
```

<details><summary>Answer</summary>

Two problems. (1) **Correctness/UB**: the loop runs to `SIZE` (1024)
regardless of `v.size()` — if `v` is shorter, `v[i]` is out-of-bounds UB
(and with `_GLIBCXX_ASSERTIONS`, it aborts). Use `v.size()` as the bound.
(2) **Optimization**: with `-D_GLIBCXX_ASSERTIONS`, `operator[]` has a
bounds-check branch **per element** → the loop **won't vectorize** (control
flow in the body). For a release build, drop `_GLIBCXX_ASSERTIONS` (keep it
for debug/CI) and use `v.data()` or `[]` without assertions so the dense
loop vectorizes. Fixed: `for (std::size_t i = 0; i < v.size(); ++i) s +=
v[i];` in a release build without assertions → vectorizes and is correct
(lesson 5 trap 3).
</details>

---

## Part C — Reasoning

### C1
`-fopt-info-vec-missed`: `file.cpp:20: missed: not vectorized: possible
aliasing between 'out' and 'w'`. `out` and `w` are separate `std::vector`s.
Two fixes, and why the compiler couldn't tell.

<details><summary>Answer</summary>

The function only sees `float* out` and `const float* w` (or similar) as
parameters — same element type, so **TBAA can't disambiguate them**, and the
compiler has no proof they point at different allocations. It must assume
`out[i] = ...` could modify `*w` / `w[j]` → can't hoist/vectorize. Fix 1:
**`__restrict`** on the pointer parameters — a promise they don't overlap;
the aliasing check disappears. Fix 2: **inline the function** (header /
`-flto`) so the compiler sees the call site `f(out.data(), w.data())` where
`out` and `w` are demonstrably different `std::vector` buffers → it drops
the check itself. (The `noinline` in example `04` is what *forced* the
pessimism to make the demo visible.) For HFT: `__restrict` on the API +
`-flto`.
</details>

### C2
Asm review: a hot loop shows `call _ZSt4sqrtd` (a call to `std::sqrt`) once
per iteration, not the `sqrtsd` instruction. What happened, and how to get
the instruction?

<details><summary>Answer</summary>

`std::sqrt` is being treated as an **opaque library call** rather than
lowered to the `sqrtsd` hardware instruction. Usual cause: **`-fmath-errno`
is in effect** (the default) — the standard says `sqrt` must set `errno` on
domain error (negative input), so the compiler emits a call to the libm
function (which sets `errno`) instead of the bare instruction (which
doesn't). Fix: **`-fno-math-errno`** (the one benign piece of `-ffast-math`
— almost nobody checks `errno` after `sqrt`) → the compiler emits `sqrtsd`
directly (~15-20 cyc, pipelined-ish) and can vectorize it (`sqrtps`).
Alternatively `__builtin_sqrt` / `std::sqrt` under `-ffast-math`. Confirm the
domain: if a negative input is possible you want a NaN, not a trap — bare
`sqrtsd` gives NaN + sets the invalid flag, which is usually what you want.
</details>

### C3
You add `-flto` to a project. Link time goes from 20s to 3 minutes, binary is
8% smaller, throughput +6%, but one integration test now fails with a wrong
computed value. Rank the likely causes and the debugging step for each.

<details><summary>Answer</summary>

**Most likely: a latent ODR violation** exposed by LTO's definition merging
(lesson 10 trap 6) — the same class/`inline` function defined differently in
two TUs (member order, a field, an `enum`, a macro-guarded difference).
Per-TU it was self-consistent; LTO picks one definition and now some code
uses the wrong layout. Debug: turn on **`-Wodr`**, diff the suspect
headers, check per-TU compile flags / `#ifdef`s. **Second: flag
inconsistency across TUs** — `-ffast-math` or `-fno-strict-aliasing` or a
different `-march` in one TU; under LTO one setting wins and semantics
change. Debug: audit the build for per-TU flag differences; unify them.
**Third: a pre-existing strict-aliasing / UB bug** that only bit once the
cross-TU inlining removed an accidental optimization barrier. Debug: run the
failing path under UBSan / `-fsanitize=undefined` (Linux), and ASan. The
+6%/-8% are LTO working; the test failure is a real bug LTO surfaced — fix
the bug, keep LTO. For the link time: `-flto=auto` / ThinLTO + a cache.
</details>

### C4
A benchmark of `mycopy(dst, src, n)` (your hand-rolled copy) reports the same
ns whether `n` is 64 or 64000. No barrier bug (you have `ClobberMemory`).
What's the compiler most likely doing?

<details><summary>Answer</summary>

The compiler recognized the copy loop as a **`memcpy` idiom** and replaced
your loop with a `call memcpy` (or an inlined `rep movsb` / `__memmove`) —
GCC/Clang do this aggressively (`-ftree-loop-distribute-patterns`). So you're
not benchmarking *your* `mycopy` at all; you're benchmarking libc's
`memcpy`, whose per-call overhead dominates for small `n` and whose
bandwidth dominates for large `n` — and if `dst`/`src` are the same across
reps and small enough, the whole thing may be near-free from the L1-resident
fast path, making 64 and 64000 look similar after the `ClobberMemory` fence
per rep serializes but doesn't add much. Fixes to benchmark *your* code:
`[[gnu::noinline]]` + `-fno-tree-loop-distribute-patterns` on that TU (or
`#pragma GCC optimize`), perturb `src`/an offset per rep so reps differ, and
scale `n` while watching ns/byte (should be flat in the bandwidth regime,
rising per-byte as fixed overhead amortizes for small `n`). Also just: if
you want a fast copy, *use* `std::memcpy` — the compiler already picked it.
</details>

---

## Part D — Challenge (build + measure, real numbers)

### D1 — The -O ladder on three loop shapes
Take three loops: (a) `c[i] = a[i] + b[i]` (map), (b) `s += a[i]` (float
reduce), (c) `n = list->next` pointer chase over a `vector<Node>` arena.
Compile each at `-O0/-O1/-O2/-O3/-O3 -march=native/-O3 -ffast-math`, measure
ns/elem. Tabulate. You should see: (a) big `-O0→-O2` + more from `-march`;
(b) nothing until `-ffast-math`; (c) only the `-O0→-O1` cliff, then flat
(latency-bound). Write which flag mattered for which shape and why.

### D2 — `__restrict` sweep
Write `stencil(out, in, w, n)` (`out[i] = w[0]*in[i-1] + w[1]*in[i] +
w[2]*in[i+1]`) as `[[gnu::noinline]]`. Measure ns/elem: (1) plain pointers,
(2) `__restrict` on `out`+`in`, (3) also `__restrict` on `w`, (4) plus
`-march=native`, (5) inlined (remove `noinline`) with plain pointers.
Explain (5) — the inlined version with plain pointers should match or beat
the `__restrict` `noinline` one, because the compiler proves non-aliasing
from the call site (the example-`04` lesson). Real numbers.

### D3 — LTO on a realistic split
Split a small "engine" into 4 TUs: `main`, `book` (a flat price-level array +
`apply(update)`), `parser` (`decode(bytes) -> update`), `risk`
(`check(order) -> bool`). `main`'s hot loop: `decode → apply → check`.
Build (a) no LTO, (b) `-flto`, (c) `-flto` + PGO (train on a generated
message stream). Measure ns/message and binary size for each. You should see
LTO give a few % (cross-TU inlining of `decode`/`apply`/`check`) and PGO a
bit more (branch layout in `decode`). Note honestly if the effect is in the
noise — a 4-function toy may not show much (lesson 10/11: the gain scales
with code size/branchiness).

### D4 — Auto-vec vs `#pragma omp simd` vs manual accumulators for a dot product
`float dot(const float* a, const float* b, int n)`. Four versions: plain
loop, `#pragma omp simd reduction(+:s)` (+ `-fopenmp-simd`), 8 named
accumulators in source, `_mm256_fmadd_ps` intrinsics. Measure ns/elem at
`-O2` and `-O2 -march=native`. Compare the **results** too (print all four
sums to full precision) — the plain and the intrinsic/manual versions differ
in the last ULPs because of summation order. Write the speed/accuracy
trade-off table.

### D5 — Devirtualization: `final` and LTO
`struct Shape { virtual double area() const = 0; };` with `Circle`, `Square`
(both `final`). A `std::vector<std::unique_ptr<Shape>>`, sum the areas in a
loop. Measure ns/shape for: (a) as written, (b) `-flto`, (c) replace with
`std::variant<Circle,Square>` + `std::visit`, (d) CRTP. Also `./build.ps1
asm` each and note whether you see `call [reg]` (indirect), a type-check
chain (speculative devirt), a jump table (`visit`), or fully inlined (CRTP).
Real numbers + the asm observation for each.

---

## Interview questions (folder-wide)

1. `-O0` `-O1` `-O2` `-O3` `-Os` `-Ofast` — one line each; production default; when `-O3` loses.
2. `inline` keyword's real meaning; what actually decides inlining; `always_inline`/`noinline` uses.
3. 4 loop transforms the compiler applies, and what blocks each.
4. Conditions for auto-vectorization; why a float reduction is special.
5. `-fopt-info-vec` / `-fopt-info-vec-missed` / `-fopt-info-inline` — reading them.
6. Constant folding vs propagation vs DCE; `const` vs `constexpr` vs `consteval`.
7. Devirtualization — when it fires, `final`, speculative devirt, LTO.
8. `[[likely]]`/`[[unlikely]]` — layout not prediction; why the effect is small; PGO.
9. Aliasing — how it blocks optimization; `__restrict` (unchecked promise); alternatives.
10. LTO — what boundary it removes; 4 optimizations it unlocks; ThinLTO; the ODR-exposure risk.
11. PGO — the 3 steps; what it improves; representative profile; AutoFDO.
12. `-march` vs `-mtune`; x86-64-v1..v4; why `native` is wrong for a shipped binary; multi-versioning.
13. `-ffast-math` sub-flags; `-ffinite-math-only` danger; safe scoped alternatives.
14. Benchmark barriers — `DoNotOptimize`/`ClobberMemory`; why they emit zero instructions; placement.
15. Asm-review checklist; vectorized vs scalar signature; `idiv` in a loop; asm-right-time-unchanged.

---

## Next
→ [`../34-ASSEMBLY/00-README.md`](../34-ASSEMBLY/00-README.md)
