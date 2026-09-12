# 01 — `if` statement

## Prerequisites
- `05-OPERATORS/03-comparison-operators.md` (`==`, `<`, `>` … sab `bool` dete hain)
- `05-OPERATORS/04-logical-operators.md` (`&&`, `||`, `!`, short-circuit)
- `03-VARIABLES-DATA-TYPES/08-bool.md` (`bool`, `true`/`false`)

## Yeh topic abhi kyun
Ab tak aapka program **seedha upar se neeche** chalta tha — har line, har baar.

`if` pehla tool hai jisse program **faisla** leta hai: "yeh code chalao **sirf agar**
yeh sach hai." Yeh programming ke 5 building blocks mein se teesra hai (folder 01
lesson 02 se yaad hai: sequence → **decision** → repetition → ...).

Loops (folder 07), functions ka early-return (folder 08), error handling (23) — sab
`if` pe khade hain. Isliye ise theek se samajhna zaroori hai.

---

## `if` ka anatomy

```cpp
if (condition) {
    // body -- chalta hai SIRF jab condition true ho
}
```

Token by token:

| Part | Kya hai | Rule |
|---|---|---|
| `if` | keyword | lowercase, hamesha |
| `(` … `)` | **zaroori** brackets | condition inke andar hi |
| `condition` | koi bhi expression jo `bool` ban sake | `true` → body chalega |
| `{` … `}` | body (compound statement) | is course mein **hamesha** lagao |

```cpp
int temperature = 45;

if (temperature > 40) {
    std::cout << "Bahut garmi\n";
}
```

`temperature > 40` ek expression hai. Iska result `true` (`1`) ya `false` (`0`).
`true` hone pe hi `std::cout` wali line chalti hai.

### `if` ek STATEMENT hai, EXPRESSION nahi

```cpp
int x = if (a > b) a else b;   // ❌ COMPILE ERROR -- if value nahi deta

int x = (a > b) ? a : b;       // ✅ ternary -- yeh expression hai
```

`if` "kaam karo" bolta hai, "value do" nahi. Jab value chahiye — ternary `?:`
(folder 05 file 08) ya `if`-`else` se pehle variable declare karke.

---

## Condition kya ho sakti hai

Kuch bhi jo `bool` mein **convert** ho jaaye:

```cpp
if (x > 5)          { }    // comparison -> bool
if (a == b)         { }    // equality   -> bool
if (isReady)        { }    // bool variable
if (a > 0 && b > 0) { }    // logical combine
if (v.empty())      { }    // function jo bool deta hai
if (flags & MASK)   { }    // int -> bool (non-zero = true)
```

### Truthiness — non-`bool` values

Agar condition `bool` nahi hai, to yeh rule lagta hai:

```
   0        -> false
   0.0      -> false
   nullptr  -> false
   baaki sab (koi bhi non-zero) -> true
```

```cpp
if (5)     { }   // true
if (-1)    { }   // true  <- non-zero, sign se matlab NAHI
if (0)     { }   // false
if (0.1)   { }   // true
if (ptr)   { }   // true agar ptr != nullptr
```

### ✅ Salah: intent explicit likho

```cpp
// ⚠️ Chalta hai, par padhne wale ko sochna padta hai "kya check ho raha hai?"
if (count)   { }
if (bytesRead) { }

// ✅ Self-documenting
if (count != 0)      { }
if (bytesRead > 0)   { }
```

**Exception** — `bool` variables aur `bool`-returning functions ko seedha likho:

```cpp
if (isReady)      { }    // ✅ best
if (!v.empty())   { }    // ✅
if (isReady == true) { } // ⚠️ redundant, aur `=` typo ka risk (file 07)
```

---

## Body: block vs single statement

Braces optional hain — bina braces ke `if` **sirf agli EK statement** control karta hai:

```cpp
if (loggedIn)
    std::cout << "welcome\n";      // yeh if ke andar
std::cout << "next line\n";        // yeh if ke BAHAR -- hamesha chalega
```

