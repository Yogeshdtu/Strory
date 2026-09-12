# 06 — Prefetching: hardware prefetchers aur `__builtin_prefetch`

## Prerequisites
- `05-locality.md`
- `31-CPU-ARCHITECTURE/06-out-of-order-execution.md` (MLP, LFBs)

## Yeh topic abhi kyun
Compulsory misses avoid nahi hote — par unki **latency** kaam ke peeche
chhupayi ja sakti hai agar line aane ka order pehle se pata ho. Yeh kaam
hardware **apne aap** karta hai predictable patterns pe (aur mostly yahi
kaafi hai). Jab pattern unpredictable ho — indirect index, gather — tab aap
`__builtin_prefetch` se manually hint de sakte ho. Example `06` ne dikhaya
ki yeh hint **aksar marginal ya ulta** padta hai bade OoO core pe — woh
lesson bhi yahin hai.

---

## Hardware prefetchers

CPU ke andar kai chhote engines jo access pattern dekh ke aage ki lines
speculatively laate hain:

| Prefetcher | Kya detect karta | Kahan |
|---|---|---|
| **Next-line** | ek line miss → agli line bhi laao | L1/L2 |
| **Adjacent-line** ("buddy") | lines ko jodo mein (128 B aligned pair) | L2 |
| **Stride / stream (IP-based)** | ek instruction se constant-stride accesses (`+8, +8, +8`) | L1/L2 |
| **Spatial / region** | ek ~4 KiB region mein bikhre accesses → poora region | L2 |

Key limits:
- **Per-page.** Stride prefetcher ka state page ke hisaab se — page boundary
  cross karo, pattern re-learn hota (kuch lines "cold" milti hain).
- **Monotonic only.** `+8,+8,+8` ✅. `+8,-4,+16` (random) ❌. Pointer chase ❌.
- **Detection latency.** Pehle 2-3 accesses pe woh pattern seekhta hai —
  chhote loops ko poora fayda nahi milta.
- **Bandwidth budget.** Aggressive prefetch bandwidth kha sakta — agar galat
  guess kiya to demand loads ke saath compete (example `06` S2).

**Zyaadatar achha sequential/strided code ke liye HW prefetcher hi kaafi hai.**
Aapka pehla kaam: access ko linear banao (lesson 05), na ki prefetch hints
bharna.

---

## Software prefetch: `__builtin_prefetch`

```cpp
__builtin_prefetch(const void* addr,
                   int rw = 0,        // 0 = read, 1 = write (RFO)
                   int locality = 3); // 0..3 : 0 = NTA (don't keep), 3 = keep in all levels
```
x86 intrinsics: `_mm_prefetch(addr, _MM_HINT_T0 / _T1 / _T2 / _NTA)`.

Yeh ek **non-binding hint** hai: CPU line laane ki koshish karega, par fault
nahi karega (bad address → silently ignore), aur reorder/drop kar sakta.
Emits `PREFETCHt0` etc. — ek load jaisa jo register nahi likhta.

### Pattern: prefetch D iterations ahead

```cpp
for (size_t i = 0; i < n; ++i) {
    if (i + D < n)
        __builtin_prefetch(&table[idx[i + D]], 0, 0);   // future address, known now
    process(table[idx[i]]);
}
```

`idx[]` ek plain array hai → `idx[i+D]` abhi pata hai → uska target line **D
iterations pehle** DRAM se maangna shuru → jab loop wahan pahunche, line
already cache mein.

**D (prefetch distance)** tune karo:
- bahut chhota → line time pe nahi aati (prefetch aur use ke beech DRAM
  latency se kam gap)
- bahut bada → prefetched line use hone se pehle evict, aur cache pollution
- typical D: 4–32, workload pe depend. **Measure** (example `06` D sweep).

---

## Example `06` — measured, aur ⚠️ Rule 2

256 MiB table, random `idx[]`, is box (~2 GHz Zen 2):

| Scenario | no prefetch | best SW prefetch | verdict |
|---|---|---|---|
| **S1** light gather (`s += table[idx[i]]`) | ~9.0 ns/lookup | ~7.9 ns (D=32) | **~1.15x** — marginal |
| **S2** heavy (64-B bucket scan + dependent `mix()`) | ~3.3 ns/lookup | ~9-10 ns | **~0.33x — 3x SLOWER** |

