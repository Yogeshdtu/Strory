# 10 — `inline` functions — asli matlab

## Prerequisites
- [`02-declaration-vs-definition.md`](02-declaration-vs-definition.md) (ODR)
- [`05-the-call-stack.md`](05-the-call-stack.md) (call overhead, prologue/epilogue)
- `07-LOOPS/09-loop-performance.md` ("measure karo, `-S` dekho")

## Yeh topic abhi kyun
`inline` keyword ka naam dhoka deta hai. Zyada tar log samajhte hain "iska matlab
'is function ko fast banao' / 'call ko hata do'." **Galat.** `inline` ka **asli**
matlab ODR se juda hai. Aur actual inlining — call ko hataana — compiler khud
`-O1`+ pe decide karta hai, keyword se nahi.

Yeh header/source design (folder 24), templates (folder 21), aur `constexpr`
(lesson 11) ka foundation hai.

---

## `inline` ka ASLI matlab — ODR exception

Recall (lesson 02): **non-`inline` function ki definition poore program mein
exactly ek** honi chahiye. Agar 2 `.cpp` ek header include karein jismein
function **defined** hai → 2 definitions → linker error.

`inline` yeh rule badalta hai:

> `inline` function ki definition **har translation unit mein** ho sakti hai
> (jitni baar chahe), bas **sab identical** honi chahiye. Linker unhe **ek** maan
> ke merge kar deta hai.

```cpp
// geometry.hpp  -- kai .cpp include karte hain
inline double areaCircle(double r) {      // ✅ inline -> har TU mein OK
    return 3.14159265 * r * r;
}
```

Bina `inline`:
```cpp
// geometry.hpp
double areaCircle(double r) { return 3.14159265 * r * r; }   // ⚠️ 2 .cpp include -> ODR violation -> linker error
```

**Isi liye header-only libraries `inline` (aur templates, jo implicitly inline
hain) use karti hain.** `inline` = "yeh definition header mein reh sakti hai."

### `inline` variables (C++17)

```cpp
// config.hpp
inline constexpr int MaxOrders = 10000;    // ✅ header mein, ek hi object poore program mein
```

---

## Actual INLINING — compiler ka kaam

"Call ko hataana, function ka body caller mein paste karna" — yeh **inline
expansion** hai, aur yeh compiler **`-O1`/`-O2`** pe **khud** karta hai, heuristics
se:

- Function chhota hai? (body ka size)
- Sirf ek/do jagah se call hota hai?
- Hot hai? (loop ke andar)
- Recursive nahi? (ya bounded)

```cpp
static int add(int a, int b) { return a + b; }   // koi `inline` keyword NAHI

for (int i = 0; i < N; ++i) s += add(i, 1);       // -O2 pe: `add` inline, koi `call`
```

### Measured (`examples/06_inline_asm_check.cpp`, GCC 15.1, `-O2`)

```
  addInline   (compiler inlines)  :   75 ms       (200M calls)
  addNoInline (forced real call)  :  448 ms
                                     -------
                                     ~6x   |   per-call overhead ~1.9 ns
```

`addNoInline` ne `__attribute__((noinline))` se compiler ko roka. `addInline`
poori tarah inline ho gaya → koi `call`, aur caller ke saath optimize
(constant-folding).

### `-O2 -S` se verify

```asm
; addInline call-site: koi `call` nahi -- body yahin
; addNoInline call-site:
        call    addNoInline(int, int)
```

`./build.ps1 asm 08-FUNCTIONS/examples/06_inline_asm_check.cpp`

---

## `inline` keyword aur inlining — RISHTA

| | `inline` keyword | Actual inlining (call hataana) |
|---|---|---|
| Kya | ODR exception (header mein define kar sakte ho) | compiler optimization |
| Kaun decide | aap (keyword likhkar) | compiler (`-O1`+ heuristics) |
| Zaroori? | header-defined functions ke liye haan | kabhi nahi — optimization hai |

- `inline` keyword **hint** deta hai "shayad inline karna chahoge" — modern
  compilers ise inlining ke liye **lgbhg ignore** karte hain (apni heuristics
  behtar hain)
- Compiler bina `inline` ke bhi inline karta hai (`static` functions, ek hi TU
  ke functions)
- Compiler `inline`-marked function ko bhi **NA** inline kar sakta hai (bada,
  recursive, address liya gaya)

**`inline` likhne ka practical reason: definition header mein chahiye.** "Fast
banane" ke liye nahi.

---

## Force karna — attributes

```cpp
[[gnu::always_inline]] inline int hot(int x) { return x * 3 + 1; }   // "hamesha inline"
[[gnu::noinline]]      int cold(int x) { ... }                        // "kabhi inline mat karo"
```

- `always_inline` — profiler ne bola, aur aap sure ho. Overuse → code bloat.
- `noinline` — debugging, benchmarking (call overhead measure karna), ya
  intentional cold path (error handlers — I-cache se door rakho).

Standard `[[likely]]`/`[[unlikely]]` (folder 06 file 08, lesson 12) hot/cold
paths ko hint karte hain — inlining aur code layout dono ko affect karte hain.

---

## Trade-off — inlining hamesha achhi nahi

```cpp
inline void bigFunction() { /* 200 lines */ }

for (...) bigFunction();          // agar inline hua: 200 lines * har call-site -> BINARY BLOAT
```

