# 08 — `std::move` is just a cast

## Prerequisites
- [`04-value-categories.md`](04-value-categories.md), [`06-move-constructor.md`](06-move-constructor.md)

## Yeh topic abhi kyun
`std::move` sabse **misunderstood** name hai standard library mein. Woh **kuch
move nahi karta**. Woh ek `static_cast<T&&>` hai — expression ko lvalue se rvalue
(xvalue) mein "cast" karta hai, taaki overload resolution move ctor/assign chune.
Actual transfer move ctor/assign karti hai. Yeh clearly samajhna zaroori — warna
`std::move` ke gotchas (const, return, POD) samajh nahi aayenge.

---

## What `std::move` actually is

```cpp
// simplified libstdc++ / libc++
template <class T>
constexpr std::remove_reference_t<T>&& move(T&& x) noexcept {
    return static_cast<std::remove_reference_t<T>&&>(x);
}
```

That's the whole thing. `std::move(x)`:
- Takes anything (forwarding reference `T&&`).
- `static_cast`s it to an **rvalue reference** to its non-reference type.
- Returns that — an **xvalue** (file 04).

**Zero runtime cost.** It compiles to nothing — no instructions. It's a
compile-time signal: *"treat this expression as an rvalue"*.

```cpp
std::string s = "hello world this is longer than SSO";

auto&& r = std::move(s);       // just a cast. s is UNCHANGED. r is a std::string&& bound to s.
std::cout << s;                // still "hello world..." -- nothing moved!
std::cout << r;                // same characters
```

`examples/05_std_move_demo.cpp` section 1: after `std::move(s)` with nothing
consuming it, `s.size()` is unchanged.

---

## When the move actually happens

The transfer occurs when a **move constructor** or **move assignment** *consumes*
that rvalue:

```cpp
std::string s = "hello world this is longer than SSO";

std::string t = std::move(s);   // NOW: std::string's MOVE ctor runs -> steals s's buffer
                                //   s is now moved-from (usually empty)
std::cout << s.size();          // 0 (moved-from)
std::cout << t;                 // "hello world..."
```

Chain of events:
1. `std::move(s)` → casts `s` to `std::string&&` (xvalue). No effect yet.
2. `std::string t = <xvalue>` → overload resolution picks `std::string(std::string&&)`
   (the move ctor).
3. The **move ctor** does the actual work: steal `s`'s pointer/size/capacity,
   null out `s`.

So: **`std::move` says "you may move from this"; the move ctor/assign does the
moving.** If nothing with move semantics consumes the rvalue, nothing moves.

---

## Gotcha 1 — `std::move` on a `const` silently COPIES

```cpp
const std::string cs = "immutable";

std::string a = std::move(cs);   // std::move(cs) is `const std::string&&`
                                 // move ctor takes `std::string&&` -> can't bind (const)
                                 // -> falls back to COPY ctor `std::string(const std::string&)`
                                 // -> cs is COPIED, not moved. No error, no warning.
```

You can't steal from a `const` object (stealing modifies the source). `std::move`
on a `const` produces a `const T&&`, which move ctors reject → the compiler
quietly uses the copy ctor. **Never make things `const` that you intend to move
from.**

---

## Gotcha 2 — `std::move` in `return` PESSIMIZES

```cpp
std::string makeGood() {
    std::string x = "result";
    return x;                    // ✅ NRVO -> x constructed directly in caller's slot (ZERO copy/move)
                                 //    (or, if NRVO fails, IMPLICIT move -- x is a local on return)
}

std::string makeBad() {
    std::string x = "result";
    return std::move(x);         // ⚠️ `return std::move(x)` -> the return expression is now an xvalue,
                                 //    NOT a plain name -> NRVO is DISABLED -> forced MOVE ctor call.
                                 //    -Wpessimizing-move warns about this.
}
```

`return localVar;` already does the right thing:
- **NRVO** (compiler courtesy — GCC/Clang do it): `x` is built in the caller's
  storage, zero copy/move.
- If NRVO can't apply, the compiler does an **implicit move** anyway (a local
  named in a `return` is treated as an rvalue since C++11).

