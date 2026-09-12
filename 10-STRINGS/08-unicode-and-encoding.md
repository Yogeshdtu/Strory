# 08 — Unicode aur encoding

## Prerequisites
- [`01-c-strings.md`](01-c-strings.md), [`02-std-string-basics.md`](02-std-string-basics.md)
- `03-VARIABLES-DATA-TYPES/07-char-and-ascii.md`

## Yeh topic abhi kyun
`std::string` ke andar bytes hain, characters nahi. `"café".size()` `4` nahi, `5`
ho sakta hai (UTF-8 mein `é` = 2 bytes). Yeh confuse karta hai `size()`,
indexing, iteration, `substr` sab ko. Yeh lesson: **kya `std::string` guarantee
karta hai (bytes), aur "character" ka matlab kahaan tootta hai.**

---

## Character sets aur encodings

- **Unicode** — har character ko ek number (**code point**) deta hai. `U+0041` =
  'A', `U+00E9` = 'é', `U+1F600` = 😀. ~150,000 assigned.
- **Encoding** — code points ko bytes mein kaise likha jaaye:

| Encoding | Bytes per code point | Notes |
|---|---|---|
| **ASCII** | 1 (values 0–127 only) | Unicode ka pehla 128 subset |
| **UTF-8** | 1–4 | ASCII-compatible, self-synchronizing, **de facto standard** |
| **UTF-16** | 2 or 4 (surrogate pairs) | Windows APIs, Java, JS internal |
| **UTF-32** | 4 (fixed) | rare — 1 code point = 1 unit, but wasteful |
| Latin-1 etc. | 1 | legacy, region-specific |

**Modern: everything is UTF-8** (source files, files on disk, network, terminals).

---

## UTF-8 mechanics

```
   code point       bytes
   U+0000..U+007F   0xxxxxxx                          (1 byte -- same as ASCII)
   U+0080..U+07FF   110xxxxx 10xxxxxx                 (2 bytes)
   U+0800..U+FFFF   1110xxxx 10xxxxxx 10xxxxxx        (3 bytes)
   U+10000..        11110xxx 10xxxxxx 10xxxxxx 10xxxxxx (4 bytes)
```

- **ASCII text is valid UTF-8** unchanged.
- **Continuation bytes** start with `10` → you can tell mid-character from a
  start byte (self-synchronizing).
- `"café"` in UTF-8 = `63 61 66 C3 A9` → **5 bytes**, 4 code points.
- `"😀"` = `F0 9F 98 80` → **4 bytes**, 1 code point.

---

## `std::string` = bytes

```cpp
std::string s = "café";        // UTF-8 source -> 5 bytes

s.size()        // 5   -- BYTES, not characters
s[3]            // '\xC3'  -- a fragment of 'é', not a character
s.substr(0, 4)  // "caf\xC3"  -- cuts 'é' in half -> INVALID UTF-8

for (char c : s) { ... }       // iterates BYTES (5 iterations)
```

`std::string` / `char` know nothing about UTF-8. It's a byte container. Anything
"per character" needs decoding.

---

## Kya guarantee hai

| Operation | Byte-safe? | Character-safe? |
|---|---|---|
| `size()` | ✅ byte count | ❌ |
| `s == "ascii"` | ✅ (byte compare) | ✅ for ASCII |
| `s.find("ascii substring")` | ✅ (UTF-8 self-sync → no false matches for valid UTF-8) | ✅ |
| `s[i]`, `substr` at arbitrary `i` | ✅ | ❌ (may split a code point) |
| `for (char c : s)` | ✅ | ❌ (bytes) |
| `std::toupper` per byte | ✅ | ❌ (ASCII only) |
| reverse the bytes | ✅ | ❌ (garbles multi-byte chars) |
| concatenation | ✅ | ✅ (valid + valid = valid) |

**Rule: substring/index at boundaries you *know* are character starts (e.g. an
ASCII delimiter). Never split at an arbitrary byte offset.**

---

## `char8_t` / `u8""` (C++20)

```cpp
const char8_t* p = u8"café";        // guaranteed UTF-8, type char8_t
std::u8string s = u8"café";          // std::basic_string<char8_t>

// interop with std::string (char) is clunky in C++20 -- often just use char + "assume UTF-8"
std::string t(reinterpret_cast<const char*>(u8"café"));
```

`char8_t` marks "this is UTF-8" in the type system, but the ecosystem (I/O, most
libraries) still centers on `char`. Pragmatic approach: **`std::string` of
`char`, documented as UTF-8**.

### `wchar_t` / `char16_t` / `char32_t`
- `wchar_t` — 2 bytes on Windows (UTF-16), 4 on Linux (UTF-32). **Non-portable
  size** — avoid in portable code; needed for Win32 `W` APIs.
- `char16_t` (`u""`) → UTF-16, `char32_t` (`U""`) → UTF-32.

---

## "Length in characters" — needs a decoder

