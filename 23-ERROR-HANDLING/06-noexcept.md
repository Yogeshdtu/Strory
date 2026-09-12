# 06 — `noexcept`

## Prerequisites
- `05-exception-safety.md`, `18-COPY-MOVE`
- `03-exceptions-basics.md` (`std::terminate`)

## Yeh topic abhi kyun
`noexcept` sirf ek documentation tag nahi — yeh ek **contract** hai jispe compiler,
standard library, aur aapke exception-safety guarantees depend karte hain. Sabse
important jagah: **move operations**. Ek missing `noexcept` `std::vector<Order>` ko
chupke se 10x slow kar sakta hai. Aur `noexcept` violate karne ka dand `terminate`
hai — koi catch nahi.

---

## Syntax

```cpp
void f() noexcept;             // "yeh throw nahi karega" — promise
void g() noexcept(true);       // same
void h() noexcept(false);      // "throw kar sakta hai" — default for normal functions
void k() noexcept(NOEXPR);     // conditional — NOEXPR ek constant bool expression

// operator form: compile-time query
static_assert(noexcept(f()));  // f() ka call noexcept hai?
```

- Normal functions: default `noexcept(false)`.
- **Destructors**: implicitly `noexcept(true)` — jab tak explicitly `noexcept(false)`
  na likho (mat likho).
- Implicitly-generated **move/copy ctor, move/copy assignment**: `noexcept` *iff*
  saare members/bases ke corresponding ops `noexcept` hain (computed).

---

## `noexcept` violate karne pe

```cpp
void f() noexcept {
    throw std::runtime_error("oops");    // -> std::terminate() TURANT
}
```

- **Search phase hi nahi hota.** `noexcept` boundary paar karne wali exception
  seedha `std::terminate`.
- Compiler `noexcept` function ke around unwinding tables / cleanup code emit
  karne ki zaroorat kam kar deta hai → **chhota code, better inlining**.
- Yeh "throw karo to crash" wala behaviour by design hai — `noexcept` ka matlab
  hi hai "isse aage exception ja hi nahi sakti, so plan mat banao".

Compiler in-function throw ko aksar warn karta (`-Wexceptions`): "function declared
`noexcept` has a throw".

---

## Sabse bada use: **move operations**

`std::vector` (aur `std::string`, `std::deque`, `std::optional` reallocation …)
purane elements ko naye storage mein le jaane ke liye `std::move_if_noexcept` use
karta hai:

```cpp
struct Order {
    std::string symbol;
    std::vector<Leg> legs;
    // move ctor: implicitly noexcept (dono members ke moves noexcept hain) ✅
};

struct Bad {
    std::string symbol;
    Bad(Bad&& o) : symbol(o.symbol) {}   // ⚠️ manually likha, noexcept nahi,
                                         //    aur COPY kar raha (typo), phir bhi:
                                         //    key point -> noexcept(false)
};
```

- Move ctor **`noexcept`** → `vector` grow karte waqt elements ko **move** karta
  hai (fast: pointer steal). Ek move throw kare to strong guarantee toot jaati —
  par `noexcept` ne kaha woh nahi hoga.
- Move ctor **`noexcept` nahi** → `vector` **copy** karta hai (slow, deep). Kyun?
  Agar 5th element ka move throw kare, toh pehle 4 already-moved elements ka source
  data ja chuka — purana buffer restore karna namumkin → strong guarantee gaya.
  Copy karo to purana buffer intact rehta hai fail pe.

**Rule:** har type jo `std::vector` mein jaayega, uska move ctor + move assignment
`noexcept` ho.

```cpp
static_assert(std::is_nothrow_move_constructible_v<Order>);
static_assert(std::is_nothrow_move_assignable_v<Order>);
```

`= default` karo jab possible — compiler sahi `noexcept` compute karega. Manually
likhna pade to `noexcept` explicitly lagao (aur nibhao).

---

## Conditional `noexcept` — generic code

```cpp
template <class T>
void swap_them(T& a, T& b) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                    std::is_nothrow_move_assignable_v<T>) {
    T tmp = std::move(a);
    a = std::move(b);
    b = std::move(tmp);
}
```

`noexcept(expr)` — `expr` ek `constexpr bool`. Wrapper apne underlying type jitna
hi "noexcept" hai. `std::swap`, container moves, `std::pair`/`std::tuple` moves —
sab conditional `noexcept` use karte hain.

---

## Kahan `noexcept` lagana chahiye

