# 08 — Microbenchmarking: Google Benchmark

## Prerequisites
- `02-timing-correctly.md`, `04-statistics.md`
- `33-COMPILER-OPTIMIZATION/14-preventing-optimization.md` (`DoNotOptimize`)
- `examples/05_google_benchmark/` (shim + `bench.cxx`)

## Yeh topic abhi kyun
Ek chhote code snippet ka number chahiye — "yeh hash function kitna ns?",
"`std::sort` vs `std::stable_sort` yahan?". Haath se timing loop likhna
possible hai (lesson 02), par phir har baar warm-up, min-of-N, sink,
iteration-count tuning khud karna padta. **Google Benchmark** yeh sb
handle karta — industry standard microbenchmark library. Yeh lesson: uska
model, kaise likho, aur woh galtiyan jo library bhi nahi rok sakti.

---

## Model: `State` ka loop

```cpp
#include <benchmark/benchmark.h>

static void BM_something(benchmark::State& state) {
    // --- SETUP (timed nahi) ---
    std::vector<int> data = make_data();

    for (auto _ : state) {
        // --- yeh body har iteration chalti, YEH timed hai ---
        int r = process(data);
        benchmark::DoNotOptimize(r);
    }

    // --- TEARDOWN (timed nahi) ---
    state.SetItemsProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_something);

BENCHMARK_MAIN();
```

- `for (auto _ : state)` — library **khud decide karta kitni iterations**
  (chhote trials chalake), jab tak stable measurement na mile (default
  ~0.5s aggregate). Tumhe iteration count nahi likhna.
- Loop ke **bahar** ka code = per-benchmark setup, timed nahi.
- Loop ke **andar** = measured. Isko chhota aur focused rakho.
- `benchmark::DoNotOptimize(x)` / `benchmark::ClobberMemory()` — warna `-O2`
  body delete kar deta (lesson 09, folder 33/14).

Build (real library):
```bash
g++ -std=c++20 -O2 bench.cpp -lbenchmark -lpthread -o bench
./bench
./bench --benchmark_repetitions=10 --benchmark_report_aggregates_only=true
./bench --benchmark_filter='BM_sort.*' --benchmark_format=json > out.json
```

> Is repo ke box pe library installed nahi — `05_google_benchmark/` mein ek
> **`minibench.hpp` shim** hai jo API ka subset deta, taaki example bina
> dependency compile + chale. `bench.cxx` mein sirf `#include` badalne se
> asli library pe switch ho jaata. Shim mein **repeats/stddev/CPU-time/JSON
> nahi** — woh asli library ka value.

---

## Arguments: ek benchmark, kai input sizes

```cpp
static void BM_memcpy(benchmark::State& state) {
    const size_t n = state.range(0);              // <- current arg
    std::vector<char> src(n, 'a'), dst(n);
    for (auto _ : state) {
        std::memcpy(dst.data(), src.data(), n);
        benchmark::DoNotOptimize(dst.data());
        benchmark::ClobberMemory();
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * n);
}
BENCHMARK(BM_memcpy)->RangeMultiplier(8)->Range(64, 1 << 16);
// -> chalta n = 64, 512, 4096, 32768, 65536

BENCHMARK(BM_foo)->Arg(1000)->Arg(1000000);       // do specific sizes
BENCHMARK(BM_2d)->Args({16, 1024})->Args({256, 64});  // 2 args: range(0), range(1)
BENCHMARK(BM_bar)->DenseRange(0, 10, 2);          // 0,2,4,6,8,10
```

`SetBytesProcessed` / `SetItemsProcessed` → output mein `bytes_per_second`
/ `items_per_second` column, jo different sizes ke beech comparison easy
karta (ns/iter alag hoga par GB/s comparable).

**Measured (`05_google_benchmark` shim, is box):**
```
BM_memcpy/4096         32 ns     128 GB/s
BM_manual_copy/4096  1003 ns       4 GB/s     <- byte loop, ~30x slower than memcpy
BM_StringCopy/8         3 ns   (SSO, no heap)
BM_StringCopy/64       49 ns   (past SSO -> heap alloc dominates)
```

