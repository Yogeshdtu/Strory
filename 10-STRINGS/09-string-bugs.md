# 09 — String bugs

## Prerequisites
- [`01-c-strings.md`](01-c-strings.md) … [`08-unicode-and-encoding.md`](08-unicode-and-encoding.md)
- `09-ARRAYS/11-common-array-bugs.md`, `08-FUNCTIONS/06-scope-and-lifetime.md`

## Yeh topic abhi kyun
String bugs mostly **lifetime** (dangling views/pointers) aur **encoding**
(byte vs character) ke hain. Kai compile ho jaate hain aur "kabhi kabhi" fail
karte hain. Yeh unka catalogue hai.

---

## Bug 1 — Dangling `std::string_view` (#1 string bug)

```cpp
// (a) view of a temporary
std::string_view sv = std::string("x") + "y";        // ⚠️ temp gone after ;

// (b) return a view of a local
std::string_view name() { std::string s = "hi"; return s; }   // ⚠️

// (c) view outlives its source
std::string_view later;
{ std::string s = "block"; later = s; }               // ⚠️ s destroyed

// (d) string_view from a std::string, then mutate the string
std::string s = "hello"; std::string_view v = s; s += " world";   // ⚠️ realloc -> v dangles

// (e) std::string::substr -> temp std::string -> view dangles
std::string_view w = s.substr(0, 3);                  // ⚠️ (use std::string_view(s).substr(...))
```

`-Wdangling` (GCC 13+) catches some. ASan catches the rest at runtime (Linux;
MinGW: no ASan). **Rule: a view must not outlive its source.** Parameters: safe.
Stored/returned: reason about ownership.

---

## Bug 2 — Dangling `.c_str()` / `.data()`

```cpp
const char* p = getConfig().c_str();       // ⚠️ temp std::string destroyed -> p dangling
::open(p, O_RDONLY);

std::string s = "path";
const char* q = s.c_str();
s = "different much longer path value here";  // ⚠️ realloc -> q dangling
```

`.c_str()`/`.data()` return a pointer **into the string's current buffer**. Any
mutation that reallocates, or destroying the string, invalidates it. Keep the
`std::string` alive and unmodified while the pointer is in use.

---

## Bug 3 — Iterator / reference invalidation

```cpp
std::string s = "hello";
char& first = s[0];              // or auto it = s.begin();
s += " world";                   // ⚠️ reallocation -> `first` / `it` dangling
s.insert(0, ">>> ");             // ⚠️ same

for (auto it = s.begin(); it != s.end(); ++it)
    if (*it == ' ') s.erase(it); // ⚠️ erase invalidates it; ++it -> UB
```

Any operation that reallocates or shifts (`+=`, `insert`, `erase`, `resize`,
`replace` with different length) invalidates iterators/pointers/references past
the change point. `reserve` upfront, or re-fetch after; use `std::erase`/
`erase_if` (C++20) for removal.

---

## Bug 4 — `std::string_view` not null-terminated → C API

```cpp
std::string_view sv = std::string_view("hello world").substr(0, 5);  // "hello"
::printf("%s\n", sv.data());          // ⚠️ prints "hello world" (no '\0' after "hello")
::open(sv.data(), O_RDONLY);          // ⚠️ path is "hello world..."

::printf("%.*s\n", (int)sv.size(), sv.data());   // ✅ length-bounded
::open(std::string(sv).c_str(), O_RDONLY);       // ✅ owning null-terminated copy
```

---

## Bug 5 — `s.size() - 1` on empty (unsigned wrap)

```cpp
std::string s;
if (s.size() - 1 >= 0) { char last = s[s.size() - 1]; }   // ⚠️ 0 - 1 -> huge -> s[huge] UB
if (!s.empty()) { char last = s.back(); }                  // ✅
```

`size()` is unsigned. (Folder 06 file 07, folder 07 file 07.)

---

## Bug 6 — `find` result checked with `>= 0`

```cpp
if (s.find("x") >= 0) { ... }         // ⚠️ npos is huge unsigned -> always true
if (s.find("x") != std::string::npos) { ... }   // ✅
```

---

## Bug 7 — `+` chain / `= +` loop → O(n²)

```cpp
std::string out;
for (auto& part : parts) out = out + part + ",";   // ⚠️ rebuilds `out` every iteration

for (auto& part : parts) { out += part; out += ','; }   // ✅ O(n) amortized
out.reserve(total);  /* even better */
```

---

## Bug 8 — Encoding: byte vs character (file 08)