```cpp
// Count UTF-8 code points (assumes valid UTF-8): non-continuation bytes
std::size_t codepointCount(std::string_view s) {
    std::size_t n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80) ++n;      // not a 10xxxxxx continuation byte
    return n;
}
```

Even this counts **code points**, not "user-perceived characters" (grapheme
clusters — e.g. `é` as `e` + combining accent, or flag emoji = 2 code points).
Real Unicode handling → a library: **ICU**, `utf8cpp`, `{fmt}`/`std::format` for
output width, C++ has no full Unicode support in the standard library.

---

## I/O and the terminal

- Source file encoding: save as UTF-8 (compilers assume it / accept `-finput-charset`).
- `std::cout << utf8String` — writes bytes; the terminal decides how to render.
  Windows console historically needs `SetConsoleOutputCP(CP_UTF8)` or a modern
  terminal.
- File I/O: `std::string` bytes go to disk as-is. No transcoding by default.

---

## Andar kya hota hai

- `std::string` stores `char` bytes contiguously. No encoding metadata.
- `s[i]` → the i-th **byte**. `s.size()` → byte count.
- Comparisons / `find` are `memcmp`/`memchr` on bytes. Valid UTF-8's
  self-synchronization means an ASCII (or valid UTF-8) needle can't match across
  a character boundary falsely.
- Decoding to code points is an explicit loop over the leading-byte patterns.

> **HFT relevance:** Wire protocols in HFT are **binary or ASCII** — FIX, ITCH,
> OUCH, exchange native protocols use ASCII fields and fixed-width numbers. UTF-8
> multi-byte handling basically doesn't appear on the hot path. It matters for
> logs, UIs, and reference-data ingestion (instrument names, descriptions) —
> where you treat `std::string` as opaque UTF-8 bytes, never index into the
> middle, and use a library if you need real character operations. Don't reverse
> or truncate-at-byte-offset a name field.

---

## Hands-on

```cpp
#include <iostream>
#include <string>
int main() {
    std::string s = "café 😀";
    std::cout << "bytes (size): " << s.size() << "\n";
    std::size_t cp = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++cp;
    std::cout << "code points:  " << cp << "\n";
    for (unsigned char c : s) std::cout << std::hex << (int)c << " ";
    std::cout << "\n";
}
```

```bash
g++ -std=c++20 -Wall -Wextra utf8.cpp -o u && ./u
```

---

## ⚠️ Traps

### Trap 1 — `size()` as character count
```cpp
if (name.size() > 20) truncate(name, 20);   // ⚠️ may cut a multi-byte char in half
```

### Trap 2 — `substr` / index at arbitrary offset
```cpp
s.substr(0, 10);            // ⚠️ byte 10 might be mid-character -> invalid UTF-8
```

### Trap 3 — `std::reverse` on UTF-8
```cpp
std::reverse(s.begin(), s.end());   // ⚠️ garbles every multi-byte character
```

### Trap 4 — `std::toupper` per byte for non-ASCII
```cpp
for (char& c : s) c = std::toupper((unsigned char)c);   // ⚠️ only ASCII; may corrupt bytes
```

### Trap 5 — `wchar_t` for portable "wide" strings
```cpp
std::wstring w = L"...";   // ⚠️ 2 bytes/char Windows, 4 Linux -- not portable. char + UTF-8
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`s.size()` = number of characters" | Byte count |
| "`s[i]` = i-th character" | i-th byte (may be a fragment) |
| "UTF-8 needs `wchar_t`" | UTF-8 fits in `char` / `std::string` |
| "`substr(0, n)` is safe" | Only at known character boundaries |
| "C++ has Unicode support" | Barely — use ICU / a library for real work |

---

## Exercises

1. **Byte vs code point:** `"héllo wörld 🌍"` — `size()` and a code-point count.
   Print every byte in hex; identify the multi-byte sequences.

2. **Safe truncate:** `std::string truncateCodepoints(std::string_view, size_t
   maxCP)` — cut at a code-point boundary, never mid-character.

3. **Validate UTF-8:** `bool isValidUtf8(std::string_view)` — check leading /
   continuation byte patterns. Test with a deliberately broken sequence.

4. **ASCII split still works:** split `"café,über,naïve"` on `','` (an ASCII
   byte) with `string_view::find` — do the multi-byte chars survive intact?

5. **Reverse damage:** `std::reverse` a string containing `é`. Print bytes
   before/after. Now write a code-point-aware reverse.

6. **`char8_t` interop:** `u8"text"` → `std::string`. What's the cast? Why is it
   awkward in C++20?

---

## Interview questions

1. `std::string` stores characters ya bytes? `"café".size()` kya?
2. UTF-8 kya hai — ASCII se kaise compatible, self-synchronizing ka matlab?
3. `s[i]` / `substr` UTF-8 pe safe hai? Kab?
4. `wchar_t` ka size portable hai?
5. Character count kaise nikaalo — aur "code point" vs "grapheme"?
6. Kaunse `std::string` operations UTF-8-safe hain, kaunse nahi?

---

## Next
→ [`09-string-bugs.md`](09-string-bugs.md)
