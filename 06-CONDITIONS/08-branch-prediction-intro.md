# 08 — Branch prediction — pehla parichay

## Prerequisites
- [`01-if-statement.md`](01-if-statement.md) (`if` = ek branch), [`05-switch-vs-if.md`](05-switch-vs-if.md)
- `01-PROGRAMMING-BASICS/01-what-is-a-computer.md` (CPU basics)
- `05-OPERATORS/04-logical-operators.md` (branchless preview)

## Yeh topic abhi kyun
Ab tak aapne `if` likhna seekha. Yeh lesson dikhata hai ki `if` ki **cost data pe
depend karti hai** — ek hi code, same values, sirf order badalne se **~7x** farq
(hum abhi measure karenge).

Yeh HFT performance engineering ka pehla asli concept hai. Yahan hum sirf
**dekhenge aur maapenge** — poora mechanism folder 31 (CPU architecture) aur uska
istemaal folder 36 (low-latency C++) mein.

---

## CPU ek assembly line hai (pipeline)

Ek instruction execute karne ke kai steps hote hain — fetch, decode, execute,
memory, write-back. Modern CPU inhe **overlap** karta hai: jab instruction 1
"execute" pe hai, instruction 2 "decode" pe, instruction 3 "fetch" pe.

```
       cycle: 1     2     3     4     5     6
  instr 1:  [F]   [D]   [E]   [M]   [W]
  instr 2:        [F]   [D]   [E]   [M]   [W]
  instr 3:              [F]   [D]   [E]   [M]   [W]
```

Ek waqt mein 10–20 instructions "in flight" hoti hain. Yeh tab tak achha hai jab
tak CPU ko pata ho **agli instruction konsi hai**.

### `if` isko todta hai

```asm
cmp   eax, 128
jge   .taken        ; jump hoga ya nahi? -- yeh EAX pe depend karta hai
...                 ; jo abhi compute ho raha hai
```

`jge` pe CPU ko nahi pata jump hoga ya nahi — kyunki `cmp` ka result abhi ready
nahi. Agar CPU ruke (stall), pipeline khaali → bahut slow.

To CPU **rukta nahi — GUESS karta hai.**

---

## Branch predictor

CPU ke andar ek hardware unit har branch ka result **predict** karta hai, history
dekh ke:

- "Yeh branch pichli 100 baar `taken` tha → is baar bhi `taken` maano"
- "Yeh branch alternate ho raha tha (T, N, T, N…) → pattern follow karo"
- Loop ke `if` jaise: "999 baar `taken`, aakhri baar `not taken`"

Modern predictors **bahut** acche hain — regular patterns pe 95–99%+ accuracy.

### Guess sahi → (lagbhag) free

CPU ne jo instructions speculatively chalayi thi, woh sahi thi. Kuch nahi ruka.
Branch ki effective cost ~0.

### Guess galat → MISPREDICTION

CPU ne galat raaste ki instructions pipeline mein bhar li thi. Ab:
1. Un sabko **flush** karo (throw away)
2. Sahi address se **dobara fill** karo
3. Pipeline ke bharne ka wait

Penalty: **~15–20 cycles** (x86). Ek simple `int` add ~1 cycle hota hai — to ek
misprediction = ~15–20 add-instructions barbaad.

---

## Measure karo — sorted vs unsorted array

`examples/04_branch_benchmark.cpp`. 32,768 ints, har ek `0..255`. Loop:

```cpp
for (std::size_t i = 0; i < n; ++i) {
    if (a[i] >= 128) {       // ~50% chance -- yeh branch
        s += a[i];
    }
}
```

Poora array 4,000 baar process (~131 million branch evaluations). **Exactly same
code aur same values** — bas ek run mein array random, doosre mein sorted.

### Measured (GCC 15.1, `-O2`, x86-64)

```
  if-branch,  UNSORTED data : ~1450 ms
  if-branch,  SORTED   data :  ~205 ms      <- ~7x tez, sirf order alag
  branchless, UNSORTED data :  ~240 ms      <- order se koi farq nahi
```

*(Aapke machine pe absolute numbers alag; ratio ~5–8x rahega.)*

### Kyun

