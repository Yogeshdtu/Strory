# 06 — Instruction-cache layout: PGO and BOLT

## Prerequisites
- `05-attribute-hot-cold.md`
- `36-LOW-LATENCY-CPP/21-instruction-cache.md`
- `33-COMPILER/*` (PGO ka intro — agar kiya)

## Yeh topic abhi kyun

`04/05` ne cold code ko raaste se hataya. Ab agla level: **poore binary ka
code layout** — hot functions ek-doosre ke paas, hot basic-blocks
straight-line. Yeh manually nahi hota — **profile-guided** tools karte hain.

---

## Kyun code layout matters

CPU code ko bhi cache karta:

| Structure | Zen 2 (is box) | Miss ki cost |
|---|---|---|
| L1 I-cache | 32 KB, 8-way | ~10-14 cyc (L2 se) |
| uop cache (op cache) | ~4 K uops | decode dobara — 3-4 cyc/uop |
| iTLB | ~64 entries (4K pages) | page walk ~10-30+ cyc |
| Branch target buffer | limited | mispredict-jaisa stall |

Agar hot code **bikhra** hai (function A yahan, uska helper 40 KB door,
uska helper 200 KB door):
- Har call ek naya L1i line, shayad naya iTLB entry.
- uop cache thrash — same hot loop baar-baar decode.
- Prefetcher (jo linear aage padhta) useless — code jump kar raha.

`perf stat` signal: **`stalled-cycles-frontend` high**, `L1-icache-load-misses`
high, `iTLB-load-misses` non-trivial. TMA: **"Frontend Bound"** bucket bada.

---

## Fix 1 — PGO (Profile-Guided Optimization)

Compiler ko batao asli run mein kya-kya chalta.

```bash
# 1. instrumented build
g++ -std=c++20 -O2 -fprofile-generate feed_handler.cpp -o fh_instr

# 2. representative workload chalao (asli jaisa data!)
./fh_instr < recorded_market_data.bin

#    -> *.gcda files likhti (counts: kaunsa branch kitni baar, kaunsa fn kitni baar)

# 3. use that profile
g++ -std=c++20 -O2 -fprofile-use -fprofile-correction feed_handler.cpp -o fh_pgo
```

PGO kya karta:
- **Function reordering** — jo functions ek-doosre ko call karte, unhe paas
  rakhta (better I-cache/iTLB locality).
- **Basic-block ordering** — hot path straight-line (fall-through), cold
  blocks function ke end mein / `.text.unlikely` mein.
- **Inlining decisions** — hot call-sites pe inline, cold pe nahi (code
  size bachata).
- **Branch hints** — asli measured bias (`[[likely]]` ka data-driven
  version).
- Register allocation, loop unrolling — hot loops ke favor mein.

Documented gains (GCC/LLVM studies, large C++ services): **5-20%**, kabhi
zyada, jab workload frontend-bound ho. HFT feed handlers exactly is
category mein aate (huge codebase, ek hot loop).

**Clang:** `-fprofile-instr-generate` / `llvm-profdata merge` /
`-fprofile-instr-use`. Ya sampling-based: `-fprofile-sample-use` (perf se
profile, no instrumented build).

---

## Fix 2 — BOLT (Binary Optimization and Layout Tool)

PGO compile-time hai. **BOLT** post-link, binary pe seedha kaam karta —
`perf` profile leta aur ELF ko **rewrite** karta.

```bash
# 1. normal (ya PGO) build, with relocations kept
g++ -std=c++20 -O2 -Wl,--emit-relocs feed_handler.cpp -o fh

# 2. perf se profile (production-jaisa load)
perf record -e cycles:u -j any,u -o perf.data -- ./fh < market_data.bin

# 3. BOLT
perf2bolt -p perf.data -o fh.fdata fh
llvm-bolt fh -o fh.bolt -data=fh.fdata \
    -reorder-blocks=ext-tsp -reorder-functions=hfsort \
    -split-functions -split-all-cold -icf=1 -dyno-stats
```

BOLT ke moves: aggressive function splitting (hot/cold parts alag),
`hfsort` function ordering (call-graph pe based), block reordering
(`ext-tsp` — I-cache ke liye near-optimal), identical-code folding.

Meta/Google ne large binaries pe PGO **ke upar** aur 5-15% dikhaya. HFT
shops jinke paas ek massive static binary hota — BOLT common hai.

PGO + BOLT **stackable** hain: PGO source-level decisions, BOLT final
layout.

---

## Fix 3 — manual (jab PGO/BOLT setup na ho)

- Cold functions `[[gnu::cold, noinline]]` (`05`).
- Hot functions ek hi TU mein, ek doosre ke paas (source order ~ layout
  order without PGO).
- `-freorder-blocks-and-partition` (`-O2` default GCC pe) — hot/cold block
  split.
