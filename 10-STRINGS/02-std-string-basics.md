# 02 — `std::string` basics

## Prerequisites
- [`01-c-strings.md`](01-c-strings.md)
- `08-FUNCTIONS/04-return-values.md` (value semantics, RVO)

## Yeh topic abhi kyun
`std::string` C-string ke saare dangers khatam kar deta hai: size tracked,
auto-grow, bounds-checked `.at()`, content `==`, embedded `'\0'` allowed. Cost:
ek heap allocation (chhoti strings ke liye SSO — file 04). Yeh text ka default
type hai.

Analogy: C-string ek khula kaagaz tha jiske end pe bas ek nishaan tha. `std::string` ek
register hai jiske cover pe likha hai "kitne page bhare hain" aur "kitne khaali bache hain" —
aur page kam padein to register khud bada ho jaata hai.

---

## Banana (construction)

```cpp
#include <string>

std::string a = "hello";
std::string b(5, 'x');            // "xxxxx"
std::string c = a + " world";     // jodna (concatenation)
std::string d(a, 1, 3);           // substring: "ell"  (index 1 se, length 3)
std::string e{other};             // copy
std::string f = std::move(other); // move -- other ka data le liya (other khaali ho jaata hai)

using namespace std::string_literals;
std::string g = "a\0b"s;          // s-suffix -> length 3, beech mein '\0' bhi chalega
```

---

## Size / capacity / empty

```cpp
s.size()          // kitne chars  (== s.length())
s.empty()         // s.size() == 0
s.capacity()      // kitni jagah allocate hai -- size se >= (badhne ki gunjaish)
s.max_size()      // theoretical limit
s.shrink_to_fit() // capacity ko size ke kareeb laane ki request (maanna zaroori nahi)
```

`size()` **O(1)** hai — `std::string` length store karke rakhta hai (C-string ke `strlen` scan ke
ulat).

⚠️ `size()` `std::string::size_type` lautata hai (= `std::size_t`, **unsigned**). Khaali string
pe `s.size() - 1` → wrap ho ke bahut bada number (folder 06 file 07). `if (!s.empty())` likho.

---

## Indexing

```cpp
s[i]              // check nahi -- i > s.size() pe UB
s.at(i)           // check hota hai -- std::out_of_range phenkta hai
s.front()         // s[0]
s.back()          // s[s.size() - 1]
s[s.size()]       // '\0'  -- PADHNA valid hai (C++11+), LIKHNA nahi
s.data()          // char*  -- null-terminated (const: C++11+, non-const: C++17)
s.c_str()         // const char*  -- null-terminated, C APIs ke liye
```

---

## Badalna — apne aap badhta hai

```cpp
s += "x";  s += 'y';                    // aakhir mein jodo
s.append("more");  s.push_back('!');
s.insert(0, ">> ");                     // kisi position pe daalo
s.replace(0, 3, "== ");                // [0,3) ki jagah "== "
s.erase(2, 4);                          // index 2 se 4 chars hatao
s.pop_back();                           // aakhri char hatao
s.clear();                             // size -> 0  (capacity BACHI RAHTI hai -- memory free nahi)
s.resize(10, ' ');                     // badhao/ghatao, ' ' se bharo
s.assign("new content");
```

`clear()` size ko 0 karta hai par buffer rakh leta hai — dobara use karo, naya allocation nahi.

---

## Comparison — content, sahi tareeke se

```cpp
std::string a = "apple", b = "apple";
a == b                                  // ✅ true  (content, pointers nahi)
a < b                                   // dictionary order (lexicographic)
a <=> b                                 // C++20 three-way
a == "apple"                            // ✅ literal se bhi compare
a.compare(b)                            // <0 / 0 / >0  (strcmp jaisa)
a.starts_with("app")   a.ends_with("le")   a.contains("ppl")   // C++20 / C++20 / C++23
```

---

## Iteration

```cpp
for (char c : s) { ... }                // range-for (bytes -- UTF-8 ke saath dhyaan, file 08)
for (char& c : s) c = std::toupper((unsigned char)c);   // badalna
for (std::size_t i = 0; i < s.size(); ++i) { ... }       // index se
for (auto it = s.begin(); it != s.end(); ++it) { ... }   // iterators
```

---

## C interop — `.c_str()` / `.data()`

```cpp
std::string path = "/tmp/f";
::open(path.c_str(), O_RDONLY);         // C API ko null-terminated char* chahiye
```

⚠️ **`.c_str()` / `.data()` ka pointer temporary hai** — `path` badla ya khatam hua to dangling
(file 09):
```cpp
const char* p = getString().c_str();   // ⚠️ temporary khatam -> p dangling
```

