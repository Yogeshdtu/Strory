# 05 — `std::string_view` (C++17)

## Prerequisites
- [`02-std-string-basics.md`](02-std-string-basics.md), [`04-string-internals-sso.md`](04-string-internals-sso.md)
- `09-ARRAYS/09-std-span.md` (`std::span` — same idea for arrays)
- `08-FUNCTIONS/06-scope-and-lifetime.md` (dangling)

## Yeh topic abhi kyun
`std::string_view` = `{ const char* ptr; size_t len; }` — ek **non-owning,
read-only view** into char data. No copy, no allocation. Yeh `std::string` ka
`std::span<const char>` equivalent hai, aur **read-only string parameters** ke
liye best type. Par dangling ka dhyaan rakhna zaroori hai.

---

## Kya hai

```cpp
#include <string_view>

std::string_view sv;              // { const char* ; size_t }  -- 16 bytes
```

Copy karna sasta (ptr + int). Destroy karne pe underlying chars ko **kuch nahi**
hota (own nahi karta).

---

## Banana — kisi bhi char source se

```cpp
std::string_view a = "literal";              // from string literal
std::string s = "hello";
std::string_view b = s;                      // from std::string (no copy)
std::string_view c(s.data() + 1, 3);         // ptr + len -> "ell"
std::string_view d = s.substr(...);           // ⚠️ std::string::substr returns std::string (copy)!
                                              //    use std::string_view(s).substr(...) for a view
const char* p = "world";
std::string_view e(p);                       // from char* (calls strlen)
std::string_view f(p, 3);                    // "wor"  (no strlen)
```

---

## Function parameter — the point

```cpp
std::size_t countChar(std::string_view s, char target) {
    std::size_t n = 0;
    for (char c : s) if (c == target) ++n;
    return n;
}

countChar("literal", 'l');           // no temp std::string
countChar(myString, 'l');            // no copy
countChar(myString.substr(0, 5), 'l'); // (this substr DOES copy -- see below)
```

```cpp
// ❌ old: forces a std::string temp for a literal / char*
void log(const std::string& msg);
log("hello");                        // "hello" -> temporary std::string (allocation if > SSO)

// ✅ new
void log(std::string_view msg);
log("hello");                        // just a { ptr, len } -- no temp, no allocation
```

**Rule: read-only string parameter → `std::string_view`.** (Take `const
std::string&` only if you specifically need `.c_str()` / null-termination inside.)

---

## API — like `std::string`, minus mutation & allocation

```cpp
sv.size()   sv.empty()   sv[i]   sv.front()   sv.back()   sv.data()
sv.find("x")   sv.rfind(...)   sv.starts_with(...)   sv.ends_with(...)   sv.contains(...)
sv.substr(pos, len)              // ✅ returns another string_view (no allocation!)
sv.remove_prefix(n)              // shrink from the front (just moves ptr, bumps len down)
sv.remove_suffix(n)              // shrink from the back
sv == other   sv < other        // content comparison
```

`std::string_view::substr` — **no allocation** (unlike `std::string::substr`).
This is why zero-copy parsers use `string_view`.

---

## 🔑 DANGLING — `string_view` ka #1 bug

`string_view` **data ko own nahi karta**. It must not outlive its source.

```cpp
// (a) view into a temporary
std::string_view bad1 = std::string("x") + "y";   // ⚠️ temp destroyed after ; -> dangling

// (b) return a view of a local
std::string_view f() {
    std::string local = "hi";
    return local;                                  // ⚠️ local destroyed -> dangling
}

// (c) view outlives the string
std::string_view later;
{ std::string s = "block"; later = s; }            // ⚠️ s gone -> later dangling

// (d) std::string mutation invalidates the view
std::string s = "hello";
std::string_view v = s;
s += " world";                                     // ⚠️ may reallocate -> v dangling

// (e) std::string::substr returns a STRING, view into it dangles
std::string_view w = s.substr(0, 3);               // ⚠️ temp std::string -> w dangling
std::string_view w = std::string_view(s).substr(0, 3);  // ✅ view into s
```

### Where it's safe
- **Function parameters** — the caller's data is alive for the call. Trivially safe.
- **Local, used immediately, source alive** — fine.

### Where it's dangerous
- **Stored** (struct member, container) — now you must reason about ownership.
- **Returned** — only if returning a view of something the *caller* owns
  (an argument, a static, a member).

---

## ⚠️ Not null-terminated

```cpp
std::string_view piece = std::string_view("hello world").substr(0, 5);  // "hello"
piece.data();                    // points to 'h' -- but piece.data()[5] is ' ', NOT '\0'

::open(piece.data(), ...);       // ⚠️ C API reads until '\0' -> reads " world..." too
::open(std::string(piece).c_str(), ...);   // ✅ make an owning, null-terminated copy
```

`string_view` is just `ptr + len` — there's no terminator guarantee. For C APIs,
materialize a `std::string`.

