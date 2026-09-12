# 13 — Memory bandwidth: latency-bound vs bandwidth-bound

## Prerequisites
- `01-memory-hierarchy.md`, `04-cache-misses.md` (MLP)
- `31-CPU-ARCHITECTURE/10-simd-basics.md`

## Yeh topic abhi kyun
"DRAM dheema hai" ke do bilkul alag matlab hain: **latency** (ek line aane
mein ~100 ns) aur **bandwidth** (per second kitne GB aa sakte, ~40 GB/s is
box). Aapki loop in mein se ek pe atki hoti hai — aur fix alag hai. Latency-
bound loop ko MLP / prefetch se attack karo. Bandwidth-bound loop ko SIMD se
attack karna bekaar — usse kam bytes move karke ya kam passes se theek karo.
Yeh distinction "roofline model" ka core hai.

---

## Latency vs bandwidth — Little's Law

```
achievable throughput = concurrency / latency
=> concurrency needed to saturate bandwidth = bandwidth × latency
```

Example (is box, approx): DRAM latency ~90 ns, per-line 64 B, peak BW
~40 GB/s.
```
lines/sec at peak = 40e9 / 64 = 625 M lines/sec
concurrency needed = 625e6 × 90e-9 ≈ 56 lines "in flight" simultaneously
```
Par ek core ke paas sirf ~10-12 line fill buffers (LFB/MSHR). **Ek core akela
DRAM bandwidth saturate nahi kar sakta** — usse ~10 lines in-flight milti
hain → ~10/56 ≈ 18% of peak from one core. Poora socket saturate karne ke
liye kai cores chahiye.

- **Latency-bound**: aapke misses **serial** hain (pointer chase) ya kam
  concurrency (chhota loop body). Effective rate ≈ 1 / latency ≈ 11 M
  lines/sec. Fix: expose parallelism — independent misses, prefetch, MLP.
- **Bandwidth-bound**: aap enough concurrency generate karte ho ki memory
  channels bhar jaate. Fix: **move fewer bytes** — smaller types, fewer
  passes (fusion), compression, blocking so data stays in cache.

---

## STREAM benchmark

The standard memory-bandwidth benchmark (John McCalpin). 4 kernels over
large arrays (>> LLC):

| Kernel | Op | Bytes/iter (r + w) |
|---|---|---|
| Copy | `c[i] = a[i]` | 16 (8r + 8w) + RFO 8 = 24 effective |
| Scale | `b[i] = k*a[i]` | 24 |
| Add | `c[i] = a[i] + b[i]` | 32 |
| Triad | `a[i] = b[i] + k*c[i]` | 32 |

Report GB/s = (bytes moved) / time. On this box a single-thread triad is
~10-15 GB/s (example `02` PART 2 sequential got ~14 GB/s); all-cores ~30-40
GB/s (DDR4-3200 dual channel theoretical ~51 GB/s).

Note the **RFO tax**: `c[i] = ...` writes a cold line → it's read first
(8 B) then written (8 B). NT stores (lesson 12) remove the read → higher
effective BW for write-heavy kernels.

---

## Read vs write bandwidth asymmetry

- **Read**: straightforward, prefetcher-assisted, close to peak.
- **Write (normal)**: RFO first → each "write" is really read+write → ~half
  the effective write BW. Plus dirty writeback later.
- **Write (NT / full-line)**: no RFO → close to peak write BW, no pollution.

So a `for (i) out[i] = f(in[i])` streaming loop over GBs: normal stores ≈
`in` read BW + `out` RFO read BW + `out` write BW. NT stores on `out` ≈
`in` read + `out` write. ~1.5× faster when bandwidth-bound.

---

## Roofline model (intuition)

Plot achievable GFLOP/s vs **arithmetic intensity** (FLOPs per byte of DRAM
traffic):

```
GFLOP/s
  |            ___________  <- compute roof (peak FLOP/s, SIMD+FMA+all cores)
  |          /
  |        /  <- slope = memory bandwidth (GB/s)
  |      /
  |    /
  |__/________________________ arithmetic intensity (FLOP/byte)
        ^ridge point
```

