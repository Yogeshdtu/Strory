# 06 — String ↔ number conversions

## Prerequisites
- [`03-string-operations.md`](03-string-operations.md), [`05-string-view.md`](05-string-view.md)
- `08-FUNCTIONS/13-main-arguments.md` (`from_chars` already used)

## Yeh topic abhi kyun
Text se number (`"42"` → `42`) aur wapas. Kai tareeke hain jinme **10x+** performance
farq aur alag error-handling. Hot-path parsing (market data, config, CLI) ke liye
sahi choose karna zaroori.

---

## Number → string

```cpp
std::to_string(42)          // "42"     -- simple, ek std::string allocate karta hai
std::to_string(3.14)        // "3.140000"  -- ⚠️ fixed 6 decimals, koi control nahi

// std::format (C++20) -- formatting ke liye sabse achha
std::format("{}", 42)               // "42"
std::format("{:.2f}", 3.14159)     // "3.14"
std::format("{:08x}", 255)          // "000000ff"
std::format("{:>10}", "hi")        // "        hi"

// std::to_chars (C++17) -- sabse tez, allocation nahi, AAPKE buffer mein likhta hai
char buf[32];
auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), 42);
std::string_view result(buf, ptr - buf);   // "42"
```

---

## String → number

### `std::from_chars` (C++17) — tez aur sakht (strict)

```cpp
#include <charconv>

std::string_view s = "42abc";
int value = 0;
auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
// value = 42
// ptr   -> 'a' pe (jahan parsing ruki)
// ec    == std::errc{}  (success)
```

| Property | `from_chars` |
|---|---|
| Allocation | **nahi** |
| Exceptions | **nahi** (`errc` lautata hai) |
| Locale | **nahi** (decimal ke liye hamesha `.`, thousands separator nahi) |
| Whitespace | **skip nahi karta** (`" 7"` → `invalid_argument`) |
| Sign | `-` haan, `+` nahi (integers ke liye) |
| Adhoora parse | `ptr` se batata hai kahan ruka |
| Errors | `errc::invalid_argument` (koi digit nahi), `errc::result_out_of_range` (overflow) |

```cpp
auto [p, ec] = std::from_chars(first, last, v);
if (ec == std::errc{})                          { /* poora/adhoora success, v valid */ }
else if (ec == std::errc::invalid_argument)     { /* yahan koi number nahi */ }
else if (ec == std::errc::result_out_of_range)  { /* type ke liye bahut bada */ }
if (p != last) { /* number ke baad kachra bacha hai */ }
```

`int`/`long`/`unsigned`/... aur `float`/`double` sab ke liye chalta hai (base aur format arguments
bhi hain).

### `std::stoi` / `stol` / `stod` / ... — aasaan, par slow

```cpp
int    i = std::stoi("42");           // std::invalid_argument / std::out_of_range phenkta hai
double d = std::stod("3.14");
long   l = std::stol("  -17xyz");     // shuru ke spaces skip, 'x' pe ruka -> -17 (kachre pe error NAHI!)

std::size_t pos;
int j = std::stoi("42abc", &pos);     // pos = 2 (jahan ruka)
```

- Galat input / overflow pe **exception phenkta hai** (exceptions — cost + control flow)
- **Shuru ke whitespace skip** karta hai, pehle non-digit pe rukta hai (**chupchaap adhoora parse**)
- Locale-aware
- `std::string` leta hai (`string_view` nahi) — temp banana pad sakta hai

### `std::atoi` / `atol` / `atof` — C wale, inse bacho

```cpp
int i = std::atoi("42");              // fail hone pe "0" -- atoi("0") se ALAG pehchaan hi nahi sakte
int j = std::atoi("99999999999999");  // ⚠️ overflow pe UB
```

Koi error reporting nahi, overflow pe UB. Sirf bharosemand, pakke sahi input ke liye.

### `std::stringstream` — flexible, bahut slow

```cpp
std::istringstream iss("42 3.14 hello");
int a; double b; std::string c;
iss >> a >> b >> c;                    // kai tokens parse
if (iss.fail()) { /* galat */ }
```

Allocation, locale, virtual dispatch — `from_chars` se **~30x slow**. Config/setup ke liye theek, hot
path pe kabhi nahi.

---

## Naapa (`examples/05_fast_parsing.cpp`, `-O2`, 100k strings, har pass ka time)

| Method | GCC 15.1 (pehle naapa) | GCC 16.2 (3 runs) |
|---|---|---|
| `std::from_chars` | ~0.86 ms (1.0x) | 1.05–1.07 ms (1.0x) |
| `std::atoi` | ~2.9 ms (~3.4x) | 3.28–4.42 ms (3.1–4.1x) |
| `std::stoi` | ~7.1 ms (~8.3x) | 8.37–8.56 ms (7.9–8.1x) — exceptions + `std::string` + locale |
| `std::stringstream` | ~26 ms (~30x) | 30.1–37.4 ms (28–35x) — allocations + locale + virtual |

**Ratios dono compilers pe lagbhag wahi rahe** — yahi asli lesson hai. Absolute ms thode alag hain
(machine ki haalat / compiler); ek-ek ms ki value pe mat atko, ratio pe dekho.

