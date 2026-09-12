# 02 — `std::string` basics

## Prerequisites
- [`01-c-strings.md`](01-c-strings.md)
- `08-FUNCTIONS/04-return-values.md` (value semantics, RVO)

## Yeh topic abhi kyun
`std::string` C-string ke saare dangers khatam kar deta hai: size tracked,
auto-grow, bounds-checked `.at()`, content `==`, embedded `'\0'` allowed. Cost:
ek heap allocation (chhoti strings ke liye SSO — file 04). Yeh text ka default
type hai.

---

## Construction

```cpp
#include <string>

std::string a = "hello";
std::string b(5, 'x');            // "xxxxx"
std::string c = a + " world";     // concatenation
std::string d(a, 1, 3);           // substring: "ell"  (index 1, length 3)
std::string e{other};             // copy
std::string f = std::move(other); // move -- other ka data steal (other empty ho jaata hai)

using namespace std::string_literals;
std::string g = "a\0b"s;          // s-suffix -> length 3, embedded '\0' OK
```

---

## Size / capacity / empty

```cpp
s.size()          // number of chars  (== s.length())
s.empty()         // s.size() == 0
s.capacity()      // allocated space -- size se >= (growth headroom)
s.max_size()      // theoretical limit
s.shrink_to_fit() // capacity ko size ke kareeb laao (non-binding request)
```

`size()` **O(1)** — `std::string` length store karta hai (C-string ke `strlen`
scan ke ulat).

⚠️ `size()` returns `std::string::size_type` (= `std::size_t`, **unsigned**).
`s.size() - 1` on empty → wrap (folder 06 file 07). Use `if (!s.empty())`.

---

## Indexing

```cpp
s[i]              // unchecked -- UB if i > s.size()
s.at(i)           // checked -- throws std::out_of_range
s.front()         // s[0]
s.back()          // s[s.size() - 1]
s[s.size()]       // '\0'  -- valid to READ (C++11+), NOT to write
s.data()          // char*  -- null-terminated (C++11+ for const, C++17 for non-const)
s.c_str()         // const char*  -- null-terminated, for C APIs
```

---

## Modifying — grows automatically

```cpp
s += "x";  s += 'y';                    // append
s.append("more");  s.push_back('!');
s.insert(0, ">> ");                     // insert at position
s.replace(0, 3, "== ");                // replace [0,3) with "== "
s.erase(2, 4);                          // remove 4 chars from index 2
s.pop_back();                           // remove last char
s.clear();                             // size -> 0  (capacity RETAINED -- memory not freed)
s.resize(10, ' ');                     // grow/shrink, pad with ' '
s.assign("new content");
```

`clear()` sets size to 0 but keeps the buffer — reuse without re-allocating.

---

## Comparison — content, correctly

```cpp
std::string a = "apple", b = "apple";
a == b                                  // ✅ true  (content, not pointers)
a < b                                   // lexicographic
a <=> b                                 // C++20 three-way
a == "apple"                            // ✅ compares to a literal
a.compare(b)                            // <0 / 0 / >0  (like strcmp)
a.starts_with("app")   a.ends_with("le")   a.contains("ppl")   // C++20/C++23
```

---

## Iteration

```cpp
for (char c : s) { ... }                // range-for (bytes -- careful with UTF-8, file 08)
for (char& c : s) c = std::toupper((unsigned char)c);   // modify
for (std::size_t i = 0; i < s.size(); ++i) { ... }       // indexed
for (auto it = s.begin(); it != s.end(); ++it) { ... }   // iterators
```

---

## C interop — `.c_str()` / `.data()`

```cpp
std::string path = "/tmp/f";
::open(path.c_str(), O_RDONLY);         // C API needs null-terminated char*
```

⚠️ **`.c_str()` / `.data()` ka pointer temporary hai** — `path` modify ya destroy
hote hi dangling (file 09):
```cpp
const char* p = getString().c_str();   // ⚠️ temporary destroyed -> p dangling
```

---

## Andar kya hota hai (libstdc++)

