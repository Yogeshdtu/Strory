# 04 — Hot / cold path separation

## Prerequisites
- `03-finding-bottlenecks.md`
- `36-LOW-LATENCY-CPP/12-branch-free-programming.md`, `21-instruction-cache.md`

## Yeh topic abhi kyun

Profile ne bataya kahan time jaata. Ab ek structural idea: **99% ke case ko
sabse tez banao, 1% ke case ko raaste se hataao.** Yeh sirf micro-opt nahi —
poore system ka shape hai.

---

## Hot path kya hai

**Hot path** = woh code jo har (ya lagbhag har) message pe chalta.
**Cold path** = jo rarely chalta (error, recovery, admin, snapshot,
session reset, logging).

Feed handler mein (`08_hot_cold.cpp` ka model):

```
message aaya
   │
   ├─ type ∈ {add, cancel, modify, trade}   ──►  HOT   (~99%)  book update
   │
   └─ type ∈ {snapshot-req, gap-recovery,        COLD  (~1%)   bulky handler:
              admin, error, session-reset}                     log format,
                                                               recovery table,
                                                               buffer clear
```

`08_hot_cold.cpp`: ~1.00% messages cold. Hot path = ~30 instructions
(book array write + checksum + EWMA). Cold handlers = ~200+ instructions
each (loop, memset, "format").

---

## Hot path ke rules

1. **Chhota** — kam instructions → L1i / uop-cache mein poora fit → frontend
   ko run-ahead karne ki jagah.
2. **Straight-line** — kam branches, aur jo hain woh **predictable** (hot
   direction hamesha same). `[[likely]]` / `[[unlikely]]` se compiler ko
   batao.
3. **Cold code beech mein nahi** — cold handler ka bulky code agar hot loop
   ke body mein inline hai, to hot path ko har iteration uske **upar se
   jump** karna padta, aur woh bytes L1i / uop-cache ki jagah khaate.
4. **No allocation, no syscall, no lock** hot path mein — woh sab cold-path
   cheezein hain (`36/04`, `36/17`).
5. **Predictable memory access** — sequential ya cached, pointer-chase nahi.

---

## Cold code ko kaise hataayein

### C++ tools

```cpp
if (likely_case) [[likely]] {
    hot_update(...);                       // straight-line, small
} else [[unlikely]] {
    handle_rare(...);                      // -> alag function (agla lesson)
}
```

- `[[likely]]` / `[[unlikely]]` (C++20) — compiler ko branch bias batao.
  Effect: hot side **fall-through** (no taken branch), cold side jump.
- Cold handler ko **`[[gnu::cold]] [[gnu::noinline]]`** function banao —
  compiler use `.text.unlikely` section mein daalta, hot code se **door**.
  (Detail + measurement: `05-attribute-hot-cold.md`.)
- `__builtin_expect(cond, 0)` — purana pre-C++20 tareeka, same effect.

### Structural

- Cold data ko cold struct mein (hot struct se `id` se link) — `07`.
- Cold-path buffers ko lazily allocate / alag pool.
- Slow-path logging ko async queue pe dhakelo (`41/07`) — hot path sirf
  ek pointer push karta.

---

## `08_hot_cold.cpp` — measured (is box)

```
messages: 8388608   cold: 83792 (1.00%)

A cold INLINE in hot loop         :  ~8.8 ns/msg
B cold [[gnu::cold]] + [[unlikely]]:  ~8.7 ns/msg   (~1% faster, run-to-run noise ke andar)
```

**Honest result (Rule 2):** is demo pe farak ~1%, noise ke barabar — jaise
`36/12` ne bhi dekha. Kyun? Hot loop ka code itna bada nahi hua ki L1i
(32 KB) / uop-cache se nikle. `[[gnu::cold]]` ne cold handlers ko `.text.unlikely`
bhej diya (asm mein confirm: `./build.ps1 asm`), par hot path pehle hi
frontend mein comfortable tha.

**Kab asli farak dikhta:** poora feed handler + parser + risk checks +
strategy — **hazaaron** instructions, jahan `perf stat` "Frontend Bound"
ya `L1-icache-load-misses` high dikhaye. Wahan hot/cold split + PGO/BOLT
(`06`) **10-30%** de sakte. Chhote demo pe number chhupaya nahi — yahi
sabak hai: **structural changes ka payoff scale-dependent hota, measure
karke confirm karo.**

Attribute lagane ki **cost 0** hai aur intent document hota — isliye lagao
hamesha, bhale hi is micro-bench pe farak na dikhe.

