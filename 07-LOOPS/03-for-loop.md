# 03 — `for` loop

## Prerequisites
- [`01-while-loop.md`](01-while-loop.md)
- `03-VARIABLES-DATA-TYPES/02-what-is-a-variable.md` (scope)
- `05-OPERATORS/03-comparison-operators.md` (signed/unsigned compare)

## Yeh topic abhi kyun
`for` sabse zyada use hone wala loop hai. Iski khaasiyat: **init, condition, aur
update — teeno ek line pe, ek jagah.** Isse "update bhoolna" (file 01 ka common
`while` bug) lagbhag khatam ho jaata hai, aur loop variable ka scope loop tak
simit rehta hai.

---

## Anatomy

```cpp
for (init-statement; condition; iteration-expression) {
    // body
}
```

```cpp
for (int i = 0; i < 5; ++i) {
    std::cout << i << " ";
}
// Output: 0 1 2 3 4
```

| Part | `for (int i = 0; i < 5; ++i)` | Kab chalta hai |
|---|---|---|
| **init** | `int i = 0` | **ek baar**, sabse pehle |
| **condition** | `i < 5` | **har iteration se pehle**; false → loop khatam |
| **iteration** | `++i` | **har iteration ke body ke baad** |

### Exact order

```
1. init:        int i = 0
2. condition:   i < 5 ?  -> false to KHATAM
3. body:        { ... }
4. iteration:   ++i
5. wapas step 2
```

```cpp
for (int i = 0; i < 3; ++i) std::cout << i;
```
```
i=0  cond(0<3)✓  body: print 0   ++i -> 1
i=1  cond(1<3)✓  body: print 1   ++i -> 2
i=2  cond(2<3)✓  body: print 2   ++i -> 3
i=3  cond(3<3)✗  KHATAM
Output: 012
```

---

## `for` ⟺ `while`

```cpp
for (INIT; COND; ITER) { BODY }
```

exactly barabar hai (scope chhod ke):

```cpp
{
    INIT;
    while (COND) {
        BODY;
        ITER;
    }
}
```

**Ek fark — `continue` ke saath:** `for` mein `continue` ke baad bhi `ITER`
chalta hai; `while` mein `continue` seedha `COND` pe jaata hai (`ITER` skip). Isi
liye "increment `continue` se skip ho gaya" wala infinite-loop bug `for` mein
nahi hota (file 06, aur `examples/04_loop_bugs.cpp` BUG 7).

---

## Loop variable ka SCOPE

```cpp
for (int i = 0; i < 5; ++i) {
    // i yahan visible
}
// i yahan visible NAHI -- compile error agar use karo
```

Yeh **feature** hai:
- `i` galti se baad mein use nahi hoga
- Do loops ek hi function mein — dono `int i` — koi clash nahi
- `-Wshadow` clean rehta hai

Agar loop ke baad final value chahiye — `for` se pehle declare karo:
```cpp
int i = 0;
for (; i < n && !found; ++i) { ... }
// i ab yahan available -- kitne tak pahunche
```

---

## Saare 3 parts optional hain

```cpp
for (;;) { }                 // infinite (== while(true))
for (; i < n; ) { ++i; }     // init aur iter khaali -> while jaisa
for (int i = 0; ; ++i) { if (done(i)) break; }   // condition khaali
```

### Multiple variables — comma operator

```cpp
for (int i = 0, j = 9; i < j; ++i, --j) {
    // i upar se, j neeche se -- do-pointer pattern (file 08)
}
```

`int i = 0, j = 9` — dono `int`. `++i, --j` — comma operator: dono chalte hain.
(Alag types chahiye to structured bindings / alag declaration.)

---

## ⚠️ Traps

### Trap 1 — off-by-one (`<` vs `<=`)
```cpp
int a[5];
for (int i = 0; i <= 5; ++i) a[i] = 0;   // ⚠️ i = 5 pe a[5] -> OUT OF BOUNDS
for (int i = 0; i < 5;  ++i) a[i] = 0;   // ✅
```
**Half-open range `[0, n)` socho:** `i < n`, `<=` nahi. `n` iterations, indices
`0 .. n-1`.

### Trap 2 — signed/unsigned compare
```cpp
std::vector<int> v = {1,2,3};
for (int i = 0; i < v.size(); ++i) { }        // ⚠️ -Wsign-compare (size() unsigned)
for (std::size_t i = 0; i < v.size(); ++i) { }// ✅
for (const auto& x : v) { }                    // ✅ best (file 04)
```

### Trap 3 — reverse loop unsigned underflow
```cpp
for (std::size_t i = v.size() - 1; i >= 0; --i) { }   // ⚠️ i >= 0 hamesha true -> infinite
for (std::size_t i = v.size(); i-- > 0; ) { }         // ✅ "i-- > 0" idiom
```
`-Wtype-limits` warn karta hai. (`examples/04_loop_bugs.cpp` BUG 3.)

### Trap 4 — stray semicolon
```cpp
for (int i = 0; i < n; ++i);      // ⚠️ khali body
    sum += a[i];                   // ek baar chalta hai, i == n ke saath -> OOB
```
`-Wempty-body`, `-Wmisleading-indentation`.