```
sizeof(std::string) == 32:
  char*  _M_p               // pointer to the char data (SSO: -> _M_local_buf)
  size_t _M_string_length   // length (O(1) size())
  union {
    char   _M_local_buf[16] // SSO: 15 chars + '\0' inline
    size_t _M_allocated_capacity
  }
```

- **Short string (≤ 15 chars)**: data lives in `_M_local_buf` — **no heap
  allocation** (file 04).
- **Longer**: `_M_p` points to a heap block; capacity in the union.
- `size()` → return `_M_string_length` (constant).
- `+=` / `append` → if it fits in capacity, `memcpy`; else reallocate (grow
  ~2x — file 04, 07).
- Copy → allocates + copies the data (SSO strings: just a memcpy of 32 bytes).
- Move → steal the pointer, leave source empty (SSO strings: copy the 32 bytes).

> **HFT relevance:** `std::string` is fine for cold/setup code and short keys
> (SSO → allocation-free). On hot paths it's used carefully: `reserve()` once and
> reuse; pass as `std::string_view` (no copy); parse into `std::string_view`
> fields (file 05, 07). A stray `std::string` copy or `operator+` chain in a
> decode loop = per-message heap traffic. Folders 36, 38.

---

## Hands-on

`examples/02_std_string.cpp` — construction, size/capacity, `.at()`, modify,
iterate, `c_str()`:

```bash
./build.ps1 10-STRINGS/examples/02_std_string.cpp
```

---

## ⚠️ Traps

### Trap 1 — `s.size() - 1` on empty
```cpp
if (s.size() - 1 >= 0) ...    // ⚠️ unsigned wrap on empty. if (!s.empty())
```

### Trap 2 — `.c_str()` dangling
```cpp
const char* p = build().c_str();   // ⚠️ temp gone. Keep the std::string alive
```

### Trap 3 — `s[s.size()]` write
```cpp
s[s.size()] = 'x';    // ⚠️ writing the '\0' slot -- UB. s.push_back('x')
```

### Trap 4 — `clear()` frees memory
```cpp
s.clear();            // size 0, capacity UNCHANGED. shrink_to_fit() to release
```

### Trap 5 — `+` chain in a loop
```cpp
for (auto& part : parts) result = result + part + ",";   // ⚠️ O(n^2) copies
for (auto& part : parts) { result += part; result += ','; }   // ✅
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`s.size()` scans like `strlen`" | O(1) — length is stored |
| "`s == other` compares pointers" | Content (unlike `const char*`) |
| "`s.clear()` releases memory" | Size 0, capacity kept |
| "`s[s.size()]` is out of bounds" | Reading `'\0'` is OK; writing is UB |
| "Every `std::string` heap-allocates" | Short strings use SSO (file 04) |

---

## Exercises

1. **API tour:** build `"[ hello , world ]"` using `+=`, `append`, `insert`.
   Then `replace`, `erase`, `substr` it apart.

2. **size vs capacity:** `std::string s; for (int i=0;i<100;++i) { s+='x';
   std::cout << s.size() << "/" << s.capacity() << "\n"; }` — capacity growth
   pattern?

3. **`.at()` throw:** `s.at(1000)` in a `try`/`catch`. Message?

4. **Move vs copy:** `std::string a = "hello world this is long"; std::string b =
   std::move(a);` — `a.size()` after? `a.empty()`?

5. **`+` chain O(n²):** join 10000 `"x"` strings with `result = result + "x"` vs
   `result += "x"`. `-O2`, time. Ratio?

6. **`c_str` lifetime:** `const char* p = std::string("temp").c_str(); std::cout
   << p;` — `-Wall`? Run — garbage? Fix.

---

## Interview questions

1. `std::string` vs C-string — 4 safety improvements?
2. `s.size()` complexity? Kyun (C-string se alag)?
3. `s.clear()` memory ka kya karta hai?
4. `s[s.size()]` — read? write?
5. `.c_str()` ka lifetime — kab dangling?
6. Move vs copy of a `std::string` — kya hota hai?

---

## Next
→ [`03-string-operations.md`](03-string-operations.md)
