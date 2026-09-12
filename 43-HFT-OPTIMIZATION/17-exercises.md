# 17 — Exercises: optimization challenges with target numbers

## Prerequisites
- Folder 43 ke `01`–`16`

Yeh folder ka last file. Har challenge ke saath ek **target** ya ek
**decision** — sirf "optimize karo" nahi. Answers `<details>` mein.

---

## Part A — methodology (process, code nahi)

### A1. Order of operations
Tumhe ek pipeline diya: parse 20%, book 25%, **risk-check 50%**, encode 5%.
Kaunsa stage pehle, aur poore pipeline ka best-case speedup agar use
2× karo?
<details><summary>Answer</summary>
Risk-check (sabse bada slice). `p = 0.5, s = 2` → `1/(0.5 + 0.25)` = **1.33×**.
Agar risk-check ko ∞-fast: `1/0.5` = 2× max. Pehle profile *iske andar*
kyun 50% hai (loop over positions? per-order recompute? division?).
</details>

### A2. The regression that hid
Ne 3 optimizations ek commit mein ki. Benchmark: baseline 1000 ns → 950 ns.
"5% faster, ship." 2 hafte baad p99 latency spike reports. Kya galat hua?
<details><summary>Answer</summary>
3 changes ek saath = attribution impossible. Sambhavna: 2 changes ne 15%
diya, 1 change ne 10% regression **added** (jo mean mein chhup gaya) aur
p99 ko kharaab kiya (e.g. ek "optimization" jo occasionally reallocates).
Ek change per commit; p50 **aur** p99 dono track; agreement gate.
</details>

### A3. Noise floor
Baseline: 5 runs → 820, 970, 1150, 890, 1010 ns. Ek optimization ne "next
run" 780 ns diya. Claim valid?
<details><summary>Answer</summary>
Nahi. Baseline spread 820–1150 (~40%). Ek 780 run baseline ki low-end
(820) se sirf 5% neeche — noise ke andar. Chahiye: best-of-N (min) ya
many-sample p50 dono versions ka, ideally core-pinned. Tab tak "improvement"
unproven.
</details>

---

## Part B — read the profiler, name the fix

### B1. `perf stat`
```
IPC ................. 0.35
branch-misses ...... 0.4% of branches
LLC-load-misses .... 180 M   (very high)
stalled-frontend ... 6%
```
Kaunsi class? Kaunse 2 lessons?
<details><summary>Answer</summary>
**Memory / backend bound.** Low IPC + huge LLC misses, branches fine,
frontend fine. → `07` (struct layout / SoA — kam bytes per element), `11`
(lookup table cache trade-off — shayad table hi evict kar rahi), `36/10`
(cache locality), `36/04-05` (preallocation — scattered heap = misses).
PGO/`06` galat lever yahan.
</details>

### B2. `perf stat`
```
IPC ................. 2.6   (good!)
branch-misses ...... 9%    (high)
LLC-load-misses .... low
stalled-frontend ... 4%
```
<details><summary>Answer</summary>
**Bad speculation** — branch mispredicts. IPC theek jab predict sahi, par
9% miss = ~15-20 cyc har miss. → `04`/`05` (hot-cold path, `[[likely]]`),
`36/06` (branchless jahan branch genuinely unpredictable), data-driven
`switch` → lookup table (`36/06` test 3, ~12×).
</details>

### B3. `perf annotate` (hottest function)
```
  0.4% : mov    rax, [rdi+8]
 42.1% : idiv   rcx
  1.2% : mov    [rdi+16], rax
```
<details><summary>Answer</summary>
Ek runtime-divisor `idiv` 42% samples kha raha (non-pipelined, ~20-40 cyc).
→ `10`: divisor `constexpr` ban sakta? power-of-2? loop-invariant
(reciprocal multiply)? ya algebra se division hi khatam
(`a/b > c` → `a > b*c`)?
</details>

