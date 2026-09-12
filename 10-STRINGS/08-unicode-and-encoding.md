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

- **Unicode** — har character ko ek number (**code point**) deta hai. `U+0041` = 'A', `U+00E9` = 'é',
  `U+1F600` = 😀. ~1.5 lakh se zyada characters assigned hain.
- **Encoding** — un code points ko bytes mein kaise likhein:

| Encoding | Ek code point = kitne bytes | Notes |
|---|---|---|
| **ASCII** | 1 (sirf values 0–127) | Unicode ka pehla 128 wala hissa |
| **UTF-8** | 1–4 | ASCII-compatible, self-synchronizing, **asal mein standard yahi hai** |
| **UTF-16** | 2 ya 4 (surrogate pairs) | Windows APIs, Java, JS ke andar |
| **UTF-32** | 4 (fixed) | kam dikhta hai — 1 code point = 1 unit, par jagah bahut khaata hai |
| Latin-1 wagairah | 1 | purane, region-specific |

**Aaj: sab kuch UTF-8 hai** (source files, disk ki files, network, terminals).

Analogy: Unicode ek badi phone directory hai jisme har character ka ek number hai. Encoding yeh tay
karti hai ki woh number kaagaz pe kitne ankon mein likha jaaye — chhote numbers kam jagah mein, bade
numbers zyada jagah mein (UTF-8).

---

## UTF-8 kaise kaam karta hai

```
   code point       bytes
   U+0000..U+007F   0xxxxxxx                          (1 byte -- ASCII jaisa hi)
   U+0080..U+07FF   110xxxxx 10xxxxxx                 (2 bytes)
   U+0800..U+FFFF   1110xxxx 10xxxxxx 10xxxxxx        (3 bytes)
   U+10000..        11110xxx 10xxxxxx 10xxxxxx 10xxxxxx (4 bytes)
```

- **ASCII text bina badle valid UTF-8 hai.**
- **Continuation bytes** `10` se shuru hote hain → byte dekh ke bata sakte ho ki yeh character ke
  beech ka hissa hai ya shuruaat (self-synchronizing).
- UTF-8 mein `"café"` = `63 61 66 C3 A9` → **5 bytes**, 4 code points.
- `"😀"` = `F0 9F 98 80` → **4 bytes**, 1 code point.

---

## `std::string` = bytes

```cpp
std::string s = "café";        // UTF-8 source -> 5 bytes

s.size()        // 5   -- BYTES, characters nahi
s[3]            // '\xC3'  -- 'é' ka ek tukda, poora character nahi
s.substr(0, 4)  // "caf\xC3"  -- 'é' ko beech se kaat diya -> INVALID UTF-8

for (char c : s) { ... }       // BYTES pe chalta hai (5 iterations)
```

`std::string` / `char` ko UTF-8 ka kuch pata nahi. Woh bas bytes ka container hai. "Har character pe"
kuch karna ho to decode karna padega.

---

## Kya guarantee hai

| Operation | Bytes ke liye safe? | Characters ke liye safe? |
|---|---|---|
| `size()` | ✅ byte count | ❌ |
| `s == "ascii"` | ✅ (byte compare) | ✅ ASCII ke liye |
| `s.find("ascii substring")` | ✅ (UTF-8 self-sync → valid UTF-8 pe galat match nahi) | ✅ |
| kisi bhi `i` pe `s[i]`, `substr` | ✅ | ❌ (code point beech se kat sakta hai) |
| `for (char c : s)` | ✅ | ❌ (bytes) |
| har byte pe `std::toupper` | ✅ | ❌ (sirf ASCII) |
| bytes ulte karna | ✅ | ❌ (multi-byte chars bigad jaate hain) |
| jodna (concatenation) | ✅ | ✅ (valid + valid = valid) |

**Rule: substring/index sirf un jagahon pe jahan aapko *pata* hai ki character shuru hota hai (jaise
ek ASCII delimiter). Kisi bhi random byte offset pe kabhi mat kaato.**

---

## `char8_t` / `u8""` (C++20)

```cpp
const char8_t* p = u8"café";        // UTF-8 ki guarantee, type char8_t
std::u8string s = u8"café";          // std::basic_string<char8_t>

// C++20 mein std::string (char) ke saath milana bhaari hai -- aksar bas char + "UTF-8 maan lo"
std::string t(reinterpret_cast<const char*>(u8"café"));
```

`char8_t` type system mein "yeh UTF-8 hai" likh deta hai, par ecosystem (I/O, zyada tar libraries)
abhi bhi `char` pe chalta hai. Practical tareeka: **`char` ki `std::string`, aur documentation mein
likho ki UTF-8 hai**.

### `wchar_t` / `char16_t` / `char32_t`
- `wchar_t` — Windows pe 2 bytes (UTF-16), Linux pe 4 (UTF-32). **Size portable nahi** — portable
  code mein bacho; Win32 ki `W` APIs ke liye zaroori.
- `char16_t` (`u""`) → UTF-16, `char32_t` (`U""`) → UTF-32.

---

## "Kitne characters?" — decoder chahiye

