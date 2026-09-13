# 09 — `std::variant`

## Prerequisites
- [`08-unions.md`](08-unions.md)
- `08-FUNCTIONS/08-function-overloading.md`, `06-CONDITIONS/04-switch-statement.md`

## Yeh topic abhi kyun
`std::variant<A, B, C>` = "inmein se **exactly ek**" — ek **type-safe tagged
union**. Active type khud track karta hai, sahi destructor chalata hai, galat
access pe `throw` karta hai, aur `std::string` jaise non-trivial types safely
handle karta hai. Raw union + manual tag ka safe replacement.

---

## Basic

```cpp
#include <variant>

std::variant<int, double, std::string> v;   // default: pehla alternative (int{} = 0)

v = 42;                                       // ab int
v = 3.14;                                     // ab double (purana int destroy)
v = std::string("hello");                     // ab std::string

v.index()                                     // 2  (kaunsa alternative -- 0 se ginti)
std::holds_alternative<std::string>(v)        // true
```

`sizeof(variant)` ≈ sabse bada alternative + chhota discriminant + alignment. GCC 16.2 pe:
`variant<int, double, std::string>` = **40** (`std::string` 32 + 1-byte index, 8 tak round up),
`variant<char, char>` = **2**.

Analogy: ek tiffin box jismein ek waqt mein ek hi cheez — roti, chawal ya dal — aur dhakkan pe ek sticker
jo batata hai andar kya hai. Union mein sticker aapko khud lagana padta tha; `variant` khud lagata hai.

---

## Access — `get`, `get_if`, `visit`

```cpp
// std::get<T> -- T nahi hai to std::bad_variant_access throw
try {
    int i = std::get<int>(v);
} catch (const std::bad_variant_access&) { /* v mein kuch aur hai */ }

// std::get_if<T> -- T* ya nullptr (throw nahi) -- check ke liye yahi behtar
if (const std::string* s = std::get_if<std::string>(&v)) {
    use(*s);
}

// std::visit -- active alternative pe dispatch (exhaustive, throw nahi)
std::visit([](const auto& value) { std::cout << value; }, v);
```

### Handler set ke saath `std::visit`

```cpp
struct Handler {
    void operator()(int i)                const { /* ... */ }
    void operator()(double d)             const { /* ... */ }
    void operator()(const std::string& s) const { /* ... */ }
};
std::visit(Handler{}, v);

// ya "overloaded" idiom (inline lambdas):
template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;   // deduction guide -- sirf C++17 mein zaroori

std::visit(overloaded{
    [](int i)                { /* ... */ },
    [](double d)             { /* ... */ },
    [](const std::string& s) { /* ... */ },
}, v);
```

(Chala ke dekha: guide hatao to `-std=c++17` pe `class template argument deduction failed`; `-std=c++20` pe
aggregate CTAD ki wajah se bina guide ke chalta hai.)

⚠️ `std::visit` **exhaustive** hai — koi alternative chhoot gaya to compile nahi hoga (`switch` ke chhoote hue
`case` jaisa chupchaap nahi). Error message library ke andar se aata hai aur ulajh sakta hai — GCC 16.2 pe:
`error: no type named 'type' in 'struct std::invoke_result<overloaded<...>, std::string&>'` — matlab "`std::string`
ke liye koi handler nahi mila".

---

## `std::variant` vs raw union vs virtual (polymorphism)

| | `std::variant` | raw `union` + tag | `virtual` (base class ptr) |
|---|---|---|---|
| Type-safe | ✅ | ❌ (haath se) | ✅ |
| Band set (saare types pata) | ✅ | ✅ | ❌ (khula — koi bhi subclass) |
| Non-trivial members | ✅ (sambhalta hai) | ❌ (lifetime haath se) | ✅ |
| Storage | inline (heap nahi) | inline | heap (aksar) + vptr |
| Dispatch cost | index pe branch (inline ho sakta hai) | aapka `switch` | indirect call (vtable) |
| Naya type jodna | `variant` + saare `visit` badlo (compiler chhoote pakdega) | sab kuch badlo | bas subclass jodo |

**Band, pata hua set of alternatives → `std::variant`** — value-type, allocation-free, cache-friendly.
**`virtual`** jab set khula / extensible ho (folder 16).

