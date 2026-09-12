# 02 — Layer 1: C++ basics (types, variables, control flow, functions)

## Prerequisites
Folders `01`–`08`. Yeh questions "warm-up" round ya phone screen ka pehla
15 min hote hain — inhe **fluently** aana chahiye, socha nahi.

## Kaise use karein
Har sawaal ka jawab pehle **bina dekhe bolo** (out loud), phir `<details>`
kholo. Agar 3 se zyada Layer-1 sawaal atkein — folders `03`–`08` dobara.

---

## A — Types aur sizes

### A1. `int` kitne bytes ka hota hai?
<details><summary>Answer</summary>
Standard sirf **minimum** guarantee karta: `int` ≥ 16 bits, aur
`sizeof(int) ≤ sizeof(long) ≤ sizeof(long long)`. Practically har
mainstream 32/64-bit platform pe 4 bytes. Jab exact width chahiye:
`<cstdint>` ke `int32_t` / `int64_t`. Wire formats aur fixed-point prices
mein hamesha fixed-width (`03/05`, `43/09`).
</details>

### A2. `size_t` kya hai aur signed kyun nahi?
<details><summary>Answer</summary>
`size_t` = unsigned integer type jo kisi bhi object ke size ko hold kar
sake (`sizeof` isi ko return karta). Unsigned isliye ki size kabhi
negative nahi hota aur addressable range poora chahiye. Trap: `size_t`
ko signed se subtract/compare karna — `-1 < v.size()` **false** hota
(`-1` huge unsigned ban jaata). C++20 `std::ssize()` / `std::cmp_less`
se safe (`12-POINTERS`, `45/12` D2/D6).
</details>

### A3. `char` signed hai ya unsigned?
<details><summary>Answer</summary>
**Implementation-defined.** x86 Linux/Windows pe usually signed, ARM pe
aksar unsigned. Isliye byte data ke liye `unsigned char` ya
`std::byte` use karo, aur `char` ko sirf text ke liye. `char`, `signed
char`, `unsigned char` teen alag types hain.
</details>

### A4. `float` aur `double` mein fark, aur `0.1 + 0.2 == 0.3` kya deta?
<details><summary>Answer</summary>
`float` = 32-bit IEEE-754 (~7 decimal digits), `double` = 64-bit (~15–16).
`0.1 + 0.2 == 0.3` → **false**: 0.1, 0.2, 0.3 binary mein exactly
represent nahi hote, rounding se sum 0.30000000000000004 aata. Money /
prices ke liye float mat use karo — integer ticks / fixed-point (`43/09`).
</details>

### A5. `auto x = 5;` ka type kya? `auto& r = x;`? `const auto` deduction?
<details><summary>Answer</summary>
`auto x = 5;` → `int` (top-level const aur references drop hote hain plain
`auto` mein). `auto& r = x;` → `int&`. `auto` template-argument deduction
ke rules follow karta. `decltype(auto)` cv-qualifiers aur reference-ness
preserve karta. `const auto x = f();` → `f()` ke return se `const T`.
</details>

### A6. `0`, `'\0'`, `nullptr`, `NULL`, `false` — sab same hain?
<details><summary>Answer</summary>
Nahi. `0` = `int`. `'\0'` = `char` value 0. `false` = `bool`. `NULL` =
implementation-defined null-pointer macro (aksar `0` ya `0L`) — overload
resolution mein `int` se takra sakta. `nullptr` = `std::nullptr_t`, ek
genuine pointer-null jo integer se distinct hai. **Hamesha `nullptr`.**
</details>

---

## B — Declarations, initialization, lifetime

### B1. Declaration vs definition?
<details><summary>Answer</summary>
Declaration compiler ko naam + type batata ("yeh exist karta hai").
Definition storage/body deta ("yeh raha yeh"). `extern int x;` /
`int f();` declarations hain; `int x = 3;` / `int f() { ... }` definitions.
ODR: har entity ek definition (functions/variables), inline/template ko
chhoot par consistent (`24-COMPILATION-LINKING`).
</details>

### B2. `int x;` (uninitialized local) — value kya?
<details><summary>Answer</summary>
**Indeterminate.** Local (automatic) scalars default-initialize nahi hote
— padhna UB. Globals/`static` zero-initialize hote. Fix: hamesha
initialize karo (`int x = 0;` / `int x{};`). `-Wmaybe-uninitialized`,
MSan (`45/12` A8).
</details>

### B3. `int x{};` aur `int x = {};` aur `int x = 0;` — fark?
<details><summary>Answer</summary>
Teeno `x` ko 0 karte. `{}` = value-initialization. `{}` narrowing ko
error banata (`int x{3.5};` compile fail), `=` nahi. Class types ke liye
`{}` aggregate-init ya default-ctor trigger karta, aur "most vexing
parse" avoid karta (`T x{};` vs `T x();` jo function declaration ban
jaata).
</details>

