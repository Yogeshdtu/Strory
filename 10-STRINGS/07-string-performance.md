# 07 — String performance — allocation is the enemy

## Prerequisites
- [`04-string-internals-sso.md`](04-string-internals-sso.md), [`05-string-view.md`](05-string-view.md), [`06-string-conversions.md`](06-string-conversions.md)
- `09-ARRAYS/10-array-performance.md`

## Yeh topic abhi kyun
`std::string` ka har heap allocation ~50-200 ns leta hai (+ possible lock, cache
miss). Text-heavy code mein yeh jud jaata hai. Yeh lesson: **allocations kahan
chhupte hain, aur unhe kaise hataao** — HFT parsing ka core.

---

## Kahan allocation hota hai

```cpp
std::string s = "this is longer than SSO";    // 1 alloc  (short strings: 0 -- file 04)
std::string t = s;                             // 1 alloc  (copy the heap buffer)
s += " and more and more and more...";        // 1 alloc per capacity overflow
std::string u = s.substr(5, 10);               // 1 alloc  (substr copies)
std::string v = a + b + c;                     // ~2 allocs (temporaries)
void f(const std::string& p); f("literal");    // 1 alloc  (literal -> temp std::string)
std::string w = std::to_string(x);             // 1 alloc
for (auto& p : parts) result = result + p;     // O(n) allocs -- O(n^2) work
```

**Zero-alloc alternatives:**

| Costs an allocation | Zero-alloc version |
|---|---|
| `void f(const std::string&)` + literal | `void f(std::string_view)` |
| `s.substr(a, b)` | `std::string_view(s).substr(a, b)` |
| `split` → `vector<string>` | `split` → `vector<string_view>` |
| `a + b + c` | `out.reserve(...); out += a; out += b; out += c;` |
| `std::to_string(x)` in a loop | `std::to_chars` into a reused buffer |
| new `std::string` per iteration | one `std::string` `.clear()`'d and reused |
| `s += x` in a loop | `s.reserve(finalSize)` first |

---

## `reserve()` — the single biggest win

```cpp
std::string s;
for (int i = 0; i < 100000; ++i) s += 'x';    // ⚠️ ~17 reallocations (2x growth), O(n) copying

std::string s;
s.reserve(100000);                             // ✅ ONE allocation
for (int i = 0; i < 100000; ++i) s += 'x';
```

Same for `std::vector<std::string>` results, output buffers, JSON builders.
Estimate an upper bound and `reserve` it.

---

## Reuse buffers — don't re-allocate per iteration

```cpp
// ❌ new std::string every line
for (std::string line; std::getline(in, line); ) { process(line); }
//   ^ getline reuses `line`'s capacity across iterations -- this is actually OK-ish

// ❌ genuinely bad -- fresh string per record
for (auto& rec : records) {
    std::string key = rec.venue + ":" + rec.symbol;   // alloc per iteration
    map[key] = ...;
}

// ✅ reuse
std::string key;
key.reserve(32);
for (auto& rec : records) {
    key.clear();                                       // keeps capacity
    key += rec.venue; key += ':'; key += rec.symbol;
    map[key] = ...;                                    // (transparent lookup avoids another alloc)
}
```

`clear()` keeps the buffer (file 02) — reuse it.

---

## Parse into `string_view`, not `string`

```cpp
// ❌ each field is a heap allocation
struct Trade { std::string symbol; std::string side; double px; long qty; };

// ✅ fields are views into the receive buffer -- ZERO allocations
struct Trade { std::string_view symbol; std::string_view side; double px; long qty; };
```

Measured elsewhere in this course: `for (auto s : names)` (copy) vs
`for (const auto& s : names)` — **~50x** slower (folder 07 file 04). Same lesson:
avoid the copy.

`examples/06_csv_parser.cpp` — parses a whole CSV into `string_view` fields, **0
`std::string` allocations**.

---

## `from_chars` / `to_chars` — no-alloc number I/O

`std::stoi` / `std::to_string` / `std::stringstream` allocate. `std::from_chars`
/ `std::to_chars` write into a caller buffer. **~8x–30x** faster (file 06,
`examples/05_fast_parsing.cpp`).

---

## Small-buffer / static strings for known-bounded data

```cpp
// Fixed-width outbound field
std::array<char, 8> symbolField;              // stack, no heap
std::ranges::fill(symbolField, ' ');
std::ranges::copy(sym, symbolField.begin());

// "static vector of char" for bounded scratch
template <std::size_t Cap> struct FixedStr {
    std::array<char, Cap> buf; std::size_t len = 0;
    void append(std::string_view s);          // asserts len + s.size() <= Cap
    std::string_view view() const { return {buf.data(), len}; }
};
```

C++26: `std::inplace_vector`; today: `boost::static_vector`, or roll your own.