- Left of the ridge (low FLOP/byte): **memory-bound** — you're on the
  diagonal, limited by bandwidth. Adding SIMD/FMA doesn't help; reduce bytes.
- Right of the ridge (high FLOP/byte): **compute-bound** — limited by ALU;
  SIMD/FMA/better ILP helps.

Examples:
- `sum += a[i]` — 1 add per 4 bytes = 0.25 FLOP/byte → deep memory-bound.
  Example `05` Test 1 SoA: scalar→AVX2 ~2.4x, then it hits the BW wall.
- Dense matmul (blocked, register-tiled) — O(N³) FLOPs, O(N²) bytes → high
  intensity → compute-bound → SIMD/FMA is the game.
- `a[i] = b[i] + s*c[i]` (triad) — 2 FLOP per 24-32 bytes ≈ 0.06-0.08
  FLOP/byte → hard memory-bound.

**Rule:** figure out which side of the ridge you're on *before* optimizing.
Memory-bound → SIMD is wasted effort; attack bytes and passes.

---

## Detecting which bound you're on

| Signal | Bound |
|---|---|
| `perf`: high `mem_load_retired.l3_miss`, IPC low, `offcore_requests_outstanding` near max | **bandwidth** |
| `perf`: L3 misses moderate but serial (pointer chase), `offcore` low | **latency** |
| Adding threads scales throughput linearly | latency-bound (each core independent) |
| Adding threads → throughput flatlines | **bandwidth-bound** (shared channels saturated) |
| SIMD version = scalar version speed | memory-bound (either kind) |
| Working set fits in L2 and it's fast | neither — compute/ILP bound |

The "add threads" test is the cleanest: bandwidth is shared, latency-hiding
(MLP) is per-core.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — SIMD a memory-bound loop
`sum += a[i]` over 1 GB — you AVX2 it and get... the same speed (example `05`
converges to ~1× past LLC). You're bandwidth-bound; the ALU was never the
limit. Reduce bytes (float not double, int16 not int32) or fuse passes.

### Trap 2 — "one core, full DRAM bandwidth"
One core gets ~15-20% of socket peak (LFB limit). Bandwidth benchmarks need
all cores. Don't size your single-threaded budget against the datasheet BW.

### Trap 3 — ignoring RFO in write-heavy loops
`out[i] = ...` costs a hidden read. Your "write bandwidth" measurement is
half of peak until you use NT stores.

### Trap 4 — multiple passes over out-of-cache data
3 passes over a 100 MB array = 300 MB traffic. Fuse to 1 pass or block it.
Each avoided pass is a linear speedup when bandwidth-bound.

### Trap 5 — bandwidth-bound but blaming the algorithm's big-O
An O(n) streaming pass and an O(n log n) blocked pass over the same data —
the blocked one can be *faster* despite more work, because it's cache-
resident (not bandwidth-bound). Big-O doesn't see bandwidth.

### Trap 6 — NUMA remote bandwidth
On a 2-socket box, accessing the other socket's memory ≈ half the bandwidth
and +50% latency (`31/14`). A "bandwidth problem" might be a "wrong NUMA
node" problem — check first-touch and pinning.

---

## > **HFT relevance**

> - **Most hot-path loops should be neither bound** — working set in L1/L2,
>   compute-light. If you're bandwidth-bound on the tick path, your working
>   set is too big — shrink it (smaller types, hot/cold, fewer instruments
>   per batch).
> - **Bulk / analytics paths (backtest, risk sweep) are bandwidth-bound** —
>   there, SIMD-ing won't help; use smaller types, single-pass, NT stores for
>   outputs, and blocking so the working set is L2/L3-resident.
> - **NT stores on write-only bulk output** (lesson 12) — recovers the RFO
>   half of write bandwidth.
> - **`perf stat -e` the offcore counters** in the harness — know your
>   steady-state DRAM traffic per second; a regression there = someone grew
>   the working set.
> - **NUMA-pin the bandwidth-heavy threads** to their memory's node.

---

## Hands-on

