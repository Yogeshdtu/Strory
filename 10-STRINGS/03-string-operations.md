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

s.find("quick")           // 4     -- pehli baar kahan mila
s.find("quick", 5)        // npos  -- index 5 se aage dhoondho
s.find('o')               // 12    -- ek char
s.rfind("o")              // 17    -- AAKHRI baar kahan mila
s.find_first_of("aeiou")  // 2     -- pehla char jo inme se KOI bhi ho
s.find_first_of(" ")      // 3     -- pehla space
s.find_first_not_of(" ")  // 0     -- pehla NON-space (trimming ke kaam aata hai!)
s.find_last_not_of(" ")   // ...   -- aakhir se trim karne ke liye

if (s.find("cat") == std::string::npos) { ... }   // nahi mila
```

`std::string::npos` = `size_t(-1)` (bahut bada number). **Hamesha `== npos` se check karo**, `>= 0`
se nahi (unsigned hai → hamesha true).

---

## Slice — `substr`

```cpp
s.substr(4)               // "quick brown fox"  -- index 4 se end tak
s.substr(4, 5)            // "quick"            -- 4 se, length 5
s.substr(s.size())        // ""                 -- theek hai (khaali)
s.substr(s.size() + 1)    // ⚠️ std::out_of_range phenkta hai
```

⚠️ `substr` **ek nayi string allocate karta hai** (copy). Bina copy ka slice chahiye →
`std::string_view` (file 05).

---

## Split — built-in nahi hai, khud likho

```cpp
std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::size_t start = 0, pos;
    while ((pos = s.find(delim, start)) != std::string::npos) {
        out.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    out.push_back(s.substr(start));          // aakhri field
    return out;
}
```

⚠️ Har `substr` copy karta hai. Zero-copy version `std::vector<std::string_view>` lautata hai
(file 05, `examples/04_string_view.cpp`, `06_csv_parser.cpp`).

---

## Replace

```cpp
s.replace(4, 5, "slow");                    // [4,9) -> "slow"  (wahin, size badal jaata hai)

// SAARE occurrences replace karna -- haath ka loop
std::string replaceAll(std::string s, std::string_view from, std::string_view to) {
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();                    // ⚠️ replacement ke aage badho (to mein from ho to infinite loop se bachav)
    }
    return s;
}
```

---

## Trim (built-in nahi)

```cpp
std::string_view trim(std::string_view s) {
    const auto b = s.find_first_not_of(" \t\n\r");
    if (b == std::string_view::npos) return {};      // sab whitespace
    const auto e = s.find_last_not_of(" \t\n\r");
    return s.substr(b, e - b + 1);
}
```

`std::string_view` return karo — koi allocation nahi.

---

## Case conversion

```cpp
#include <cctype>
#include <algorithm>

std::transform(s.begin(), s.end(), s.begin(),
    [](unsigned char c) { return std::tolower(c); });   // ⚠️ unsigned char mein cast!
```

⚠️ `std::tolower(int)` ko seedha (shayad negative) `char` dena **UB** hai. Hamesha pehle
`static_cast<unsigned char>(c)` karo.

(Yeh sirf ASCII ke liye hai. Unicode case folding apne aap mein poori library ka kaam hai — file 08.)

---

## Jodna — `+` vs `+=` vs `append`

```cpp
std::string r = a + b + c;                  // kuch strings ke liye theek -- pehla + ek temp banata hai

// ❌ O(n^2) -- har iteration mein poori string dobara banti hai
for (auto& p : parts) r = r + p;

// ✅ O(n) -- amortized, wahin append
for (auto& p : parts) r += p;

