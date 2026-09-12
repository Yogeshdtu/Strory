# 08 — Rvalue references (`T&&`) — pehla parichay

## Prerequisites
- [`01-what-is-a-reference.md`](01-what-is-a-reference.md) … [`04-const-references.md`](04-const-references.md)
- Folder 10 file 03 (SSO — string copy = alloc + memcpy)

## Yeh topic abhi kyun
Ab tak jo `T&` dekha woh **lvalue reference** thi. C++11 ne ek doosri kism di:
**rvalue reference `T&&`** — yeh un objects se bind hoti hai jo **khatam hone
wale hain** (temporaries, `std::move` ki hui cheezein). Ispe **move semantics**
khadi hai: mehngi copy ki jagah sasti "steal". **Poora topic folder 18 mein** —
yahan sirf mental model + kyun.

---

## lvalue vs rvalue — 10-second version

- **lvalue** = jiska naam hai, jiska address le sakte ho, jo baad mein bhi rahega.
  `x`, `arr[i]`, `*p`, `obj.member`.
- **rvalue** = temporary / literal / expression ka result jo is statement ke baad
  mar jaayega. `42`, `a + b`, `makeString()`, `std::move(x)`.

```cpp
int x = 5;
int& lr = x;          // ✅ lvalue ref  <- lvalue
int& lr2 = 5;         // ❌ lvalue ref  <- rvalue  (nahi)
int&& rr = 5;         // ✅ rvalue ref  <- rvalue
int&& rr2 = x;        // ❌ rvalue ref  <- lvalue  (nahi -- x lvalue hai)
int&& rr3 = std::move(x);   // ✅ std::move(x) x ko rvalue ki tarah "cast" karta hai
```

`const int& cr = 5;` bhi valid tha (file 04) — `const` lvalue ref rvalue ko bind
kar leta hai. Farq: `T&&` se aap us temporary ko **modify / loot** sakte ho;
`const T&` se sirf padh.

---

## Kyun: copy mehngi, move sasti

```cpp
std::string a = "................ 100 chars ................";
std::string b = a;              // COPY: naya buffer alloc + 100 bytes memcpy
std::string c = std::move(a);   // MOVE: c ne a ka buffer pointer "chura liya".
                                //       koi alloc nahi, koi memcpy nahi. a ab empty (valid) state.
```

`std::string` (folder 10) ke andar ek heap pointer + size + capacity hai. Copy =
naya heap block + saara data. Move = bas woh 3 fields transfer, `a` ko empty kar
do. **O(n) → O(1).**

Move constructor / move assignment ka signature `T&&` leta hai:

```cpp
struct Buffer {
    int*  data = nullptr;
    std::size_t n = 0;

    Buffer(Buffer&& other) noexcept              // MOVE ctor
        : data(other.data), n(other.n) {
        other.data = nullptr;                    // source ko "khaali" kar do
        other.n = 0;
    }
    Buffer& operator=(Buffer&& other) noexcept { /* similar: free old, steal, null out */ }
};
```

---

## `std::move` naam ka jhooth

`std::move(x)` **kuch move nahi karta**. Woh sirf ek `static_cast<T&&>(x)` hai —
`x` ko "main rvalue hoon" ka tag laga deta hai, taaki overload resolution move
ctor/assignment chune. Asli kaam move ctor karta hai.

```cpp
std::string s = "hello";
std::string t = std::move(s);   // move ctor chali -> s ka buffer t mein
// s ab "valid but unspecified" -- usually empty. s.clear()/s = "x" OK; s.size() bharosa mat karo
```

**⚠️ `std::move` ke baad source ko sirf phir se assign ya destroy karo** — uski
value pe depend mat karo.

---

## `T&&` parameter ≠ hamesha rvalue reference (forwarding reference)

```cpp
void sink(std::string&& s);        // yeh sach mein rvalue reference -- sirf rvalue lega

template <class T>
void relay(T&& x);                 // yeh FORWARDING reference (T deduce hota hai)
                                   // lvalue do -> T = U&,  x: U&    (lvalue ref)
                                   // rvalue do -> T = U,   x: U&&   (rvalue ref)
```