```bash
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/02_stride_access.cpp
#   PART 2 sequential ~14 GB/s (single-thread streaming) vs random ~2 GB/s

./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/05_aos_vs_soa.cpp
#   SoA scalar->AVX2 gain SHRINKS as data grows past LLC = hitting BW wall

# a quick triad, single vs all threads (Linux):
#   likwid-bench -t stream ... ; or a hand-rolled OpenMP triad
perf stat -e offcore_requests_outstanding.all_data_rd,LLC-load-misses ./prog
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "DRAM slow = one number" | latency (~90 ns/line) vs bandwidth (~40 GB/s) — different fixes |
| "SIMD makes any loop faster" | memory-bound loop: no change; reduce bytes instead |
| "one core saturates DRAM" | ~15-20% of socket peak (LFB limit) |
| "write BW = read BW" | RFO halves normal write BW; NT stores recover it |
| "more work = slower" | cache-resident blocked O(n log n) can beat bandwidth-bound O(n) |
| "bandwidth problem" | on NUMA, might be a "remote node" problem |

---

## Exercises

1. Loop `for (i) s += a[i];` over `a` = 500 MiB of `double`. On this box
   (~14 GB/s single-thread streaming). Roughly how long? If you change `a`
   to `float` (250 MiB)? To `int16` (125 MiB) with a wider accumulator?

   <details><summary>Answer</summary>

   `double`: 500 MiB / 14 GB/s ≈ **37 ms**, memory-bound (0.125 FLOP/byte).
   `float`: 250 MiB / 14 GB/s ≈ **18 ms** — ~2× faster purely from moving
   half the bytes; the add throughput was never the limit. `int16`: 125 MiB
   / 14 GB/s ≈ **9 ms** — ~4× vs double. This is the memory-bound lesson:
   the speedup came from **data size**, not from vectorizing the add (which
   was already free). Precision permitting, smaller types are the biggest
   lever on a bandwidth-bound reduction.
   </details>

2. Aapka analytics pass 3 alag loops mein ek 2 GB array pe chalta hai
   (normalize, then clip, then accumulate). All-cores DRAM ~35 GB/s. Current
   time aur fused (1 loop) time estimate karo.

   <details><summary>Answer</summary>

   Each pass reads 2 GB (and normalize/clip write 2 GB each). Rough traffic:
   pass1 2R+2W, pass2 2R+2W, pass3 2R = 12 GB / 35 GB/s ≈ **340 ms**. Fused:
   read 2 GB once, write 2 GB once (normalized+clipped in place), accumulate
   in registers = 4 GB / 35 GB/s ≈ **115 ms** — ~3× from removing passes.
   Add NT stores on the in-place write (no RFO) → ~3 GB → ~85 ms. The
   algorithm's FLOPs didn't change; the memory traffic did.
   </details>

3. Ek loop ka scalar aur AVX2 version bilkul same time lete hain. Loop
   `c[i] = a[i]*b[i] + d[i]` over arrays >> LLC. Kya bound? Ek fix jo
   actually madad kare.

   <details><summary>Answer</summary>

   **Bandwidth-bound.** 3 reads (a,b,d = 24 B) + 1 write (c = 8 B) + RFO on
   c (8 B) = 40 B per iter for 2 FLOP → 0.05 FLOP/byte, deep in the
   memory-bound region → AVX2's 8× ALU throughput is irrelevant, the channels
   are full either way. Fixes that help: (1) **NT store on `c`** — kills the
   RFO, 40 B → 32 B, ~20% faster. (2) **Smaller types** if precision allows
   (float→half, or fixed-point int16) — linear with byte count. (3) **Fuse**
   this with the producer/consumer of these arrays so they're not re-streamed.
   (4) **Block** if there's reuse elsewhere. SIMD alone: no.
   </details>

---

## Interview questions

1. Latency-bound vs bandwidth-bound — definition aur har ek ka fix direction.
2. Little's Law — concurrency needed to saturate BW; why one core can't.
3. STREAM triad — the 4 kernels, aur "RFO tax" on the write.
4. Roofline model — arithmetic intensity, the ridge, aur kaunsa side SIMD
   help karta.
5. "Add threads" test — bandwidth vs latency bound kaise batata.
6. Read vs write bandwidth asymmetry — kyun, aur NT stores ka role.
7. "SIMD version = scalar version speed" — kya conclude karte ho.

---

## Next
→ [`14-measuring-cache.md`](14-measuring-cache.md)
