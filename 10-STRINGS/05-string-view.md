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

Copy karna sasta (ptr + ek number). Destroy karne pe asli chars ko **kuch nahi** hota (maalik nahi hai).

Analogy: `string_view` ek **highlighter** hai — kitaab ke ek hisse ko mark karta hai, par kitaab uski
nahi. Kitaab (asli string) phenk di, to highlighter kisi khaali jagah ko mark kar raha hoga.

---

## Banana — kisi bhi char source se

```cpp
std::string_view a = "literal";              // string literal se
std::string s = "hello";
std::string_view b = s;                      // std::string se (copy nahi)
std::string_view c(s.data() + 1, 3);         // ptr + len -> "ell"
std::string_view d = s.substr(...);           // ⚠️ std::string::substr std::string lautata hai (copy)!
                                              //    view chahiye to std::string_view(s).substr(...)
const char* p = "world";
std::string_view e(p);                       // char* se (andar strlen chalta hai)
std::string_view f(p, 3);                    // "wor"  (strlen nahi)
```

---

## Function parameter — asli kaam

```cpp
std::size_t countChar(std::string_view s, char target) {
    std::size_t n = 0;
    for (char c : s) if (c == target) ++n;
    return n;
}

countChar("literal", 'l');           // koi temp std::string nahi
countChar(myString, 'l');            // copy nahi
countChar(myString.substr(0, 5), 'l'); // (yeh substr copy KARTA hai -- neeche dekho)
```

```cpp
// ❌ purana: literal / char* ke liye zabardasti std::string temp
void log(const std::string& msg);
log("hello");                        // "hello" -> temporary std::string (SSO se bada ho to allocation)

// ✅ naya
void log(std::string_view msg);
log("hello");                        // bas { ptr, len } -- na temp, na allocation
```

**Rule: read-only string parameter → `std::string_view`.** (`const std::string&` tabhi lo jab andar
khaas taur pe `.c_str()` / null-termination chahiye.)

---

## API — `std::string` jaisa, bas badlaav aur allocation ke bina

```cpp
sv.size()   sv.empty()   sv[i]   sv.front()   sv.back()   sv.data()
sv.find("x")   sv.rfind(...)   sv.starts_with(...)   sv.ends_with(...)   sv.contains(...)  // contains: C++23
sv.substr(pos, len)              // ✅ ek aur string_view lautata hai (allocation nahi!)
sv.remove_prefix(n)              // aage se chhota karo (ptr aage, len kam)
sv.remove_suffix(n)              // peeche se chhota karo
sv == other   sv < other        // content comparison
```

`std::string_view::substr` — **allocation nahi** (`std::string::substr` ke ulat). Isiliye zero-copy
parsers `string_view` use karte hain.

---

## 🔑 DANGLING — `string_view` ka #1 bug

`string_view` **data ka maalik nahi**. Use apne source se zyada nahi jeena chahiye.

```cpp
// (a) temporary ka view
std::string_view bad1 = std::string("x") + "y";   // ⚠️ ; ke baad temp khatam -> dangling

// (b) local ka view return karna
std::string_view f() {
    std::string local = "hi";
    return local;                                  // ⚠️ local khatam -> dangling
}

// (c) view string se zyada jee gaya
std::string_view later;
{ std::string s = "block"; later = s; }            // ⚠️ s gaya -> later dangling

// (d) std::string badalne se view invalid
std::string s = "hello";
std::string_view v = s;
s += " world";                                     // ⚠️ realloc ho sakta hai -> v dangling

// (e) std::string::substr STRING lautata hai, uska view dangle karta hai
std::string_view w = s.substr(0, 3);               // ⚠️ temp std::string -> w dangling
std::string_view w = std::string_view(s).substr(0, 3);  // ✅ s ke andar ka view
```

### Kahan safe hai
- **Function parameters** — caller ka data call ke dauraan zinda hai. Apne aap safe.
- **Local, turant use, source zinda** — theek.

### Kahan khatarnak hai
- **Store kiya** (struct member, container) — ab ownership ke baare mein sochna padega.
- **Return kiya** — sirf tab jab view kisi aisi cheez ka ho jiska maalik *caller* hai (ek argument,
  ek static, ek member).

---

## ⚠️ Null-terminated nahi hota

```cpp
std::string_view piece = std::string_view("hello world").substr(0, 5);  // "hello"
piece.data();                    // 'h' pe point karta hai -- par piece.data()[5] ' ' hai, '\0' NAHI

::open(piece.data(), ...);       // ⚠️ C API '\0' tak padhti hai -> " world..." bhi padh legi
::open(std::string(piece).c_str(), ...);   // ✅ owning, null-terminated copy banao
```

`string_view` bas `ptr + len` hai — terminator ki koi guarantee nahi. C APIs ke liye `std::string`
banao.

---

## `std::string_view` vs `const std::string&` vs `std::string`

| Parameter | Literal `"x"` do | `std::string` argument do | Copy? | Andar null-terminated? |
|---|---|---|---|---|
| `std::string_view` | free view | free view | nahi | ❌ |
| `const std::string&` | **temp `std::string` banta hai** | free | nahi (`std::string` arg ke liye) | ✅ |
| `std::string` (by value) | parameter seedha literal se banta hai (SSO se bada ho to allocation) | lvalue → copy, rvalue → move | haan (lvalue pe) | ✅ |