---

## `std::string_view` vs `const std::string&` vs `std::string`

| Parameter | Literal `"x"` | `std::string` arg | Copy? | Null-term inside? |
|---|---|---|---|---|
| `std::string_view` | free view | free view | no | ❌ |
| `const std::string&` | **temp `std::string`** | free | no (for `std::string` arg) | ✅ |
| `std::string` (by value) | temp + move | copy or move | yes-ish | ✅ |

Default read-only param: **`std::string_view`**. Exception: you call `.c_str()`
inside → `const std::string&` (or make a copy).

---

## Andar kya hota hai

- `std::string_view` = 2 words (`ptr`, `len`), passed in 2 registers — zero
  overhead vs `const char*` + separate `size_t`.
- `sv[i]` → `ptr[i]`, a scaled load.
- `substr` / `remove_prefix` / `remove_suffix` → pure pointer arithmetic on
  `ptr` and `len`. No allocation, no copy.
- `sv == other` → `memcmp` after a length check.
- `-O2` inlines all of it → a `string_view` loop compiles to the same code as a
  raw pointer loop.

> **HFT relevance:** `std::string_view` is *the* string type on HFT hot paths:
> parse a fixed message into `string_view` fields (symbol, side, tag) — zero
> allocations, zero copies, just offsets into the receive buffer. `substr` /
> `remove_prefix` for tokenizing. The dangling rule is the catch: if a parsed
> record is *stored* past the buffer's life (e.g. the buffer is recycled), those
> views must become owning copies, or the buffer must be pinned for the record's
> lifetime. This is a classic zero-copy-parser bug. Folder 38.

---

## Hands-on

`examples/04_string_view.cpp` — one function all sources, cheap slicing,
zero-copy split, 5 dangling traps. `examples/06_csv_parser.cpp` — a real
zero-copy parser:

```bash
./build.ps1 10-STRINGS/examples/04_string_view.cpp
./build.ps1 10-STRINGS/examples/06_csv_parser.cpp
```

---

## ⚠️ Traps

### Trap 1 — `std::string::substr` into a view
```cpp
std::string_view v = s.substr(0, 3);   // ⚠️ substr makes a std::string -> v dangles
```

### Trap 2 — view of a temporary
```cpp
std::string_view v = getName() + "!";  // ⚠️ temp gone
```

### Trap 3 — storing a view past its source
```cpp
struct Rec { std::string_view name; };
Rec r{ parse(recvBuffer) };  recvBuffer.clear();   // ⚠️ r.name dangling
```

### Trap 4 — `.data()` to a C API
```cpp
printf("%s", sv.data());        // ⚠️ not null-terminated. printf("%.*s", (int)sv.size(), sv.data())
```

### Trap 5 — `string_view` from `std::string` then mutate the string
```cpp
std::string_view v = s;  s += "x";   // ⚠️ reallocation -> v dangling
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`string_view` copies the chars" | Non-owning view — 16 bytes |
| "`string_view` is safe against dangling" | Only if source outlives it |
| "`string_view::substr` allocates" | No — pointer math (unlike `std::string::substr`) |
| "`sv.data()` is null-terminated" | No — `ptr + len`, no terminator |
| "Take `const std::string&` for read-only params" | `std::string_view` — no temp for literals |

---

## Exercises

1. **One function:** `bool isPalindrome(std::string_view)` — literal,
   `std::string`, `substr` of a view. Handle empty, single char.

2. **Trim view:** `std::string_view trim(std::string_view)` — `find_first_not_of`
   / `find_last_not_of`. No allocation. Test all-whitespace.

3. **Zero-copy split:** `std::vector<std::string_view> split(std::string_view,
   char)` — views into the input. Verify `.data()` pointers land inside the
   original.

4. **Dangling hunt:** write each of the 5 dangling patterns. Which does `-Wall` /
   `-Wdangling` catch? Run one under (Linux) ASan.

5. **`.data()` to printf:** print a non-terminated `string_view` correctly with
   `%.*s`.

6. **substr trap:** `std::string s = "hello world"; std::string_view v =
   s.substr(0, 5); std::cout << v;` — `-Wall`? Fix.

7. **Parameter refactor:** take a function `void process(const std::string&)`
   called with lots of literals. Change to `std::string_view`. Measure temp
   `std::string` allocations before/after (`operator new` counter).

---

## Interview questions

1. `std::string_view` — contents, size, ownership?
2. `std::string_view` parameter vs `const std::string&` — literal ke saath kya farq?
3. `string_view::substr` vs `std::string::substr` — allocation?
4. `string_view` kab dangle karta hai — 3 patterns?
5. `sv.data()` C API ko dena kyun galat?
6. Parsed records ko store karna hai jo `string_view` fields rakhte hain — kya karo?

---

## Next
→ [`06-string-conversions.md`](06-string-conversions.md)
