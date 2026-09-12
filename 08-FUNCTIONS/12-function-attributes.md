# 12 — Function attributes

## Prerequisites
- [`04-return-values.md`](04-return-values.md) (`[[nodiscard]]`)
- [`10-inline-functions.md`](10-inline-functions.md) (`[[gnu::always_inline]]`)
- `06-CONDITIONS/08-branch-prediction-intro.md` (`[[likely]]`/`[[unlikely]]`)

## Yeh topic abhi kyun
Attributes `[[...]]` compiler ko **extra jaankari** dete hain — "yeh return value
ignore mat karo", "yeh function throw nahi karega", "yeh path rare hai", "yeh
function purana hai". Kuch sirf warnings/diagnostics hain, kuch **codegen** badal
dete hain. HFT aur library code mein bahut use hote hain.

---

## Standard attributes (`[[name]]`)

### `[[nodiscard]]` — return value use karo

```cpp
[[nodiscard]] bool tryLock();
[[nodiscard]] std::optional<Order> parse(std::string_view);
[[nodiscard("balance check zaroori")]] int withdraw(Account&, int);   // C++20 -- message

tryLock();              // ⚠️ warning: ignoring return value
if (tryLock()) { }      // ✅
```

Kab: return value ignore karna **almost hamesha bug** ho — error codes, `empty()`
vs `clear()`, allocation results, `[[nodiscard]]` types (`std::unique_ptr` khud
nodiscard-ish). Class pe bhi laga sakte ho (`class [[nodiscard]] Error { };`).

### `[[maybe_unused]]` — "use nahi hua to warning mat do"

```cpp
void f([[maybe_unused]] int debugId) {
    #ifdef DEBUG
    log(debugId);
    #endif
    // release build mein debugId use nahi hota -> normally -Wunused-parameter
}

[[maybe_unused]] auto result = compute();   // sirf assert mein use hota hai
assert(result > 0);                          // NDEBUG mein assert gayab -> result unused
```

### `[[deprecated]]` — "ab use mat karo"

```cpp
[[deprecated("use parseV2() instead")]]
Order parse(std::string_view s);

parse("...");           // ⚠️ warning: 'parse' is deprecated: use parseV2() instead
```

API migration ke liye — purana function rakho par callers ko nudge karo.

### `[[noreturn]]` — "yeh function kabhi return nahi karta"

```cpp
[[noreturn]] void fatal(std::string_view msg) {
    std::cerr << msg << "\n";
    std::abort();        // ya throw, ya exit, ya infinite loop
}

int f(int x) {
    if (x < 0) fatal("negative");
    return x * 2;        // compiler jaanta hai fatal() ke baad yahan nahi aata -> no "missing return" warning
}
```

Compiler ko batata hai: iske baad ka code unreachable hai → warnings suppress,
optimization better.

### `[[likely]]` / `[[unlikely]]` (C++20) — branch hint

```cpp
if (rc != 0) [[unlikely]] {
    return handleError(rc);      // cold path -- compiler isse "door" rakhta hai
}
process();                        // hot path -- straight-line, no jump

for (auto& x : v) {
    if (x.valid) [[likely]] { use(x); }
    else                     { skip(x); }
}
```

- Common path ko fall-through (no branch taken) banata hai
- Cold code ko function ke end mein / alag section mein rakhta hai → **I-cache**
  hot code ke liye saaf
- ⚠️ **Galat hint = regression.** Sirf jab pakka pata ho (error paths, assertions).
  Warna PGO (profile-guided optimization, folder 33) use karo.

Folder 06 file 08 mein branch prediction ke saath yeh detail mein hai.

### `[[assume(expr)]]` (C++23) — "yeh hamesha sach hai"

```cpp
void scale(int* p, int n) {
    [[assume(n > 0)]];           // compiler maan leta hai n > 0 -> better codegen (no n<=0 checks)
    for (int i = 0; i < n; ++i) p[i] *= 2;
}
```

⚠️ Agar assumption jhoothi ho → **UB**. Bahut dhyaan se.

### `[[fallthrough]]` (folder 06 file 04)

`switch` mein jaan-boojh kar fallthrough document karta hai.

---

## Compiler-specific (`[[gnu::name]]` / `__attribute__`)