Default read-only parameter: **`std::string_view`**. Exception: andar `.c_str()` chahiye →
`const std::string&` (ya copy banao).

---

## Andar kya hota hai

- `std::string_view` = 2 words (`ptr`, `len`). Parameter kaise pahunchta hai woh ABI pe depend
  karta hai — Linux (SysV) pe 2 registers mein; Windows x64 pe (is course ki machine) 16-byte
  struct pointer ke through (`09-ARRAYS/07` mein `std::span` ke liye GCC 16.2 assembly se dekha,
  aur loop timing mein koi fark nahi mila).
- `sv[i]` → `ptr[i]`, ek scaled load.
- `substr` / `remove_prefix` / `remove_suffix` → `ptr` aur `len` pe sirf pointer arithmetic. Na
  allocation, na copy.
- `sv == other` → pehle length check, phir `memcmp`.
- `-O2` sab inline kar deta hai → `string_view` loop raw pointer loop jaisa hi code ban jaata hai.

> **HFT relevance:** HFT hot paths pe `std::string_view` hi *the* string type hai: fixed message ko
> `string_view` fields (symbol, side, tag) mein parse karo — zero allocations, zero copies, bas
> receive buffer mein offsets. Tokenizing ke liye `substr` / `remove_prefix`. Pakad dangling wala
> rule hai: agar parsed record buffer ki zindagi ke baad bhi *store* hua (jaise buffer recycle ho
> gaya), to un views ko owning copies banana padega, ya buffer ko record ki zindagi tak pakad ke
> rakhna padega. Yeh zero-copy parser ka classic bug hai. Folder 38.

---

## Hands-on

`examples/04_string_view.cpp` — ek function saare sources pe, sasta slicing, zero-copy split, 3
dangling traps + null-termination trap. `examples/06_csv_parser.cpp` — ek asli zero-copy parser:

```bash
./build.ps1 10-STRINGS/examples/04_string_view.cpp
./build.ps1 10-STRINGS/examples/06_csv_parser.cpp
```

---

## ⚠️ Traps

### Trap 1 — `std::string::substr` ka view banana
```cpp
std::string_view v = s.substr(0, 3);   // ⚠️ substr std::string banata hai -> v dangle
```

### Trap 2 — temporary ka view
```cpp
std::string_view v = getName() + "!";  // ⚠️ temp gaya
```

### Trap 3 — view ko source se zyada der store karna
```cpp
struct Rec { std::string_view name; };
Rec r{ parse(recvBuffer) };  recvBuffer.clear();   // ⚠️ r.name dangling
```

### Trap 4 — `.data()` C API ko dena
```cpp
printf("%s", sv.data());        // ⚠️ null-terminated nahi. printf("%.*s", (int)sv.size(), sv.data())
```

### Trap 5 — `std::string` se view banao, phir string badlo
```cpp
std::string_view v = s;  s += "x";   // ⚠️ reallocation -> v dangling
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`string_view` chars copy karta hai" | Non-owning view — 16 bytes |
| "`string_view` dangling se safe hai" | Sirf tab jab source zyada jeeye |
| "`string_view::substr` allocate karta hai" | Nahi — pointer math (`std::string::substr` ke ulat) |
| "`sv.data()` null-terminated hai" | Nahi — `ptr + len`, terminator nahi |
| "Read-only params ke liye `const std::string&` lo" | `std::string_view` — literals ke liye temp nahi banta |
| "`string_view` hamesha registers mein pass hota hai" | Linux pe haan; Windows x64 pe pointer ke through (09/07) |

---

## Exercises

1. **Ek function:** `bool isPalindrome(std::string_view)` — literal, `std::string`, aur view ka
   `substr`. Khaali aur ek char bhi sambhalo.

2. **Trim view:** `std::string_view trim(std::string_view)` — `find_first_not_of` /
   `find_last_not_of`. Allocation nahi. Sab-whitespace test karo.

3. **Zero-copy split:** `std::vector<std::string_view> split(std::string_view, char)` — input ke
   andar ke views. Check karo ki `.data()` pointers original ke andar hi pade hain.

4. **Dangling hunt:** paanchon dangling patterns likho. GCC `-Wall -Wextra` kaunse pakadta hai? (Clang ho
   to `-Wdangling` bhi try karo — GCC mein yeh flag hai hi nahi.) Ek ko (Linux) ASan ke neeche chalao.
   <details><summary>Answer (GCC wala hissa)</summary>

   GCC 16.2 `-Wall -Wextra` (`-O0` aur `-O2` dono) ne **paanchon mein se ek pe bhi** warning nahi di —
   chala ke dekha. `string_view` dangling ke liye compiler pe bharosa mat karo; ASan / review / design
   (views sirf parameters mein) hi asli bachaav hai.
   </details>

5. **`.data()` printf ko:** non-terminated `string_view` ko `%.*s` se sahi print karo.

6. **substr trap:** `std::string s = "hello world"; std::string_view v = s.substr(0, 5); std::cout
   << v;` — `-Wall` kuch kehta hai? Fix karo.

7. **Parameter refactor:** ek `void process(const std::string&)` lo jo bahut saare literals ke saath
   call hota ho. `std::string_view` mein badlo. Pehle/baad temp `std::string` allocations gino
   (`operator new` counter).

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