Yeh silent bug ka source hai (file 07 mein poora). Isliye is course ka rule:

> **HAMESHA braces lagao — chahe body ek hi line ho.**

```cpp
if (loggedIn) {
    std::cout << "welcome\n";
}
```

Fayde:
1. Baad mein doosri line add karo to bug nahi banta
2. `git diff` saaf rehta hai
3. "Dangling else" ambiguity khatam (file 02)

---

## Andar kya hota hai — `if` = ek BRANCH

CPU ke liye `if` ka matlab: **conditional jump**.

```cpp
if (x > 40) {
    doSomething();
}
```

Compiler isse aisa kuch banata hai (Intel syntax, simplified):

```asm
        cmp     dword ptr [x], 40      ; x ko 40 se compare karo
        jle     .skip                  ; agar x <= 40 -> body skip karo (jump)
        call    doSomething
.skip:
        ...                            ; aage ka code
```

- `cmp` → flags set karta hai
- `jle` (jump if less-or-equal) → **conditional branch**

### Yeh "muft" nahi hai

Modern CPU pipelined hai — woh agli instructions **pehle se** fetch/decode kar leti
hai. Par `jle` pe usse pata nahi jump hoga ya nahi. To woh **guess** karti hai
(branch predictor).

- Guess **sahi** → koi rukavat nahi (~0 extra cost)
- Guess **galat** → pipeline flush → **~15–20 cycles** barbaad

Ek `int age = 20;` jitna kaam ~1 cycle hota hai. To ek misprediction = ~15–20
statements ki cost.

> **HFT relevance:** Hot path (har market-data message pe chalne wala code) mein
> **data-dependent** branches — jinka result har baar badalta hai — latency ki
> **tail (p99, p99.9)** ko phaila dete hain. Average theek dikhta hai, par kabhi-
> kabhi ka misprediction spike deadline miss kara deta hai.
>
> Isi wajah se low-latency code mein: predictable branches rakhna, ya branch ko
> hi hata dena (branchless / `cmov` / table lookup), ya data ko sort/bucket karke
> branch predictable banana. Poora demo **file 08** mein — sorted vs unsorted
> array pe measured **~7x** farq. Deep dive folder 31 aur 36 mein.

Abhi ke liye bas itna: **`if` ek real cost wali cheez hai, aur woh cost data pe
depend karti hai.**

---

## Scope — `if` block ke andar declare kiya variable

```cpp
if (int n = getCount(); n > 0) {   // C++17 -- "if with initializer" (file 06)
    use(n);                        // n yahan visible
}
// n yahan visible NAHI
```

Purana style:

```cpp
{
    int n = getCount();
    if (n > 0) {
        use(n);
    }
}   // n ka scope yahan khatam
```

Chhota scope = kam bugs, kam naam ki takraar. Detail file 06 mein.

---

## Hands-on

`examples/01_if_else.cpp` chalao:

```bash
# repo root se:
./build.ps1 06-CONDITIONS/examples/01_if_else.cpp          # Windows (PowerShell)
make FILE=06-CONDITIONS/examples/01_if_else.cpp            # Linux/Mac/Git-Bash

# ya seedha:
g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_if_else.cpp -o ifelse && ./ifelse
```

Woh dikhata hai: basic `if`, `if/else`, `else-if` chain, truthiness, braces ka
trap, guard clause, aur ternary.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `if` ko expression samajhna
```cpp
int max = if (a > b) a else b;   // ❌
int max = (a > b) ? a : b;       // ✅
```

### Trap 2 — Braces chhodna aur galti se do statements samajhna
```cpp
if (x > 0)
    a();
    b();      // ⚠️ hamesha chalega -- if ke andar nahi hai
```

### Trap 3 — `=` vs `==`
```cpp
if (status = 0) { }    // ⚠️ assignment! status ab 0, condition false
if (status == 0) { }   // ✅
```
`-Wall` warning deta hai (`-Wparentheses`). File 07 mein poora.