---

## Andar kya hota hai (libstdc++)

```
sizeof(std::string) == 32:
  char*  _M_p               // char data ka pointer (SSO: -> _M_local_buf)
  size_t _M_string_length   // length (isliye size() O(1))
  union {
    char   _M_local_buf[16] // SSO: 15 chars + '\0' andar hi
    size_t _M_allocated_capacity
  }
```

- **Chhoti string (≤ 15 chars)**: data `_M_local_buf` mein, object ke andar hi — **heap
  allocation nahi** (file 04).
- **Lambi string**: `_M_p` ek heap block pe point karta hai; capacity union mein.
- `size()` → `_M_string_length` lautao (constant time).
- `+=` / `append` → capacity mein fit ho to `memcpy`; warna reallocate (~2x badhta hai — file 04, 07).
- Copy → naya allocation + data copy (SSO string: bas inline chars copy).
- Move → lambi string ka heap pointer "chura" lo, source khaali chhodo. SSO string ke paas churaane
  ko pointer hi nahi — uske inline chars copy hote hain, isliye short string ka move copy jitna hi sasta/mehnga hai.

> **HFT relevance:** Cold/setup code aur chhoti keys ke liye `std::string` theek hai (SSO →
> allocation nahi). Hot paths pe dhyaan se: ek baar `reserve()` karo aur dobara use karo;
> `std::string_view` ki tarah pass karo (copy nahi); parse karke `std::string_view` fields banao
> (file 05, 07). Decode loop mein ek bhatki hui `std::string` copy ya `operator+` ki chain = har
> message pe heap ka kaam. Folders 36, 38.

---

## Hands-on

`examples/02_std_string.cpp` — construction, size/capacity, `.at()`, badalna, iterate, `c_str()`:

```bash
./build.ps1 10-STRINGS/examples/02_std_string.cpp
```

---

## ⚠️ Traps

### Trap 1 — khaali string pe `s.size() - 1`
```cpp
if (s.size() - 1 >= 0) ...    // ⚠️ khaali pe unsigned wrap. if (!s.empty()) likho
```

### Trap 2 — `.c_str()` dangling
```cpp
const char* p = build().c_str();   // ⚠️ temp khatam. std::string ko zinda rakho
```

### Trap 3 — `s[s.size()]` pe likhna
```cpp
s[s.size()] = 'x';    // ⚠️ '\0' wali jagah pe likhna -- UB. s.push_back('x')
```

### Trap 4 — samajhna ki `clear()` memory free karta hai
```cpp
s.clear();            // size 0, capacity WAHI. Memory chhodni hai to shrink_to_fit()
```

### Trap 5 — loop mein `+` ki chain
```cpp
for (auto& part : parts) result = result + part + ",";   // ⚠️ O(n^2) copies
for (auto& part : parts) { result += part; result += ','; }   // ✅
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`s.size()` `strlen` jaisa scan karta hai" | O(1) — length stored hai |
| "`s == other` pointers compare karta hai" | Content (`const char*` ke ulat) |
| "`s.clear()` memory chhod deta hai" | Size 0, capacity bachi rehti hai |
| "`s[s.size()]` out of bounds hai" | `'\0'` padhna theek; likhna UB |
| "Har `std::string` heap allocate karta hai" | Chhoti strings SSO use karti hain (file 04) |
| "String move hamesha bas pointer churaana hai" | Lambi string ke liye haan; SSO string ke chars copy hote hain |

---

## Exercises

1. **API tour:** `+=`, `append`, `insert` se `"[ hello , world ]"` banao. Phir `replace`, `erase`,
   `substr` se usko tod ke alag karo.

2. **size vs capacity:** `std::string s; for (int i=0;i<100;++i) { s+='x'; std::cout << s.size()
   << "/" << s.capacity() << "\n"; }` — capacity kis pattern mein badhti hai?

3. **`.at()` throw:** `try`/`catch` mein `s.at(1000)`. Message kya aaya?

4. **Move vs copy:** `std::string a = "hello world this is long"; std::string b = std::move(a);` —
   uske baad `a.size()`? `a.empty()`?

5. **`+` chain O(n²):** 10000 `"x"` strings ko `result = result + "x"` vs `result += "x"` se jodo.
   `-O2`, time lo. Ratio kya aaya?

6. **`c_str` lifetime:** `const char* p = std::string("temp").c_str(); std::cout << p;` — `-Wall`
   warning deta hai? Chalao — garbage aaya? Fix karo.

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
