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
std::to_string(42)          // "42"     -- simple, allocates a std::string
std::to_string(3.14)        // "3.140000"  -- ⚠️ fixed 6 decimals, no control

// std::format (C++20) -- best for formatting
std::format("{}", 42)               // "42"
std::format("{:.2f}", 3.14159)     // "3.14"
std::format("{:08x}", 255)          // "000000ff"
std::format("{:>10}", "hi")        // "        hi"

// std::to_chars (C++17) -- fastest, no allocation, writes into YOUR buffer
char buf[32];
auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), 42);
std::string_view result(buf, ptr - buf);   // "42"
```

---

## String → number

### `std::from_chars` (C++17) — the fast, strict one

```cpp
#include <charconv>

std::string_view s = "42abc";
int value = 0;
auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
// value = 42
// ptr   -> points at 'a' (where parsing stopped)
// ec    == std::errc{}  (success)
```

| Property | `from_chars` |
|---|---|
| Allocation | **none** |
| Exceptions | **none** (returns `errc`) |
| Locale | **none** (always `.` for decimal, no thousands sep) |
| Whitespace | **not skipped** (`" 7"` → `invalid_argument`) |
| Sign | `-` yes, `+` no (for integers) |
| Partial parse | reports stop position via `ptr` |
| Errors | `errc::invalid_argument` (no digits), `errc::result_out_of_range` (overflow) |

```cpp
auto [p, ec] = std::from_chars(first, last, v);
if (ec == std::errc{})                          { /* full/partial success, v valid */ }
else if (ec == std::errc::invalid_argument)     { /* no number here */ }
else if (ec == std::errc::result_out_of_range)  { /* too big for the type */ }
if (p != last) { /* trailing junk after the number */ }
```

Works for `int`/`long`/`unsigned`/... and `float`/`double` (base and format args
available).

### `std::stoi` / `stol` / `stod` / ... — convenient, slower

```cpp
int    i = std::stoi("42");           // throws std::invalid_argument / std::out_of_range
double d = std::stod("3.14");
long   l = std::stol("  -17xyz");     // skips leading ws, stops at 'x' -> -17 (no error for junk!)

std::size_t pos;
int j = std::stoi("42abc", &pos);     // pos = 2 (where it stopped)
```

- **Throws** on bad input / overflow (exceptions — cost + control flow)
- **Skips leading whitespace**, stops at first non-digit (**silent partial parse**)
- Locale-aware
- Takes a `std::string` (not `string_view`) — may force a temp

### `std::atoi` / `atol` / `atof` — C, avoid

```cpp
int i = std::atoi("42");              // "0" on failure -- INDISTINGUISHABLE from atoi("0")
int j = std::atoi("99999999999999");  // ⚠️ UB on overflow
```

No error reporting, UB on overflow. Only for trusted, known-good input.

### `std::stringstream` — flexible, very slow

```cpp
std::istringstream iss("42 3.14 hello");
int a; double b; std::string c;
iss >> a >> b >> c;                    // multi-token parsing
if (iss.fail()) { /* bad */ }
```

Allocates, locale, virtual dispatch — **~30x slower** than `from_chars`. Fine for
config/setup, never for hot paths.

---

## Measured (`examples/05_fast_parsing.cpp`, GCC 15.1, `-O2`, 100k strings)

```
  std::from_chars   :  ~0.86 ms    (1.0x  -- baseline)
  std::atoi         :  ~2.9 ms     (~3.4x)
  std::stoi         :  ~7.1 ms     (~8.3x  -- exceptions + std::string + locale)
  std::stringstream :  ~26 ms      (~30x   -- allocations + locale + virtual)
```

Strictness demo:
```
  "42"                 -> value 42, stopped at index 2
  "42abc"              -> value 42, stopped at index 2
  "abc"                -> invalid (no digits)
  "  7"                -> invalid (from_chars does NOT skip leading whitespace)
  "999999999999999999" -> out of range for int
```

---

## Decision

```
Hot path / parser / untrusted input   -> std::from_chars  (+ handle errc)
Formatting output (aligned, precision) -> std::format (C++20) / std::to_chars
Quick one-off, trusted input, cold     -> std::stoi / std::to_string
Multi-token / stream-y                 -> stringstream (cold only)
Never                                  -> atoi/atof (no error handling, UB on overflow)
```

---

## Andar kya hota hai

- `from_chars` → a tight digit-accumulation loop (`v = v*10 + (c - '0')`) with an
  overflow check; for floats, a fast path + fallback. No heap, no locale object.
- `stoi` → constructs (sometimes) a `std::string`, calls `strtol` with a locale,
  checks `errno`, then throws on error — the throw path builds an exception object.
- `stringstream` → allocates the stream + buffer, goes through `num_get` facet
  (virtual), locale-imbued.
- `to_string` → allocates a `std::string`, `snprintf`-style formatting.

> **HFT relevance:** Feed parsing (FIX tags, ITCH fields, order-entry) is
> allocation-free and exception-free: `std::from_chars` or a hand-rolled parser
> over `std::string_view` fields, errors as return values (`std::optional` /
> `std::expected`). `std::stoi`/`stringstream`/`to_string` are banned from the
> hot path — one exception throw or heap allocation per message blows the
> latency budget. Folder 38.

---

## Hands-on

```bash
./build.ps1 fast 10-STRINGS/examples/05_fast_parsing.cpp
```

---

## ⚠️ Traps

### Trap 1 — `from_chars` doesn't skip whitespace
```cpp
std::from_chars(" 42", ...);   // ⚠️ invalid_argument. Trim first, or advance past ws
```

### Trap 2 — `stoi` silent partial parse
```cpp
std::stoi("42abc");            // 42 -- no error. Check `pos` or use from_chars
```

### Trap 3 — `atoi` on overflow / bad input
```cpp
std::atoi("bad");              // 0 -- looks like a valid zero
std::atoi("9999999999");       // UB
```

### Trap 4 — `to_string(double)` formatting
```cpp
std::to_string(3.14159);      // "3.141590" -- fixed 6 decimals, trailing zeros. Use std::format
```

### Trap 5 — `from_chars` return value ignored
```cpp
int v; std::from_chars(a, b, v);   // ⚠️ if it failed, v is untouched (may be garbage)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`stoi` and `from_chars` are equivalent" | `from_chars`: no alloc/exception/locale, ~8x faster |
| "`from_chars` skips leading spaces" | It doesn't — strict |
| "`atoi` returns an error code" | No — `0` on failure, UB on overflow |
| "`to_string` gives clean float output" | Fixed 6 decimals — use `std::format` |
| "`from_chars` throws on bad input" | Returns `errc` — you must check it |

---

## Exercises

1. **`from_chars` wrapper:** `std::optional<int> parseInt(std::string_view)` —
   success only if the *whole* string is consumed. Test `"42"`, `"42 "`, `"x"`,
   `""`, `"-5"`, `"+5"`.

2. **Whitespace:** make `parseInt` skip leading whitespace before calling
   `from_chars`.

3. **Benchmark:** reproduce `05_fast_parsing.cpp` numbers on your machine. Add
   `std::strtol` to the comparison.

4. **`to_chars` round-trip:** `int → to_chars → string_view → from_chars → int`.
   Verify for `INT_MIN`, `0`, `INT_MAX`.

5. **`std::format`:** print a price table — right-aligned symbol (width 6),
   price with 2 decimals (width 10), qty with thousands... (thousands sep needs
   locale or manual).

6. **Error taxonomy:** call `from_chars` on `"abc"`, `""`, `"99999999999999999"`,
   `"42x"` — print which `errc` / `ptr` position each gives.

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