Strictness demo (dono pe same):
```
  "42"                 -> value 42, stopped at index 2
  "42abc"              -> value 42, stopped at index 2
  "abc"                -> invalid (no digits)
  "  7"                -> invalid (from_chars shuru ke whitespace skip NAHI karta)
  "999999999999999999" -> out of range for int
```

---

## Faisla kaise karein

```
Hot path / parser / untrusted input    -> std::from_chars  (+ errc handle karo)
Output formatting (align, precision)   -> std::format (C++20) / std::to_chars
Ek baar ka kaam, bharosemand, cold     -> std::stoi / std::to_string
Kai tokens / stream jaisa              -> stringstream (sirf cold)
Kabhi nahi                             -> atoi/atof (error handling nahi, overflow pe UB)
```

---

## Andar kya hota hai

- `from_chars` → ek tight digit-jodne wala loop (`v = v*10 + (c - '0')`) overflow check ke saath;
  floats ke liye ek fast path + fallback. Na heap, na locale object.
- `stoi` → (kabhi-kabhi) `std::string` banata hai, locale ke saath `strtol` call, `errno` check,
  phir error pe throw — throw wala raasta exception object banata hai.
- `stringstream` → stream + buffer allocate, `num_get` facet (virtual) ke through, locale ke saath.
- `to_string` → `std::string` allocate, `snprintf` jaisi formatting.

> **HFT relevance:** Feed parsing (FIX tags, ITCH fields, order-entry) allocation-free aur
> exception-free hoti hai: `std::from_chars`, ya `std::string_view` fields pe haath se likha parser,
> errors return value ki tarah (`std::optional` / `std::expected`). `std::stoi`/`stringstream`/
> `to_string` hot path pe ban hain — har message pe ek exception throw ya heap allocation latency
> budget uda deta hai. Folder 38.

---

## Hands-on

```bash
./build.ps1 fast 10-STRINGS/examples/05_fast_parsing.cpp
```

---

## ⚠️ Traps

### Trap 1 — `from_chars` whitespace skip nahi karta
```cpp
std::from_chars(" 42", ...);   // ⚠️ invalid_argument. Pehle trim karo, ya space ke aage badho
```

### Trap 2 — `stoi` ka chupchaap adhoora parse
```cpp
std::stoi("42abc");            // 42 -- koi error nahi. `pos` check karo ya from_chars lo
```

### Trap 3 — overflow / galat input pe `atoi`
```cpp
std::atoi("bad");              // 0 -- ek valid zero jaisa dikhta hai
std::atoi("9999999999");       // UB
```

### Trap 4 — `to_string(double)` ki formatting
```cpp
std::to_string(3.14159);      // "3.141590" -- fixed 6 decimals, peeche zeros. std::format lo
```

### Trap 5 — `from_chars` ka result ignore karna
```cpp
int v; std::from_chars(a, b, v);   // ⚠️ fail hua to v ko haath nahi lagaya jaata (garbage reh sakta hai)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`stoi` aur `from_chars` ek jaise hain" | `from_chars`: na alloc/exception/locale, ~8x tez (dono compilers pe naapa) |
| "`from_chars` shuru ke spaces skip karta hai" | Nahi — strict |
| "`atoi` error code deta hai" | Nahi — fail pe `0`, overflow pe UB |
| "`to_string` saaf float output deta hai" | Fixed 6 decimals — `std::format` lo |
| "`from_chars` galat input pe throw karta hai" | `errc` lautata hai — aapko check karna padega |

---

## Exercises

1. **`from_chars` wrapper:** `std::optional<int> parseInt(std::string_view)` — success sirf tab jab
   *poori* string consume ho. `"42"`, `"42 "`, `"x"`, `""`, `"-5"`, `"+5"` pe test karo.

2. **Whitespace:** `parseInt` ko `from_chars` call se pehle shuru ke whitespace skip karwao.

3. **Benchmark:** `05_fast_parsing.cpp` ke numbers apni machine pe dobara naapo. Comparison mein
   `std::strtol` bhi jodo.

4. **`to_chars` round-trip:** `int → to_chars → string_view → from_chars → int`. `INT_MIN`, `0`,
   `INT_MAX` ke liye verify karo.

5. **`std::format`:** ek price table print karo — symbol right-aligned (width 6), price 2 decimals ke
   saath (width 10), qty thousands separator ke saath... (thousands separator ke liye locale ya haath
   ka code chahiye).

6. **Error taxonomy:** `"abc"`, `""`, `"99999999999999999"`, `"42x"` pe `from_chars` chalao — har
   ek kaunsa `errc` / `ptr` position deta hai, print karo.

---

## Interview questions

1. `std::from_chars` vs `std::stoi` — 4 differences?
2. `from_chars` whitespace / partial parse / error reporting?
3. `std::atoi` ke 2 problems?
4. `std::to_string(double)` vs `std::format` for floats?
5. Hot-path number parsing ke liye kya, aur kyun (allocation, exceptions)?
6. `from_chars` ka return value (`ptr`, `ec`) — kaise use karo?

---

## Next
→ [`07-string-performance.md`](07-string-performance.md)