Wrapping it in `std::move` turns it into an expression that isn't a plain name →
NRVO is impossible → you *guarantee* at least a move ctor call. `-Wpessimizing-move`
(in `-Wall`) flags this. `examples/05_std_move_demo.cpp` section 4 shows
`makeGood()` prints nothing (NRVO), `makeBad()` prints "MOVE".

**Rule: `return x;` or `return T{...};`. Never `return std::move(x);`.**

(Exception: `return std::move(member_);` when `member_` is a data member — a
member isn't a local, so no NRVO applies; `std::move` there is correct. Also
`return std::move(x)` when `x` is a *function parameter by value* and the return
type differs — narrow cases.)

---

## Gotcha 3 — `std::move` on a POD does nothing useful

```cpp
int a = 5;
int b = std::move(a);   // int has no move ctor distinct from copy -> b = 5, a = 5 (unchanged)

struct Point { double x, y; };
Point p1{1, 2};
Point p2 = std::move(p1);   // trivial type -> move == copy -> p1 unchanged, p2 is a bit-copy
```

For trivially-copyable types, "move" and "copy" are identical (a register/memory
copy). `std::move` on them is harmless but pointless. Move only helps types that
**own heap resources** (`std::string`, `std::vector`, `unique_ptr`, your buffer
classes).

---

## Gotcha 4 — using a moved-from object

```cpp
std::vector<int> v = {1, 2, 3};
auto w = std::move(v);        // w steals v's buffer
v.push_back(4);               // ⚠️ v is "valid but unspecified" -- for std::vector usually empty,
                              //    so this appends to an empty vector. Works, but don't ASSUME.
int x = v[0];                 // ⚠️ if v is empty -> UB. Don't read moved-from state.
v = {5, 6};                   // ✅ assigning a fresh value is always fine
```

After moving from an object: **assign to it or destroy it**. Don't read its
value. (Standard library types are left "valid but unspecified" — often but not
guaranteed empty.)

---

## The real, correct uses of `std::move`

```cpp
// 1. Move a named local into a container / function that will consume it
std::string s = build();
sink.push_back(std::move(s));      // s is dead after this -> move, don't copy

// 2. Move a member out in a move ctor / move assignment
Widget(Widget&& o) noexcept : name_(std::move(o.name_)) {}

// 3. Move into a member from a by-value parameter (the "sink" idiom)
void setName(std::string name) { name_ = std::move(name); }
obj.setName("hello");             // temporary -> moved into param -> moved into member. Zero copies.

// 4. Force move where the compiler would copy (e.g. std::move(param) at the end of a function)
```

The common thread: **an lvalue that you're finished with**, that you want
transferred instead of copied.

---

## Andar kya hota hai

- `std::move` → `static_cast<T&&>` → **no instructions**. `-O0` might show a
  redundant `mov` (a cast of a reference); `-O2` erases it entirely.
- The behavior change is 100% in **overload resolution**: with the argument now
  an rvalue, the compiler selects `T(T&&)` / `operator=(T&&)` instead of the
  `const T&` versions.
- `remove_reference_t` in `std::move`'s definition handles the case where `T`
  deduces to a reference (from an lvalue argument) — ensures the result is always
  `X&&`, never `X& &&`.