Kyun itna kam / ulta:
- **S1**: `table[idx[i]]` ke addresses ek doosre pe depend nahi karte. Bade
  OoO core ka MLP (~10 LFBs) already ~10 misses overlap kar leta → effective
  ~9 ns (true DRAM ~80). SW prefetch bas thoda aur → ~1.15x.
- **S2**: har lookup 16 sequential loads (ek line) + dependent `mix` chain.
  Woh 16 independent loads **already** memory system saturate kar dete
  (~3.3 ns/lookup, S1 se tez!). SW prefetch daalna extra requests → LFB /
  bandwidth ke liye demand loads se competition → 3x dheema.

**Sabak:** SW prefetch bade OoO core pe "free speedup" NAHI hai. Woh tab
jeetta hai:
1. **In-order / narrow-OoO core** (embedded, purane, kuch cloud instances).
2. **Verified MLP deficit** — `perf` bole memory-parallelism low hai
   (dependency chain, ROB fill).
3. **HW prefetcher blind spot** cross karni ho — jaan-boojh ke page-boundary
   ke paar prefetch, ya non-monotonic pattern jise HW nahi pakadta.

Warna: pehle contiguous / SoA / blocking. Prefetch aakhri scalpel.

---

## `PREFETCHNTA` aur non-temporal

`locality = 0` (`_MM_HINT_NTA`) = "yeh line laao par cache hierarchy pollute
mat karo" — L1 mein (ya ek chhote NTA buffer mein) daalo, L2/L3 skip. Use:
ek bade array ko ek baar stream karke padhna jise turant dobara nahi chahiye
→ NTA se woh aapka hot L2/L3 data evict nahi karega.

Trade-off: agar aapne galti se woh line dobara chahi to full miss. NTA sirf
sach-much-use-once data pe.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "prefetch add karo, tez ho jaayega"
Example `06`: ~1.15x best case, 3x slower worst case. Bade OoO core pe aksar
bekaar. Measure before/after; agar <5% gain ya regression → hatao.

### Trap 2 — prefetch pointer-chase mein
`__builtin_prefetch(node->next)` — `node->next` ko padhne ke liye `node` ka
load poora hona chahiye → aap prefetch "ahead" nahi kar rahe, bas ek redundant
load. Fayda tab hi jab aap `node->next->next` (2-ahead) already jaante ho
(rare).

### Trap 3 — galat D (distance)
D too small = koi fayda nahi. D too large = evict before use + pollution.
Sweep karke best chuno, aur woh CPU/workload-specific hai (portable nahi).

### Trap 4 — HW prefetcher ke against ladna
Sequential loop mein manual prefetch = HW already kar raha hai, aap sirf
issue slots aur bandwidth waste kar rahe. Manual prefetch **sirf** un patterns
pe jinhe HW nahi pakadta (indirect, gather).

### Trap 5 — `rw=1` (write prefetch) galat jagah
Write-prefetch line ko "modified" state mein RFO karta — agar aap actually
nahi likhne wale to aapne bekaar mein exclusive ownership le liya, doosre
cores se woh line cheeni.

### Trap 6 — prefetch instructions ka overhead bhoolna
Har `__builtin_prefetch` ek uop hai — issue port, decode bandwidth. Ek tight
loop mein 2 prefetch/iter = 2 extra uops/iter. Agar loop already
front-end-bound, yeh ulta.

---

## > **HFT relevance**

> - **Pehla instinct: layout, prefetch nahi.** SoA + contiguous + flat
>   structures se HW prefetcher khush → aur reliable than manual hints.
> - **Jab prefetch: known-ahead addresses.** Market data messages ek ring
>   mein — agla message ka slot pata hai, use prefetch karo jab current
>   process kar rahe ho (yeh sequential hai to HW bhi kar deta — measure).
> - **Order book walk mein gather:** agar aap price levels ko ek index array
>   se touch karte ho (`level[order_idx[i]]`), `order_idx[i+D]` ka level
>   prefetch — par pehle dekho MLP already overlap to nahi kar raha.
> - **`PREFETCHNTA` for one-shot scans:** ek pura log/replay buffer padhna
>   jise hot path dobara nahi chahiye → NTA se L2/L3 safe.
> - **Har prefetch ko measure se justify karo.** "Lagta hai madad karega"
>   HFT mein kaafi nahi — before/after number, aur `perf` counters.

---

## Hands-on