```cpp
// UTF-8 code points gino (valid UTF-8 maan ke): jo continuation byte nahi hain
std::size_t codepointCount(std::string_view s) {
    std::size_t n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80) ++n;      // 10xxxxxx continuation byte nahi hai
    return n;
}
```

Yeh bhi **code points** ginta hai, "insaan ko dikhne wale characters" nahi (grapheme clusters — jaise
`é` ko `e` + combining accent se likhna, ya flag emoji = 2 code points). Asli Unicode kaam → library:
**ICU**, `utf8cpp`, output width ke liye `{fmt}`/`std::format`. C++ standard library mein poora
Unicode support nahi hai.

---

## I/O aur terminal

- Source file ki encoding: UTF-8 mein save karo (compilers yahi maante hain / `-finput-charset` lete hain).
- `std::cout << utf8String` — bytes likhta hai; dikhana kaise hai woh terminal tay karta hai. Windows
  console ko purane zamane se `SetConsoleOutputCP(CP_UTF8)` ya modern terminal chahiye hota hai.
- File I/O: `std::string` ke bytes disk pe jaise ke taise. Default mein koi transcoding nahi.

---

## Andar kya hota hai

- `std::string` `char` bytes ek saath (contiguous) rakhta hai. Encoding ki koi jaankari nahi.
- `s[i]` → i-th **byte**. `s.size()` → byte count.
- Comparisons / `find` bytes pe `memcmp`/`memchr` hain. Valid UTF-8 self-synchronizing hai, isliye ASCII
  (ya valid UTF-8) needle character ki boundary ke aar-paar galat match nahi kar sakti.
- Code points mein decode karna ek explicit loop hai jo leading-byte patterns dekhta hai.

> **HFT relevance:** HFT ke wire protocols **binary ya ASCII** hote hain — FIX, ITCH, OUCH, exchange ke
> native protocols ASCII fields aur fixed-width numbers use karte hain. Hot path pe UTF-8 multi-byte
> handling lagbhag aati hi nahi. Yeh logs, UIs, aur reference-data ingestion (instrument names,
> descriptions) mein matter karta hai — wahan `std::string` ko opaque UTF-8 bytes maano, beech mein
> index mat karo, aur asli character operations chahiye to library lo. Kisi name field ko ulta ya
> byte offset pe truncate mat karo.

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

GCC 16.2 pe (source UTF-8 mein save) asli output:
```
bytes (size): 10
code points:  6
63 61 66 c3 a9 20 f0 9f 98 80
```
`c a f` (3) + `é` (`c3 a9`, 2) + space (1) + `😀` (`f0 9f 98 80`, 4) = 10 bytes, 6 code points.

---

## ⚠️ Traps

### Trap 1 — `size()` ko character count samajhna
```cpp
if (name.size() > 20) truncate(name, 20);   // ⚠️ multi-byte char beech se kat sakta hai
```

### Trap 2 — kisi bhi offset pe `substr` / index
```cpp
s.substr(0, 10);            // ⚠️ byte 10 character ke beech ho sakta hai -> invalid UTF-8
```

### Trap 3 — UTF-8 pe `std::reverse`
```cpp
std::reverse(s.begin(), s.end());   // ⚠️ har multi-byte character bigad jaata hai
```

### Trap 4 — non-ASCII pe har byte ka `std::toupper`
```cpp
for (char& c : s) c = std::toupper((unsigned char)c);   // ⚠️ sirf ASCII; bytes bigad sakte hain
```

### Trap 5 — portable "wide" strings ke liye `wchar_t`
```cpp
std::wstring w = L"...";   // ⚠️ Windows pe 2 bytes/char, Linux pe 4 -- portable nahi. char + UTF-8 lo
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`s.size()` = kitne characters" | Byte count |
| "`s[i]` = i-th character" | i-th byte (tukda ho sakta hai) |
| "UTF-8 ke liye `wchar_t` chahiye" | UTF-8 `char` / `std::string` mein aa jaata hai |
| "`substr(0, n)` safe hai" | Sirf pata ho ki wahan character shuru hota hai |
| "C++ mein Unicode support hai" | Na ke barabar — asli kaam ke liye ICU / library |

---

## Exercises

1. **Byte vs code point:** `"héllo wörld 🌍"` — `size()` aur code-point count. Har byte hex mein print
   karo; multi-byte sequences pehchano.

2. **Safe truncate:** `std::string truncateCodepoints(std::string_view, size_t maxCP)` — code-point ki
   boundary pe kaato, kabhi character ke beech nahi.

3. **UTF-8 validate:** `bool isValidUtf8(std::string_view)` — leading / continuation byte patterns check
   karo. Jaan-boojh kar tooti hui sequence se test karo.

4. **ASCII split ab bhi chalta hai:** `"café,über,naïve"` ko `','` (ek ASCII byte) pe `string_view::find`
   se split karo — kya multi-byte chars sahi-salamat bache?

5. **Reverse ka nuksaan:** `é` wali string pe `std::reverse`. Pehle/baad bytes print karo. Ab code-point
   samajhne wala reverse likho.

6. **`char8_t` interop:** `u8"text"` → `std::string`. Cast kya lagega? C++20 mein yeh bhaari kyun hai?

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