### B4. Static local variable kab initialize hota?
<details><summary>Answer</summary>
Pehli baar jab control uske declaration se guzarta ("construct on first
use"), aur yeh thread-safe hai (C++11+, "magic statics"). Program end tak
zinda. Isse "static initialization order fiasco" (cross-TU globals) fix
hota — global ki jagah ek function-local `static` return karo (`25/02`,
`45/12` E5).
</details>

### B5. Scope aur lifetime alag cheezein hain — samjhao.
<details><summary>Answer</summary>
Scope = source region jahan naam **visible** hai (compile-time). Lifetime
= duration jab object ki **storage valid** hai (run-time). Ek local ka
scope block-end pe khatam, lifetime bhi. Par ek `static` local ka scope
block hai, lifetime poora program. Dangling pointer = lifetime khatam par
tum abhi bhi point kar rahe (`13-REFERENCES/09`).
</details>

---

## C — Control flow

### C1. `if (x = 5)` — bug kyun?
<details><summary>Answer</summary>
`=` assignment hai, `==` nahi. `x` ko 5 set karta, phir 5 truthy hai to
branch hamesha lega, aur `x` clobber ho gaya. `-Wall` warn karta
("suggest parentheses around assignment used as a truth value"). Fix:
`==`, ya `-Werror=parentheses` (`45/12` C2).
</details>

### C2. `switch` fallthrough kya hai?
<details><summary>Answer</summary>
`case` ke baad `break`/`return` na ho to control agle `case` mein "gir"
jaata. Kabhi jaan-boojh kar (shared handling) — tab `[[fallthrough]];`
mark karo. Warna bug. `-Wimplicit-fallthrough` (`-Wextra`) catch karta
(`06-CONDITIONS/04`).
</details>

### C3. `for (int i = 0; i < v.size(); ++i)` — is loop mein ek subtle
issue kya hai?
<details><summary>Answer</summary>
`i` `int` (signed) hai, `v.size()` `size_t` (unsigned) — `-Wsign-compare`
warn karta, aur bade vectors pe `i` overflow kar sakta (UB). Fix:
`size_t i`, ya range-for (`for (auto& e : v)`), ya index chahiye to
`std::ssize(v)` (C++20). `07-LOOPS`, `45/12` D6.
</details>

### C4. `while (true)` vs `for (;;)` — fark?
<details><summary>Answer</summary>
Semantically identical — dono infinite loop. `for(;;)` historically kuch
compilers pe `while(1)` ke "condition always true" warning se bachta tha;
aaj koi fark nahi. Style choice.
</details>

### C5. Ternary `a ? b : c` — `b` aur `c` ka type kya hona chahiye?
<details><summary>Answer</summary>
Common type mein convert hone chahiye (usual arithmetic conversions, ya
ek doosre mein convert). `cond ? 1 : 2.0` → `double` (dono). Mismatched
pointer/class types → compile error ya surprising conversions. Ternary ek
**expression** hai (value deta), `if` statement hai.
</details>

---

## D — Functions

### D1. Pass by value vs pass by reference vs pass by pointer — kab kaunsa?
<details><summary>Answer</summary>
**By value:** chhote cheap-to-copy types (`int`, `double`, small structs),
ya jab function ko apni copy chahiye. **By const reference:** bade
read-only objects (`const std::string&`, `const std::vector<T>&`) — copy
avoid. **By reference (non-const):** function ko caller ka object modify
karna hai (out-param — ya better, return karo). **By pointer:** optional
("null = absent"), ya C API, ya array + size. Modern default: value for
small, `const&` for large read, `&` for mutate. (`08-FUNCTIONS/03`.)
</details>

### D2. Function overloading kaise resolve hota?
<details><summary>Answer</summary>
Compile-time, argument types se: exact match > promotion (`char`→`int`) >
standard conversion > user-defined conversion > ellipsis. Ambiguous → hard
error. Return type overloading mein count **nahi** hota. `const`-ness of
member function counts (`f() const` vs `f()`). (`08-FUNCTIONS/08`.)
</details>

### D3. Default arguments — kahan likhe jaate, aur ek trap?
<details><summary>Answer</summary>
Declaration mein (usually header), ek hi baar. Trap: default argument
value **call site** pe substitute hoti, aur virtual functions ke liye
**static type** se aati — `Base* p = &derived; p->f();` jahan `f(int x=1)`
Base mein aur `f(int x=2)` Derived mein → `x` **1** milega (Base ka
default), body Derived ka. Confusing → virtual functions pe default args
avoid karo. (`08-FUNCTIONS/07`.)
</details>

### D4. Recursion vs iteration — HFT context mein kya prefer?
<details><summary>Answer</summary>
Iteration — predictable stack usage, no call overhead, no stack-overflow
risk on adversarial input, better for the optimizer. Recursion clean hota
tree/divide-conquer ke liye par hot path pe usually loop + explicit
`std::stack`. Tail recursion compilers optimize kar *sakte* (not
guaranteed in C++). (`08-FUNCTIONS/05`, `20-DSA`.)
</details>

### D5. `inline` keyword kya karta — sach mein?
<details><summary>Answer</summary>
**ODR ke liye** — ek function/variable ko multiple TUs mein define karne
ki permission (linker duplicates merge karta). "Inline this call" ek
**hint** tha jo aaj compilers largely ignore karte — woh apni heuristics
(size, call frequency, `-O` level) se decide karte. Forcing:
`[[gnu::always_inline]]`; preventing: `[[gnu::noinline]]`. (`33/03`.)
</details>

### D6. Function ka return value — copy hota hai har baar?
<details><summary>Answer</summary>
Nahi — **copy elision**. C++17 se `return T{...};` (prvalue) mein copy/move
**guaranteed elided** — object seedha caller ke slot mein banta. Named
local return (`return x;`) → NRVO (optional par universal). Isliye "return
by value" of a large object usually free hai. (`18-COPY-MOVE`, `25/05`.)
</details>

### D7. `void f(int arr[])` — `arr` ka type function ke andar kya hai?
<details><summary>Answer</summary>
`int*`. Array parameters **decay** to pointers — `int arr[]`, `int
arr[10]`, `int* arr` sab identical signatures. `sizeof(arr)` andar =
`sizeof(int*)` (8), array ka size nahi. Isliye size alag se pass karo, ya
`std::span<int>` / `std::array<int,N>&` / `std::vector<int>&` use karo.
(`09-ARRAYS`, `12-POINTERS`.)
</details>

---

## E — Small output-prediction

### E1. `int x = 5; std::cout << x++ << " " << x;` — output?
<details><summary>Answer</summary>
`5 5` ya `5 6` — **unspecified**. `<<` chain ke arguments ke evaluation
ka order sequenced nahi hai (pre-C++17 to UB tha overlapping; C++17 se
each `<<` operand indeterminately sequenced — still not guaranteed
left-to-right for the *values*). Point: ek statement mein same variable
ko modify + read mat karo. (`05-OPERATORS/10`.)
</details>

### E2. `for (int i = 0; i < 5; i++); { std::cout << i; }` — kya print
hota (agar `i` scope mein ho)?
<details><summary>Answer</summary>
Loop body ek **empty statement** hai (`;` ke baad). `{ ... }` block loop
ke **bahar** hai, ek baar chalega. `-Wempty-body` warn karta. Classic
stray-semicolon bug. (`06-CONDITIONS`, `45/12` C.)
</details>

### E3. `char c = 200; std::cout << (c > 0);` — kya print, aur kyun
platform-dependent?
<details><summary>Answer</summary>
Agar `char` signed (x86) → 200 `char` mein fit nahi, `-56` ban jaata,
`c > 0` → `0`. Agar `char` unsigned (some ARM) → 200, `c > 0` → `1`.
`char` ki signedness implementation-defined (`A3`). Byte values ke liye
`unsigned char`.
</details>

### E4. `#define SQ(x) x*x` then `SQ(2+3)` — result?
<details><summary>Answer</summary>
`2+3*2+3` = `11`, not 25. Macro text substitution hai, precedence ka koi
gyaan nahi. Fix: `#define SQ(x) ((x)*(x))`, ya better — ek `constexpr`
function `constexpr auto sq(auto x){ return x*x; }` (type-safe,
scoped, debuggable). (`22-MODERN-CPP`, `45/12` C3.)
</details>

### E5. `int a = 1; int b = a++ + ++a;` — `b` kya?
<details><summary>Answer</summary>
Pre-C++17: **UB** (`a` do baar modified without sequencing). C++17 se
still **unspecified** value — don't write this. Interviewer yeh isliye
poochta ki tum "don't do this" bolo, koi number nahi. (`05-OPERATORS/10`.)
</details>

---

## Interview tips for Layer 1

- Yeh round speed + confidence ka hai. Long pauses = weak signal.
- "Implementation-defined" / "unspecified" / "UB" ke fark ko crisp rakho
  (`23-ERROR-HANDLING/13`): UB = anything can happen; unspecified =
  compiler picks, need not document; impl-defined = compiler picks, must
  document.
- Jab "kitne bytes / kya value" pooche — pehle guarantee batao, phir "in
  practice on x86-64 it's...".
- `nullptr` over `NULL`, `{}`-init, `<cstdint>` fixed widths, `constexpr`
  over macros — yeh "modern C++ hygiene" signals hain.

## Next
→ [`03-pointers-memory-questions.md`](03-pointers-memory-questions.md)
