# 04 — Return values

## Prerequisites
- [`03-parameters-and-arguments.md`](03-parameters-and-arguments.md)
- `06-CONDITIONS/03-nested-conditions.md` (multiple returns / guard clauses)
- `05-OPERATORS/08-ternary-operator.md`

## Yeh topic abhi kyun
Function ka **output**. Ek value, kai raaston se, ya kuch bhi nahi (`void`). Aur
kuch aham cheezein: multiple return statements kab theek hain, bade objects
return karne ki cost (aur RVO jo usse free bana deta hai), aur `[[nodiscard]]`
jo aapko return value ignore karne se rokta hai.

---

## `return` — do kaam

```cpp
int biggest(int a, int b) {
    if (a > b) return a;       // 1. value bhejo  2. function TURANT khatam
    return b;
}
```

`return X;`:
1. `X` ko caller tak pahunchao (`biggest(3, 4)` ki jagah `4`)
2. Function abhi ke abhi exit — aage ka code nahi chalta

`return;` (bina value) sirf `void` functions mein — jaldi exit.

---

## Multiple returns — modern C++ mein theek hain

```cpp
// ✅ Guard clauses -- har edge case ek return (folder 06 file 03)
std::string_view classify(int age) {
    if (age < 0)   return "invalid";
    if (age < 13)  return "child";
    if (age < 20)  return "teen";
    return "adult";
}
```

Purana "single return per function" rule C ke manual cleanup era ka tha. C++ mein
**RAII** (folder 17) cleanup khud karta hai, aur guard-clause style **zyada
readable** hai. Multiple returns theek — bas logic clear rakho.

⚠️ **Har raaste pe return** hona chahiye (non-`void`):

```cpp
int f(int x) {
    if (x > 0) return 1;
    if (x < 0) return -1;
    // x == 0 -> koi return nahi -> UB
}
```
`-Wreturn-type` (part of `-Wall`): *"control reaches end of non-void function"*.
Fix: aakhri `return 0;`.

---

## Return by VALUE — aur RVO (copy free hoti hai)

```cpp
std::vector<int> makeSequence(int n) {
    std::vector<int> result;
    result.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) result.push_back(i);
    return result;                 // "poora vector copy hoga?" -- NAHI
}

std::vector<int> v = makeSequence(1000);   // no copy, no move -- RVO
```

### Return Value Optimization (RVO / copy elision)

Compiler `result` ko **seedha caller ki `v` ki jagah** construct karta hai —
koi copy nahi, koi move nahi. C++17 se **guaranteed** jab aap ek prvalue return
karo:

```cpp
std::string greet() { return "hello"; }   // C++17: guaranteed no copy/move
```

Named local return (NRVO) bhi lagbhag hamesha elide hota hai (guaranteed nahi, par
har real compiler karta hai).

**Iska matlab:** bade objects by value return karna **theek hai** — "expensive
copy" wali chinta purani hai. `return result;` likho, `return std::move(result)`
**mat** likho (woh NRVO ko todta hai — folder 18).

---

## Return by REFERENCE — savdhani se

```cpp
int& biggerOf(int& a, int& b) {   // ek reference lautta hai
    return (a > b) ? a : b;
}

int x = 3, y = 7;
biggerOf(x, y) = 100;             // y ab 100 -- lauti hui reference pe assign
```

Valid hai jab lauti hui cheez **caller ke paas zinda** rahe (yahan `x`/`y`
caller ke hain).

### ⚠️ Return reference to LOCAL — UB

```cpp
int& bad() {
    int local = 42;
    return local;                // ⚠️ local function ke saath GAYAB -> dangling reference
}
int& r = bad();                  // r ek mari hui jagah point karta hai -> UB
```

`-Wreturn-local-addr` / `-Wdangling` warn karte hain. **Local ko by value return
karo** (RVO se free). Reference tabhi jab data caller/member/static ho (lesson 06).

---

## `void` — kuch return nahi

```cpp
void printReport(const Report& r) {
    std::cout << r.title << "\n";
    // side effect (printing) hi "output" hai
}
```

Kab: function ka kaam **karna** hai, batana nahi (I/O, kisi object ko modify
karna, logging).

---

## Multiple values return karna

C++ me tuple/struct/pair se:

```cpp
// 1. struct -- named, sabse readable
struct MinMax { int min; int max; };
MinMax findMinMax(const std::vector<int>& v) {
    return { *std::min_element(v.begin(), v.end()),
             *std::max_element(v.begin(), v.end()) };
}
auto [lo, hi] = findMinMax(data);       // structured binding

// 2. std::pair / std::tuple
std::pair<bool, int> parse(std::string_view s);
auto [ok, value] = parse("42");

// 3. out-parameter (C-style, ab kam use)
bool parse(std::string_view s, int& out);
```

**Prefer struct** (naam se pata chalta hai kya kya) ya `std::optional` /
`std::expected` (folder 23) for "value ya error".

---

## `[[nodiscard]]` — return value ignore mat karo

```cpp
[[nodiscard]] bool tryConnect();

tryConnect();          // ⚠️ warning: ignoring return value of 'tryConnect'
if (tryConnect()) { }  // ✅
```

Jab return value ignore karna **almost hamesha bug** ho (`empty()` vs `clear()`,
error codes, `reserve`, allocation results) — `[[nodiscard]]` lagao.

