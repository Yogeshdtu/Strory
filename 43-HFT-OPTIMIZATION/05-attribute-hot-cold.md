# 05 — `__attribute__((hot/cold))`, section placement, `[[likely]]`

## Prerequisites
- `04-hot-cold-path-separation.md`

## Yeh topic abhi kyun

04 ne concept diya. Ab **exact tools** — kaunsa attribute, kya karta,
compiler kaise binary layout badalta, aur kaise verify karo (asm).

---

## The tools

| Tool | Standard | Kya karta |
|---|---|---|
| `[[likely]]` / `[[unlikely]]` | C++20 | branch bias hint — likely side fall-through |
| `__builtin_expect(x, c)` | GCC/Clang | wahi, pre-C++20 |
| `[[gnu::hot]]` | GCC/Clang attr | function ko "frequently executed" mark — aggressive inline into it, `.text.hot` section, PGO-jaisa treatment |
| `[[gnu::cold]]` | GCC/Clang attr | function "rarely executed" — `.text.unlikely` section, size-optimize, callers mein iske calls ko unlikely maano |
| `[[gnu::noinline]]` | GCC/Clang attr | inline mat karo (cold body ko out-of-line rakhne ke liye zaroori) |
| `[[noreturn]]` | C++11 | function return nahi karta (abort/throw path) — caller cold-treats |
| `-freorder-blocks-and-partition` | GCC flag (`-O2` default) | basic blocks ko hot/cold partition mein tod, cold ko alag section |

---

## Section placement — kya hota hai

Normal: sab code `.text` mein, source order mein. Ek function ka cold `if`
branch uske hot code ke **beech** baith jata.

`[[gnu::cold]]` / `-freorder-blocks-and-partition` ke saath:

```
.text.hot        [ hot_update  run_outlined ka hot loop  ... ]   <- ek jagah, dense
.text            [ normal functions ]
.text.unlikely   [ cold_snapshot  cold_recovery  cold_admin ... ]  <- door, alag pages
```

Faayda:
- Hot code **contiguous** → ek L1i region, ek iTLB entry set, prefetcher
  linear chal sakta.
- Cold code **kabhi load nahi hota** jab tak call na ho — L1i / iTLB /
  page cache mein jagah nahi ghera.
- Branch se cold tak ka jump ab ek "far" jump hai (predicted not-taken,
  cost ~0 jab tak actually na ho).

---

## Verify — asm dekho (guess mat karo)

```bash
./build.ps1 asm 43-HFT-OPTIMIZATION/examples/08_hot_cold.cpp
# ya:
g++ -std=c++20 -O2 -S -masm=intel 08_hot_cold.cpp -o - | c++filt | less
```

Dekhne ka:
- `run_outlined` ka loop: `hot_update` inline (straight-line), cold cases
  = `call cold_snapshot` etc. (ek call instruction, body door).
- `cold_snapshot` ki definition ke upar `.section .text.unlikely` (ya
  `.text$...` MinGW pe).
- `run_inline` ka loop: cold body ka poora code (`memset`, 64-iter loop)
  loop ke **andar** — bada.

Agar `[[gnu::cold]]` ke bawajood cold body inline dikhe → `[[gnu::noinline]]`
add karo.

---

## Measured (is box) — `08_hot_cold.cpp`

```
A cold INLINE in hot loop          :  ~8.8 ns/msg
B cold [[gnu::cold]] + [[unlikely]] :  ~8.7 ns/msg   (~1% faster, ≈ noise)
```

**Farak chhota — Rule 2.** Reason (`04` mein bhi): hot loop already
frontend-comfortable tha. Attribute ne asm layout theek se badla (verify
kiya) par is workload pe frontend bottleneck nahi tha, isliye wall-time
pe ~0.

**Toh kyun lagayein?**
1. **Cost 0** — koi runtime penalty nahi.
2. **Intent** — code padhne wale ko dikhta "yeh rare hai".
3. **Scale pe payoff** — bade binary (`perf` "Frontend Bound" high) mein
   10-30% documented (LLVM/GCC PGO studies, `06`).
4. **PGO/BOLT ka foundation** — profile-guided tools inhi hints ko
   amplify karte.

Yeh ek **"lagao by default, measure at scale"** technique hai — `36/06`
branchless (jo situational hai) se alag.

---

## `[[gnu::hot]]` — dusra side

Ek function jo tumhe pata hai **hamesha** chalega (main event loop ka
core):

