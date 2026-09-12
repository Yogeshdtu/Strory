# 13 — `std::format` and `std::print`

## Prerequisites
- Folder 04 (I/O, `printf` dangers, iostreams), folder 10 (strings)
- [`examples/07_format_print.cpp`](examples/07_format_print.cpp)

## Yeh topic abhi kyun
Text formatting ke teen puraane options the: `printf` (fast, **unsafe** — format
string bug = UB), iostreams (`<<` — type-safe but verbose, slow, locale-heavy),
manual string building (tedious). C++20 ka **`std::format`** teenon ko replace
karta: **type-safe** (galat `{}` = compile error), Python-style spec, custom
formatters, aur `std::format_to_n` se **allocation-free**. `std::print` (C++23)
seedha stream pe.

---

## `std::format` basics

```cpp
#include <format>

std::string s = std::format("{} + {} = {}", 2, 3, 5);        // "2 + 3 = 5"
std::string p = std::format("{0} {1} {0}", "A", "B");        // "A B A"  -- positional
std::string n = std::format("{:.2f}", 3.14159);             // "3.14"
```

- Each `{}` is a replacement field; `{{` / `}}` are literal braces.
- The type is deduced from the argument — **a mismatched spec or a missing
  argument is a compile-time error** (the format string is checked at compile
  time via `consteval`), not `printf`'s runtime UB.
- No locale by default (`{:L}` opts in) → deterministic across machines.

---

## The format spec mini-language

`{[index]:[[fill]align][sign][#][0][width][.precision][type]}`

```cpp
std::format("{:<10}", "left");     // "left      "   left-align, width 10
std::format("{:>10}", "right");    // "     right"   right-align
std::format("{:^10}", "mid");      // "   mid    "   center
std::format("{:*<10}", "x");       // "x*********"   fill char '*'
std::format("{:08.2f}", 3.5);      // "00003.50"    zero-pad, width 8, 2 decimals
std::format("{:+}", 7);            // "+7"          always show sign
std::format("{:#x}", 255);         // "0xff"        alternate form (0x/0o/0b prefix)
std::format("{:#b}", 5);           // "0b101"
std::format("{:.3e}", 123456.0);   // "1.235e+05"
std::format("{:d}", 'A');          // "65"          format a char as its int value
std::format("{:>{}}", "x", 8);     // width taken from the NEXT argument (8)
```

Types: `d`/`b`/`o`/`x` (integer bases), `f`/`e`/`g`/`a` (float), `s` (string),
`c` (char), `p` (pointer), `?` (C++23 — debug/quoted).

---

## Where the output goes

```cpp
// build a string:
std::string line = std::format("px={:.2f} qty={}", 101.25, 300);

// write to a stream (C++23):
std::print("px={:.2f} qty={}\n", 101.25, 300);        // to stdout
std::println("done");                                  // adds '\n'
std::print(stderr, "error: {}\n", msg);
std::print(someOfstream, "{}\n", row);

// C++20 (no std::print): format then output
std::fputs(std::format("...\n").c_str(), stdout);
std::cout << std::format("...\n");

// format INTO an iterator / buffer -- no std::string, no allocation:
char buf[64];
auto res = std::format_to_n(buf, sizeof(buf) - 1, "px={:.2f} qty={}", 101.25, 300);
*res.out = '\0';                                       // res.size = chars that WOULD have been written
```

`std::format_to(OutputIt, ...)` and `std::format_to_n(OutputIt, n, ...)` are the
allocation-free forms — write into a `char[]`, a `std::pmr` buffer, a
`back_inserter`, etc.

---

## Custom formatters

Make your type formattable by specializing `std::formatter<T>`:

```cpp
struct Price { long ticks; int tickSize; };

template <>
struct std::formatter<Price> {
    bool compact = false;
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it == 'c') { compact = true; ++it; }
        return it;                                     // must return the iterator to '}'
    }
    auto format(const Price& p, std::format_context& ctx) const {
        double v = double(p.ticks) * p.tickSize / 10000.0;
        return compact ? std::format_to(ctx.out(), "{:.2f}", v)
                       : std::format_to(ctx.out(), "${:.4f} ({} ticks)", v, p.ticks);
    }
};

std::format("{}",  Price{1912400, 1});   // "$191.2400 (1912400 ticks)"
std::format("{:c}", Price{1912400, 1});  // "191.24"
```

