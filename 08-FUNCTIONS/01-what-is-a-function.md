# 01 — Function kya hai

## Prerequisites
- `07-LOOPS/` (poora)
- `06-CONDITIONS/03-nested-conditions.md` (guard clauses)
- `02-CPP-FIRST-STEPS/06-braces-blocks-and-scope.md` (blocks, scope)

## Yeh folder kyun
Ab tak aapke program ek lambi list of statements the — `main()` ke andar sab kuch.
Functions **paanchva building block** hain: **reuse**. Ek naam ke peeche code ka
ek tukda, jise aap baar-baar bula sakte ho, alag-alag inputs ke saath.

Aur is folder mein **call stack** deeply samjhenge (lesson 05) — jo pointers
(folder 12), recursion, aur debugging ka foundation hai.

---

## Function — ek naam wala code block

```cpp
int add(int a, int b) {     // signature
    return a + b;           // body
}
```

| Part | `int add(int a, int b)` |
|---|---|
| **return type** | `int` — call ke baad kya value milegi |
| **naam** | `add` — isse call karte hain |
| **parameters** | `(int a, int b)` — input, function ke andar ke local variables |
| **body** | `{ ... }` — jo kaam hota hai |

### Call

```cpp
int x = add(3, 4);          // 3, 4 = ARGUMENTS
```

1. `3` `a` mein copy, `4` `b` mein copy (folder — pass by value, lesson 03)
2. body chalti hai → `return 7`
3. `add(3, 4)` poore expression ki jagah `7` aa jaata hai
4. `x = 7`

---

## Kyun functions — 4 wajah

### 1. DRY — Don't Repeat Yourself

```cpp
// ❌ Bina function -- same logic 3 jagah
double p1 = base1 + base1 * 0.18;
double p2 = base2 + base2 * 0.18;
double p3 = base3 + base3 * 0.18;
// GST 18% se 12% karna hai? -> 3 jagah badlo, ek bhoole -> bug

// ✅ Ek function
double withTax(double base) { return base + base * 0.18; }
double p1 = withTax(base1), p2 = withTax(base2), p3 = withTax(base3);
// GST badalna? -> ek jagah
```

### 2. Abstraction — "kya" chhupao "kaise" se

```cpp
if (isPrime(n)) { ... }         // padhne wale ko "kaise" jaanna zaroori nahi
```

`isPrime` ke andar loop, modulo, sab hai — par call site pe sirf **intent**
dikhta hai.

### 3. Naam se dokumentation

```cpp
x = (x < lo) ? lo : (x > hi ? hi : x);       // ⚠️ kya kar raha hai?
x = clamp(x, lo, hi);                          // ✅ saaf
```

### 4. Test karne layak tukde

Ek 200-line `main()` test karna mushkil. `withTax`, `isPrime`, `parseOrder` —
har ek alag se test ho sakta hai (folder 46).

---

## Anatomy — token by token

```cpp
void printBox(std::string_view title) {
    const std::string bar(title.size() + 4, '=');
    std::cout << bar << "\n= " << title << " =\n" << bar << "\n";
}
```

- `void` — kuch return nahi karta (sirf side effect: printing)
- `printBox` — naam (verb rakho: `printBox`, `computeTotal`, `isValid`)
- `std::string_view title` — ek parameter (input)
- `{ ... }` — body, jismein local variables (`bar`) function ke andar hi zinda

### `void` function

```cpp
void greet(std::string_view name) {
    std::cout << "Hi, " << name << "\n";
    // koi return nahi -- ya jaldi nikalne ke liye bare `return;`
}
// int x = greet("Yo");   // ❌ ERROR -- void kuch nahi deta
```

`return;` (bina value) `void` function mein jaldi exit ke liye — guard clause
style (folder 06):

```cpp
void process(const Order& o) {
    if (!o.valid()) return;      // guard
    if (o.qty == 0) return;
    // ... real work ...
}
```

---

## Ek function doosre ko call karta hai

```cpp
int square(int x)        { return x * x; }
int sumOfSquares(int a, int b) { return square(a) + square(b); }
```

`sumOfSquares(3, 4)` → `square(3)` → `9`, `square(4)` → `16`, → `25`. Har call
apna **stack frame** banati hai (lesson 05).

---

## Function ka scope — locals bahar nahi jaate

```cpp
int addTax(int base) {
    int tax = base * 18 / 100;      // `tax` sirf is function ke andar
    return base + tax;
}
// `tax` yahan exist nahi karta
```