---

## `PauseTiming` / `ResumeTiming` — per-iteration setup

Kabhi har iteration ko fresh state chahiye (jaise `sort` ko unsorted data):

```cpp
static void BM_sort(benchmark::State& state) {
    std::vector<int> v(state.range(0));
    std::iota(v.begin(), v.end(), 0);
    std::mt19937 rng(42);
    for (auto _ : state) {
        state.PauseTiming();                     // shuffle timed nahi
        std::shuffle(v.begin(), v.end(), rng);
        state.ResumeTiming();

        std::sort(v.begin(), v.end());
        benchmark::DoNotOptimize(v.data());
    }
}
```

⚠️ `PauseTiming`/`ResumeTiming` **khud mehenge** (~ hundreds of ns each,
they read the clock). Agar tumhara timed body bhi ~100 ns hai, pause/resume
ka overhead dominate karega. Tab: setup ko **batch** karo (N pre-shuffled
copies bana lo loop ke bahar, phir loop mein index se pick), ya ek alag
approach.

---

## Fixtures — shared setup across benchmarks

```cpp
class MyFixture : public benchmark::Fixture {
public:
    std::vector<uint64_t> data;
    void SetUp(const benchmark::State&) override {
        data.resize(1 << 16);
        std::mt19937_64 rng(7);
        for (auto& x : data) x = rng();
    }
    void TearDown(const benchmark::State&) override { data.clear(); }
};

BENCHMARK_F(MyFixture, SumReduce)(benchmark::State& state) {
    for (auto _ : state) {
        uint64_t s = 0;
        for (uint64_t x : data) s += x;
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK_F(MyFixture, XorReduce)(benchmark::State& state) { /* ... */ }
```

`SetUp` har benchmark ke liye chalta (har arg ke liye bhi). Bade shared data
ke liye handy — repeat nahi karna padta.

---

## Aur useful bits

| Feature | Kya |
|---|---|
| `--benchmark_repetitions=N` | poora benchmark N baar → `_mean`, `_median`, `_stddev`, `_cv` rows |
| `--benchmark_min_time=2s` | har benchmark kam se kam 2s chalao (stable) |
| `benchmark::DoNotOptimize` | value ko "escape" — compute zaroori, 0 instr |
| `benchmark::ClobberMemory` | "saari memory badli" — pending stores retained |
| `state.SetLabel("...")` | output row mein ek note |
| `state.counters["hits"] = ...` | custom counters (rate/avg) — `--benchmark_counters_tabular` |
| `->Threads(N)` / `->ThreadRange(1,8)` | multi-threaded benchmark, contention measure |
| `->Complexity(benchmark::oN)` | multiple sizes se BigO fit karke report |
| `->UseManualTime()` | tum khud `state.SetIterationTime(sec)` do (GPU, async) |
| `RegisterBenchmark("name", fn)` | runtime pe register (loop se generate) |
| `BENCHMARK(fn)->Name("nice")` | display name override |

---

## Common mistakes (jo library bhi nahi rokti)

1. **No barrier** → body deleted. `05` ka `BM_reduce_NO_barrier` measured
   **0.49 ns/iter** (loop gone) vs `BM_reduce_WITH_barrier` **1030 ns/iter**.
   Library `DoNotOptimize` deta hai — par tumhe **lagana** padta hai.
2. **Constant / compile-time-known input** → const-folded. Input ko
   `state.range(0)` se ya ek runtime source se lo (lesson 09).
3. **Loop-invariant body** → hoisted out of the `for(_ : state)` loop.
   Body ko iteration pe depend karao (`data[i % n]`, ya carry a value).
4. **Setup inside the timed loop** — allocation, `make_data()` har
   iteration → tum allocation naap rahe. Loop ke bahar, ya `PauseTiming`.