GCC/Clang. Portable code mein `#if defined(__GNUC__)` se guard karo ya macro.

| Attribute | Kya |
|---|---|
| `[[gnu::always_inline]]` | force inline (lesson 10) |
| `[[gnu::noinline]]` | force NO inline (debugging, benchmarking, cold path) |
| `[[gnu::hot]]` / `[[gnu::cold]]` | "yeh function hot/cold hai" — code layout, optimization level |
| `[[gnu::pure]]` | "sirf args + globals padhta hai, koi side effect nahi" — compiler CSE kar sakta hai |
| `[[gnu::const]]` | "sirf args padhta hai (globals bhi nahi), no side effect" — aur aggressive |
| `[[gnu::flatten]]` | "is function ke andar ke saare calls inline karo" |
| `[[gnu::target("avx2")]]` | is function ke liye specific ISA |
| `[[gnu::aligned(64)]]` | alignment (data pe zyada, functions pe kabhi) |

```cpp
[[gnu::const]] int square(int x) { return x * x; }   // f(3) + f(3) -> ek hi baar compute
[[gnu::hot]] void matchOrders(Book& b);               // hot path -- optimize hard, layout together
[[gnu::cold]] [[noreturn]] void panic(const char*);   // cold -- door rakho
```

---

## `noexcept` — exceptions ke baare mein promise

Technically specifier hai, attribute nahi — par yahin fit baithta hai.

```cpp
void swap(T& a, T& b) noexcept;        // "yeh function throw NAHI karega"
int parse(std::string_view) noexcept;  // agar throw kiya -> std::terminate() (crash)
```

- **Documentation + contract**: caller bharosa kar sakta hai
- **Optimization**: `noexcept` functions ke around compiler exception-handling
  machinery (unwind tables, cleanup) skip kar sakta hai — chhoti functions mein
  measurable
- **`std::vector` growth**: `T` ka move constructor `noexcept` ho to `vector`
  reallocation pe **move** karta hai (fast); warna **copy** (safe but slow) —
  folder 18
- ⚠️ `noexcept` function ne throw kiya → `std::terminate()`. Jhootha promise mat do.

```cpp
[[nodiscard]] constexpr int add(int a, int b) noexcept { return a + b; }
```

---

## Andar kya hota hai

- **`[[nodiscard]]`, `[[deprecated]]`, `[[maybe_unused]]`**: sirf **compile-time
  diagnostics** — koi codegen impact nahi.
- **`[[likely]]`/`[[unlikely]]`, `[[gnu::hot]]/[[gnu::cold]]`**: **code layout**
  — hot code ko contiguous, cold ko alag `.text.unlikely` section mein. Branch
  ki fall-through direction. I-cache behaviour.
- **`[[gnu::pure]]/[[gnu::const]]`**: compiler duplicate calls **eliminate** kar
  sakta hai (CSE), loop se hoist kar sakta hai.
- **`[[noreturn]]`, `[[assume]]`**: unreachable code / invariants → optimization
  aur warning suppression.
- **`noexcept`**: unwind machinery skip, `std::move` in containers.

> **HFT relevance:**
> - `[[nodiscard]]` har error-returning / risk-check function pe — missed check =
>   disaster.
> - `[[unlikely]]` + `[[gnu::cold]]` error/reject/log paths pe → hot decode/match
>   loop straight-line, I-cache saaf (measurable p99 improvement).
> - `noexcept` hot path functions pe → no unwind overhead; move-heavy code fast.
> - `[[gnu::const]]/[[gnu::pure]]` pure helpers (price math, field extract) pe →
>   CSE, hoisting.
> - `[[gnu::hot]]` on the match loop; `[[gnu::flatten]]` to force-inline the
>   helper tree.
> - `[[assume]]` for invariants the type system can't express (buffer aligned,
>   count positive) — bahut savdhani se.
> Folders 23 (exceptions), 33 (compiler opt), 36 (low-latency).

---

## Hands-on

```cpp
#include <iostream>

[[nodiscard]] bool check(int x) { return x > 0; }
[[deprecated("use v2")]] int oldApi() { return 1; }
[[noreturn]] void die() { std::abort(); }
[[gnu::const]] int sq(int x) { return x * x; }

int main() {
    check(5);                    // warning: ignoring return value
    oldApi();                    // warning: deprecated
    int r = sq(4) + sq(4);       // -O2: sq(4) ek hi baar compute (const)
    std::cout << r << "\n";
}
```