Common shortcut: derive from `std::formatter<std::string>` (or another built-in)
and delegate in `format()`.

`examples/07_format_print.cpp` has this exact `Price` formatter plus a table, all
spec features, and `format_to_n`.

---

## `std::format` vs the alternatives

| | `printf` | iostreams `<<` | `std::format` |
|---|---|---|---|
| type safety | **none** (format/arg mismatch = UB) | full | full (compile-time checked format string) |
| verbosity | terse | verbose (`<< std::setw(8) << std::fixed << ...`) | terse |
| custom types | no | `operator<<` | `std::formatter<T>` |
| locale | via `%'d` etc. | sticky, on by default | off by default, `{:L}` to opt in → deterministic |
| speed | fast | slow (sync, sentries, locale) | fast (compile-time parse, no locale) — competitive with `printf`, often faster than iostreams |
| allocation | writes to a buffer | internal buffers | `std::string` result, **or** `format_to_n` into your buffer (zero alloc) |
| positional args | POSIX `%1$d` (non-portable) | no | `{0}` `{1}` portably |

---

## Andar kya hota hai

- The format string is parsed at **compile time** (`std::format` takes a
  `consteval`-checked `std::format_string<Args...>`). A bad `{}` or wrong type →
  the program doesn't compile. `printf`'s `%d`/pointer mismatch is a runtime read
  of the wrong bytes.
- Formatting dispatches to `std::formatter<T>::format` — a compile-time-resolved
  call, no `va_arg` type erasure, no `%`-string re-scan of the spec at runtime
  for the fast paths.
- `std::format` returns a `std::string` → one allocation for the result.
  `std::format_to_n(buf, n, ...)` writes directly into your storage → **zero
  allocation**, and returns how many chars *would* have been written (so you can
  detect truncation).
- No locale means no per-character locale facet lookups (a real iostreams cost)
  and bit-identical output across hosts — important for logs you diff.
- `std::print` (C++23) formats into a stack/small buffer then does one write —
  faster than a chain of `<<` (each `<<` is a virtual call through the stream's
  `sentry` machinery).

> **HFT relevance:** `std::format` / `std::print` replace `printf` (unsafe) and
> iostreams (slow) for **logging and diagnostics** — type-safe, deterministic
> (no locale), and `std::format_to_n` writes into a **preallocated buffer with no
> allocation**, which is what a low-latency structured logger needs (format the
> record into a ring-buffer slot, no `std::string`, no `malloc`). Custom
> `std::formatter<T>` for domain types (a `Price`, an `OrderId`, a `Timestamp`)
> keeps log call sites clean. It still does **string work** — any formatted
> output stays **off the tick path**; the hot path writes raw fields to a binary
> journal and formats later. But for everything that *is* text (audit logs,
> replay dumps, admin output), `std::format` + `format_to_n` is the right, fast,
> safe tool.

---

## Hands-on

```bash
./build.ps1 22-MODERN-CPP/examples/07_format_print.cpp
```

`examples/07`: basics, alignment/width/fill, number bases & sign, a formatted
table, a custom `Price` formatter, and `format_to_n` into a `char[64]`. Add a
`std::formatter<OrderId>` and a `std::formatter` that delegates to
`std::formatter<double>`.

---

## ⚠️ Traps

### Trap 1 — runtime format string
```cpp
std::string fmt = getFmt();
std::format(fmt, x);   // ❌ not consteval-checkable -> compile error. Use std::vformat(fmt, std::make_format_args(x))
```

### Trap 2 — `format_to_n` and forgetting it may truncate
```cpp
auto r = std::format_to_n(buf, 8, "{}", 123456789);   // writes 8 chars; r.size == 9 -> it was truncated. Check r.size <= n
```