5. **`PauseTiming` overhead > body** — chhote body pe pause/resume ka clock
   read dominate. Batch the setup instead.
6. **`SetItemsProcessed` bhoolna** jab sizes compare kar rahe ho → ns/iter
   se apples-to-oranges. Rate column add karo.
7. **`-O0` / debug build** — abstraction penalty, no inline → numbers
   meaningless. Hamesha `-O2`+ (`05` ka build script `-O2`).
8. **CPU-time vs wall-time confuse** — GB dono deta. Multi-threaded / sleep
   wale benchmarks mein `CPU` (per-thread sum) aur `Time` (wall) alag; sahi
   wala padho.
9. **Frequency not pinned** — turbo ramp / thermal → 10–20% run-to-run.
   `--benchmark_repetitions` + `_cv` dekho; `_cv > 5%` → machine tune karo
   (lesson 09, folder 31/13).
10. **Benchmark ≠ reality** — L2-resident micro-bench, real program L3-miss
    bound → number 3–10x off in situ (lesson 01 trap 2). Micro-bench se
    hypothesis, profiler se in-context confirm.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `for (auto _ : state)` ke bahar bhi `DoNotOptimize` bhoolna
Loop ka result loop ke bahar bhi use / escape hona chahiye warna poora loop
(saari iterations) fold ho sakta.

### Trap 2 — `state.range(0)` ko `int` maan lena
Woh `int64_t` deta. `size_t` chahiye to `static_cast<size_t>(state.range(0))`
— warna `-Wsign-conversion` / silent wrap on huge args.

### Trap 3 — fixture `SetUp` cost ko benchmark maan lena
`SetUp` timed nahi hai, par agar woh 5s leta to `--benchmark_min_time`
tuning frustrating. Heavy shared data → static / once.

### Trap 4 — repetitions ke bina regression detect karna
1 run ka number ±10% noise. CI mein `--benchmark_repetitions=10
--benchmark_report_aggregates_only=true`, phir `_median` compare + `_cv`
gate.

### Trap 5 — `Threads()` benchmark ka number galat padhna
Multi-threaded benchmark mein `Time` = wall (all threads overlap),
`items_per_second` aggregate. Per-thread cost = `CPU` column ya
`Time × threads`. Contention `Time` ke growth se dikhta.

---

## Hands-on

```bash
cd 35-PROFILING-BENCHMARKING/examples/05_google_benchmark
./build.ps1        # (ya ./build.sh) — shim se compile + run
```
Output mein dekho:
- `BM_reduce_NO_barrier` ~0.5 ns (deleted) vs `_WITH_barrier` ~1030 ns
- `BM_memcpy` vs `BM_manual_copy` — ~30x (intrinsic vs byte loop)
- `BM_StringCopy/8` (SSO, ~3 ns) vs `/64` (heap, ~49 ns) — allocation cliff
- `BM_sort_shuffled` — `PauseTiming` se shuffle excluded

Asli library ke saath: `#include <benchmark/benchmark.h>`, `-lbenchmark
-lpthread`, phir `--benchmark_repetitions=10` se stddev/cv bhi milega.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Google Benchmark sab handle karta" | iteration count / warm-up haan; barriers / input opacity tum |
| "`for (auto _ : state)` = N fixed iterations" | library adaptively decide karta |
| "setup loop ke andar OK" | loop ke bahar ya `PauseTiming` — warna allocation naapte |
| "ek run kaafi" | `--benchmark_repetitions` + median + cv |
| "`-O0` se bhi relative comparison valid" | nahi — abstraction penalty distorts |
| "micro-bench number = production number" | working-set / context alag → in-situ verify |

---

## Exercises