| Lagao | Kyun |
|---|---|
| Move ctor / move assignment | `vector` realloc fast; strong guarantee |
| `swap` | Copy-and-swap, "reset to good" fail-free |
| Destructors | (already implicit) — kabhi throw mat karo |
| Simple accessors / `size()` / `empty()` / comparisons | sach mein throw nahi karte; callers optimize |
| `main`-loop hot functions jo genuinely throw-free hain | inlining, no cleanup code |

| Mat lagao | Kyun |
|---|---|
| Functions jo genuinely throw kar sakti (alloc, parse, I/O) | jhoota `noexcept` = `terminate` land-mine |
| "shayad kabhi throw na kare" wali cheezein | `noexcept` ek promise hai, guess nahi |

**"`noexcept` everywhere" anti-pattern hai** — pehli baar jab woh function throw
karega (ya koi callee), `terminate`. Sirf wahan jahan aap *guarantee* de sakte ho.

---

## `noexcept` aur `throw()` (purana)

```cpp
void f() throw();        // C++98 dynamic exception spec — DEPRECATED, C++17 mein removed
void f() throw(X, Y);    // "sirf X ya Y" — removed, kabhi use mat karo
void f() noexcept;       // ✅ iska modern replacement (throw() ke barabar)
```

`throw()` ki runtime cost thi (violation pe `std::unexpected` machinery).
`noexcept` cleaner aur faster.

---

## Andar kya hota hai

- `noexcept` function ke call site pe compiler **landing pads / cleanup**
  generate nahi karta (ya minimal) → I-cache friendly, zyada inline.
- Violation: compiler ne `std::terminate` ka call implicitly daal rakha hai
  `noexcept` boundary pe — ya `__cxa_call_terminate`. Search phase skip.
- `std::is_nothrow_*` traits `noexcept(expr)` operator pe bane hain — compiler
  known-noexcept ops ka table rakhta hai (builtins, `= default` computed, declared).
- **ABI note:** `noexcept` C++17 se function **type** ka part hai — `void() noexcept`
  aur `void()` alag types. Function pointers ke liye matter karta.

---

## > **HFT relevance**
> `noexcept` HFT code mein do kaam karta hai:
>
> 1. **`std::vector<T>` / small-buffer containers of hot types** — `Order`,
>    `PriceLevel`, `MarketUpdate` — inke move ops `noexcept` na hue to realloc pe
>    deep copies = latency spike + memory bloat. Har hot value type pe
>    `static_assert(std::is_nothrow_move_constructible_v<T>)`.
>
> 2. **Inlining / code size** — hot loop ke functions `noexcept` marked hone se
>    compiler cleanup code nahi banata, better inline karta. `-fno-exceptions`
>    build mein toh sab kuch effectively `noexcept` hai, par explicit marking
>    intent bhi document karta hai aur mixed-mode builds mein bachata hai.
>
> `noexcept` ko discipline se lagao: hot-path helpers jo sach mein throw-free hain
> (arithmetic, bit ops, ring-buffer index math, `parse` jo error *value* return
> karta) — haan. Kuch bhi jo allocate/`throw`/log-with-alloc karta — nahi.

---

## Hands-on

```bash
./build.ps1 23-ERROR-HANDLING/examples/02_exception_safety.cpp   # noexcept swap + copy-and-swap
```

Ek type `struct Heavy { std::string a, b; std::vector<int> v; };` banao. Do version:
(1) `Heavy(Heavy&&) = default;` (2) `Heavy(Heavy&&) {}` (member-wise nahi likha →
actually error; use `Heavy(Heavy&& o): a(std::move(o.a)), b(std::move(o.b)),
v(std::move(o.v)) {}` bina `noexcept`). Dono ke liye
`std::is_nothrow_move_constructible_v<Heavy>` print karo, phir
`std::vector<Heavy>` ko 1M tak `push_back` karke time karo. Farq dekho.

---

## ⚠️ Traps

### Trap 1 — jhoota `noexcept`
```cpp
std::string serialize() const noexcept {   // ⚠️ string banata hai -> alloc -> bad_alloc -> terminate
    return fmt::format("{}", *this);
}
```

### Trap 2 — move ctor manually likha, `noexcept` bhoola
```cpp
Order(Order&& o) : sym(std::move(o.sym)), legs(std::move(o.legs)) {}   // noexcept(false)!
// -> std::vector<Order> realloc pe copy
```
`... {} ` ke aage `noexcept` ya bas `= default`.

### Trap 3 — `noexcept` destructor mein throw
```cpp
~Writer() { if (dirty_) flush(); }   // flush() throw -> terminate (dtor noexcept)
```