> **HFT relevance:** `std::move` is free (a cast) — it's a correctness/perf
> annotation, not an operation. Its value in hot-adjacent code: hand a big
> `std::vector<Order>` / `std::string` payload into a queue or a member without
> copying (`q.push(std::move(batch))`). The traps bite in practice: a `const`
> source silently copies (make it non-const); `return std::move(local)` disables
> NRVO (write `return local`); moving from a POD is a no-op (don't bother). A
> profiler showing an unexpected `operator new` right after a "move" is usually
> gotcha 1 or a missing `std::move` on a named lvalue.

---

## Hands-on

```bash
./build.ps1 18-COPY-MOVE/examples/05_std_move_demo.cpp
```

Section 1: `std::move(s)` alone changes nothing. Section 2: a move ctor consumes
it → `s` emptied. Section 3: `std::move` on const → copy ctor chosen. Section 4:
`return std::move(x)` → forced move (`-Wpessimizing-move`), vs `return x` → NRVO.
Section 5: `std::move` into a container.

---

## ⚠️ Traps

### Trap 1 — `return std::move(local)` (pessimization)
```cpp
T f() { T x; return std::move(x); }   // ⚠️ disables NRVO. return x;
```

### Trap 2 — `std::move` on a `const` (silent copy)
```cpp
const T ct = ...;  T a = std::move(ct);   // ⚠️ COPIES. Don't const things you'll move
```

### Trap 3 — reading a moved-from object
```cpp
auto b = std::move(a);  use(a.field);   // ⚠️ valid but unspecified. Assign/destroy only
```

### Trap 4 — expecting `std::move` to do something by itself
```cpp
std::move(v);   // ⚠️ statement with no effect (-Wunused-value-ish). Nothing consumed the rvalue
```

### Trap 5 — `std::move` on the last use of a parameter that outlives the call... isn't always safe
```cpp
void f(std::string& s) { g(std::move(s)); }   // ⚠️ caller's s is now moved-from -- did the caller expect that?
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::move(x)` moves x" | It's a `static_cast<T&&>` — a move ctor/assign does the moving |
| "`std::move` has a runtime cost" | Zero — compiles to nothing |
| "`return std::move(x)` is an optimization" | Pessimization — kills NRVO (`-Wpessimizing-move`) |
| "`std::move` on a `const` still moves" | Falls back to copy (silently) |
| "After `std::move(x)`, x is destroyed / invalid" | Valid but unspecified — assign or destroy it |

---

## Exercises

1. **Nothing moved:** `std::string s(100, 'a'); auto&& r = std::move(s);
   std::cout << s.size();` — output? Why?

   <details><summary>Answer</summary>

   `100` — `std::move(s)` is just a cast; `r` is an rvalue reference bound to
   `s`, but no move ctor/assign consumed it, so `s` is untouched.
   </details>

2. **const trap:** `const std::vector<int> src = {1,2,3};  std::vector<int> dst =
   std::move(src);` — is `src` empty after? What ctor ran?

   <details><summary>Answer</summary>

   `src` is unchanged (still `{1,2,3}`). `std::move(src)` is `const std::vector<int>&&`;
   the move ctor needs `std::vector<int>&&` → can't bind → the **copy** ctor ran.
   `dst` is a deep copy.
   </details>

3. **Pessimization:** `Big f() { Big x; fill(x); return std::move(x); }` — the
   `-Wpessimizing-move` warning says what? Rewrite the return.

   <details><summary>Answer</summary>

   "moving a local object in a return statement prevents copy elision". Rewrite:
   `return x;` — NRVO builds `x` directly in the caller's storage (zero
   copy/move); if NRVO fails, `x` is implicitly moved anyway.
   </details>

4. **Sink idiom:** `void Widget::setName(std::string name) { name_ = std::move(name);
   }` — trace the number of allocations/moves for `w.setName("literal")` and
   `std::string s = "x"; w.setName(s);` and `w.setName(std::move(s));`.

   <details><summary>Answer</summary>

   `setName("literal")` → temporary `std::string` from the literal → **moved**
   into `name` (param) → **moved** into `name_`. 1 alloc (the temp), 0 copies.
   `setName(s)` (lvalue) → `s` **copied** into `name` → **moved** into `name_`. 1
   copy. `setName(std::move(s))` → `s` **moved** into `name` → **moved** into
   `name_`. 0 copies, `s` emptied.
   </details>

5. **Moved-from reuse:** `std::string s = "abc"; auto t = std::move(s); s +=
   "xyz"; std::cout << s;` — is this UB? What's likely printed?

   <details><summary>Answer</summary>

   Not UB — `+=` on a moved-from `std::string` is a valid mutating operation (it
   doesn't read an unspecified value in a way that matters). Likely prints
   `"xyz"` (libstdc++ leaves moved-from strings empty), but the standard only
   guarantees "valid" — don't rely on it being exactly `"xyz"`; `s = "xyz";`
   would be unambiguous.
   </details>

---

## Interview questions

1. `std::move` actually kya karta hai (implementation)? Runtime cost?
2. Move kab hota hai — `std::move` ke baad kya chahiye?
3. `std::move` on a `const` — kya hota, kyun?
4. `return std::move(x)` kyun bura? `-Wpessimizing-move`?
5. Moved-from object ke saath kya karna safe hai?
6. `std::move` ke 3 legit uses?

---

## Next
→ [`09-rule-of-five.md`](09-rule-of-five.md)