### Andar se dekha — `Shape = variant<Circle, Square>` ka area (GCC 16.2, `-O2`)

```
areaV(variant<Circle,Square> const&):      // std::visit
    cmp   BYTE PTR [rcx+8], 0      // 1-byte index check
    jne   .square
    ... pi * r * r ...
    ret
.square:
    ... s * s ...
    ret

areaP(Base const&):                        // virtual
    mov   rax, [rcx]               // vptr load
    mov   rax, [rax+16]            // vtable se function pointer
    cmp   rax, <VC::area>          // compiler ka andaaza (speculative devirtualization)
    jne   ...                      //   galat nikla -> indirect call
```

`visit` = ek compare + branch, koi pointer chase nahi. `virtual` = do memory loads + compare, andaaza galat to
indirect call. (Size dono ka 16 bytes: `Shape` = double + index; `VC` = vptr + double.)

---

## Aam use

```cpp
// Parsed event -- kuch message types mein se ek
using Event = std::variant<Quote, Trade, Reject>;
std::vector<Event> stream;
for (const Event& e : stream) std::visit(EventPrinter{}, e);

// "value ya error" -- par aam taur pe std::expected (C++23) / std::optional behtar
std::variant<Result, ErrorCode> compute();

// state machine -- har state ek struct
using State = std::variant<Idle, Connecting, Connected, Failed>;
```

---

## Gotchas

```cpp
// valueless_by_exception -- assignment beech mein throw kare to variant "khaali" ho sakta hai
if (v.valueless_by_exception()) { /* bahut kam -- sirf jab move/copy throw kare */ }

// duplicate alternatives -- type se nahi, index se
std::variant<int, int> w;   // std::get<int>(w) -- compile error. std::get<0>(w)

// v = {} -- RESET karta hai: value-initialized variant = PEHLA alternative
v = {};                      // variant<int, double, std::string> -> index 0, int 0 (GCC 16.2 pe dekha)
```

`std::monostate` — "abhi koi value nahi" ke liye khaali alternative:
```cpp
std::variant<std::monostate, Quote, Trade> v;   // default: monostate (valid "khaali")
```

---

## Andar kya hota hai

- `std::variant` = sabse bade alternative jitna aligned byte buffer + ek `index` (aksar 1 byte). Heap nahi.
- Assignment → current alternative destroy (non-trivial ho to), naya wahin construct, `index` update.
- `std::get_if<T>` → `index == index_of<T> ? buffer ko T* ki tarah : nullptr`.
- `std::visit` → `index` pe sahi handler instantiation tak dispatch; `-O2` pe chhote visitors inline → `switch` jaisa
  branch, indirect call nahi (upar assembly).
- `std::bad_variant_access` `std::get` mismatch pe throw hota hai (asli `throw` — us path ki cost hai). GCC 16.2
  ka message: `std::get: wrong index for variant`.

> **HFT relevance:** HFT app logic mein band event/message sets ke liye `std::variant` pehli pasand hai:
> feed pe `variant<Quote, Trade, Reject, ...>`, `std::visit` se dispatch (inline branch, vtable nahi, heap nahi —
> band set ke liye `virtual` se behtar; upar assembly mein ek `cmp` vs do loads). Inline storage → cache-friendly,
> aur exhaustiveness check "naye message type ko handle karna bhool gaye" compile time pe pakadta hai. Sabse garam
> decode path pe PODs ka haath se bana tagged union (andar `std::string` nahi) + raw `switch` aakhri thoda sa
> bachata hai. `virtual` sirf sach mein khuli hierarchies ke liye. Folders 16, 36, 38.

---

## Hands-on

`examples/06_variant.cpp` — `Event = variant<Quote, Trade, Reject>`, `index()`, `get`/`get_if`, `visit`, stream
processing:

```bash
./build.ps1 11-STRUCTS/examples/06_variant.cpp
```

---

## ⚠️ Traps

### Trap 1 — bina check `std::get<T>`
```cpp
int i = std::get<int>(v);   // ⚠️ v int nahi to throw. get_if, ya pehle holds_alternative
```