| Data | `a[i] >= 128` ka pattern | Predictor | Result |
|---|---|---|---|
| **Unsorted** | random — coin toss har iteration | ~50% galat | har doosri iteration ~15-20 cycle penalty → **slow** |
| **Sorted** | pehle saare `< 128` (N,N,N…), phir saare `>= 128` (T,T,T…). Branch sirf **ek baar** palat-ta hai | ~100% sahi | branch lagbhag free → **fast** |

Data ki *values* nahi badli — sirf **order** badla, jisse branch **predictable** ban
gaya.

---

## Branchless — branch ko hata do

Agar branch inherently unpredictable hai (50/50, data-dependent), to `if` hi mat
likho:

```cpp
// Branch ke saath
if (a[i] >= 128) s += a[i];

// Branchless -- comparison ka result (0/1) multiply karo
s += static_cast<long long>(a[i] >= 128) * a[i];
```

`a[i] >= 128` → `bool` → `0` ya `1`. Koi jump nahi → koi prediction nahi → **data
order se farq nahi**. Benchmark mein branchless ~240 ms — unsorted branch (~1450 ms)
se bahut tez, par sorted branch (~205 ms) se thoda **slow** (kyunki har element ka
kaam karta hai, chahe `< 128` ho).

Compiler khud bhi yeh kar sakta hai — `cmov` (conditional move) instruction, ya
vectorization (masked add). `-O2` pe aksar karta hai. (Isi wajah se is example mein
ek chhota compiler barrier daala gaya hai — warna GCC branch hata deta aur demo
hi gayab ho jaata. Yeh khud folder 33 ka topic hai.)

### Trade-off

| | Predictable branch | Unpredictable branch |
|---|---|---|
| **`if` (branch)** | ✅ lagbhag free | ❌ ~15-20 cyc/miss |
| **branchless (`cmov`/mask)** | ⚠️ thoda slower (dono side ka kaam) | ✅ constant, koi miss nahi |

**Rule: MEASURE karo. Predictable branch = rehne do. Unpredictable + hot = branchless
try karo, phir dobara maapo.**

---

## `[[likely]]` / `[[unlikely]]` (C++20)

Compiler ko hint — kaunsa raasta common hai:

```cpp
if (rc != 0) [[unlikely]] {
    return handleError(rc);      // compiler isse "cold" path maan ke
}                                 // door rakhega -- hot path straight-line
process();
```

- Common path ko fall-through (no jump) banata hai → I-cache aur predictor dono khush
- Cold code ko function ke end mein / alag section mein rakhta hai

⚠️ **Galat hint = ulta nuksaan.** Sirf tab lagao jab pakka pata ho (error paths,
assertion failures). Aur pehle bhi — measure. `__builtin_expect` iska purana form
hai.

> **HFT relevance:** Yeh lesson chhota hai par iska asar poore HFT track pe hai:
>
> - **Tail latency:** Average latency achha dikh sakta hai jabki har 1000th
>   message pe misprediction spike deadline miss kara de. HFT p99/p99.9 pe judge
>   hota hai, average pe nahi (folder 35, 36).
> - **Hot loops branchless likhe jaate hain:** market data decoders, order book
>   updates, risk checks — jahaan branch data-dependent ho.
> - **Data ko predictable banaya jaata hai:** sorting, bucketing, partitioning
>   taaki branches ek-taraf jhuk jaayein (jaise sorted array demo).
> - **`[[likely]]`/`[[unlikely]]` aur PGO** (profile-guided optimization) common
>   path ko straight-line rakhne ke liye (folder 33).
> - **`switch` jump tables** predictable indirect branches (folder 05).
>
> Poora mechanism (BTB, BHT, TAGE predictors, speculative execution, Spectre)
> folder 31. Istemaal folder 36.

---

## Hands-on

```bash
# -O2 ZAROORI -- -O0 pe yeh experiment jhootha hai
./build.ps1 fast 06-CONDITIONS/examples/04_branch_benchmark.cpp
# ya: make fast FILE=06-CONDITIONS/examples/04_branch_benchmark.cpp
```

Experiments:
1. `REPS` badhao/ghatao — ratio same rehna chahiye
2. Values `0..255` ki jagah `0..10` karo (`>= 5` ~50% par chhota range) — farq?
3. `dist(0, 255)` ki jagah sab `200` bhar do (branch hamesha taken) — teenon
   versions ka time?
