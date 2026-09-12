# 03 — String operations

## Prerequisites
- [`02-std-string-basics.md`](02-std-string-basics.md)

## Yeh topic abhi kyun
`std::string` ke kaam ki cheezein: search, slice, split, replace, trim, case.
Zyada tar text processing inhi ka combination hai.

---

## Search — `find` family

```cpp
std::string s = "the quick brown fox";

s.find("quick")           // 4     -- first occurrence index
s.find("quick", 5)        // npos  -- search starting at 5
s.find('o')               // 12    -- char
s.rfind("o")              // 17    -- LAST occurrence
s.find_first_of("aeiou")  // 2     -- first char that is ANY of these
s.find_first_not_of(" ")  // 0     -- first NON-space (trimming!)
s.find_last_not_of(" ")   // ...   -- for trimming the end

if (s.find("cat") == std::string::npos) { ... }   // not found
```

`std::string::npos` = `size_t(-1)` (huge). **Hamesha `== npos` se check karo**,
`>= 0` se nahi (unsigned → always true).

---

## Slice — `substr`

```cpp
s.substr(4)               // "quick brown fox"  -- from index 4 to end
s.substr(4, 5)            // "quick"            -- from 4, length 5
s.substr(s.size())        // ""                 -- ok (empty)
s.substr(s.size() + 1)    // ⚠️ throws std::out_of_range
```

⚠️ `substr` **allocates a new string** (copy). For a no-copy slice → `std::string_view`
(file 05).

---

## Split — no built-in, do it yourself

```cpp
std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::size_t start = 0, pos;
    while ((pos = s.find(delim, start)) != std::string::npos) {
        out.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    out.push_back(s.substr(start));          // last field
    return out;
}
```

⚠️ Each `substr` copies. Zero-copy version returns `std::vector<std::string_view>`
(file 05, `examples/04_string_view.cpp`, `06_csv_parser.cpp`).

---

## Replace

```cpp
s.replace(4, 5, "slow");                    // [4,9) -> "slow"  (in place, resizes)

// replace ALL occurrences -- manual loop
std::string replaceAll(std::string s, std::string_view from, std::string_view to) {
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();                    // ⚠️ skip past the replacement (avoid infinite loop if to contains from)
    }
    return s;
}
```

---

## Trim (no built-in)

```cpp
std::string_view trim(std::string_view s) {
    const auto b = s.find_first_not_of(" \t\n\r");
    if (b == std::string_view::npos) return {};      // all whitespace
    const auto e = s.find_last_not_of(" \t\n\r");
    return s.substr(b, e - b + 1);
}
```

Return `std::string_view` — no allocation.

---

## Case conversion

```cpp
#include <cctype>
#include <algorithm>

std::transform(s.begin(), s.end(), s.begin(),
    [](unsigned char c) { return std::tolower(c); });   // ⚠️ cast to unsigned char!
```

⚠️ `std::tolower(int)` — passing a plain (possibly negative) `char` is **UB**.
Always `static_cast<unsigned char>(c)` first.

(This is ASCII-only. Unicode case folding is a whole library — file 08.)

---

## Concatenation — `+` vs `+=` vs `append`

```cpp
std::string r = a + b + c;                  // ok for a few -- but each + makes a temp

// ❌ O(n^2) -- rebuilds the string every iteration
for (auto& p : parts) r = r + p;

// ✅ O(n) -- amortized, in place
for (auto& p : parts) r += p;

// ✅ better -- one allocation
std::string r;
r.reserve(totalSize);
for (auto& p : parts) r += p;
```

`std::format` (C++20) / `std::string` streams for structured building.

---

## `std::string` vs `<algorithm>` / `<ranges>`

`std::string` is a container of `char` — most `<algorithm>` works:

```cpp
std::count(s.begin(), s.end(), 'a');
std::ranges::sort(s);
auto it = std::ranges::find(s, 'x');
std::erase(s, ' ');                          // C++20 -- remove all spaces
std::ranges::all_of(s, ::isdigit);          // (with the unsigned-char cast in practice)
```

