# 20 — `<regex>`: basics, and why it's slow

## Prerequisites
- [`19-filesystem.md`](19-filesystem.md), folder 10 (strings), [`16-functional.md`](16-functional.md)

## Yeh topic abhi kyun
Log parsing, config validation, quick text extraction — regex handy hai. Par
`std::regex` ki **do badi problems** hain jo har C++ dev ko pata honi chahiye:
(1) compile-time bahut slow, aur (2) `std::regex` object banana **mehenga** hai —
loop mein banaya to disaster. HFT parsers regex **kabhi** use nahi karte hot
path pe. Ye lesson: use kaise karein, aur kyun sambhal ke.

---

## The three operations

```cpp
#include <regex>

std::regex re(R"(\d{4}-\d{2}-\d{2})");        // build ONCE (expensive -- see below). raw string avoids \\ hell

// 1. match the WHOLE string:
bool ok = std::regex_match("2026-08-30", re);           // true only if the entire input matches

// 2. find the FIRST occurrence anywhere:
std::smatch m;
if (std::regex_search(line, m, re)) {
    std::string date = m[0];                             // the whole match
    // m[1], m[2]... are capture groups; m.position(0), m.prefix(), m.suffix()
}

// 3. replace:
std::string out = std::regex_replace(line, re, "DATE");  // all matches -> "DATE"
std::string out2= std::regex_replace(line, std::regex(R"((\w+)=(\w+))"), "$2:$1");  // $1 $2 backrefs
```

Iterate all matches:
```cpp
for (std::sregex_iterator it(line.begin(), line.end(), re), end; it != end; ++it) {
    const std::smatch& mm = *it;
    use(mm.str(), mm.position());
}
```

`smatch` = matches into a `std::string` (holds iterators into it — **the string
must outlive `m`**, and must not be a temporary). `cmatch` for `const char*`.

## Grammars and flags

```cpp
std::regex re(pattern, std::regex::ECMAScript);   // default -- Perl-ish, what you expect
std::regex re2(pattern, std::regex::icase | std::regex::optimize);   // case-insensitive; spend more time compiling for faster matching
// other grammars: basic, extended (POSIX), awk, grep, egrep
```

`std::regex::optimize` — asks the implementation to do extra work at construction
for faster matching. Worth it if the regex is reused a lot.

---

## Why it's slow — the two problems

### 1. Constructing `std::regex` is expensive

The constructor **parses and compiles** the pattern into an internal NFA/state
machine, with allocations. libstdc++'s implementation is not fast at this.
Ballpark: constructing a moderately complex `std::regex` can cost **microseconds
to tens of microseconds** — thousands of times more than a `regex_search` on a
short string.

```cpp
// ❌ catastrophic: recompiles the regex every call
bool isDate(const std::string& s) {
    std::regex re(R"(\d{4}-\d{2}-\d{2})");   // built + destroyed every call
    return std::regex_search(s, re);
}

// ✅ compile once
bool isDate(const std::string& s) {
    static const std::regex re(R"(\d{4}-\d{2}-\d{2})");   // thread-safe init, built once
    return std::regex_search(s, re);
}
```

### 2. Matching itself is slow and can blow up

- `std::regex` matching is **backtracking**. A pattern like `(a+)+$` on
  `"aaaaaaaaaaaaaaaaX"` is **catastrophic backtracking** — exponential time,
  effectively a hang. Untrusted patterns + untrusted input = a DoS
  (ReDoS).
- Even well-behaved patterns: libstdc++ `std::regex` is roughly **an order of
  magnitude slower** than RE2, PCRE2, or `std::string::find` / hand-written
  parsing for the same job. Heavy allocation per match, no JIT.
- Compile-time: `<regex>` is a large, heavily-templated header — including it and
  instantiating `basic_regex`/`match_results` noticeably slows **your build**.

## Alternatives

| Need | Use instead |
|---|---|
| Fixed substring / prefix / split on a char | `std::string::find`, `std::string_view`, `std::ranges::views::split` |
| Parse a known structured format (CSV, key=value, fixed fields) | hand-written scan / `std::from_chars` — 10–100x faster, no allocation |
| Serious regex work, big inputs, untrusted patterns | **RE2** (linear time, no catastrophic backtracking) or **PCRE2** (with JIT), `ctre` (compile-time regex, header-only, very fast) |
| Compile-time-known pattern, max speed | `ctre::match<"...">(sv)` (CTRE library) — the regex compiles to a state machine at C++ compile time, zero runtime construction |

---

## Andar kya hota hai

- `basic_regex` ctor: tokenize the pattern → parse to an AST → build a state
  machine (a graph of `_State` nodes with transitions), allocating nodes. Flags
  like `optimize` trigger extra passes. All of this is per-construction.
- `regex_search`/`regex_match`: a recursive backtracking executor walks the state
  machine against the input; on a mismatch it rewinds and tries alternatives.
  `match_results` allocates a vector of sub-match iterator pairs. Quantifier
  nesting → branching factor → exponential worst case.