- **Code bloat** → bada binary → **I-cache pressure** → hot loop ka doosra code
  evict → **slower**
- Compile time badhta hai
- Debugging mushkil (stack traces mein function gayab)

Isi liye compiler chhoti functions inline karta hai, badi nahi. `-O3` /
`-finline-functions` aggressive hai; `-Os` (size optimize) kam.

> **HFT relevance:** Hot-path functions inline honi chahiye — ~2 ns/call bachta
> hai, aur cross-function optimization (constant fold, CSE, register allocation)
> khulti hai. Isli ye critical helpers **header mein** (`inline` / `constexpr` /
> templates) rakhe jaate hain, `.cpp` mein nahi (cross-TU calls jo LTO ke bina
> inline nahi hote). **LTO** (`-flto`, folder 33) cross-TU inlining enable karta
> hai. Cold paths (`log_error`, `handle_reject`) ko `[[gnu::noinline]]` /
> `[[unlikely]]` se hot code se **alag** rakha jaata hai — I-cache saaf rehta hai.
> Rule: hot = header/inline, cold = out-of-line.

---

## Hands-on

```bash
./build.ps1 fast 08-FUNCTIONS/examples/06_inline_asm_check.cpp   # benchmark
./build.ps1 asm  08-FUNCTIONS/examples/06_inline_asm_check.cpp   # assembly -- call hai ya nahi

# ODR demo:
# util.hpp mein `int magic() { return 42; }` (bina inline) -> 2 .cpp include -> link error
# `inline` lagao -> theek
```

---

## ⚠️ Traps

### Trap 1 — `inline` = "make it fast" samajhna
`inline` ODR ke liye hai. Speed compiler + `-O2` deta hai.

### Trap 2 — header mein non-`inline` function define karna
```cpp
// util.hpp
int helper() { return 1; }         // ⚠️ multiple TUs -> ODR violation -> linker error
inline int helper() { return 1; }  // ✅
```

### Trap 3 — `inline` functions ki definitions alag (non-identical)
```cpp
// a.cpp: inline int f() { return 1; }
// b.cpp: inline int f() { return 2; }   // ⚠️ ODR violation -- UB, linker aksar chup rehta hai
```
`inline` definition **har TU mein byte-for-byte same** honi chahiye → isi liye
header mein rakho (ek source of truth).

### Trap 4 — recursive function ko `always_inline`
```cpp
[[gnu::always_inline]] int fib(int n) { return n<2?n:fib(n-1)+fib(n-2); }  // ⚠️ error / ignored
```

### Trap 5 — `-O0` pe "kuch inline nahi hua" se ghabrana
`-O0` = koi inlining (debugging). Production `-O2`. Benchmarks `-O2`+.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`inline` function ko fast banata hai" | ODR exception; speed = compiler + `-O2` |
| "`inline` ke bina compiler inline nahi karta" | Karta hai — `static`/same-TU/small functions |
| "`inline` likha to zaroor inline hoga" | Hint only — compiler ignore/decline kar sakta hai |
| "Inlining hamesha faster" | Bloat → I-cache pressure → slower for big functions |
| "Cross-TU call inline ho jaata hai" | Nahi (bina LTO) — isi liye hot helpers header mein |

---

## Exercises

1. **ODR:** `math.hpp` mein `int dbl(int x) { return x*2; }` (bina `inline`). 2
   `.cpp` files include karein + `dbl` use karein. Link karo — error? `inline`
   lagao — theek?

2. **Benchmark:** `examples/06_inline_asm_check.cpp` chalao. `addInline` vs
   `addNoInline` ratio + per-call ns apni machine pe.

3. **Assembly:** `./build.ps1 asm 08-FUNCTIONS/examples/06_inline_asm_check.cpp` —
   `addInline` ke call-site pe `call` hai? `addNoInline` ke?

4. **`always_inline` bloat:** ek 30-line function `[[gnu::always_inline]]` mark
   karo, use 10 jagah se call karo. `nm --print-size` / binary size before-after.

5. **`noinline` for measurement:** ek chhoti function bina `noinline` aur
   `[[gnu::noinline]]` ke saath — 100M calls, `-O2` time. Overhead nikalo.

6. **LTO:** 2 `.cpp` — ek mein hot function, doosre mein loop jo use kare.
   `-O2` (no LTO) vs `-O2 -flto` pe assembly/time — cross-TU inline hua LTO se?

7. **Header-only:** ek chhoti `inline`-based header library banao (`clamp`,
   `lerp`, `sign`) aur 2 `.cpp` se use karo.

---

## Interview questions

1. `inline` keyword ka asli matlab kya hai (ODR ke context mein)?
2. `inline` keyword aur actual inlining — kaun kya decide karta hai?
3. `inline` ke bina compiler inline kar sakta hai? `inline` ke saath NA kar sakta hai?
4. Header mein non-`inline` function define karne pe kya hota hai?
5. Inlining kab bura hai (code bloat / I-cache)?
6. Cross-TU function call inline hota hai? LTO ka role?
7. `[[gnu::always_inline]]` / `[[gnu::noinline]]` kab use karo?

---

## Next
→ [`11-constexpr-functions.md`](11-constexpr-functions.md)
