# 05 — Rvalue references (`T&&`)

## Prerequisites
- [`04-value-categories.md`](04-value-categories.md)
- Folder 13 file 08 (rvalue reference intro), folder 13 file 02 (reference vs pointer)

## Yeh topic abhi kyun
`T&&` — "rvalue reference" — C++11 ka naya reference type. Yeh **rvalues**
(temporaries, `std::move`'d objects) se bind hota hai. Iske do bilkul alag uses
hain: (1) **move ctor/assign parameters** (yeh lesson + files 06-07), aur (2)
**forwarding references** in templates (file 12 — jahan `T&&` ka matlab alag hai).
Confusion na ho, isliye dono clearly.

---

## `T&&` — binds to rvalues

```cpp
int x = 5;

int&  lref = x;              // lvalue ref  <- lvalue      ✅
int&& rref = 5;              // rvalue ref  <- prvalue     ✅
int&& r2   = x + 1;          // rvalue ref  <- prvalue     ✅
int&& r3   = std::move(x);   // rvalue ref  <- xvalue      ✅

int&& bad  = x;              // ❌ ERROR -- x is an lvalue, rvalue ref can't bind
int&  bad2 = 5;              // ❌ ERROR -- lvalue ref can't bind an rvalue (const int& can)
```

| Reference type | Binds to |
|---|---|
| `T&` | non-const lvalues |
| `const T&` | **everything** (lvalues, const lvalues, rvalues) — the fallback |
| `T&&` | rvalues (prvalues + xvalues) — **not** lvalues |
| `const T&&` | rare; const rvalues (mostly a "don't move a const" blocker) |

An rvalue reference lets a function say: *"give me something you're done with,
and I'll take its guts."*

---

## Overload resolution: `T&` vs `T&&` vs `const T&`

```cpp
void process(std::string&  s) { std::cout << "lvalue: modify in place\n"; }
void process(std::string&& s) { std::cout << "rvalue: I can steal from s\n"; }
void process(const std::string& s) { std::cout << "const: read-only\n"; }

std::string a = "x";
const std::string ca = "y";

process(a);              // -> std::string&        (non-const lvalue prefers T&)
process(ca);             // -> const std::string&  (const)
process(a + "!");        // -> std::string&&       (prvalue prefers T&&)
process(std::move(a));   // -> std::string&&       (xvalue)
```

If both `T&` and `const T&` and `T&&` overloads exist, the compiler picks the
**most specific** for the argument's value category. This is how a class provides
"modify-in-place for lvalues, steal-guts for rvalues".

---

## The lifetime-extension rule (for `T&&` local variables)

```cpp
std::string makeStr();

const std::string& cr = makeStr();   // ✅ const lvalue ref extends the temporary's life
std::string&&      rr = makeStr();   // ✅ rvalue ref ALSO extends it -- same rule (folder 13 file 04)

std::cout << rr;                      // valid -- the temporary lives as long as `rr`
```

Binding a temporary to a **local** `const T&` or `T&&` extends the temporary's
lifetime to the reference's scope. But **not** through function calls, not for
members, not for returns — same limits as folder 13 file 04.

---

## `T&&` as a move-ctor parameter (the primary use)

```cpp
class Buffer {
    size_t n_ = 0;
    int*   data_ = nullptr;
public:
    Buffer(Buffer&& o) noexcept                 // MOVE ctor -- parameter is `Buffer&&`
        : n_(o.n_), data_(o.data_) {
        o.n_ = 0;
        o.data_ = nullptr;                       // steal + null out the source
    }
};

Buffer a{100};
Buffer b = std::move(a);   // std::move(a) is an rvalue -> binds Buffer&& -> move ctor -> steal
// a is now empty (n_=0, data_=nullptr) -- valid, but "moved-from"
```

The `Buffer&&` parameter says "the caller has an expiring `Buffer`; I'll take its
`data_` pointer instead of allocating a new buffer and copying". Details file 06.

---

## ⚠️ Named `T&&` is an lvalue (again)

```cpp
Buffer(Buffer&& o) noexcept
    : data_(o.data_) {           // `o.data_` -- fine, just reading a pointer
    otherFunc(o);                // ⚠️ `o` here is an LVALUE -> otherFunc gets Buffer& / const Buffer&
    otherFunc(std::move(o));     // ✅ turned back into an rvalue
}
```

Inside the move ctor, `o` has a name → the expression `o` is an lvalue. To pass
it *onward* as movable, you must `std::move(o)` again. (This is why move ctors do
`std::move(o.member_)` for each member — file 06.)

---

## `T&&` in templates ≠ rvalue reference (forwarding reference — file 12)

```cpp
void f(std::string&& s);          // rvalue reference -- s must be an rvalue

template <class T>
void g(T&& x);                    // FORWARDING reference -- T is DEDUCED
                                  //   g(lvalue) -> T = U&,  x : U&   (lvalue ref!)
                                  //   g(rvalue) -> T = U,   x : U&&  (rvalue ref)

auto&& y = anything;              // FORWARDING reference (auto&&)
```

**`T&&` is a forwarding reference ONLY when `T` is a template parameter being
deduced (or `auto&&`).** A concrete `std::string&&`, `Buffer&&`,
`std::vector<int>&&` is a plain rvalue reference. Full treatment in file 12; for
now just recognize the difference.

---

## Andar kya hota hai

- `T&&` is, at the ABI level, **a pointer** — same as `T&` (folder 13 file 02).
  The difference is purely in the type system: it changes which overload the
  compiler selects. `sizeof`, codegen for passing — identical to `T&`.
- Binding a prvalue to a `T&&` local → the prvalue is materialized into a
  temporary in the caller's stack frame, the reference points at it, its
  destructor is scheduled at the reference's scope end (lifetime extension).
- `const T&&` exists mainly so you can `= delete` it to **prevent** moving from
  const rvalues, or to detect them — you almost never write a `const T&&`
  parameter that does work.
- Reference collapsing (`T& &&` → `T&`, etc.) only happens with deduced/aliased
  references — the engine behind forwarding references (file 12).

> **HFT relevance:** rvalue references are the type-system hook for zero-copy
> transfer. A hot-path API that consumes a buffer takes `Buffer&&` (or by value +
> move), so callers `f(std::move(buf))` and the buffer's `data_` pointer is
> handed over with no allocation. The distinction "concrete `T&&` = rvalue ref,
> deduced `T&&` = forwarding ref" matters when writing generic containers /
> wrappers (pools, queues) — getting it wrong forces copies. `const T&&` shows
> up as `= delete`d overloads to statically forbid moving from immutable data.

---

## Hands-on

```bash
./build.ps1 18-COPY-MOVE/examples/03_value_categories.cpp   # what binds to T&&
./build.ps1 18-COPY-MOVE/examples/04_move_semantics.cpp     # T&& as a move-ctor param
```

Write a `struct Sink` with `void take(std::string&&)` and `void take(const
std::string&)`; call it with a literal, a named `std::string`, and
`std::move(named)` — observe which overload fires.

---

## ⚠️ Traps

### Trap 1 — `T&&` binding an lvalue
```cpp
int x;  int&& r = x;   // ❌ ERROR. int&& r = std::move(x);  or  int& r = x;
```

### Trap 2 — using a named `T&&` parameter without `std::move`
```cpp
void consume(Widget&& w) { store(w); }   // ⚠️ store COPIES (w is an lvalue). store(std::move(w))
```

### Trap 3 — concrete `T&&` vs deduced `T&&`
```cpp
void f(std::vector<int>&& v);        // rvalue ref -- only rvalues
template<class T> void g(T&& v);     // forwarding ref -- lvalues too (file 12)
```

### Trap 4 — `T&&` return + expecting lifetime extension through a call
```cpp
std::string&& f() { return std::string("temp"); }   // ⚠️ dangling -- returned ref outlives nothing
```

### Trap 5 — overloading on `T&` and `T&&` but forgetting `const T&`
```cpp
void h(T&); void h(T&&);   // ⚠️ h(constObj) fails to compile -- add const T& (or just use const T& + T&&)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`T&&` means 'move'" | It's a reference type that *binds to* rvalues; a move ctor *uses* it |
| "`T&&` binds to anything" | Only rvalues. `const T&` binds to anything |
| "Every `T&&` is a forwarding reference" | Only deduced `T&&` / `auto&&`; concrete `Widget&&` is a plain rvalue ref |
| "Inside a `T&& x` function, `x` is an rvalue" | The name is an lvalue — `std::move(x)` to pass it on |
| "`T&&` is a different size / cost than `T&`" | Identical — a pointer; the difference is overload selection |

---

## Exercises

1. **Binding quiz:** `int i; const int ci = 0;` — which compile?
   `int&& a = i;`, `int&& b = 7;`, `int&& c = i + 0;`, `int&& d = std::move(i);`,
   `int&& e = ci;`, `const int&& f = 7;`, `int&& g = std::move(ci);`.

   <details><summary>Answer</summary>

   `a` ❌ (lvalue). `b` ✅. `c` ✅ (prvalue). `d` ✅ (xvalue). `e` ❌ (const
   lvalue). `f` ✅ (const rvalue ref ← prvalue). `g` ❌ (`std::move(ci)` is
   `const int&&`, can't bind non-const `int&&`).
   </details>

2. **Overload selection:** `void p(std::string&); void p(const std::string&);
   void p(std::string&&);` — for `std::string s; const std::string cs; ` —
   `p(s)`, `p(cs)`, `p("hi")`, `p(s + s)`, `p(std::move(s))`, `p(std::move(cs))`.

   <details><summary>Answer</summary>

   `p(s)` → `std::string&`. `p(cs)` → `const std::string&`. `p("hi")` →
   `std::string&&` (temporary). `p(s + s)` → `std::string&&`. `p(std::move(s))`
   → `std::string&&`. `p(std::move(cs))` → `const std::string&`.
   </details>

3. **Named param:** `void enqueue(Job&& j) { queue_.push_back(j); }` — this
   compiles and works but is a performance bug. Why? Fix.

   <details><summary>Answer</summary>

   `j` inside `enqueue` is an lvalue (it has a name) → `push_back(j)` copies the
   `Job`. Fix: `queue_.push_back(std::move(j));`.
   </details>

4. **Concrete vs deduced:** for each, is `x` a forwarding reference or a plain
   rvalue reference? `void a(int&& x);`, `template<class T> void b(T&& x);`,
   `template<class T> void c(std::vector<T>&& x);`, `auto&& x = f();`,
   `template<class T> void d(const T&& x);`.

   <details><summary>Answer</summary>

   `b` forwarding, `x` (`auto&&`) forwarding. `a` plain rvalue ref. `c` plain
   (the deduced `T` is inside `vector<T>`, not the top-level `&&`). `d` plain
   (`const T&&` is never a forwarding reference).
   </details>

5. **Lifetime extension:** `std::string&& r = std::string("hi") + "!";  std::cout
   << r;` — valid? What about `std::string&& bad() { return std::string("x"); }`?

   <details><summary>Answer</summary>

   First: **valid** — binding the temporary to a local `T&&` extends its lifetime
   to `r`'s scope. Second: **dangling** — the returned `std::string&&` refers to
   a local temporary that's destroyed when `bad()` returns (`-Wreturn-local-addr`).
   Lifetime extension doesn't cross a function return.
   </details>

---

## Interview questions

1. `T&&` kis se bind hota, `T&` / `const T&` se kaise alag?
2. Overload set `T&` / `const T&` / `T&&` — kaunsa argument kaunsa chunta?
3. `T&&` local variable ka lifetime-extension rule?
4. Move ctor mein `T&&` parameter — uska use?
5. "Named `T&&` is an lvalue" — kyun, aur move ctor mein iska kya matlab (`std::move(o.member)`)?
6. Concrete `T&&` vs deduced `T&&` — kaise pehchano, kyun matter karta?

---

## Next
→ [`06-move-constructor.md`](06-move-constructor.md)