- No caching between calls, no bytecode JIT (unlike PCRE2-JIT), no linear-time
  guarantee (unlike RE2's Thompson NFA simulation).
- `std::regex_replace` builds the output string incrementally with more
  allocations.

> **HFT relevance:** `std::regex` is **absent from any latency-sensitive path** —
> it allocates per match, backtracks (unbounded worst case), and even
> constructing one is µs-scale. Market-data and FIX-ish parsers are hand-written:
> fixed offsets, `memchr` for delimiters, `std::from_chars` for numbers — no
> allocation, branch-predictable, ns-scale per field. Regex, if used at all,
> lives in **config validation and log tooling** at startup, and even there the
> `std::regex` objects are `static const` (built once) or the code uses RE2 /
> CTRE. A `std::regex` compiled inside a per-message function has tanked
> throughput in real systems.

---

## Hands-on

```cpp
// regex_cost.cpp -- construction vs match cost
#include <regex>
#include <chrono>
#include <cstdio>
using Clock = std::chrono::steady_clock;

int main() {
    const std::string s = "2026-08-30T12:34:56.789Z";
    const char* pat = R"((\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2}))";

    auto t0 = Clock::now();
    for (int i = 0; i < 1000; ++i) { std::regex re(pat); (void)re; }
    auto t1 = Clock::now();

    std::regex re(pat);
    std::smatch m;
    auto t2 = Clock::now();
    for (int i = 0; i < 1000; ++i) { std::regex_search(s, m, re); }
    auto t3 = Clock::now();

    auto us = [](auto a, auto b){ return std::chrono::duration<double, std::micro>(b - a).count() / 1000.0; };
    std::printf("construct: %.3f us/call\n", us(t0, t1));
    std::printf("search   : %.3f us/call\n", us(t2, t3));   // typically construct >> search
}
```
```bash
g++ -std=c++20 -O2 regex_cost.cpp -o rc && ./rc
```

---

## ⚠️ Traps

### Trap 1 — constructing the regex in a loop / per call
```cpp
for (auto& line : lines) if (std::regex_search(line, std::regex(pat))) ...   // ⚠️ recompiles every iteration. static const std::regex re(pat); outside
```

### Trap 2 — catastrophic backtracking
```cpp
std::regex re(R"((\w+\s*)+$)");
std::regex_search(longNoMatchLine, re);   // ⚠️ can take seconds / hang on a non-matching line. Rewrite the pattern; or use RE2
```

### Trap 3 — `smatch` bound to a temporary
```cpp
std::smatch m;
std::regex_search(getLine(), m, re);   // ⚠️ getLine() temporary destroyed -> m holds dangling iterators. Store the string first
```

### Trap 4 — `regex_search` vs `regex_match` confusion
```cpp
std::regex_match(line, re);   // ⚠️ requires the WHOLE line to match. For "contains a date" use regex_search
```

### Trap 5 — expecting `\d` etc. without a raw string
```cpp
std::regex re("\d+");     // ❌ "\d" is an unknown escape in a normal string literal. R"(\d+)" or "\\d+"
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::regex re(pat)` is cheap" | It parses + compiles + allocates — µs-scale; build once (`static const`) |
| "`std::regex` is linear time" | Backtracking — exponential worst case (ReDoS). RE2 is the linear one |
| "regex is fine for parsing market data" | Hand-written fixed-field parsing is 10–100x faster and allocation-free |
| "`regex_match` finds the pattern in the string" | It matches the **entire** string; use `regex_search` for "contains" |
| "`std::regex` performance ≈ PCRE/RE2" | libstdc++ `std::regex` is ~10x slower and has no JIT |

---

## Exercises

1. **Fix the hot loop:** `for (auto& l : millionLines) if
   (std::regex_search(l, std::regex(R"(ERROR|FATAL)"))) count++;` — make it fast.

   <details><summary>Answer</summary>

   Hoist the regex: `static const std::regex re(R"(ERROR|FATAL)"); for (auto& l :
   millionLines) if (std::regex_search(l, re)) count++;`. Even better here:
   `l.find("ERROR") != npos || l.find("FATAL") != npos` — no regex at all.
   </details>

2. **Extract fields:** from `"px=101.25 qty=300 sym=AAPL"` get the three values.
   Regex version, then the reason a hand parser wins.

   <details><summary>Answer</summary>

   `static const std::regex re(R"((\w+)=(\S+))"); for (std::sregex_iterator
   it(s.begin(), s.end(), re), end; it != end; ++it) kv[(*it)[1]] = (*it)[2];`.
   Hand parser: `split on ' '`, then `split each on '='`, `std::from_chars` for
   numbers — no regex compile, no `smatch` allocations, ~ns per field.
   </details>

3. **ReDoS:** why does `(a+)+$` blow up on `"aaaa...aaab"`? What class of regex
   engine avoids it?

   <details><summary>Answer</summary>

   The nested quantifiers let the engine partition the run of `a`s in
   exponentially many ways; each fails at the `b`/end and backtracks to try the
   next partition → O(2ⁿ). A **Thompson-NFA / DFA** engine (RE2) simulates all
   states at once in O(n·m) — no backtracking, no blow-up.
   </details>

4. **match vs search:** you want "line is exactly an ISO date". Which function
   and pattern?

   <details><summary>Answer</summary>

   `std::regex_match(line, std::regex(R"(\d{4}-\d{2}-\d{2})"))` — `regex_match`
   requires the whole line to match, so no `^...$` anchors are needed.
   </details>

5. **CTRE pitch:** what does `ctre::match<R"(\d{4}-\d{2}-\d{2})">(sv)` give you
   that `std::regex` can't?

   <details><summary>Answer</summary>

   The pattern is parsed and turned into a matcher **at C++ compile time** →
   **zero** runtime construction cost, no allocation, and matching that's
   typically several times faster than `std::regex`. Downside: pattern must be a
   compile-time literal, and it adds compile time / a dependency.
   </details>

---

## Interview questions

1. `std::regex` ki 2 badi problems (construction cost, backtracking)?
2. `static const std::regex` kyun — loop mein banane se kya hota?
3. `regex_match` vs `regex_search` — fark?
4. Catastrophic backtracking / ReDoS kya, kaunsa engine bachata (RE2)?
5. Market data parsing regex se kyun nahi — kya use hota?
6. `smatch` lifetime rule — temporary string ke saath kya galat?

---

## Next
→ [`21-type-traits.md`](21-type-traits.md)