```cpp
std::string name = "José";                    // 5 bytes
if (name.size() > 4) name = name.substr(0, 4); // ⚠️ cuts 'é' in half -> invalid UTF-8
std::reverse(name.begin(), name.end());        // ⚠️ garbles multi-byte chars
```

Truncate / index only at known character boundaries.

---

## Bug 9 — `std::tolower` / `std::toupper` with a plain `char`

```cpp
for (char& c : s) c = std::toupper(c);        // ⚠️ UB if c < 0 (non-ASCII byte)
for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));  // ✅
```

`<cctype>` functions require an `int` that's a valid `unsigned char` or `EOF`.
Negative `char` → UB.

---

## Bug 10 — Comparing `std::string` to `const char*` when the `char*` may be null

```cpp
const char* p = maybeNull();
if (std::string(p) == "x") { ... }            // ⚠️ std::string(nullptr) -> UB
if (p && std::string_view(p) == "x") { ... }  // ✅
```

---

## Bug 11 — `getline` and the trailing `'\r'` (CRLF files)

```cpp
std::string line;
std::getline(in, line);                        // on a CRLF file: line ends with '\r'
if (!line.empty() && line.back() == '\r') line.pop_back();   // ✅ strip it
```

---

## Detection

| Bug | Tool |
|---|---|
| dangling view / `c_str` | `-Wdangling` (partial), ASan (Linux), lifetime lints |
| iterator invalidation | ASan, `_GLIBCXX_DEBUG` |
| unsigned wrap / `>= 0` on `find` | `-Wtype-limits` (sometimes), review |
| encoding | tests with non-ASCII data; UTF-8 validators |
| `tolower(char)` | `-Wall` doesn't catch — review / lint |
| `std::string(nullptr)` | ASan / UBSan; review |

> **HFT relevance:** In a zero-copy parser, the dangling-view bug (Bug 1/2) is
> the one that bites: a `std::string_view` field of a parsed record points into
> a receive buffer that gets recycled for the next packet → the record now reads
> the next message's bytes. Fixes: pin the buffer for the record's lifetime, or
> copy the field into an owning small string / arena. Encoding bugs are rare on
> the hot path (ASCII protocols) but matter for reference data. `-Wall -Wextra
> -Werror` + ASan/UBSan + fuzzing on parsers. Folder 38, 45.

---

## Hands-on

`examples/04_string_view.cpp` (5 dangling patterns), `examples/06_csv_parser.cpp`
(the "views into the source buffer" ownership note):

```bash
./build.ps1 10-STRINGS/examples/04_string_view.cpp
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`string_view` copies, so it's safe" | Non-owning — dangles if source dies |
| "`.c_str()` pointer is stable" | Invalidated by mutation / destruction |
| "`s.find(x) >= 0`" | `npos` is huge unsigned — `!= npos` |
| "`substr(0, n)` is UTF-8 safe" | Only at character boundaries |
| "`tolower(c)` on `char` is fine" | UB for negative — cast to `unsigned char` |

---

## Exercises

1. **Dangling repro:** write Bug 1(a)–(e). Which does `-Wdangling` catch? (Linux)
   run one under ASan.

2. **`c_str` lifetime:** `const char* p = std::string("x").c_str(); puts(p);` —
   `-Wall`? Behaviour? Fix two ways.

3. **Invalidation:** `std::string s = "a b c d e"; for (auto it = s.begin(); it
   != s.end(); ++it) if (*it == ' ') s.erase(it);` — bug? Fix with
   `std::erase(s, ' ')`.

4. **Non-null-terminated view → printf:** demonstrate the wrong output, fix with
   `%.*s`.

5. **Encoding truncate:** `"naïve"` truncated to "4 characters" — show the
   byte-offset version breaking it, then a code-point-safe version.

6. **`tolower` UB:** `std::string s = "café"; for (char& c : s) c = std::toupper(c);`
   — build with `-fsanitize=undefined` (Linux). What does UBSan say? Fix.

7. **CRLF:** read a file with `\r\n` line endings via `getline`; show the
   trailing `'\r'`; strip it.

---

## Interview questions

1. Dangling `std::string_view` — 3 ways it happens? Where is it safe?
2. `.c_str()` pointer kab invalidate hota hai?
3. `std::string` iterator/reference invalidation — kaunse operations?
4. `string_view.data()` C API ko dena kyun galat? Fix?
5. `s.find(x)` ka result kaise check karo (aur `>= 0` kyun galat)?
6. UTF-8 string ko truncate karna — safe kaise?

---

## Next
→ [`10-exercises.md`](10-exercises.md)