Har call pe `tax` **naya banta hai** aur function khatam hote hi **gayab** ho
jaata hai (automatic storage — lesson 05, 06).

---

## Andar kya hota hai (preview — lesson 05)

Function call = CPU ke liye:
1. Arguments registers/stack mein rakho (calling convention)
2. Return address save karo (kahan wapas aana hai)
3. `call` instruction → function ke code pe jump
4. Function apna **stack frame** banata hai (locals ke liye jagah)
5. `return` → value ek register mein, frame hataao, return address pe wapas jump

Yeh sab "call overhead" hai — chhoti functions ke liye compiler ise **inline**
karke poori tarah hata deta hai (lesson 10, measured ~6x farq).

> **HFT relevance:** Functions cost-free abstraction ho sakte hain — `-O2` pe
> chhoti hot functions inline ho jaati hain (zero overhead). Par ek **virtual**
> call, ek **`std::function`**, ya ek cross-TU call jo inline na ho — yeh sab hot
> path mein nanoseconds jodte hain (lesson 06_inline benchmark: ~1.9 ns/call).
> HFT code templates aur `constexpr` (lessons 10–11) se abstraction rakhta hai
> **bina** runtime cost ke. Folders 21, 36.

---

## Hands-on

`examples/01_first_functions.cpp` — basic functions, `void`, function-calls-
function, declare-before-use, DRY:

```bash
./build.ps1 08-FUNCTIONS/examples/01_first_functions.cpp
```

---

## ⚠️ Traps

### Trap 1 — return bhoolna (non-void)
```cpp
int f(int x) {
    int y = x * 2;
    // return y;  <- bhool gaye
}
```
`-Wreturn-type`: *"control reaches end of non-void function"* → UB. Hamesha
`-Wall`.

### Trap 2 — `void` function se value ki ummeed
```cpp
void log(std::string_view s);
if (log("hi")) { }        // ❌ ERROR
```

### Trap 3 — function ko call karna bhool jaana
```cpp
if (isReady) { }          // ⚠️ `isReady` ek function hai -> function pointer (non-null) -> hamesha true!
if (isReady()) { }        // ✅
```
`-Waddress` / `-Wbool-operation` kabhi warn karte hain.

### Trap 4 — bahut bada function
Ek function ek kaam kare. 100+ lines / 5+ responsibilities → tod do.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Function bas code chhota dikhata hai" | Reuse + abstraction + testability + naam-dokumentation |
| "`void` function `return` nahi kar sakta" | `return;` (bina value) — jaldi exit |
| "Function call free hai" | Call overhead real (~1-2 ns); chhoti fns inline ho jaati hain |
| "Locals function ke baad bhi rehte hain" | Automatic storage — call khatam, local gayab |
| "`if (fn)` function ko call karta hai" | Nahi — `fn` ka address (truthy). `fn()` call karta hai |

---

## Exercises

1. **DRY karo:** yeh 3 blocks ek function se replace karo —
   ```cpp
   std::cout << "+" << std::string(20, '-') << "+\n";
   std::cout << "| Report            |\n";
   std::cout << "+" << std::string(20, '-') << "+\n";
   // (aur do baar aisa hi, alag titles ke saath)
   ```

2. **Function chain:** `cube(int)` likho `square()` use karke. `cube(3)` = 27?

3. **`void` + guard:** `void printGrade(int marks)` — `marks < 0 || marks > 100`
   pe `"invalid"` print karke `return;`, warna grade.

4. **Return bhoolna:** yeh compile karo `-Wall` ke saath —
   ```cpp
   int biggest(int a, int b) { if (a > b) return a; }
   ```
   Warning? `b >= a` pe kya return hoga? Fix.

5. **`isReady` trap:** ek `bool ready()` function banao, phir `if (ready)` (bina
   `()`) likho `-Wall` ke saath. Warning aayi? Output kya?

6. **Refactor:** ek 40-line `main()` jo (1) input padhe (2) validate kare
   (3) compute kare (4) print kare — 4 functions mein tod do.

---

## Interview questions

1. Function use karne ke 4 fayde?
2. `void` function `return` kaise use karta hai?
3. Function call ka "overhead" kya-kya hai?
4. Local variable ka lifetime? Har call pe kya hota hai?
5. `if (fn)` vs `if (fn())` — kya farq?
6. Ek function ka "ek kaam" hone ka rule kyun?

---

## Next
→ [`02-declaration-vs-definition.md`](02-declaration-vs-definition.md)