1. Yeh benchmark 0.0 ns/iter report karta. Do bugs hain. Dono batao aur fix.
   ```cpp
   static void BM_pow(benchmark::State& state) {
       for (auto _ : state) {
           double r = std::pow(2.0, 10.0);
       }
   }
   BENCHMARK(BM_pow);
   ```
   <details><summary>Answer</summary>

   **Bug 1 — no sink:** `r` kahin use nahi → `-O2` poora body delete →
   loop empty → 0 ns. Fix: `benchmark::DoNotOptimize(r);`.
   **Bug 2 — constant input:** `std::pow(2.0, 10.0)` dono args compile-time
   constants, `pow` `constexpr`-evaluable (ya compiler ka builtin folds it)
   → `r = 1024.0` compile time pe, koi runtime `pow` call hi nahi. Fix:
   ek arg ko opaque karo — `double base = state.range(0); DoNotOptimize(base);
   double r = std::pow(base, 10.0);` ya `state.range` se exponent bhi. Dono
   fix ke baad tum asli `pow` ki latency naapoge (~tens of ns).
   </details>

2. `BM_sort` mein har iteration `std::vector<int> v(1000000);` loop ke andar
   banta hai, fir shuffle, fir sort. Reported time "sort" se 5x zyada aa
   raha expected se. Kya ho raha, kaise theek?

   <details><summary>Answer</summary>

   Har iteration mein timed body ke andar: (a) **`std::vector<int>
   v(1000000)`** — 4 MB `malloc` + **zero-fill** (~value-init), (b)
   `std::iota` ya fill, (c) `std::shuffle` — 1M swaps + 1M RNG calls, (d)
   `std::sort`. Tum in **chaaron** ko naap rahe ho, sirf sort nahi.
   Allocation + zero + shuffle >> sort ke comparison-swaps for already-random
   data → 5x inflation. Fix: `v` ko loop ke **bahar** banao (ek baar), aur
   shuffle ko `state.PauseTiming()` / `ResumeTiming()` ke beech — sirf `sort`
   + `DoNotOptimize` timed. Agar pause/resume overhead bhi problem (sort
   chhota hai chhote n pe), to loop ke bahar `k` pre-shuffled copies banao
   aur `for(_ : state)` mein `copies[i++ % k]` sort karo.
   </details>

3. Ek benchmark `--benchmark_repetitions=20` pe `_mean = 500 ns`, `_median
   = 460 ns`, `_stddev = 120 ns`, `_cv = 24%`. Number kitna trust karein?
   Kya karein?

   <details><summary>Answer</summary>

   `_cv = 24%` **bahut high** — clean microbenchmark `< 2–5%` hota. Yeh
   number abhi **trustworthy nahi**; mean > median (kuch runs bahut slow) →
   external interference. Karein: (1) **machine tune** — frequency governor
   `performance`, turbo off (ya locked), thermal headroom, background load
   band. (2) **pin** — `taskset -c 3 ./bench` isolated core pe. (3) **more
   min_time** — `--benchmark_min_time=2s` har rep zyada stable. (4)
   `--benchmark_repetitions` badhao (50) aur **`_median`** pe focus (mean se
   robust). (5) check: benchmark khud non-deterministic to nahi (data-
   dependent branches, alloc). Tuned box pe `_cv` `< 3%` aana chahiye; tab
   `_median` ko baseline maano, aur regressions ko **relative** (± X% vs
   rolling baseline) check karo, absolute nahi.
   </details>

---

## Interview questions

1. Google Benchmark ka `State` loop model — iteration count kaun decide karta.
2. `DoNotOptimize` vs `ClobberMemory` — kya farak, kab kaunsa.
3. `->Range()` / `->Args()` / `SetBytesProcessed` — kis liye.
4. `PauseTiming`/`ResumeTiming` — use aur uska apna overhead.
5. Library ke bawajood microbenchmark mein kaunsi 4 galtiyan possible hain.
6. `--benchmark_repetitions` aur `_cv` — regression testing mein kaise use.
7. Microbenchmark ka number production se kyun alag ho sakta — 3 reasons.

---

## Next
→ [`09-benchmark-pitfalls.md`](09-benchmark-pitfalls.md)