> **HFT relevance:** ek exchange feed handler mein "normal incremental
> update" 99.99% hai; "snapshot recovery after gap" 0.01% par 100× bada
> code. Recovery code ko hot path se physically alag rakhna (alag function,
> alag TU, `.text.unlikely`) — hot path ka p99.9 tight rehta.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "cold" ko galat identify karna
Jo tumhe rare lagta woh profile mein 20% ho sakta (e.g. ek "error" jo
actually har 5th message pe fire hota). **Profile se** hot/cold decide karo,
gut se nahi.

### Trap 2 — hot path mein ek `try`/`catch` ya `std::function`
Exceptions ka zero-cost sirf **non-throwing** path pe hota — par `catch`
block ka code aur unwinding tables hot function ke saath rehti (I-cache).
`std::function` = indirect call + possible heap. `36/13`, `36/14`.

### Trap 3 — cold handler inline reh gaya
`[[gnu::cold]]` bina `[[gnu::noinline]]` ke — compiler chhota samajhkar
inline kar sakta. Dono lagao. Asm check karo.

### Trap 4 — sab kuch `[[likely]]` mark karna
Har branch pe `[[likely]]` = compiler ko koi info nahi (sab "likely" =
kuch bhi nahi). Sirf **sach mein** skewed branches pe.

### Trap 5 — micro-demo pe farak na dikhe to concept chhod dena
`08` pe ~1% aaya. Iska matlab "hot/cold bekaar" nahi — iska matlab "is
scale pe frontend bottleneck nahi tha". Bade binary pe `perf` se verify.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Hot/cold = sirf `[[likely]]` lagana | Structural: alag functions, alag sections, alag data |
| Cold code hot function mein rehne do, branch to skip kar dega | Bytes phir bhi I-cache/uop-cache ghere; jump overhead |
| Attribute lagao tabhi jab benchmark farak dikhaye | Cost 0, intent-doc, scale pe faayda — hamesha lagao |
| Rare = safe to ignore performance-wise | Rare-but-huge code hot path ka layout kharab karta |

---

## Hands-on

```bash
./build.ps1 fast 43-HFT-OPTIMIZATION/examples/08_hot_cold.cpp
./build.ps1 asm  43-HFT-OPTIMIZATION/examples/08_hot_cold.cpp   # B: cold_* .text.unlikely mein
```

---

## Exercises

1. `08_hot_cold.cpp` mein cold fraction 1% se 20% kar do (`(r % 5u) == 0u`).
   A vs B ab? Kyun badla?
   <details><summary>Answer</summary>
   Cold ab itna common ki `[[unlikely]]` galat ho gaya — B mein har 5th
   msg ek mispredicted branch + function call. A/B dono dheeme, aur B ka
   [[unlikely]] ulta nuksaan kar sakta. Sabak: "cold" ki definition
   profile pe tiki honi chahiye.
   </details>

2. Hot path mein tumne ek `printf` debug line chhod di ("sirf development
   ke liye"). Kya-kya kharaab hota latency ke liye?
   <details><summary>Answer</summary>
   `printf` = format parsing + `write()` syscall (kernel transition) +
   possible lock (stdout) + huge code (I-cache). Ek line poore hot path ka
   p99 barbaad. Cold-path async logger pe bhejo (`41/07`), ya compile-time
   `if constexpr (kDebug)`.
   </details>

3. Ek "risk check" function normal case mein bas 2 comparisons hai, par
   limit-breach case mein 500 lines (alerts, unwind, notify). Kaise
   structure karoge?
   <details><summary>Answer</summary>
   Hot: 2 comparisons inline. Breach: `[[unlikely]]` branch → `[[gnu::cold,
   noinline]] handle_breach()` alag function (ya alag TU). Hot path ka code
   = 2 cmp + 1 not-taken branch. Breach ka 500 lines kabhi I-cache mein
   nahi aata jab tak fire na ho.
   </details>

---

## Interview questions

1. Hot path aur cold path define karo. Feed handler mein 3 cold examples?
2. Cold code agar hot loop mein inline hai (par branch se skip hota) — phir
   bhi kya nuksaan?
3. `[[likely]]`/`[[unlikely]]` compiler ko kya batate, code pe kya asar?
4. `08_hot_cold.cpp` pe farak ~1% aaya. Iska sahi interpretation?
5. Cold-path logging ko hot path se kaise decouple karoge?

---

## Next
→ [`05-attribute-hot-cold.md`](05-attribute-hot-cold.md)