```cpp
[[nodiscard]] int computeChecksum(std::span<const std::byte>);
[[nodiscard]] std::optional<Order> parseOrder(std::string_view);
```

Poora attributes lesson 12 mein.

---

## Return type deduction — `auto`

```cpp
auto add(int a, int b) { return a + b; }        // -> int
auto makeVec() { return std::vector<int>{1,2,3}; }

// trailing return type (jab pehle likhna mushkil ho)
auto divide(int a, int b) -> double { return static_cast<double>(a) / b; }
```

`auto` return: saare `return` statements ka type **same** hona chahiye. Public
API mein explicit type likhna better (documentation).

---

## Andar kya hota hai

- **Small return** (`int`, pointer, small struct ≤ 16 bytes): value ek register
  mein (`rax`, ya `rax:rdx` pair) — free.
- **Big return** (bada struct/vector): caller ek hidden pointer pass karta hai
  (jahan result likhna hai), function usme seedha construct karta hai — **RVO**.
  Koi extra copy nahi.
- `void`: kuch nahi, bas `ret`.

`examples/06_inline_asm_check.cpp` — `-O2` pe chhota function poori tarah inline,
`return` bhi gayab.

> **HFT relevance:** Bade result structs by value return karna RVO ki wajah se
> zero-cost hai — out-parameters ki C-style gymnastics ki zaroorat nahi. Hot
> functions jo `bool`/error-code deti hain — `[[nodiscard]]` se galti se ignore
> hone se bacho (ek missed risk-check return = disaster). `std::expected`
> (folder 23) exceptions ke bina error return karta hai — HFT `-fno-exceptions`
> builds mein common.

---

## Hands-on

```bash
./build.ps1 08-FUNCTIONS/examples/01_first_functions.cpp
```

Ek program: `std::vector<int> makeBig(int n)` return by value. `-O2 -S` se dekho
— koi copy/move `call` hai? (RVO). Phir `int& bad()` local return karke
`-Wall` warning dekho.

---

## ⚠️ Traps

### Trap 1 — kisi raaste pe return nahi
```cpp
int f(int x) { if (x) return 1; }    // -Wreturn-type; UB on x == 0
```

### Trap 2 — return reference to local
```cpp
std::string& name() { std::string s = "x"; return s; }   // dangling -> UB
```

### Trap 3 — `return std::move(local)` (pessimization)
```cpp
std::vector<int> f() { std::vector<int> v; ...; return std::move(v); }  // ⚠️ NRVO tod diya
std::vector<int> f() { std::vector<int> v; ...; return v; }             // ✅
```

### Trap 4 — return value ignore
```cpp
v.empty();          // ⚠️ shayad `v.clear()` likhna tha; [[nodiscard]] hota to warning
```

### Trap 5 — narrowing on return
```cpp
int getCount() { return someLongLongValue; }   // ⚠️ silent truncation; -Wconversion
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Ek function mein ek hi `return`" | Multiple returns / guard clauses = zyada readable |
| "Bada object by value return = mehngi copy" | RVO/copy elision → free (C++17 guaranteed for prvalue) |
| "`return std::move(x)` faster hai" | NRVO tod deta hai — plain `return x;` |
| "Return reference se copy bachti hai" | Sirf jab object caller/member/static ho — local = UB |
| "Return value ignore karna theek hai" | Error/bool returns pe aksar bug — `[[nodiscard]]` |

---

## Exercises

1. **Guard-clause returns:** `char grade(int marks)` — `<0`/`>100` → `'X'`, warna
   A/B/C/D/F. Har raaste pe return? `-Wall` clean?

2. **RVO check:** `std::vector<int> range(int n)` (by value). `auto v = range(1000);`
   `-O2 -S -masm=intel` — koi copy/move constructor `call`?

3. **Dangling:** yeh chalao `-Wall` ke saath —
   ```cpp
   const std::string& firstWord(const std::string& s) {
       std::string w = s.substr(0, s.find(' '));
       return w;
   }
   ```
   Warning? Fix (by value return).

4. **Multiple values:** `struct Stats { double mean; double stddev; };`
   `Stats analyze(const std::vector<double>&)`. Structured binding se use karo.

5. **`[[nodiscard]]`:** `[[nodiscard]] bool withdraw(Account&, int amount);`
   likho. Bina `if` ke call karo — warning? Ab handle karo.

6. **`auto` return:** `auto safeDivide(int a, int b)` — `b == 0` pe kya return
   karoge? (Hint: `std::optional<double>` — par phir sab returns same type.)

7. **out-param vs return:** `bool tryParse(std::string_view, int& out)` aur
   `std::optional<int> parse(std::string_view)` — dono likho, use karo. Kaunsa
   call site cleaner?

---

## Interview questions

1. `return` ke do kaam?
2. Multiple return statements — modern C++ mein theek kyun (C se alag)?
3. RVO / copy elision kya hai? C++17 ne kya guarantee kiya?
4. `return std::move(localVector)` kyun galat hai?
5. Return reference to local — kya hota hai? Compiler warning?
6. `[[nodiscard]]` kya karta hai? Kab lagate ho?
7. Function se 2 values kaise return karo — 3 tareeke, kaunsa best?

---

## Next
→ [`05-the-call-stack.md`](05-the-call-stack.md)