### Trap 4 — Non-bool condition ko "sign wala check" samajhna
```cpp
int diff = -3;
if (diff) { }          // TRUE (-3 non-zero hai). "diff positive hai" NAHI.
if (diff > 0) { }      // ✅ agar positive check karna tha
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`if` value return karta hai" | `if` statement hai; value chahiye to `?:` |
| "Bina braces `if` agle block ko chalata hai" | Sirf agli **ek statement** ko |
| "`if (x)` matlab x positive hai" | `x != 0` — negative bhi true |
| "`if` free hai, bas ek check" | Ek branch — misprediction ~15–20 cycles |
| "`if (a = b)` compile error" | Sirf warning — silently galat chalega |

---

## Exercises

1. **Output predict karo:**
   ```cpp
   int x = -5;
   if (x) std::cout << "A ";
   if (x > 0) std::cout << "B ";
   if (!x) std::cout << "C ";
   if (x < 0) std::cout << "D ";
   ```
   <details><summary>Answer</summary>
   `A D` — `x` non-zero to `if (x)` true (A). `x > 0` false (no B). `!x` false (no C).
   `x < 0` true (D).
   </details>

2. **Braces ka bug:** yeh code likho aur chalao —
   ```cpp
   bool premium = false;
   if (premium)
       std::cout << "10% discount\n";
       std::cout << "free shipping\n";
   ```
   `premium` `false` hai. Output kya aaya? Kyun? Fix karo.
   <details><summary>Answer</summary>
   `free shipping` chhap gaya — woh line `if` ke bahar hai. Har non-premium user
   ko free shipping mil gaya. Fix: dono lines ko `{ }` mein daalo.
   </details>

3. **Explicit vs truthiness:** in dono ka behaviour compare karo —
   ```cpp
   int errorCode = -1;   // -1 = "unknown error"
   if (errorCode) std::cout << "error hua\n";
   if (errorCode != 0) std::cout << "error hua (explicit)\n";
   ```
   Dono same output dete hain. Kaunsa padhne mein saaf hai? Ab `errorCode` ko `0`
   karke dekho.

4. **`if` ki cost:** `examples/04_branch_benchmark.cpp` chalao (`-O2` ke saath).
   Sorted aur unsorted array pe timing note karo. Abhi sirf number dekho — samajh
   file 08 mein aayegi.

5. **Guard clause likho:** yeh nested function ko flatten karo —
   ```cpp
   std::string_view access(bool loggedIn, bool isAdmin, bool hasToken) {
       if (loggedIn) {
           if (hasToken) {
               if (isAdmin) return "full";
               else return "user";
           } else return "no-token";
       } else return "guest";
   }
   ```
   <details><summary>Answer</summary>

   ```cpp
   std::string_view access(bool loggedIn, bool isAdmin, bool hasToken) {
       if (!loggedIn) return "guest";
       if (!hasToken) return "no-token";
       if (isAdmin)   return "full";
       return "user";
   }
   ```
   </details>

6. **Truthiness table:** in sab ke liye `true`/`false` likho (pehle socho, phir
   code se verify): `if (1)`, `if (0)`, `if (-0.0)`, `if (0.0001)`, `if ('0')`,
   `if ("")`, `if (nullptr)`.
   <details><summary>Answer</summary>
   `true, false, false, true, true` (`'0'` ka code 48), `true` (string literal ek
   non-null pointer hai! ⚠️), `false`.
   </details>

---

## Interview questions

1. `if` statement hai ya expression? `if-else` aur `?:` mein fark?
2. `if (x)` kab `true` hota hai jab `x` `int` ho? `x = -1` pe?
3. Bina braces `if` kitni statements control karta hai?
4. CPU level pe `if` kya banta hai? "Cost" kya hai?
5. Branch misprediction ki penalty kitni hoti hai (order of magnitude)?
6. `if (v.size())` vs `if (!v.empty())` — kya farq, kaunsa better?

---

## Next
→ [`02-else-and-else-if.md`](02-else-and-else-if.md)
