# Examples — Folder 32 (cache & memory performance)

> Portable `.cpp` (Linux nahi chahiye — sab x86-64 / MinGW-Windows pe chalte).
> **`-O2` mandatory** — `-O0` pe har number bekaar. `./build.ps1 folder
> 32-CACHE-MEMORY-PERFORMANCE` → 8/8 OK. Benchmarks: `./build.ps1 fast <file>`.
> `09_perf_analysis.sh` compile nahi hota (Linux `perf` workflow, `.sh`).

## The box these were measured on

**AMD Ryzen 7 4700U** (Zen 2, family 0x17), 8 cores / no SMT, **~2.0 GHz
effective** (mobile, power-save/throttled). L1d 32 KiB/core 8-way, L2
512 KiB/core, L3 8 MiB / 4-core CCX. DDR4-3200 dual-channel (~40 GB/s
achievable). AVX2 + FMA; no AVX-512.

**⇒ Quote the RATIOS, not the absolute ns.** A frequency-locked HFT box at
3–5 GHz gives different absolutes; the shapes (~10× column-major, ~7× random
vs sequential, ~2× SoA scan, ~6–40× false sharing, ~95 ns DRAM latency) are
microarchitectural and carry over. Numbers also swing ±~30% run-to-run here.

## Compile / run

```bash
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/03_matrix_traversal.cpp
# ... `folder` compiles all 8 (strict debug flags, compile-check only).
```

## Examples

| File | Lesson(s) | Kya dikhata / measured (this box, ~2 GHz) |
|---|---|---|
| `01_cache_line_size.cpp` | 02 | sequential stride ramp (min-of-9 passes) over 64 MiB. **ns/access ~0.32 (1 B) → ~1.0 (16 B) → ~3.4 (64 B) → ~5.4 (128 B)**. Knee ≈ line size, but ⚠️ **smeared 64→128 B by the HW prefetcher** — the sharp 64-B cliff is in `02` (random order). |
| `02_stride_access.cpp` | 05, 06, 13 | PART 1: sequential, varying stride → **useful GB/s collapses 8.3 → 0.7** (fetch 64 B, use 4 B). PART 2: whole-line reads, **sequential 4.6 ns/line (13.8 GB/s) vs random 31.8 ns/line (2.0 GB/s) → ~6.9×**. Random still < true ~80 ns (MLP overlaps ~10 independent misses). |
| `03_matrix_traversal.cpp` | 05, 03, 11 | 4096² int, row-major vs column-major. **`-O2`: col-major ~10× slower** (0.25 vs 2.55 ns/elem). ⚠️ **`-O3 -march=native`: GCC `-ftree-loop-interchange` swaps the nest → ratio ~1.0** — the compiler fixed it (Rule 2). |
| `04_false_sharing.cpp` | 07, 02 | 4 threads, own `uint64_t` counter, 80M inc. **padded (`alignas(64)`) ~0.2–0.3 ns/inc stable; packed ~1.8–3.7+ ns/inc → tax ~6× to ~44×, and it changes every run** (OS puts the threads on different cores → line bounces different distances). False sharing is slow *and jittery*. |
| `05_aos_vs_soa.cpp` | 09, 10 | 4M particles, 8 fields. **T1** scan x,y,z sequential → **SoA ~2.0×**. **T2** update x,y,z (6/8 fields) sequential → **SoA ~2.4× — gap did NOT shrink** (SoA vectorizes, AoS's 32-B stride doesn't). **T3** random order, all 8 fields → **AoS ~3×** (1 line vs 8 lines per particle). |
| `06_prefetch.cpp` | 06 | ⚠️ **cautionary, Rule 2.** S1 light gather: `__builtin_prefetch` D=32 → **~1.1×** (marginal — MLP already overlaps ~10 misses). S2 heavy bucket-scan + dependent mix: prefetch → **~0.33× = 3× SLOWER** (16 independent loads already saturate memory; prefetch requests contend). SW prefetch on a big OoO core is a scalpel, not free money. |
| `07_matrix_blocking.cpp` | 15, 05 | C = A·B, N=768 float. naive `ijk` **1.4 GFLOP/s** → `ikj` (loop-order fix) **~4×** → blocked 64×64 **~3.6× — ~10–15% SLOWER than plain `ikj`**. ⚠️ Rule 2: `ikj` already auto-vectorizes + L3 absorbs B's reuse at this N; naive tiling needs a tuned microkernel to beat it. Loop order is the real win. |
| `08_tlb_hugepages.cpp` | 11, 01, 04 | pointer-chase, 1 slot/page, page count 16 → 8192. curve A (1 page, TLB always hit) flat **1.2 ns**; curve B (N pages) 3.4 ns (64 pages) → 15 ns (1024/4 MiB) → **~95 ns (2048+/8 MiB+)**. ⚠️ 4 KiB pages: cliff conflates STLB reach (~6 MiB) + L3→DRAM (~8 MiB); can't isolate TLB portably (needs `mmap` aliasing). |
| `09_perf_analysis.sh` | 14 | Linux `perf` workflow: `perf stat` (MPKI, IPC, TLB), `--topdown`, `perf record`/`annotate` to localize, `perf c2c` for false sharing, cachegrind fallback. Not compiled (`.sh`). Run on a Linux/WSL box. |

## Notes / jaan-boojh kar cheezein

- **`keep()` / `asm volatile("" : "+r,m"(v) : : "memory")` barriers** in every
  benchmark — without them `-O2` DCEs the loops / computes closed forms
  (folder 31 learned this repeatedly). Google-Benchmark `DoNotOptimize` style,
  emits zero instructions.
- **Min-of-N-passes timing** in `01` — OS jitter / frequency dips only *add*
  time, so the minimum is the cleanest signal on this throttled laptop.
- **`03`, `07` — the compiler/hardware defeated the naive expectation**, and
  that's taught explicitly (Rule 2): `-O3` does loop-interchange for you;
  naive blocking loses to a good loop order without a tuned kernel; `06`
  prefetch is marginal-or-harmful on a big OoO core. These "it didn't work
  the textbook way" findings are the point, not a bug to hide.
- **`04` false-sharing ratio is deliberately reported as a range** (~6–40×)
  and the example prints a note — the run-to-run variance *is* the lesson
  (false sharing → non-deterministic latency).
- **`08` honest limitation stated in-file**: with 4 KiB pages, page count and
  memory footprint grow together, so the TLB cliff and the cache cliff
  coincide (~8 MiB). Isolating pure-TLB needs `mmap` aliasing (Linux-only).
- **No `broken_on_purpose` file.** Every example is a correct program
  measuring a real memory-system property.
- **Numbers are this-box, ~2 GHz throttled AMD Zen 2.** Shapes/ratios port;
  absolutes don't. Re-tune constants (block size, prefetch distance) on the
  production box (frequency-locked, SMT-off — folder 31 lessons 12–13).
- All 8 compile clean under `-std=c++20 -Wall -Wextra -Wpedantic -Wshadow
  -Wconversion -Wsign-conversion -Wcast-align -Wunused -Wnull-dereference
  -Wdouble-promotion` (`./build.ps1 folder 32-CACHE-MEMORY-PERFORMANCE`).
