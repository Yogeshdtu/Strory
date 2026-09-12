# 10 — Rule of Zero (revisited, move-semantics angle)

## Prerequisites
- [`09-rule-of-five.md`](09-rule-of-five.md)
- Folder 17 file 10 (Rule of Zero — RAII members) — this builds on it

## Yeh topic abhi kyun
Folder 17 file 10 ne Rule of Zero introduce kiya: agar saare members RAII hain
to koi special member likho hi mat. Ab **move semantics ke context mein** dobara
— kyunki Rule of Zero ka sabse bada practical fayda yahi hai: **compiler-generated
move ctor/assign O(1), `noexcept`, aur bilkul correct hote hain** — aur aapko
Rule-of-Five ki poori boilerplate (jo galat likhna aasan hai) likhni hi nahi
padti.

---

## Rule of Zero = correct moves for free

```cpp
// ❌ hand-written Rule of Five -- 5 functions, each an opportunity for a bug
class Session {
    Socket* sock_;
    char*   buf_;
    size_t  bufLen_;
public:
    Session(...);
    ~Session();                                    // delete sock_; delete[] buf_;
    Session(const Session&);                       // deep copy both
    Session& operator=(const Session&);            // self-check, exception-safe, deep copy both
    Session(Session&&) noexcept;                   // steal both, null source
    Session& operator=(Session&&) noexcept;        // release own, steal, null source, self-check
    // ~50 lines. Forget to null one pointer in the move ctor -> double-free.
};

// ✅ Rule of Zero -- ZERO special members, all correct
class Session {
    std::unique_ptr<Socket> sock_;                 // RAII
    std::vector<char>       buf_;                   // RAII
public:
    Session(...) : sock_(std::make_unique<Socket>(...)), buf_(4096) {}
    // compiler generates:
    //   ~Session()              -> ~buf_(), ~sock_()      (each frees itself)
    //   copy ctor / copy assign -> DELETED (unique_ptr non-copyable) -> Session is move-only
    //   move ctor               -> sock_(move(o.sock_)), buf_(move(o.buf_))   -- O(1), noexcept
    //   move assign             -> member-wise move assign                     -- O(1), noexcept
};
```

`Session` is move-only, its moves are O(1) pointer steals, they're `noexcept`
(so `std::vector<Session>` grows by moving), and there is **no code to get
wrong**.

---

## The generated move ops in detail

For a Rule-of-Zero class `C` with members `m1, m2, m3`:

```cpp
C(C&& o) noexcept(N) : m1_(std::move(o.m1_)), m2_(std::move(o.m2_)), m3_(std::move(o.m3_)) {}

C& operator=(C&& o) noexcept(N) {
    m1_ = std::move(o.m1_);
    m2_ = std::move(o.m2_);
    m3_ = std::move(o.m3_);
    return *this;
}
// N = true iff every member's corresponding move op is noexcept
```

- Each member is moved via **its own** move ctor/assign. `std::string`/`std::vector`
  steal their heap buffers. `unique_ptr` steals its pointer. `int`/`double`/POD
  → copied (move == copy for trivial types).
- **`noexcept` propagates** — `std::string` and `std::vector` and `unique_ptr`
  move ops are all `noexcept`, so a Rule-of-Zero class made of them has
  `noexcept` moves → containers move it, never copy (file 13).
- The generated move assignment is member-wise; for `std::string`/`std::vector`
  members it correctly releases the old buffer (their `operator=(T&&)` does) —
  no leak.

---

## When Rule of Zero gives you a *copyable* type vs move-only

| Members | Generated copy | Generated move | Type is... |
|---|---|---|---|
| `int`, `std::string`, `std::vector<T>` | deep copy (member-wise) | O(1) steal (member-wise) | **copyable + movable** |
| includes a `std::unique_ptr<T>` | deleted (unique_ptr non-copyable) | O(1) steal | **move-only** |
| includes a `std::shared_ptr<T>` | shallow (refcount bump) | O(1) steal | copyable + movable |
| includes a `std::mutex` / `std::atomic` | deleted | deleted | **immovable** (careful!) |