### Trap 3 — expecting locale-grouped numbers by default
```cpp
std::format("{}", 1000000);       // "1000000"   -- NO thousands separators
std::format("{:L}", 1000000);     // "1,000,000" -- opt in with :L (and it becomes locale-dependent)
```

### Trap 4 — custom formatter's `parse()` not returning the '}' iterator
```cpp
constexpr auto parse(auto& ctx) { return ctx.begin(); }   // ⚠️ if you consumed spec chars, return the iterator AFTER them (at '}')
```

### Trap 5 — using `std::print` on a pre-C++23 / partial libstdc++
```cpp
#include <print>   // ❌ may be missing. Fall back to std::fputs(std::format(...).c_str(), stdout)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::format` is `printf` with `{}`" | Type-safe (format string checked at compile time), no locale, custom formatters |
| "`std::format` always allocates" | It returns a `std::string`, but `format_to_n` / `format_to` write into your buffer with no allocation |
| "Numbers get thousands separators automatically" | No — `{}` is plain; `{:L}` opts into locale grouping |
| "iostreams and `std::format` perform similarly" | `std::format` skips the locale/sentry machinery — typically faster, closer to `printf` |
| "You can pass a runtime format string to `std::format`" | No — use `std::vformat` + `std::make_format_args` for that |

---

## Exercises

1. **Safety:** `printf("%d %s", "hello", 42);` — what happens? Same with
   `std::format("{} {}", "hello", 42);`?

   <details><summary>Answer</summary>

   `printf`: UB — it reads a `const char*` as an `int` and an `int` as a
   `const char*` (likely garbage or a crash), no diagnostic. `std::format`: fine
   — `{}` deduces each argument's type; prints `hello 42`. A *wrong* spec like
   `{:d}` on the string would be a **compile error**.
   </details>

2. **Zero-alloc log line:** format `"[{}] {} px={:.2f}\n"` (a level string, a
   symbol, a price) into a fixed `char[128]` with no allocation.

   <details><summary>Answer</summary>

   `char buf[128]; auto r = std::format_to_n(buf, sizeof(buf) - 1, "[{}] {}
   px={:.2f}\n", level, sym, px); *r.out = '\0'; write(fd, buf, r.out - buf);` —
   check `r.size < sizeof(buf)` for truncation.
   </details>

3. **Table:** print rows `{sym, px, qty}` as `sym` left in width 6, `px` right in
   width 12 with 2 decimals, `qty` right in width 10.

   <details><summary>Answer</summary>

   `std::format("{:<6}{:>12.2f}{:>10}\n", r.sym, r.px, r.qty)` per row (see
   `examples/07` section 4).
   </details>

4. **Custom formatter:** make `struct Ts { std::int64_t ns; };` format as
   `"HH:MM:SS.nnnnnnnnn"`.

   <details><summary>Answer</summary>

   `template <> struct std::formatter<Ts> { constexpr auto parse(auto& ctx){
   return ctx.begin(); } auto format(Ts t, auto& ctx) const { auto s = t.ns /
   1'000'000'000; auto frac = t.ns % 1'000'000'000; return std::format_to(
   ctx.out(), "{:02}:{:02}:{:02}.{:09}", (s/3600)%24, (s/60)%60, s%60, frac); }
   };`
   </details>

5. **Runtime format:** you read a format string from a config file. How do you
   use it with `std::format`'s machinery?

   <details><summary>Answer</summary>

   `std::vformat(runtimeFmt, std::make_format_args(a, b, c))` — `vformat` takes a
   `string_view` format (not compile-time checked) + type-erased args. A bad spec
   here throws `std::format_error` at runtime instead of failing to compile.
   </details>

---

## Interview questions

1. `std::format` vs `printf` — type safety, format-string checking kab hota?
2. `std::format` vs iostreams — speed, locale, verbosity?
3. `std::format` allocation — `format_to_n` kaise avoid karta?
4. Custom type formattable kaise (`std::formatter<T>` — `parse` + `format`)?
5. `{}` vs `{:L}` — thousands separators / locale?
6. HFT logging mein `std::format` — hot path pe kyun nahi, kahan sahi?

---

## Next
→ [`14-migration-guide.md`](14-migration-guide.md)