### Trap 4 — `noexcept(noexcept(...))` galat likhna
```cpp
template <class T> void f(T t) noexcept(noexcept(T(t)));   // ✅ double noexcept: outer spec, inner query
template <class T> void f(T t) noexcept(T(t));             // ⚠️ T(t) bool nahi -> galat
```

### Trap 5 — `noexcept` ko throughput optimization samajh ke sab pe thok dena
Pehle exception jo us path se guzri → `terminate`. Guarantee do jab de sakte ho.

### Trap 6 — `throw()` (purana) use karna
C++17 mein removed. `noexcept`.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`noexcept` sirf documentation" | Contract — `vector`/`move_if_noexcept`/optimizer ispe act karte |
| "`noexcept` function throw kare to caller catch karega" | Nahi — `std::terminate`, koi catch nahi |
| "manual move ctor apne aap `noexcept`" | Nahi — likho `noexcept` ya `= default` |
| "`noexcept` everywhere = fast" | Pehli violation pe `terminate`; sirf guaranteed-safe pe |
| "`throw()` aur `noexcept` alag semantics" | `throw()` == `noexcept`; `throw()` deprecated/removed |
| "`noexcept` function type ka part nahi" | C++17 se hai — `void() noexcept` alag type |

---

## Exercises

1. **Predict:**
   ```cpp
   struct A { A(){} A(A&&){} };                       // move ctor: noexcept?
   struct B { std::string s; };                       // implicit move: noexcept?
   ```

   <details><summary>Answer</summary>

   `A`: move ctor user-declared, no `noexcept` → `noexcept(false)`.
   `B`: implicit move ctor → `std::string`'s move is `noexcept` → `B`'s implicit
   move is `noexcept(true)`.
   </details>

2. **Fix the perf bug:**
   ```cpp
   class Quote { std::string sym; double px; long qty;
     Quote(Quote&& o) : sym(std::move(o.sym)), px(o.px), qty(o.qty) {} };
   std::vector<Quote> book;   // grows a lot
   ```

   <details><summary>Answer</summary>

   Manual move ctor `noexcept(false)` → `vector` copies on realloc. Add `noexcept`
   (all member moves are noexcept), or just `Quote(Quote&&) = default;` and also
   `= default` the move assignment. Add
   `static_assert(std::is_nothrow_move_constructible_v<Quote>);`.
   </details>

3. **`terminate` hunt:** `std::string format_row() const noexcept` — 3 tarah se
   yeh `terminate` kar sakta hai. Batao.

   <details><summary>Answer</summary>

   (1) Return `std::string` construction → allocation → `std::bad_alloc`.
   (2) Andar `+`/`append`/`std::format` bhi allocate karte. (3) Koi bhi callee
   (`to_string`, stream ops) throw kar sakta. Sab `noexcept` boundary → `terminate`.
   Fix: `noexcept` hatao, ya fixed buffer + `std::format_to_n` (no alloc).
   </details>

4. **Conditional:** `template <class T> struct Wrap { T v; Wrap(Wrap&&)
   noexcept(???); };` — `???` kya?

   <details><summary>Answer</summary>

   `noexcept(std::is_nothrow_move_constructible_v<T>)`. (Ya `Wrap(Wrap&&) =
   default;` — compiler yahi compute karega.) Wrapper apne `T` jitna noexcept.
   </details>

5. **ABI:** `using Fn = void(*)() noexcept;` — kya `void g() {}` ka address `Fn`
   mein daal sakte ho? Ulta?

   <details><summary>Answer</summary>

   `void g()` → `Fn` (noexcept ptr): **nahi** (C++17), `g` noexcept guarantee
   nahi deta. Ulta — `void h() noexcept {}` ka address `void(*)()` (non-noexcept
   ptr) mein: **haan**, noexcept se non-noexcept implicit conversion allowed.
   </details>

---

## Interview questions

1. `noexcept` violate karne pe kya? Search phase hota hai?
2. Move ctor `noexcept` na ho to `std::vector` realloc pe kya, aur kyun?
3. `std::move_if_noexcept` ka role.
4. Destructors ka default exception spec? Usse kyun nahi todna?
5. Conditional `noexcept(expr)` — `expr` kya hona chahiye, kis ke liye use hota?
6. "`noexcept` everywhere" kyun anti-pattern hai?
7. `throw()` vs `noexcept` — history aur farq.
8. C++17 mein `noexcept` function type ka part banne ka ek observable effect.

---

## Next
→ [`07-custom-exceptions.md`](07-custom-exceptions.md)