A `std::mutex` or `std::atomic<T>` member makes the class **immovable** —
sometimes surprising. If such a class needs to be movable, you must handle the
mutex explicitly (or hold it via a pointer / `std::unique_ptr`).

---

## The one exception where Rule of Zero needs help: pImpl

```cpp
// widget.hpp
class Widget {
    class Impl;                                    // forward-declared
    std::unique_ptr<Impl> pimpl_;
public:
    Widget();
    ~Widget();                                     // declared here, DEFINED in .cpp
    Widget(Widget&&) noexcept;                     // same
    Widget& operator=(Widget&&) noexcept;          // same
};

// widget.cpp -- Impl is complete here
class Widget::Impl { /* real members */ };
Widget::Widget() : pimpl_(std::make_unique<Impl>()) {}
Widget::~Widget() = default;
Widget::Widget(Widget&&) noexcept = default;
Widget& Widget::operator=(Widget&&) noexcept = default;
```

`std::unique_ptr<Impl>` needs `Impl` **complete** to generate `~unique_ptr<Impl>`
and the move ops (which may destroy an old `Impl`). In the header `Impl` is
incomplete → you must **declare** the dtor + move ops in the header and
`= default` them in the `.cpp`. The *logic* is still Rule of Zero (member-wise);
it's just physically split to satisfy the completeness requirement (folder 15
file 12).

---

## Andar kya hota hai