### Trap 2 — non-exhaustive `visit`
```cpp
std::visit(overloaded{ [](int){}, [](double){} }, v);   // ❌ v std::string rakh sakta hai to compile nahi hoga
```
(Yeh feature hai — par "default" ki ummeed thi to chaunkaata hai. Default chahiye to ek generic
`[](const auto&){}` lambda jodo — par tab naye types ki galti compiler nahi pakdega.)

### Trap 3 — duplicate alternatives + `get<T>`
```cpp
std::variant<int, int> v;  std::get<int>(v);   // ❌ static assertion failed: T must occur exactly once in alternatives
```

### Trap 4 — `variant` ko default mein "khaali" samajhna
```cpp
std::variant<Quote, Trade> v;   // default-constructed Quote rakhta hai, KHAALI NAHI. std::monostate use karo
```

### Trap 5 — khule/extensible set ke liye `variant`
```cpp
using Shape = std::variant<Circle, Square>;   // ⚠️ Triangle jodna = har visit badlo. Khula set hai to virtual
```

### Trap 6 — `v = {}` ko "no-op" ya "error" samajhna
```cpp
v = {};   // ⚠️ chupchaap PEHLE alternative pe reset (int 0) -- purani string destroy ho gayi
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`variant` heap allocate karta hai" | Inline storage — sabse bada alternative + tag |
| "`std::get<T>` mismatch pe null deta hai" | Throw karta hai — `std::get_if` null deta hai |
| "`visit` mein default case hota hai" | Exhaustive — chhoota alternative = compile error |
| "`variant` default mein khaali banta hai" | Pehla alternative rakhta hai; khaali ke liye `std::monostate` |
| "`v = {}` kuch nahi karta / compile nahi hota" | Pehle alternative pe reset karta hai |
| "`variant` hamesha `virtual` ki jagah le leta hai" | Sirf band set ke liye; khuli hierarchy ke liye `virtual` |

---

## Exercises

1. **Basics:** `std::variant<int, std::string> v;` — pehle int, phir string assign karo. Har baar `v.index()`.
   `holds_alternative` checks.

2. **`get_if` dispatch:** `std::vector<std::variant<int, double>>` — `get_if` se ints aur doubles ka alag-alag sum.

3. **`visit` + overloaded:** `overloaded` idiom se `variant<Quote, Trade, Reject>` ke har alternative ko alag
   tarah print karo.

4. **Exhaustiveness:** apne `visit` se ek handler hatao — compile error kya aaya? Ek `std::monostate` alternative
   jodo — ab kya?
   <details><summary>Answer</summary>

   Handler hatane pe GCC 16.2: `no type named 'type' in 'struct std::invoke_result<overloaded<...>, X&>'` (X = jiska
   handler nahi). `std::monostate` jodne pe **wahi error** `std::monostate&` ke liye — naye alternative ka bhi
   handler chahiye (`[](std::monostate) {}`). Yahi exhaustiveness ka faayda hai.
   </details>

5. **State machine:** `variant<Idle, Running, Done>` aur ek `tick(State&)` jo `visit` karke transition kare. Chalao.

6. **variant vs virtual:** `{Circle, Square}` ke liye "area" dono tarah likho (`std::variant` + `visit`, aur base
   class + `virtual`). `sizeof` milao, aur dispatch ka assembly (`-O2 -S`, functions pe `[[gnu::noipa]]`).
   <details><summary>Answer (GCC 16.2, Windows x64)</summary>

   `sizeof(Shape)` 16 (double + 1-byte index + padding), `sizeof(VC)` 16 (vptr + double). `visit`:
   `cmp BYTE PTR [rcx+8], 0` + ek branch. `virtual`: vptr load, vtable se pointer load, speculative-devirtualization
   `cmp`, miss hone pe indirect call.
   </details>

---

## Interview questions

1. `std::variant` vs raw union — 3 safety farq?
2. `std::get` vs `std::get_if` — error behaviour?
3. `std::visit` — exhaustiveness? `switch` se fark?
4. `std::variant` vs `virtual` polymorphism — band vs khula set, cost?
5. `std::monostate` kya hai, kab chahiye?
6. `std::variant` storage — heap ya inline?
7. `v = {}` kya karta hai?

---

## Next
→ [`10-enums.md`](10-enums.md)