### B4. `perf c2c`
```
HITM: 1.2 M   on cache line 0x...40, offsets 0x00 and 0x08
  writer 1: enqueue()  (thread A)
  writer 2: dequeue()  (thread B)
```
<details><summary>Answer</summary>
**False sharing** — `head_` (offset 0) aur `tail_` (offset 8) ek line pe,
do threads likhte → line ping-pong. → `08`: `alignas(64)` dono pe +
trailing pad; ya `41`'s cached-opposite-index trick (contention hi kam
karo).
</details>

---

## Part C — "yeh optimization ne slow kiya — kyun"

### C1.
`std::map<Price, Level>` → `std::vector<Level>` sorted rakha. Top-of-book
scan tez hua, par overall **20% slower**. (Yeh `39`'s real result hai.)
<details><summary>Answer</summary>
HFT churn = constant add/cancel near touch → naya level insert / empty
level erase = `O(n)` `memmove` of all levels beyond it. Har message pe ek
memmove >> ek map node alloc. Sorted vector sirf static-book full-scan
workloads pe jeetta. `39/06`.
</details>

### C2.
Ek 64 KB lookup table add ki `f(x)` ko replace karne ko (`f` = 2 mul + 1
shift). Micro-bench 8× faster; production pipeline mein 5% **slower**.
<details><summary>Answer</summary>
Micro-bench mein table akeli L1 mein. Production working set (book + order
pool + ...) already L1/L2 bhar raha; 64 KB table ne use evict kiya → table
access ab L2/L3, **aur** book access bhi ab miss. `f` (~5 cyc) recompute
sasta tha. `11` trap 1.
</details>

### C3.
Hot loop ke ek `if (rare) {...}` ko branchless `sel = mask ? a : b` banaya.
Predictable tha (99% ek taraf). Loop **slower**.
<details><summary>Answer</summary>
Branchless **hamesha dono sides** compute karta + dependency chain lamba.
Predictable branch ~0 cost (predictor 99% sahi). Branchless ne free branch
ko forced work se replace kiya. `36/06`: branchless sirf **genuinely
unpredictable** branch pe.
</details>

### C4.
`std::vector<Order>` ko `std::vector<Order*>` (pointers to pooled objects)
banaya "taaki move sasta ho". Scan **3× slower**.
<details><summary>Answer</summary>
`vector<Order>` = contiguous, prefetcher-friendly, ek scan ek stream.
`vector<Order*>` = pointer chase — har `Order` heap mein alag jagah, har
deref ek potential miss. Move sasta hua par har **read** mehnga. AoS values
> AoP jab scan hota. `07`, `36/10`.
</details>

---

## Part D — hit the target (code + measure)

### D1. Parse under 50 ns
`pipeline.hpp` `PipelineV0::parse_naive` ko standalone benchmark karo
(10M calls, ek clock). Target: **< 50 ns/call** (v0 likely ~200-400 ns).
<details><summary>Answer</summary>
Steps (`13`): (1) `substr` → `const char*` walk (kill 5 mallocs). (2)
`stod`/`stoi` → `*10 + (*p-'0')` loop, price as `int64` scale 100. (3)
fixed offsets for known-position fields. `PipelineV3` ka parse yahi hai —
standalone < 20 ns/call is box pe. Target easily met.
</details>

### D2. Division-free mid + SMA cross
Ek signal: `fire = (bid+ask)/2 > SMA_W(mid) * 1.001`, `W` runtime config.
Zero `div`/`idiv` in the hot path. Verify with `perf annotate` (no `idiv`).
<details><summary>Answer</summary>
`W` config-time → `constexpr` if possible (`>> log2 W`), warna reciprocal
(`10` D). Cross: `mid > sma*1.001` → `mid*1000 > sma*1001` →
`(bid+ask)*1000*W > ring_sum*1001` (ring_sum = Σ over W of `bid+ask`).
All integer. `pipeline.hpp` V3 `signal_and_encode_` — copy that shape.
`grep -E "idiv|div " asm` → empty.
</details>

### D3. Struct diet
`FatOrder` (`07`, 64 B). Ek "cancel by id" path ko sirf `{level_idx, qty,
side}` chahiye. Design a layout jisme cancel-scan **8 bytes/order** chhue,
full record abhi bhi accessible.
<details><summary>Answer</summary>
Split: `struct HotLoc { i32 level_idx; i16 qty; u8 side; }` (8 B, in a
`vector<HotLoc>` indexed by id) + `struct ColdOrder { u64 ts_recv, ts_book;
u32 participant, flags; ... }` (alag `vector`, same id index). Cancel touches
only `HotLoc[id]`. Audit/log touches `ColdOrder[id]`. `pipeline.hpp` V3's
`id_loc_` = the hot half.
</details>

### D4. End-to-end 10×
`04_before_after.cpp` chalao. v0 → v3 speedup note karo. Ab v0 mein se
**sirf** allocation hatao (reused buffers, `reserve`) — koi algorithm
change nahi. Kitna milta akela?
<details><summary>Answer</summary>
Reused `std::string` for parse + `out_.reserve()` + a `std::pmr` monotonic
buffer for map nodes → allocation cost lagbhag gaya. Expect ~2-4× (mean) aur
**tail bahut zyada** (realloc/`malloc`-page spikes gaye — `15`, `36/04-05`).
Algorithm (map, stod) abhi bhi hai → baaki ~20× uske baad. Allocation alone
= a big, cheap first win.
</details>

---

## Part E — judgement (`16`)

### E1.
Pipeline p99.9 = 800 ns. Requirement: < 5 µs. Ek engineer 3 hafte ka plan
laaya "SIMD parse, 1.3× end-to-end". Go/no-go?
<details><summary>Answer</summary>
**No-go** (abhi). 6× headroom already. 1.3× end-to-end se p99.9 ~615 ns —
requirement pe koi asar nahi. 3 hafte ka opportunity cost + AVX2
maintenance/portability + downclock risk. Revisit agar requirement tighten
ho ya competition data kuch aur kahe.
</details>

### E2.
Ek change 12% faster hai, par ek strict-aliasing violation pe based ("abhi
kaam karta hai"). Reviewer?
<details><summary>Answer</summary>
**Reject.** UB — agli compiler version / `-O3` / different inlining pe
silently toot sakta, aur toot-ne pe **wrong results** (crash nahi, jo
worse). 12% ke liye correctness gamble nahi. `memcpy` / `std::bit_cast` se
same speed legally milta usually — woh likho.
</details>

### E3.
Team ka "fastest" engineer ek 400-line hand-vectorized order-book scan
likh kar gaya. 1.4× on that scan. Woh ab dusri team mein hai. Keep or
revert?
<details><summary>Answer</summary>
Depends: (a) us scan ka % of hot path — agar 5%, 1.4× = 2% global, aur
koi maintain nahi kar sakta → **revert to the clear version**, bus-factor
risk > 2%. (b) Agar 40% of hot path aur well-tested + documented + a
fallback exists → keep, par ek doosre engineer ko onboard karo. Un-owned
critical code = liability.
</details>

---

## Self-check

Tum tab ready ho jab bina dekhe bata sako:

1. Optimization ke 6 steps, aur har ek skip karne ka nuksaan.
2. `perf stat` ke 4 signals → 4 problem classes → kaunse lessons.
3. Amdahl's law formula, aur kab woh **under**-predict karta (global
   changes).
4. Fixed-point kyun (correctness pehle, speed baad), scale kaise chunna.
5. `div` ke 4 alternatives aur har ek ki precondition.
6. AoS vs SoA — ek-ek case jahan har ek jeeta.
7. Optimization rokne ke 5 signals.

---

---

**Folder 43 complete.**

## Next
→ [`../44-HFT-PROJECTS/00-README.md`](../44-HFT-PROJECTS/00-README.md)