- The generated move ctor is `inline`, member-wise, and at `-O2` collapses to the
  minimal set of `mov`s + null-outs (for `std::string`/`vector` moves) — often
  ~6-12 instructions total for a 2-3 member class. No allocation, no branching
  (unless a member's move assign has a self-check).
- `noexcept(N)` where `N` is computed from the members at compile time — you can
  query it with `std::is_nothrow_move_constructible_v<C>`.
- A hand-written Rule-of-Five that's *correct* generates the same code — the
  benefit of Rule of Zero is purely "no bug surface + less code", not speed.

> **HFT relevance:** Rule of Zero is the default for every non-POD type in an HFT
> codebase — you get correct, `noexcept`, O(1) moves with zero maintenance, and
> the type is `is_nothrow_move_constructible` so `std::vector<T>` growth moves.
> POD hot types are trivially-everything (the ultimate Rule of Zero) → `memcpy`-
> relocatable in pools. The gotchas to watch: a `std::mutex`/`std::atomic` member
> silently makes a control-plane type immovable; a pImpl needs its dtor/moves
> split into the `.cpp`. A hand-written move ctor on a hot type gets a review
> comment: "why isn't this Rule of Zero?" — usually the answer is it should be.

---

## Hands-on

```bash
./build.ps1 18-COPY-MOVE/examples/04_move_semantics.cpp
```

Replace `Buffer`'s raw `int*` + hand-written 5 special members with a
`std::vector<int> data_;` member and delete all 5. Verify: still compiles, still
moves O(1) (via `vector`'s move), `std::is_nothrow_move_constructible_v<Buffer>`
is `true`, and it's now *copyable* too (deep copy, for free).

---

## ⚠️ Traps

### Trap 1 — `std::mutex` / `std::atomic` member → immovable class
```cpp
class Stats { std::mutex m_; long count_; };   // ⚠️ Stats is non-copyable AND non-movable
// std::vector<Stats> -> can't grow. Hold the mutex via unique_ptr, or use a different design
```

### Trap 2 — any user special member breaks Rule of Zero
```cpp
class C { std::vector<int> v_; ~C() = default; };   // ⚠️ move ops no longer implicit -> vector<C> copies (file 9)
```

### Trap 3 — pImpl `= default` in the header
```cpp
// widget.hpp: ~Widget() = default;   // ⚠️ Impl incomplete -> error. Declare here, = default in .cpp
```

### Trap 4 — shared_ptr member when you meant deep copy
```cpp
class Doc { std::shared_ptr<Body> body_; };   // ⚠️ "copy" of Doc SHARES the Body (shallow). Value member for deep
```

### Trap 5 — assuming Rule of Zero is always copyable
```cpp
class Session { std::unique_ptr<Socket> s_; };  Session b = a;   // ❌ move-only (unique_ptr). b = std::move(a);
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Rule of Zero means write `= default` for all 5" | Write **none** — any declaration counts |
| "Hand-written Rule of Five is faster than Rule of Zero" | Same codegen; Rule of Zero just has no bug surface |
| "A Rule of Zero class is always copyable" | Depends on members — `unique_ptr` → move-only |
| "`std::mutex` member is fine, just don't copy it" | It makes the class **immovable** too — surprising |
| "pImpl breaks Rule of Zero" | The *logic* is Rule of Zero; you split dtor/moves into the `.cpp` |

---

## Exercises

1. **Rule of Zero it:** the correct hand-written `Buffer` from file 09 (5
   special members, raw `int*`). Rewrite with a single member. What does the
   generated move ctor do? Is `Buffer` now copyable?

   <details><summary>Answer</summary>

   `class Buffer { std::vector<int> data_; public: explicit Buffer(size_t n) :
   data_(n) {} };` — 0 special members. Generated move ctor: `data_(std::move(o.data_))`
   → steals the vector's buffer, O(1), `noexcept`. Now **copyable** too (deep
   copy via `vector`'s copy ctor), for free.
   </details>

2. **Immovable surprise:** `class Counter { std::atomic<long> n_{0}; };
   std::vector<Counter> v(10);` — does this compile? `v.push_back(Counter{});`?

   <details><summary>Answer</summary>

   `std::vector<Counter> v(10)` — value-initializes 10 in place, compiles.
   `v.push_back(Counter{})` → may need to reallocate → needs to move/copy
   `Counter` → `std::atomic` is non-copyable **and** non-movable → compile error.
   Fix: `reserve` the final size up front, or store `std::unique_ptr<Counter>`,
   or redesign.
   </details>

3. **pImpl split:** why does `~Widget() = default;` in `widget.hpp` (with
   `class Impl;` forward-declared) fail, and where does it go?

   <details><summary>Answer</summary>

   `= default` in the header instantiates `~unique_ptr<Impl>()` there, which
   needs `Impl` complete to `delete` it → incomplete-type error. Move it to
   `widget.cpp` (`Widget::~Widget() = default;`) where `Impl` is defined. Same
   for the move ctor/assign.
   </details>

4. **Copyable or not:** for each Rule-of-Zero class, copyable? movable?
   `A { std::string s; }`, `B { std::unique_ptr<T> p; }`, `C { std::shared_ptr<T>
   p; }`, `D { std::mutex m; }`, `E { std::string s; std::unique_ptr<T> p; }`.

   <details><summary>Answer</summary>

   `A` both. `B` move-only. `C` both (copy = refcount bump). `D` neither. `E`
   move-only (the `unique_ptr` deletes copy for the whole class).
   </details>

5. **noexcept check:** for `class Msg { std::string topic_; std::vector<std::byte>
   payload_; std::int64_t seq_; };` — is `std::is_nothrow_move_constructible_v<Msg>`
   true? Why does it matter for `std::vector<Msg>`?

   <details><summary>Answer</summary>

   True — `std::string`, `std::vector`, and `std::int64_t` all have `noexcept`
   move ctors, so the generated `Msg` move ctor is `noexcept`. This means
   `std::vector<Msg>` reallocation **moves** each `Msg` (O(1) steals) instead of
   copying (deep-copying `topic_` and `payload_` every grow).
   </details>

---

## Interview questions

1. Rule of Zero move semantics ke liye kya deta (correct + noexcept + O(1))?
2. Generated move ctor kya karta (member-wise), `noexcept` kab?
3. `unique_ptr` vs `shared_ptr` member — class copyable/movable pe kya asar?
4. `std::mutex` / `std::atomic` member — kya problem?
5. pImpl Rule of Zero kyun nahi (technically), workaround?
6. Hand-written Rule of Five vs Rule of Zero — codegen difference? Kyun Rule of Zero?

---

## Next
→ [`11-copy-elision.md`](11-copy-elision.md)
