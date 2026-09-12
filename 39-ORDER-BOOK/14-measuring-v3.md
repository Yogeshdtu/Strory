# 14 — Measuring V3: the final benchmark

## Prerequisites
- [`07`](07-flat-array-book.md) se [`13`](13-book-snapshots.md) tak
- `examples/06_orderbook_v3_bench.cpp`, `examples/07_comparison_suite.cpp`

## Yeh topic abhi kyun
V3 build ho chuka. Ab spec ka process poora karo: **re-benchmark, aur
V1/V2 se compare karo.**

---

## Measured (`06_orderbook_v3_bench.cpp`, SAME workload, N=200000, `-O2`)

```
=== BookV3 (flat array + intrusive list + flat hash) ===
  ALL ops                p50  120.2   p99    480.9   p99.9     661.3   max    43031.8  ns
  Add only               p50  120.2   p99    440.8   p99.9     531.0   max    14006.6  ns
  Execute/Cancel only    p50  130.2   p99    531.0   p99.9     701.3   max    12714.2  ns
```

## Teeno versions, ek table mein

| | V1 (map) | V2 (sorted vector) | V3 (flat+intrusive) |
|---|---|---|---|
| **ALL p50** | 140.3 ns | 150.3 ns | **120.2 ns** ✅ |
| **ALL p99.9** | 1502.9 ns | 2745.2 ns | **661.3 ns** ✅ |
| **Add p99.9** | 10700.3 ns | 2755.2 ns | **531.0 ns** ✅ |
| **Exec/Cancel p50** | 200.4 ns | 360.7 ns | **130.2 ns** ✅ |
| **Exec/Cancel p99.9** | 961.8 ns | 2063.9 ns | **701.3 ns** ✅ |

**V3 wins EVERY metric** — na sirf V1 se better, V2's regression (06) bhi
poori tarah fix hui. Yeh koi accident nahi — 07-09 mein har V1/V2
weakness explicitly identify karke fix ki gayi thi.

---

## `07_comparison_suite.cpp` — same-process cross-check

```
=== V1 vs V2 vs V3 -- 200000 ops, same workload ===

  V1 std::map+list+unordered_map p50  150.3   p99    891.7   p99.9    1743.3
  V2 sorted vector+deque         p50  160.3   p99   1332.5   p99.9    3196.1
  V3 flat array+intrusive+hash   p50  120.2   p99    480.9   p99.9     681.3

p99.9 ratio V1/V3 = 2.6x
```

**Numbers thode alag hain standalone runs se** (150.3 vs 140.3 for V1,
etc.) — teeno EK process mein sequentially chalne se cache/allocator
state thoda alag hota (35/06 ka run-to-run-variance lesson, ab
"same-process-multiple-workload" variant ke saath). **Shape/ratio same
rehta**: V2 worst, V3 best, dono runs mein.

---

## Correctness — poori tarah verified

```
=== Cross-version correctness ===
order_count: V1=92066 V2=92066 V3=92066  match? haan
best_bid:    V1=9999 V2=9999 V3=9999  match? haan
best_ask:    V1=10001 V2=10001 V3=10001  match? haan
best_bid_qty: V1=253810 V2=253810 V3=253810  match? haan
```

**200000 operations ke baad, teeno EXACT SAME final state** — behavior-
equivalence proof jo speed-comparison ko meaningful banata (agar teeno
alag results dete, "V3 fast hai" claim karne ka koi matlab nahi hota).

---

## Kya cache-miss-level pe ho raha (structural reasoning — Linux/perf
verify-able)

```
V1: har tree-node + har list-node ALAG heap allocation
    -> traversal = RANDOM memory jumps (32-CACHE: "1 sec = L1" analogy)

V2: price-levels contiguous (GOOD), par order-within-level scan
    -> deque chunks + linear find_if = extra cache lines touched

V3: arena CONTIGUOUS (orders), array CONTIGUOUS (levels)
    -> add/reduce/remove sab predictable, nearby memory access
    -> minimal pointer-chasing (sirf intrusive-list next/prev, jo arena
       ke andar hi rehte -- far jump nahi)
```

Yeh Windows box pe hum `perf` nahi chala sakte (35/10-14 Linux-only) —
par **measured numbers khud is theory ko support karte** (V3's tail V1/V2
se dramatically tighter, jo cache-miss-variance kam hone ka signal hai).
Agar tum Linux/WSL pe ho, `perf stat -e cache-misses ./v1b ./v2b ./v3b`
chala ke isi ko directly confirm kar sakte.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — single run ke numbers ko "final truth" maan lena
07's numbers standalone-run se thode alag hain — dono legitimate
measurements hain, dikhata hai **kyun multiple runs/context matter
karte** (35/04's "sample size" lesson).

### Trap 2 — is result ko "V3 hamesha best hai, koi trade-off nahi" padhna
02/07 ke trade-offs (bounded range, bounded capacity, code complexity)
abhi bhi lagte — speed win FREE nahi aaya, 15 mein poora accounting.

### Trap 3 — correctness-check ko "optional nice-to-have" samajhna
Bina cross-version equivalence proof ke, speed comparison **meaningless**
hoti — agar V3 fast hai kyunki woh galat/incomplete kaam kar raha, woh
"optimization" nahi hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| V3 sirf V1 se better hai | V1 AUR V2 (06's regression fix karke) dono se better |
| Numbers har run same aane chahiye | Run-to-run variance normal hai (35/06); ratio/shape consistent rehta |
| Speed comparison bina correctness-check ke valid hai | Cross-version equivalence proof pehle zaroori |
| V3 ka win "free" hai | Explicit trade-offs (02, 07) ke saath aaya |

---

## Exercises

1. V3's Add p99.9 (531.0 ns) aur Execute/Cancel p99.9 (701.3 ns) V1/V2
   se bahut kam hain, par ek doosre se bhi CHHOTA farak hai (531 vs 701,
   ~1.3x) — jabki V1/V2 mein Add aur Execute/Cancel ke beech bahut zyada
   farak tha. Kyun?
   <details><summary>Answer</summary>
   V3 dono operations (Add, Reduce) mein SIMILAR-SHAPE work karta — O(1)
   array-index/arithmetic (Add), O(1) hash-lookup + direct-slot-access
   (Reduce) — koi operation "occasionally bahut mehnga" (V1's tree-
   rebalance-on-new-level, V2's linear-scan) nahi karta. Consistent,
   predictable cost profile hi ek "well-designed" data structure ki
   nishaani hai — extremes kam hone chahiye.
   </details>

2. Agar tumhare paas Linux access hota, `perf stat` se kaunsa specific
   counter V3 ke fayde ko sabse directly confirm karega?
   <details><summary>Answer</summary>
   `cache-misses` (ya zyada specific: `LLC-load-misses` / `L1-dcache-
   load-misses`) — V1/V2 ka pointer-chasing (tree/scan) zyada cache
   misses generate karega, V3 ka contiguous-arena access kam. `perf stat
   -e cache-misses,cache-references` se miss-rate directly compare kar
   sakte (35/10 ka poora workflow).
   </details>

---

## Interview questions

1. Teeno versions ke ALL-ops p50/p99.9 numbers batao, comparison ke
   saath.
2. Correctness-equivalence proof kyun speed-comparison se PEHLE zaroori
   hai?
3. Standalone-run vs same-process-comparison numbers mein farak kyun
   aata?
4. Structurally (bina perf ke bhi) kaise justify karoge ki V3 kam
   cache-misses generate karega?

---

## Next
→ [`15-what-changed-and-why.md`](15-what-changed-and-why.md)