- Chhota rakho: template bloat (`36/23`, lesson `12`), unrolling sirf jab
  measured faayda.
- `__attribute__((section(".text.hot")))` se manually kuch functions group
  karo (last resort).

---

## Is folder ke examples pe

`08_hot_cold.cpp` chhota hai — ek loop, do run functions. PGO/BOLT ka
faayda yahan **measurable nahi** (frontend bottleneck hi nahi). Yeh
technique **poore feed handler / poore trading binary** ke scale pe hai.
Isliye yahan concept + workflow, number nahi (jo number hota woh fake
hota — Rule 2).

> **HFT relevance:** ek production trading binary 50-500 MB text ho sakta
> (templates, vendored libs, strategy variants). Ek tick ka hot path usme
> se ~10 KB chhuta. PGO+BOLT us 10 KB ko physically ek jagah la dete —
> p99.9 pe measurable, kyunki tail hiccups aksar iTLB/I-cache miss hote.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — non-representative training workload
PGO ka profile ek "hello world" run se banaya → production mein ulta hint.
Training data **production jaisa** hona chahiye (recorded feed, real
message mix).

### Trap 2 — PGO profile stale
Code badla, purana `.gcda` use kiya → `-Wcoverage-mismatch` warnings,
galat hints. CI mein profile refresh karo.

### Trap 3 — BOLT bina `--emit-relocs`
BOLT ko relocations chahiye binary rewrite ke liye. Bina — BOLT fail ya
degraded.

### Trap 4 — frontend-bound nahi, phir bhi PGO se miracle expect karna
Agar `perf stat` "Backend Bound (memory)" dikhata — PGO code layout se
5% milega shayad, asli problem data cache hai (`07`, `11`). Pehle diagnose.

### Trap 5 — PGO ko debugging ke saath mix karna
PGO binary ka control flow reorder hota — debugger stepping confusing,
`perf annotate` line mapping thoda off. Dev builds bina PGO, release ke
saath.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| PGO = "compiler magic, hamesha faster" | Sirf frontend/branch-bound code pe; data-bound pe kam |
| BOLT PGO ki jagah | BOLT PGO **ke upar** — final layout pass |
| Code layout sirf size ka issue | I-cache/uop/iTLB miss = p99.9 tail hiccups |
| Manual section attributes = PGO | PGO measured counts use karta; manual = guess |

---

## Hands-on

```bash
# PGO ka chhota demo (Linux/MinGW dono):
g++ -std=c++20 -O2 -fprofile-generate 43-HFT-OPTIMIZATION/examples/08_hot_cold.cpp -o /tmp/hc_i
/tmp/hc_i
g++ -std=c++20 -O2 -fprofile-use -fprofile-correction 43-HFT-OPTIMIZATION/examples/08_hot_cold.cpp -o /tmp/hc_p
/tmp/hc_p    # farak ~noise is chhote demo pe -- workflow yaad rakho
```

---

## Exercises

1. `perf stat` output: IPC 1.9 (achha), `stalled-cycles-frontend` 8%,
   `LLC-load-misses` bahut high. PGO/BOLT se kitna milega?
   <details><summary>Answer</summary>
   Kam — yeh **backend/memory-bound** hai (frontend theek, IPC achha).
   PGO layout se shayad 2-3%. Asli kaam: `07` (struct layout), `11`
   (lookup cache), `36/10` (data locality). PGO galat lever.
   </details>

2. PGO training run tune ne unit-test suite se kiya (jo error paths ko
   heavily exercise karta). Production pe kya hoga?
   <details><summary>Answer</summary>
   Compiler error-handling ko "hot" maan lega → error paths straight-line
   / inlined, **normal path** cold-treated. Production (jahan errors rare)
   mein hot path ka layout kharab. Training = production traffic ka
   replay hona chahiye.
   </details>

3. Tumhare binary mein 3 strategy variants hain (templates se), har ek 2 MB
   text. Ek run mein sirf 1 active hota. BOLT kya karega?
   <details><summary>Answer</summary>
   `-split-all-cold` + `hfsort`: jo variant is profile mein chala uska hot
   code ek jagah; baaki 2 variants ka code (aur active variant ke cold
   parts) `.text.cold` mein door. Active hot path ka I-cache/iTLB
   footprint 6 MB se ~10 KB tak effectively.
   </details>

---

## Interview questions

1. PGO ke 3 steps? Training workload kaisa hona chahiye aur kyun?
2. PGO code layout ke liye kya-kya reorder karta (functions, blocks,
   inline)?
3. BOLT PGO se alag kaise (kab, kis input pe, kya rewrite)?
4. `perf stat` mein kaunse counters "frontend bound" batate?
5. PGO se 15% mila; ab BOLT bhi laga sakte? Stack hota hai?

---

## Next
→ [`07-struct-layout-tuning.md`](07-struct-layout-tuning.md)