Sirf **`T&&` jahan `T` ek deduced template parameter ho** (ya `auto&&`)
forwarding reference hai. Concrete `std::string&&` nahi. Iske saath
`std::forward<T>(x)` chalta hai (perfect forwarding). **Yeh sab folder 18** —
abhi bas pehchan'na: har `&&` "rvalue only" nahi hota.

```cpp
auto&& r = anything;   // forwarding ref -- lvalue pe lvalue-ref, rvalue pe rvalue-ref
```

---

## Reference collapsing (file 02 se aage)

Templates/aliases mein `&` + `&&` combine ho sakte:

| Likha | Collapse |
|---|---|
| `T&  &`  | `T&` |
| `T&  &&` | `T&` |
| `T&& &`  | `T&` |
| `T&& &&` | `T&&` |

"Ek bhi lvalue-ref ho to result lvalue-ref." Yehi rule forwarding reference ka
`T` deduction possible banata hai.

---

## Andar kya hota hai

- `T&&` bhi ABI level pe **ek address** hai — `const T&` / `T&` jaisa hi pass
  hota hai. Farq **type system mein** hai: kaunsa constructor/overload chunega.
- **Move ctor** = kuch pointer/int field copies + source ko null/zero karna.
  `std::string` move ≈ 3 word copies + 3 word zeroing vs copy = `malloc` +
  `memcpy(n)` + `free` (baad mein). Isliye move O(1), copy O(n).
- **`noexcept` matter karta hai:** `std::vector` grow karte waqt elements ko move
  karega **sirf agar** move ctor `noexcept` ho — warna copy karega (strong
  exception guarantee ke liye). Isliye move ctor/assignment hamesha `noexcept`.
- Compiler `std::move` ko literally `mov`/`lea` mein badal deta — koi call nahi,
  koi runtime cost nahi. Woh ek cast hai.

> **HFT relevance:** startup / config / non-hot paths mein move semantics bade
> buffers (`std::vector<Order>`, `std::string` payloads) ko copy ki jagah O(1)
> transfer karke allocation churn hataati hai. **Hot path** mein aksar pehle se
> hi koi allocation nahi hoti (pre-sized arrays, pools) — to move ka fayda kam,
> par `std::vector` return karne wale setup functions, message queue handoff,
> aur object relocation mein yeh default technique hai. `noexcept` move ctors
> zaroori — warna `std::vector` reallocation chup-chaap copy pe gir jaata hai.

---

## Hands-on

```cpp
#include <iostream>
#include <string>
#include <utility>

int main() {
    std::string a(200, 'x');
    std::string b = a;                 // copy
    std::string c = std::move(a);      // move
    std::cout << "b.size=" << b.size()
              << "  c.size=" << c.size()
              << "  a.size=" << a.size() << " (moved-from, usually 0)\n";
    std::cout << "&b[0]=" << static_cast<const void*>(b.data()) << "\n";
    std::cout << "&c[0]=" << static_cast<const void*>(c.data())
              << "   (a ka purana buffer -- naya alloc nahi hua)\n";
}
```

```bash
g++ -std=c++20 -Wall -Wextra hands.cpp -o h && ./h
```

Folder 18 mein iska proper benchmark (copy vs move, `operator new` counter) aayega.

---

## ⚠️ Traps

### Trap 1 — `std::move` ke baad source use karna
```cpp
std::string s = "hi";
auto t = std::move(s);
std::cout << s;        // ⚠️ valid but unspecified -- usually "" par bharosa mat karo
```

### Trap 2 — `const` object pe `std::move`
```cpp
const std::string s = "hi";
auto t = std::move(s);   // ⚠️ chup-chaap COPY hui -- const se move ctor bind nahi, const& copy ctor chala
```

### Trap 3 — return mein `std::move` (pessimization)
```cpp
std::string f() { std::string s; ...; return std::move(s); }   // ⚠️ RVO tod deta hai! bas `return s;`
```

### Trap 4 — `T&&` ko forwarding reference samajhna jab woh nahi hai
```cpp
void g(std::string&& s);   // yeh sirf rvalue lega -- forwarding ref NAHI (T deduced nahi)
```