```cpp
[[gnu::hot]] static void process_tick(Book& bk, const Msg& m) { ... }
```

Effect: compiler is function ke andar aggressively inline karta, ise
`.text.hot` mein daalta, aur register allocation / block ordering ko
iske favor mein tune karta. `perf`/PGO ka manual version.

**Saavdhani:** har function ko `hot` mark karna = kuch nahi (sab hot = kuch
hot nahi). Sirf sach mein hot 2-3 core functions.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `[[likely]]` ko `[[gnu::hot]]` samajhna
`[[likely]]` ek **branch** hint hai (kaunsa arm). `[[gnu::hot]]` ek
**function** attribute hai. Alag cheezein, saath use hoti.

### Trap 2 — cold function ko `static` na banana / ek hi TU mein na rakhna
Cross-TU pe compiler cold-ness propagate nahi kar sakta (bina LTO). Cold
handlers ko `static` + `[[gnu::cold]]`, ya alag TU jise `-O2` compile karo.

### Trap 3 — MSVC pe GCC attributes
`[[gnu::cold]]` MSVC pe ignore. MSVC: `__declspec(noinline)` + PGO
(`/GL /LTCG /PGD`). Portable: `[[likely]]`/`[[unlikely]]` (C++20, sab
compilers).

### Trap 4 — asm verify skip karna
"Attribute laga diya, ho gaya" — nahi. Compiler version / flags ke hisaab
se behavior badalta. `-S` se **dekho** cold body kahan gaya.

### Trap 5 — `__builtin_expect` ko probability samajhna
`__builtin_expect(x, 1)` ka matlab "x usually 1" nahi "**x == 1 is the
likely branch**". Value match hona chahiye, sirf truthy nahi.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| `[[gnu::cold]]` = slow function | = **rarely-called** function (layout hint, not speed) |
| Attribute lagte hi 10% faster | Scale-dependent; is demo pe ~0, bade binary pe 10-30% |
| `[[likely]]` har `if` pe | Sirf genuinely (>90%) skewed branches |
| PGO ki jagah attributes kaafi | Attributes = manual approximation of PGO; PGO better data-driven |

---

## Hands-on

```bash
./build.ps1 fast 43-HFT-OPTIMIZATION/examples/08_hot_cold.cpp
./build.ps1 asm  43-HFT-OPTIMIZATION/examples/08_hot_cold.cpp | grep -A2 -i "unlikely\|cold_"
```

---

## Exercises

1. `08` mein se `[[gnu::noinline]]` hata do (sirf `[[gnu::cold]]` rakho).
   Asm mein cold body kahan hai ab?
   <details><summary>Answer</summary>
   Compiler cold body ko `run_outlined` ke loop mein wapas inline kar
   sakta hai (chhota samajhkar), bas `.text.unlikely` block ke andar. B
   ab A jaisa dikhne lag sakta. `noinline` cold-body-out-of-line ka
   guarantee deta.
   </details>

2. `[[likely]]`/`[[unlikely]]` `run_outlined` se hata do. Farak?
   <details><summary>Answer</summary>
   Is box pe ~0 (branch pehle se ~99% predictable, HW predictor khud
   seekh leta). Hint compiler ke **static** layout ke liye zyada matter
   karta (cold side ko fall-through se hataana) — bade functions mein
   dikhta.
   </details>

3. Tumhare paas 200 KB ka ek monster event-loop function hai, `perf stat`
   `stalled-cycles-frontend` 45%. Kaunse teen kadam?
   <details><summary>Answer</summary>
   (1) Cold branches (`error`, `recovery`, `logging`) ko `[[gnu::cold,
   noinline]]` alag functions mein — hot loop ka byte-size girao. (2) PGO:
   `-fprofile-generate` → representative run → `-fprofile-use` (`06`). (3)
   Agar aur chahiye: BOLT (`06`) post-link. Har kadam ke baad `perf stat`
   re-check.
   </details>

---

## Interview questions

1. `[[gnu::hot]]` vs `[[gnu::cold]]` vs `[[likely]]` — kya-kya, kis level pe?
2. `.text.unlikely` section ka faayda (I-cache / iTLB / page terms mein)?
3. `__builtin_expect(ptr == nullptr, 0)` ka exact matlab?
4. Attribute laga diya, wall-time farak 0. Bekaar? Kyun/kyun nahi?
5. PGO attributes se behtar kaise (data source ke terms mein)?

---

## Next
→ [`06-instruction-cache-layout.md`](06-instruction-cache-layout.md)