### Trap 5 — body mein loop variable modify
```cpp
for (int i = 0; i < n; ++i) {
    process(a[i]);
    ++i;              // ⚠️ ab step 2 ka -> aadhe elements skip
}
```
Step chahiye to iteration-expression mein: `i += 2`.

### Trap 6 — float loop counter
```cpp
for (double x = 0.0; x != 1.0; x += 0.1) { }   // ⚠️ x kabhi thik 1.0 nahi -> infinite
for (int k = 0; k < 10; ++k) { double x = k * 0.1; }   // ✅
```

### Trap 7 — condition / bound har iteration recompute
```cpp
for (int i = 0; i < expensive(); ++i) { }       // ⚠️ expensive() har baar
const int n = expensive();
for (int i = 0; i < n; ++i) { }                  // ✅
```
(`v.size()` sasta hai — usse mat daro. Par arbitrary function call se bacho.)

---

## Andar kya hota hai

```cpp
for (int i = 0; i < n; ++i) body();
```

```asm
        mov     dword ptr [i], 0     ; init
        jmp     .check
.loop:
        call    body
        inc     dword ptr [i]        ; ++i
.check:
        cmp     dword ptr [i], [n]
        jl      .loop                ; i < n -> loop
```

- Loop branch (`jl`) **highly predictable** → predictor ~100% → control cost ~0
  (folder 06 file 08)
- `-O2` pe: dead loop hataya jaata hai, counters registers mein aate hain, loop
  unroll/vectorize hota hai agar body simple ho (file 09)
- Counting-down loops (`for (i = n; i-- > 0;)`) kabhi ek instruction bacha dete
  hain (`i` ka `0` se compare free hota hai — `dec` sets zero flag) — par yeh
  micro-optimization hai, readability pehle

> **HFT relevance:** Hot loops mein `for (std::size_t i = 0; i < n; ++i)` over a
> contiguous array (`std::vector`, `std::array`) preferred hai — compiler ise
> vectorize karta hai aur access pattern cache-friendly hota hai (file 09,
> folder 32). `std::list` / pointer-chasing wale loops HFT hot path mein avoid
> kiye jaate hain — har node ek cache miss.

---

## Hands-on

`examples/01_loop_types.cpp` (for vs while equivalence), aur `examples/03_nested_patterns.cpp`
(nested `for`):

```bash
./build.ps1 07-LOOPS/examples/01_loop_types.cpp
./build.ps1 07-LOOPS/examples/03_nested_patterns.cpp
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`i <= n` se `n` iterations" | Woh `n+1` iterations, aur `a[n]` OOB. `i < n` |
| "Loop variable loop ke baad milta hai" | Nahi — scope loop tak. Chahiye to pehle declare |
| "`for` `while` se tez" | Same assembly; `for` bas structure better |
| "`for (;;)` infinite hai, isliye galat" | Valid — `break`/`return` ke saath |
| "Iteration-expression body se pehle chalta hai" | Body ke **baad** (condition se pehle) |

---

## Exercises

1. **Trace karo:**
   ```cpp
   for (int i = 2; i <= 8; i += 3) std::cout << i << " ";
   ```
   <details><summary>Answer</summary>`2 5 8 ` — `11 <= 8` false.</details>

2. **Off-by-one:** yeh array `arr[5]` ko `0..4` set karna chahiye —
   ```cpp
   for (int i = 1; i <= 5; ++i) arr[i] = i;
   ```
   2 bugs batao aur fix karo.
   <details><summary>Answer</summary>`i=1` se shuru (index 0 miss), `i<=5` (index 5 OOB). Fix: `for (int i = 0; i < 5; ++i)`.</details>

3. **Reverse:** `10 9 8 ... 1` print karo `for` se. Phir `std::size_t` index se
   ek `vector` ko ulta print karo (`i-- > 0` idiom).

4. **Two variables:** `for` mein `i` (0 se up) aur `j` (9 se down) — jab tak
   `i < j`. Har iteration `i` aur `j` print karo.

5. **`for` ⟺ `while`:** is `for` ko `while` mein badlo, `continue` ke behaviour
   pe dhyaan do —
   ```cpp
   for (int i = 0; i < 10; ++i) { if (i % 2) continue; std::cout << i << " "; }
   ```

6. **FizzBuzz:** `for (int i = 1; i <= 30; ++i)` — 3→Fizz, 5→Buzz, dono→FizzBuzz.

7. **Nested:** `for` ke andar `for` — `n x n` grid mein `(i, j)` print karo jahan
   `i == j` (diagonal).

8. **Bound recompute:** ek loop likho jisme condition mein `v.size()` hai, aur ek
   jisme koi `slowCount()` hai. Dono `-O2 -S` se dekho — `v.size()` hoist hua?
   `slowCount()` hua?

---

## Interview questions

1. `for` ke teen parts kab-kab evaluate hote hain (order)?
2. `for` aur `while` mein `continue` ka behaviour kaise alag?
3. Loop variable ka scope kya, aur yeh faayda kyun?
4. `for (size_t i = n-1; i >= 0; --i)` — bug aur 2 fixes?
5. Half-open range `[0, n)` convention ke 2 faayde?
6. `for (;;)` valid hai? Kaise nikalte ho?
7. Loop condition mein function call — kab problem, kab nahi?

---

## Next
→ [`04-range-based-for.md`](04-range-based-for.md)
