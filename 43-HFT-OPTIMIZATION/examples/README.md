# Examples — Folder 43 (HFT optimization, end-to-end)

> Portable `.cpp` — sab MinGW/Windows x86-64 pe chalte. **`-O2` mandatory**
> (benchmarks). `./build.ps1 folder 43-HFT-OPTIMIZATION` → **7/7 OK**
> under strict flags. `pipeline.hpp` shared header (glob use nahi karta).
> `02_profile_analysis.sh` = Linux/`perf` workflow — `bash -n` checked,
> is box pe chalayi nahi (no `perf` on Windows).

## The box these were measured on

**AMD Ryzen 7 4700U** (Zen 2), ~2.0 GHz, **Windows x64 + MinGW-w64 GCC
15.1.0**. TSC ~2.0 GHz (`ticks_per_ns` 1.9962). Unpinned desktop —
**ratios/shapes quote karo**, absolutes run-to-run ~20% jhoolte (folder
35/06's lesson, `02-establishing-baseline.md` mein carry-forward).

## Scope

Yeh folder **methodology** ka hai: measure → profile → hypothesize →
change → re-measure → **explain**. Individual techniques folder 36 ne
isolation mein sikhaye; yahan unhe ek **connected pipeline** pe apply
karke, har change ka **number aur reason** nikala jata hai.

`pipeline.hpp` do versions rakhta:
- **`PipelineV0`** — realistic-naive: `std::stringstream`-style `substr`
  parse + `std::stod`, `std::map<double>` + `std::list` book, `std::deque`
  re-sum SMA, `std::string` order encode.
- **`PipelineV3`** — hand int-parse + fixed-point price, flat-array book +
  cached top-of-book + dense-id direct index, ring-buffer running-sum SMA
  (no division), fixed-layout POD order encode.

Dono ka input **ek hi** deterministic feed (`make_feed`). Dono ka signal
logic **numerically identical** (V3 integer arithmetic mein wahi cross-
condition jo V0 float mein) — `04_before_after.cpp` isko **agreement gate**
se verify karta (speedup se pehle).

## Compile / run

```bash
./build.ps1 fast 43-HFT-OPTIMIZATION/examples/04_before_after.cpp
```

## Examples

| File | Lesson(s) | Kya |
|---|---|---|
| `pipeline.hpp` | 13–15 | `PipelineV0` + `PipelineV3` + `make_feed` + rdtsc harness + percentile helper |
| `01_baseline_pipeline.cpp` | 01, 02, 03 | v0 baseline — (A) true per-tick avg (1 clock) + (B) per-stage rdtsc attribution |
| `02_profile_analysis.sh` | 03, 06 | Linux `perf` bottleneck hunt: `stat` (kya bound) → `report` (kaunsa fn) → `annotate` (kaunsi line) → signal→lesson map. `bash -n` only. |
| `03_optimized_pipeline.cpp` | 09–14 | v3 — same feed, same (A)+(B) format, side-by-side comparable |
| `04_before_after.cpp` | 01, 13, 14, 15 | v0 vs v3 **ek process** — speedup + per-stage ratios + **output agreement gate** |
| `05_fixed_point.cpp` | 09 | float rounding bug (measured), int64 fixed-point parse/format round-trip, arith cost, scale discipline |
| `06_division_elimination.cpp` | 10 | `div` vs magic-multiply (const) vs shift (pow2) vs reciprocal-multiply — measured, **only the divide** timed |
| `07_struct_tuning.cpp` | 07 | padding (field order), fat vs slim struct, AoS vs SoA — memory-traffic-bound scan, measured |
| `08_hot_cold.cpp` | 04, 05, 06 | hot/cold path split — `[[gnu::cold]]` + `[[unlikely]]` vs inline, measured (~noise on this box — honest Rule-2 result, carries 36/12) |

## Key numbers (this box, ratios — absolutes vary ~20% run-to-run)

### End-to-end (`04_before_after.cpp`, 120k msgs, `-O2`)

```
per-tick (true avg, clean loop):
  v0 (naive)     :  ~1.9 – 2.7  µs/tick
  v3 (optimized) :  ~25  – 45   ns/tick
  speedup        :  ~60 – 80x
per-stage ratio (rdtsc attribution — v3 absolutes are ~80 ns probe-cost-limited):
  parse ~25x     book ~25–29x     signal ~4x
output agreement : 110 / 110 order-fire ticks IDENTICAL (fixed-point == float here)
```

### Isolated techniques

```
05  fixed-point : 0.1 x10 (double) = 0.99999999999999988898 != 1.0 ; int64 exact
                  int64 add+cmp ~0.85 ns vs double ~1.05 ns/elem  (speed = minor; correctness = the point)
06  division     : var/var `div` ~4.1 ns  |  /const (magic) ~0.31 (13x)
                  >>k (pow2) ~0.25 (17x)   |  reciprocal-mul (invariant) ~0.59 (7x)
07  struct       : sizeof 24 -> 16 (field reorder, 34% smaller, 0 behaviour change)
                  fat 64B/elem scan ~3.5 ns  |  slim 8B ~0.52 (6.7x)  |  SoA 8B ~0.42 (8.3x)
08  hot/cold     : ~1% cold path ; inline vs [[gnu::cold]]+[[unlikely]] ~1% (run-to-run noise)
                  -> honest: frontend not the bottleneck at this scale (see 36/12); attribute cost is 0, keep it
```

## Correctness

`04_before_after.cpp` **gate**: v0 aur v3 ka order-fire tick stream
field-compare hota hai **speedup report se pehle**. 110/110 identical is
run mein — fixed-point signal math (`09`, `10`) ne float ko exactly
reproduce kiya. Divergence (kabhi 1-2 boundary float-flip) ko report kiya
jaata, chhupaya nahi (`09` trap).