// ✅ aur behtar -- ek hi allocation
std::string r;
r.reserve(totalSize);
for (auto& p : parts) r += p;
```

`a + b + c` mein kya hota hai: `a + b` ek naya temporary banata hai. Phir `temp + c` ke liye C++11
ka rvalue overload (`operator+(std::string&&, ...)`) **usi temporary mein append** karta hai — nayi
string nahi banti, par capacity kam padi to reallocation ho sakta hai. Loop wala `r = r + p` isliye
mehnga hai kyunki `r` ek lvalue hai — har baar poora `r` copy hota hai.

Structured building ke liye `std::format` (C++20) / string streams.

---

## `std::string` aur `<algorithm>` / `<ranges>`

`std::string` `char` ka container hai — zyada tar `<algorithm>` chalta hai:

```cpp
std::count(s.begin(), s.end(), 'a');
std::ranges::sort(s);
auto it = std::ranges::find(s, 'x');
std::erase(s, ' ');                          // C++20 -- saare spaces hatao
std::ranges::all_of(s, ::isdigit);          // (asal code mein unsigned-char cast ke saath)
```

---

## Andar kya hota hai

- `find` → seedha substring search (libstdc++ default mein Boyer-Moore nahi) — worst case O(n·m).
  Bade text mein baar-baar search karna ho to `std::boyer_moore_searcher`.
- `substr` → allocate + slice ka `memcpy`.
- `replace` alag length ke replacement ke saath → baaki hissa khiskana / reallocate ho sakta hai.
- `+` → ek temporary `std::string`; aage ke `+` rvalue overload se usi mein append (upar dekho).
- `+=` → wahin append; capacity khatam hone pe hi reallocate (~2x growth).

> **HFT relevance:** Parsers mein `std::string_view` ke operations lo (`substr`, `find`,
> `remove_prefix`) — woh sirf pointer arithmetic hain, allocation zero. Hot path pe
> `std::string::substr`/`+`/`replace` har ek allocator tak jaata hai. Fixed-format message se field
> nikaalna index ka hisaab hai, `find` nahi. Folder 38.

---

## Hands-on

`examples/02_std_string.cpp`, `examples/04_string_view.cpp` (zero-copy split / trim),
`examples/06_csv_parser.cpp`:

```bash
./build.ps1 10-STRINGS/examples/04_string_view.cpp
./build.ps1 10-STRINGS/examples/06_csv_parser.cpp
```

---

## ⚠️ Traps

### Trap 1 — `find` ka result `>= 0` se check
```cpp
if (s.find("x") >= 0) ...        // ⚠️ npos bahut bada unsigned -> hamesha true. == npos likho
```

### Trap 2 — range ke bahar `substr`
```cpp
s.substr(s.size() + 1);          // ⚠️ exception. s.substr(std::min(pos, s.size()))
```

### Trap 3 — negative `char` ke saath `tolower(char)`
```cpp
std::tolower(c);                 // ⚠️ c < 0 pe UB. std::tolower((unsigned char)c)
```

### Trap 4 — `replaceAll` ka infinite loop
```cpp
while ((p = s.find(from, p)) != npos) { s.replace(p, from.size(), to); }
// ⚠️ agar `to` mein `from` ho, ya p aage na badhe -> infinite. p += to.size();
```

### Trap 5 — loop mein `r = r + x`
```cpp
for (...) r = r + x;             // ⚠️ O(n^2). r += x;
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`find` na mile to `-1` deta hai" | `std::string::npos` (bahut bada unsigned) — `== npos` |
| "`substr` sasta view hai" | Copy allocate karta hai — view chahiye to `string_view` |
| "`std::string` mein `split` / `trim` hai" | Nahi — khud likho (`string_view` return karo) |
| "`char` ke liye `tolower(c)` theek hai" | Negative pe UB — `unsigned char` mein cast karo |
| "`a + b + c` mein har `+` nayi string banata hai" | Pehla `+` temp banata hai, baaki usi mein append (realloc ho sakta hai) — ek allocation chahiye to `reserve` + `+=` |

---

## Exercises

1. **find/rfind:** `"a.b.c.d"` mein — pehle `'.'` ka index, aakhri `'.'` ka, aur doosre `'.'` ka
   (position argument use karo).

2. **split → vector<string>:** implement karo, khaali fields sambhalo (`"a,,c"` → 3), aur shuru /
   aakhir mein delimiter.

3. **trim (string_view):** `trim("  \t hi \n ")` → `"hi"`. Sab whitespace → `""`. Ek char. Khaali.

4. **replaceAll:** `replaceAll("aaa", "a", "aa")` — `"aaaaaa"` aana chahiye, infinite nahi. `pos +=
   to.size()` kyun zaroori hai?

5. **toLower ASCII:** `std::string lower(std::string s)` — `unsigned char` cast ke saath. `"HeLLo123"`
   pe test karo.

6. **Efficiently banao:** `vector<string>` ko `", "` separator se jodo — naive `+` vs `reserve` + `+=`.
   100000 elements pe `-O2` time lo.

7. **`std::string` as container:** `std::count_if` se digits gino; `std::erase` se saare spaces hatao.

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