```bash
g++ -std=c++20 -Wall -Wextra attr.cpp -o a       # warnings dekho
g++ -std=c++20 -O2 -S attr.cpp -o - | c++filt | grep -c 'call.*sq'   # 1 (CSE)
```

---

## ⚠️ Traps

### Trap 1 — `[[likely]]` guess pe lagana
Galat hint → hot path pe extra jump → regression. Measure / PGO.

### Trap 2 — `noexcept` jhootha
```cpp
int parse(std::string_view s) noexcept { return std::stoi(std::string(s)); }  // ⚠️ stoi throws -> terminate()
```

### Trap 3 — `[[gnu::const]]` on impure function
```cpp
[[gnu::const]] int nextId() { static int c = 0; return ++c; }   // ⚠️ side effect! compiler CSE -> galat results
```

### Trap 4 — `[[assume]]` false
```cpp
[[assume(ptr != nullptr)]];   // agar ptr null hua -> UB, code deleted null-checks
```

### Trap 5 — GNU attributes bina guard, non-GCC compiler pe
```cpp
[[gnu::always_inline]]   // MSVC pe unknown attribute (warning, ignored) -- guard ya macro
```

### Trap 6 — `[[nodiscard]]` overuse
Har getter pe lagana noise banata hai. Sirf jab ignore = likely bug.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Attributes runtime pe kuch karte hain" | Zyada tar compile-time diagnostics; kuch codegen/layout |
| "`[[likely]]` free speedup" | Galat guess = slower. Measure / PGO |
| "`noexcept` bas documentation" | Optimization + `std::move`-in-vector + terminate-on-throw |
| "`[[nodiscard]]` har function pe" | Sirf jab ignore = bug |
| "`[[gnu::const]]` = `const` function" | Alag — "no side effects, args-only" for CSE |
| "`[[assume]]` safe hint hai" | False assume = UB |

---

## Exercises

1. **`[[nodiscard]]`:** `[[nodiscard]] bool saveToDisk();` — bina `if` ke call
   karo. Warning? Ab handle karo. Ek `class [[nodiscard]] Status` banao.

2. **`[[deprecated]]`:** `[[deprecated("use computeV2")]] int compute();` +
   `int computeV2();`. `compute()` call pe warning text?

3. **`[[noreturn]]`:** `[[noreturn]] void abort_msg(const char*)`. Ek function
   `int f(int)` jismein kuch paths `abort_msg` call karein — bina aakhri `return`
   ke `-Wall` clean? `[[noreturn]]` hata ke dekho.

4. **`[[gnu::const]]` CSE:** `[[gnu::const]] int hash(int)` — `hash(x) + hash(x)`
   `-O2 -S` mein kitne `call hash`? Attribute hata ke?

5. **`[[likely]]`:** ek parse loop mein valid-case ko `[[likely]]`, error ko
   `[[unlikely]]`. `-O2 -S` — error-handling code function ke end mein gaya?

6. **`noexcept` + vector:** `struct Slow { Slow(Slow&&) { /* work */ } };` vs
   `struct Fast { Fast(Fast&&) noexcept {} };` — `std::vector` mein 1000 push_back,
   reallocation pe move hua ya copy? (Instrument constructors.)

7. **Portable macro:** `#define ALWAYS_INLINE` jo GCC/Clang pe
   `[[gnu::always_inline]] inline`, MSVC pe `__forceinline`, warna `inline` ho.

---

## Interview questions

1. `[[nodiscard]]` kya karta hai? Kab lagate ho?
2. `[[likely]]`/`[[unlikely]]` — codegen pe kya asar? Risk?
3. `[[noreturn]]` compiler ko kya batata hai? Faayda?
4. `noexcept` — documentation ke alawa 2 concrete effects?
5. `[[gnu::pure]]` aur `[[gnu::const]]` mein fark? Compiler kya kar sakta hai?
6. Kaunse attributes sirf diagnostics hain, kaunse codegen badalte hain?
7. `[[assume]]` (C++23) — faayda aur khatra?

---

## Next
→ [`13-main-arguments.md`](13-main-arguments.md)