### Trap 5 — moved-from ko re-use karne se pehle assign na karna
```cpp
v.push_back(std::move(x));
x.append("more");        // ⚠️ pehle x = something / x.clear() -- moved-from state pe operate mat karo
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::move` object ko move karta hai" | Sirf `static_cast<T&&>` — ek tag. Move ctor kaam karta hai |
| "Har `T&&` rvalue reference hai" | Deduced `T&&` / `auto&&` = forwarding reference |
| "Moved-from object invalid hai" | Valid par unspecified — assign/destroy OK, value pe bharosa nahi |
| "`return std::move(local)` faster" | RVO todta hai — dheema. `return local;` |
| "`const` object move ho sakta hai" | Nahi — chup-chaap copy pe girta hai |

---

## Exercises

1. **Bind kya hoga:** `int x = 1;`
   ```cpp
   int&  a = x;
   int&& b = x;
   int&& c = 10;
   int&& d = std::move(x);
   const int& e = 10;
   ```

   <details><summary>Answer</summary>

   `a` ✅. `b` ❌ (`x` lvalue, `int&&` lvalue se bind nahi). `c` ✅. `d` ✅
   (`std::move(x)` → `int&&`). `e` ✅ (`const&` rvalue ko leta hai).
   </details>

2. **Copy vs move dikhao:** hands-on program chalao. `b.data()` aur `c.data()`
   addresses print karo. `c` ka address `a` ke original buffer jaisa kyun, `b`
   ka alag kyun?

   <details><summary>Answer</summary>

   `b = a` copy — naya heap buffer, alag address. `c = std::move(a)` — `a` ka
   buffer pointer `c` ne le liya, koi alloc nahi → `c.data()` == `a` ka purana
   buffer address. `a` ab null/empty.
   </details>

3. **`std::move` on const:** `const std::string s(200,'x'); std::string t =
   std::move(s);` — move hui ya copy? Kaise verify (allocation count / `s` ke
   baad `s.size()`)?

   <details><summary>Answer</summary>

   Copy hui — `const` `s` se `string(string&&)` bind nahi hota, `string(const
   string&)` chala. `s.size()` abhi bhi 200 (moved-from hota to unspecified).
   Allocation counter (folder 10 style) ek `new` dikhata.
   </details>

4. **Pessimization:** do functions — `std::string good(){ std::string s(50,'a');
   return s; }` aur `std::string bad(){ std::string s(50,'a'); return
   std::move(s); }`. `-O2 -fno-elide-constructors` ke bina dono ka behaviour?
   `bad` kyun bura?

   <details><summary>Answer</summary>

   `good` → NRVO: `s` seedha caller ki jagah banta, zero move/copy. `bad` →
   `std::move(s)` NRVO disable karta hai → ek move ctor call forced. Move sasta
   hai par zero se zyada — `bad` strictly worse.
   </details>

5. **Forwarding ref pehchano:** in mein se kaun forwarding reference hai?
   ```cpp
   void a(int&&);
   template<class T> void b(T&&);
   template<class T> void c(const T&&);
   auto&& d = foo();
   template<class T> void e(std::vector<T>&&);
   ```

   <details><summary>Answer</summary>

   `b` ✅ (deduced `T&&`). `d` ✅ (`auto&&`). `a` ❌ (concrete). `c` ❌ (`const
   T&&` — forwarding ref nahi). `e` ❌ (`vector<T>&&`, `T` deduce hota hai par
   parameter `T&&` form mein nahi).
   </details>

---

## Interview questions

1. lvalue aur rvalue mein farq — 3 examples har ek ke.
2. `T&&` kis se bind hoti hai jo `T&` se nahi? `const T&` se kya extra deti hai?
3. `std::move` actually kya karta hai? Naam kyun misleading?
4. Move ctor `std::string` ke liye O(1) kyun, copy O(n) kyun?
5. Forwarding reference vs rvalue reference — syntax se kaise pehchano?
6. Move ctor `noexcept` kyun hona chahiye? `std::vector` grow ke context mein.

---

## Next
→ [`09-reference-bugs.md`](09-reference-bugs.md)