---

## Andar kya hota hai

- `find` → naive substring search (libstdc++: not Boyer-Moore by default) — O(n·m)
  worst case. For big haystacks + repeated searches, `std::boyer_moore_searcher`.
- `substr` → allocate + `memcpy` the slice.
- `replace` with a different-length replacement → may shift the tail / reallocate.
- `+` → creates a temporary `std::string` (allocation) per operator, unless the
  compiler applies string concatenation optimizations (limited).
- `+=` → append in place; reallocate only when capacity exceeded (~2x growth).

> **HFT relevance:** In parsers, prefer `std::string_view` operations (`substr`,
> `find`, `remove_prefix`) — they're pure pointer arithmetic, zero allocation.
> `std::string::substr`/`+`/`replace` on a hot path each hit the allocator.
> Field extraction from a fixed-format message is index math, not `find`. Folder
> 38.

---

## Hands-on

`examples/02_std_string.cpp`, `examples/04_string_view.cpp` (zero-copy split /
trim), `examples/06_csv_parser.cpp`:

```bash
./build.ps1 10-STRINGS/examples/04_string_view.cpp
./build.ps1 10-STRINGS/examples/06_csv_parser.cpp
```

---

## ⚠️ Traps

### Trap 1 — `find` result `>= 0`
```cpp
if (s.find("x") >= 0) ...        // ⚠️ npos is huge unsigned -> always true. == npos
```

### Trap 2 — `substr` out of range
```cpp
s.substr(s.size() + 1);          // ⚠️ throws. s.substr(std::min(pos, s.size()))
```

### Trap 3 — `tolower(char)` with negative char
```cpp
std::tolower(c);                 // ⚠️ UB if c < 0. std::tolower((unsigned char)c)
```

### Trap 4 — `replaceAll` infinite loop
```cpp
while ((p = s.find(from, p)) != npos) { s.replace(p, from.size(), to); }
// ⚠️ if `to` contains `from`, or p not advanced -> infinite. p += to.size();
```

### Trap 5 — `r = r + x` in a loop
```cpp
for (...) r = r + x;             // ⚠️ O(n^2). r += x;
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`find` returns `-1` on miss" | `std::string::npos` (huge unsigned) — `== npos` |
| "`substr` is a cheap view" | Allocates a copy — `string_view` for a view |
| "`std::string` has `split` / `trim`" | No — write them (return `string_view`) |
| "`tolower(c)` is fine for `char`" | UB for negative — cast to `unsigned char` |
| "`a + b + c` is one allocation" | Multiple temporaries — `reserve` + `+=` |

---

## Exercises

1. **find/rfind:** in `"a.b.c.d"` — index of first `'.'`, last `'.'`, second
   `'.'` (use the position arg).

2. **split → vector<string>:** implement, handle empty fields (`"a,,c"` → 3),
   leading/trailing delim.

3. **trim (string_view):** `trim("  \t hi \n ")` → `"hi"`. All-whitespace → `""`.
   Single char. Empty.

4. **replaceAll:** `replaceAll("aaa", "a", "aa")` — should be `"aaaaaa"`, not
   infinite. Why the `pos += to.size()`?

5. **toLower ASCII:** `std::string lower(std::string s)` — with the
   `unsigned char` cast. Test `"HeLLo123"`.

6. **Build efficiently:** join `vector<string>` with `", "` separator — naive
   `+` vs `reserve` + `+=`. `-O2`, time on 100000 elements.

7. **`std::string` as container:** count digits in a string with `std::count_if`;
   remove all spaces with `std::erase`.

---

## Interview questions

1. `std::string::npos` kya hai? `find` ka result kaise check karo?
2. `substr` ki cost? No-copy alternative?
3. Split / trim built-in hain? Return type kya rakhoge?
4. `std::tolower(char)` ka trap?
5. `+` chain vs `+=` — complexity? `reserve` ka role?
6. `std::string` pe kaunse `<algorithm>` chalte hain?

---

## Next
→ [`04-string-internals-sso.md`](04-string-internals-sso.md)