4. `-O0` se compile karke chalao — ab sorted vs unsorted ka farq gaya? Kyun?

---

## ⚠️ Traps / Common mistakes

### Trap 1 — Branchless ko "hamesha tez" maan lena
Predictable branch (loop conditions, error checks) pe `if` **tez** hai — CPU free
mein guess kar leti hai, aur branchless dono side ka kaam karta hai.

### Trap 2 — `-O0` pe branch benchmark
Optimizer off → sab kuch slow aur branch-heavy → results meaningless.

### Trap 3 — Micro-optimizing cold code
Jo code second mein ek baar chalta hai, uska branch prediction se koi farq nahi.
Sirf **hot loops** (profiler ne bataya) matter karte hain.

### Trap 4 — `[[likely]]` guess ke aadhaar pe lagana
Galat hint compiler ko ulta guide karta hai. Measure ya PGO. Default: mat lagao.

### Trap 5 — Yeh sochna ki compiler tumhare `if` ko waisa hi rakhega
`-O2` pe compiler `if` ko `cmov`/branchless/vectorized bana sakta hai. `-S` se
verify (file 05).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`if` ka cost fixed hai" | Data-dependent — predictable ~0, mispredict ~15-20 cyc |
| "Sorted array process karna slow (extra sort)" | Loop khud ~7x tez ho gaya predictability se |
| "Branchless code hamesha better" | Sirf unpredictable branches pe; predictable pe slower |
| "Average latency hi matter karti hai" | HFT mein tail (p99/p99.9) — mispredict spikes |
| "`[[likely]]` free improvement hai" | Galat hint = regression |

---

## Exercises

1. **Predict the ratio:** benchmark chalane se pehle guess karo — sorted vs unsorted
   ka farq kitna? Phir chalao. Kitne door the?

2. **Range experiment:** `04_branch_benchmark.cpp` mein values `0..1` kar do aur
   condition `a[i] >= 1`. Ab unsorted bhi predictable (~50% par simple pattern)?
   Time note karo.

3. **Always-taken:** array ko sab `255` se bhar do. Teenon versions (unsorted
   branch, sorted branch, branchless) ka time — kya sab lagbhag same? Kyun?

4. **`-O0` vs `-O2`:** same file dono flags pe compile karke chalao. `-O0` pe
   sorted/unsorted farq report karo. Kya bacha? Explanation likho.

5. **Branchless likho:** yeh loop ko bina `if` ke —
   ```cpp
   int positives = 0;
   for (int x : data) if (x > 0) positives++;
   ```
   <details><summary>Answer</summary>
   `for (int x : data) positives += (x > 0);` — `bool` 0/1 add hota hai.
   </details>

6. **`[[unlikely]]`:** ek parse function likho jisme error-check ko `[[unlikely]]`
   mark karo. `-O2 -S` se assembly dekho — error handling code function ke end
   mein gaya (cold section)?

7. **Sochne wala:** ek trading system mein "agar order price band ke bahar hai to
   reject" check hai. 99.99% orders valid hote hain. Yeh branch predictable hai ya
   nahi? `if` theek hai ya branchless? `[[likely]]`?
   <details><summary>Answer</summary>
   Highly predictable (~always not-taken). `if` bilkul theek — predictor ~100%
   sahi. `[[likely]]` valid-path pe / `[[unlikely]]` reject-path pe lagana
   reasonable hai (straight-line hot path). Branchless yahan faayda nahi dega.
   </details>

---

## Interview questions

1. CPU pipeline kya hai? `if` usse kaise interfere karta hai?
2. Branch predictor kya karta hai? Misprediction ki penalty (order of magnitude)?
3. Sorted array pe loop unsorted se tez kyun ho sakta hai — same values ke saath?
4. Branchless code kab tez, kab slow? `cmov` ka trade-off?
5. `[[likely]]` / `[[unlikely]]` kya karte hain? Risk kya hai?
6. HFT mein tail latency (p99) average se zyada kyun matter karti hai — branches se connection?
7. Compiler tumhare `if` ko branchless bana sakta hai? Kaise check karoge?

---

## Next
→ [`09-exercises.md`](09-exercises.md)