---

## `std::string` copy cost — length-dependent (file 04)

- Short (≤ SSO threshold): copy = 32-byte memcpy, **no allocation**.
- Long: `new` + `memcpy(len)`.
- **Move**: long strings → O(1) (steal pointer); short strings → 32-byte copy.

So `std::string` for **short keys** (symbols, codes) is fine even on hot-ish
paths. Long text → views + reserve + reuse.

---

## Andar kya hota hai

- `operator new` → allocator: free-list lookup, possibly a syscall (`mmap`/`brk`)
  for big/first allocations, possibly a lock (multi-threaded). Tens to hundreds
  of ns, and unpredictable (tail latency).
- First touch of a fresh page → page fault + zero-fill (folder 29).
- `+=` beyond capacity → `new` bigger + `memcpy` + `delete` — plus the new buffer
  is cold.
- `string_view` ops → pointer arithmetic only. `reserve` → one allocation,
  amortizes the rest.

> **HFT relevance:** On the market-data / order-entry path the target is **zero
> allocations per message**. Techniques: `std::string_view` fields over the
> receive buffer; `std::from_chars` for numbers; preallocated + reused scratch
> `std::string`s; fixed `std::array<char, N>` for outbound fixed-width fields;
> arena/pool allocators for anything that must own (folder 36). An allocation in
> a decode loop is a p99 latency spike (allocator lock / page fault) and is
> caught in code review and by allocation-counting tests. Folders 36, 38.

---

## Hands-on

```bash
g++ -std=c++20 -O0 10-STRINGS/examples/03_sso_demo.cpp -o sso && ./sso   # capacity growth
./build.ps1 fast 10-STRINGS/examples/05_fast_parsing.cpp                  # from_chars vs rest
./build.ps1 10-STRINGS/examples/06_csv_parser.cpp                         # zero-copy parse
```

Add a global `operator new` counter (like `03_sso_demo.cpp`) to `06_csv_parser.cpp`
— confirm 0 allocations during parsing.

---

## ⚠️ Traps

### Trap 1 — `const std::string&` param called with literals
```cpp
void log(const std::string& m);  log("hi");   // ⚠️ temp std::string. std::string_view
```

### Trap 2 — `substr` for a view
```cpp
if (s.substr(0, 3) == "GET") ...   // ⚠️ allocates. std::string_view(s).substr(0, 3)
```

### Trap 3 — `+` chain / `= +` loop
```cpp
for (...) result = result + piece;   // ⚠️ O(n^2). reserve + +=
```

### Trap 4 — fresh `std::string` per loop iteration
```cpp
for (...) { std::string tmp = build(); use(tmp); }   // ⚠️ alloc/free per iter. Hoist + clear()
```

### Trap 5 — benchmarking string code at `-O0` and trusting it
`-O2` for perf; but note allocation elision (file 04) can hide costs — count
allocations, don't just time.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::string` is cheap, use it everywhere" | Each long-string op is a heap allocation |
| "`substr` is a view" | It copies — `string_view::substr` for a view |
| "`reserve` is a micro-optimization" | Turns O(n) allocations into 1 |
| "`std::string` copy cost is fixed" | Length-dependent (SSO vs heap) |
| "Parsing into `std::string` fields is fine" | `std::string_view` fields → 0 allocations |

---

## Exercises

1. **Allocation audit:** add an `operator new` counter to `06_csv_parser.cpp`.
   Allocations during parsing? Now change `Trade`'s fields to `std::string` —
   count again.

2. **`reserve`:** build a 1MB string via `+=` with and without `reserve`. `-O0`
   (avoid elision), count allocations + time.

3. **`substr` vs view:** `1M` times, `s.substr(0, 5) == "hello"` vs
   `std::string_view(s).substr(0, 5) == "hello"`. `-O2`, time.

4. **Reuse:** build `venue + ":" + symbol` keys for 100k records — fresh
   `std::string` per iter vs one reused + `clear()`. Allocation count.

5. **Param type:** `void handle(const std::string&)` vs `std::string_view`,
   called 1M times with a literal. Count temp allocations.

6. **Fixed string:** implement `FixedStr<32>` (`append`, `view`, overflow
   assert). Use it to format a fixed-width record with zero heap.

---

## Interview questions

1. `std::string` allocation kahan-kahan chhupta hai (5 places)?
2. `reserve()` ka faayda — quantify?
3. Parse karte waqt `std::string` vs `std::string_view` fields — allocation farq?
4. `const std::string&` param + literal call — kya hota hai?
5. Buffer reuse (`clear()` + `+=`) kyun better than fresh string per iteration?
6. HFT hot path pe string allocation target kya, aur kaise achieve karo?

---

## Next
→ [`08-unicode-and-encoding.md`](08-unicode-and-encoding.md)