```bash
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/06_prefetch.cpp
#   S1: ~1.15x (marginal)   S2: ~0.33x (3x SLOWER) -- Rule 2 in action

# HW prefetcher ka asar: example 02 PART 1 (sequential, prefetcher ON) vs
# PART 2 random (prefetcher andha) -- ~7x farak, bina kisi manual hint ke.

# Linux -- memory parallelism dekho (kya SW prefetch ki gunjaish hai):
perf stat -e mem_load_retired.l3_miss,offcore_requests_outstanding.demand_data_rd ./prog
# BIOS mein HW prefetchers toggle kar ke A/B bhi kar sakte ho (MSR 0x1A4)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "SW prefetch = free speedup" | example 06: ~1.15x best, 3x worse worst |
| "prefetch har miss hide karta" | sirf jab MLP kam ho aur address known-ahead |
| "pointer-chase ko prefetch karo" | next address current load pe depend — nahi hota |
| "HW prefetcher optional hai" | zyaadatar sequential code ke liye woh hi kaafi |
| "D koi bhi chalega" | too small = useless, too large = evict+pollute; sweep |
| "prefetch ka koi cost nahi" | uop + issue slot + bandwidth; front-end-bound loop me ulta |

---

## Exercises

1. `for (i) hist[bucket(key[i])]++;` — `key[i]` sequential array, `bucket()`
   ek hash. `hist` 4 MiB (L2 se bada). HW prefetcher `key[]` ke liye kaam
   karega? `hist[...]` ke liye? SW prefetch kahan help kar sakta?

   <details><summary>Answer</summary>

   `key[]` sequential → HW prefetcher ✅ (woh stream detect kar lega). `hist[
   bucket(...)]` → address `key[i]` ke hash pe depend, effectively random over
   4 MiB → HW prefetcher ❌. SW prefetch: `bucket(key[i+D])` compute karke
   `__builtin_prefetch(&hist[that], 1 /*write*/, 0)` D-ahead — yeh madad kar
   *sakta* hai kyunki har `hist[...]++` ek RMW hai (read miss + write) aur
   consecutive updates ke addresses independent hain but the increment itself
   is short → ROB shallow-fills. **Measure** — is box pe shayad 1.2-1.5x
   (S1 jaisa). Behtar: keys ko partial-sort/partition by bucket-range pehle
   (radix), phir hist access locality-friendly.
   </details>

2. Aapne ek tight loop mein `__builtin_prefetch` 2 baar/iter daala aur code
   5% dheema ho gaya, jabki cache miss rate wahi hai. Kya hua?

   <details><summary>Answer</summary>

   Loop pehle se **front-end / issue bound** tha (ya bandwidth bound), memory-
   latency bound nahi. Har prefetch ek extra uop hai — decode + issue port
   consume karta. 2 extra uops/iter ne throughput gira diya, aur cache miss
   rate isliye same hai kyunki HW prefetcher ya MLP already woh data laa raha
   tha. Prefetch hatao. Sabak: prefetch sirf latency-bound loops mein, aur
   sirf jab measurement gain dikhaye.
   </details>

3. `PREFETCHNTA` kab use karoge, aur ek scenario jahan woh backfire karega?

   <details><summary>Answer</summary>

   **Use**: ek bada buffer (say 200 MB replay log) jise aap ek baar linearly
   scan karke parse karte ho aur hot path use dobara nahi chhuta — NTA se woh
   200 MB aapke L2/L3 se hot trading data ko evict nahi karega (cache
   pollution avoid). **Backfire**: agar aapne galti se woh data thodi der
   baad dobara access kiya (e.g. second pass, ya woh actually hot nikla) —
   NTA ne use L2/L3 mein rakha hi nahi tha → har re-access full DRAM miss.
   NTA = "main kasam khaata hoon yeh use-once hai."
   </details>

---

## Interview questions

1. HW prefetchers ke types aur har ek kya detect karta.
2. Stride prefetcher ki 2 key limitations (per-page, monotonic).
3. `__builtin_prefetch` ke 3 args — `rw` aur `locality` ka matlab.
4. "Prefetch D iterations ahead" pattern — D chhota/bada hone se kya.
5. Example `06` ka result — SW prefetch bade OoO core pe kyun marginal/ulta.
6. SW prefetch kab actually jeetta hai (3 conditions).
7. `PREFETCHNTA` — use case aur ek failure mode.

---

## Next
→ [`07-false-sharing.md`](07-false-sharing.md)
