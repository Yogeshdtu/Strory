# 09 — Rule of Five (and `= default` / `= delete`)

## Prerequisites
- [`03-rule-of-three.md`](03-rule-of-three.md), [`06-move-constructor.md`](06-move-constructor.md), [`07-move-assignment.md`](07-move-assignment.md)

## Yeh topic abhi kyun
C++11 ne 2 naye special members diye — move ctor, move assign. Ab agar aap ek
raw resource manage kar rahe ho, to **paanchon** consistent hone chahiye. Aur
ek chhota par crucial detail: **kaunsa special member declare karne se kaunse
auto-generate hote hain** — yeh galat samajhne se silent copy/move bugs aate
hain.

---

## The five special member functions

```cpp
class Widget {
public:
    ~Widget();                              // 1. destructor
    Widget(const Widget&);                  // 2. copy constructor
    Widget& operator=(const Widget&);       // 3. copy assignment
    Widget(Widget&&) noexcept;              // 4. move constructor       (C++11)
    Widget& operator=(Widget&&) noexcept;   // 5. move assignment        (C++11)
};
```

**Rule of Five:** if your class manages a raw resource and you write any one of
these, write (or `= default` / `= delete`) **all five** — they must agree on how
the resource is copied, moved, and released.

---

## The correct Rule of Five (a raw-buffer class)

```cpp
class Buffer {
    size_t n_    = 0;
    int*   data_ = nullptr;
public:
    explicit Buffer(size_t n) : n_(n), data_(new int[n]) {}

    ~Buffer() { delete[] data_; }

    Buffer(const Buffer& o) : n_(o.n_), data_(new int[o.n_]) {          // copy ctor: deep
        std::memcpy(data_, o.data_, n_ * sizeof(int));
    }
    Buffer& operator=(const Buffer& o) {                                 // copy assign: deep, safe
        if (this != &o) {
            int* fresh = new int[o.n_];
            std::memcpy(fresh, o.data_, o.n_ * sizeof(int));
            delete[] data_;
            data_ = fresh; n_ = o.n_;
        }
        return *this;
    }

    Buffer(Buffer&& o) noexcept : n_(o.n_), data_(o.data_) {            // move ctor: steal
        o.n_ = 0; o.data_ = nullptr;
    }
    Buffer& operator=(Buffer&& o) noexcept {                            // move assign: release + steal
        if (this != &o) {
            delete[] data_;
            n_ = o.n_; data_ = o.data_;
            o.n_ = 0; o.data_ = nullptr;
        }
        return *this;
    }
};
```

Every path — construction, copy, move, assignment, destruction — handles the
`int[]` correctly. (In real code: **just use `std::vector<int> data_;` → Rule of
Zero**, file 10. This full form is what you write *inside* such wrappers, or for
weird C resources.)

---

## The generation rules (memorize this table)

| You declare... | Compiler still generates... |
|---|---|
| nothing | all 5 (copy + move + dtor) |
| **destructor** | copy ctor + copy assign (⚠️ **deprecated**); **NO move ctor, NO move assign** |
| **copy ctor** | copy assign + dtor; **NO move ctor, NO move assign** |
| **copy assign** | copy ctor + dtor; **NO move ctor, NO move assign** |
| **move ctor** | dtor; copy ctor + copy assign **= deleted**; NO move assign |
| **move assign** | dtor; copy ctor + copy assign **= deleted**; NO move ctor |
| any **move** op | the **copy** ops become `= delete`d (move-only class) |

Two big takeaways:

### 1. Declaring a destructor kills the implicit move ops

```cpp
class C {
    std::vector<int> v_;
public:
    ~C() = default;          // ⚠️ still counts as "user-declared" -> move ops NOT generated
};

std::vector<C> vec;
vec.push_back(C{});          // realloc later -> C is COPIED (no move ctor) -> deep-copies v_ every time
```

Even `~C() = default;` suppresses implicit move generation. A "harmless" logging
destructor makes `std::vector<C>` reallocation copy instead of move — a silent
perf regression (`examples/08_noexcept_vector.cpp` shows ~3x).

### 2. Declaring a move op deletes the copy ops

```cpp
class D {
public:
    D(D&&) noexcept = default;   // declaring a move ctor...
    // ...implicitly `= delete`s the copy ctor and copy assign.
};
D a;
D b = a;                        // ❌ compile error -- copy ctor deleted
```

This is usually **what you want** for a move-only type (like `unique_ptr`). If
you need it copyable too, `= default` the copy ops as well (full Rule of Five).

---

## `= default` and `= delete`

```cpp
class Widget {
public:
    Widget() = default;                            // ask for the compiler's version, explicitly
    ~Widget() = default;

    Widget(const Widget&)            = default;    // "generate the member-wise copy"
    Widget& operator=(const Widget&) = default;
    Widget(Widget&&) noexcept            = default;// "generate the member-wise move"
    Widget& operator=(Widget&&) noexcept = default;
};
```

- **`= default`** — "compiler, generate the standard version". Use it to bring
  back a special member that another declaration suppressed, or to make Rule of
  Five explicit and self-documenting. The `= default`ed function is still optimal
  (member-wise, `noexcept` where possible).
- **`= delete`** — "this operation is forbidden; using it is a compile error".
  For non-copyable / non-movable types, unwanted conversions, etc.

```cpp
// non-copyable, movable (like unique_ptr)
class Handle {
public:
    Handle(const Handle&)            = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&&) noexcept            = default;   // still need to declare (or they'd be deleted by the above)
    Handle& operator=(Handle&&) noexcept = default;
    ~Handle();
};

// non-copyable, non-movable (like std::mutex)
class Lock {
public:
    Lock(const Lock&)            = delete;
    Lock& operator=(const Lock&) = delete;
    // no move ops declared, and copy is deleted -> move ops are also deleted -> immovable
};
```

**`= delete`d copy + no move declared → the move ops are also deleted → the type
is immovable.** To be move-only you must explicitly `= default` (or write) the
move ops.

---

## The five patterns you'll actually write

| Pattern | Declare |
|---|---|
| **Rule of Zero** (best) | nothing — all RAII members |
| **Move-only owner** | `= delete` copy ops, `= default` (or write) move ops, `~` |
| **Immovable** (mutex-like) | `= delete` copy ops (move ops auto-delete) |
| **Full Rule of Five** (copyable + movable raw owner) | write/`= default` all five |
| **Polymorphic base** | `virtual ~Base() = default;` + `= delete` or `= default` the copy/move (avoid slicing — folder 16 file 08) |

---

## Andar kya hota hai

- `= default`ed special members are generated `inline` and are member-wise —
  identical to what the implicit version would be, but now visible in the class
  and unaffected by the "declaring one suppresses another" rules for the *other*
  members you didn't declare... actually no: declaring even a `= default` one
  still counts. The value of `= default` is (a) documentation, (b) forcing
  generation of something that was suppressed.
- `= delete`d functions **participate in overload resolution** — if a deleted
  function is the best match, you get a "use of deleted function" error (not a
  silent fallback). This is how `= delete` blocks unwanted conversions
  precisely.
- The generated move ops are `noexcept` iff every member's corresponding move op
  is `noexcept` — so Rule-of-Zero classes with `std::string`/`std::vector`/
  `unique_ptr` members get `noexcept` moves for free.

> **HFT relevance:** the generation rules are a correctness minefield. The two
> that bite: (1) a stray destructor (logging, a debug hook, a `= default`)
> silently disables moves → `std::vector<T>` growth copies → allocation storm;
> (2) forgetting that `= delete`d copy without `= default`ed move = immovable →
> a type that can't go into a `std::vector` or be returned by value at all.
> HFT code style: Rule of Zero by default; when a resource wrapper is needed,
> the full Rule of Five written explicitly with `noexcept` moves and
> `static_assert(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>)`;
> `-Werror=deprecated-copy` to catch the "user dtor + implicit copy" case.

---

## Hands-on

```bash
./build.ps1 18-COPY-MOVE/examples/04_move_semantics.cpp     # a class with all of copy + move
./build.ps1 fast 18-COPY-MOVE/examples/08_noexcept_vector.cpp # "declared dtor -> no move -> copies" effect
```

`std::is_nothrow_move_constructible_v` for the two variants in `08` (1 vs 0).

---

## ⚠️ Traps

### Trap 1 — user destructor suppresses implicit move
```cpp
class C { std::vector<int> v_; ~C() = default; };   // ⚠️ no implicit move -> vector<C> realloc copies
```

### Trap 2 — `= delete` copy without `= default` move → immovable
```cpp
class C { C(const C&) = delete; C& operator=(const C&) = delete; };   // ⚠️ also non-movable. Add = default move ops
```

### Trap 3 — declaring move ctor, forgetting move assign (or vice versa)
```cpp
class C { C(C&&) noexcept = default; };   // ⚠️ no move ASSIGN generated; copy ops deleted. Declare both moves
```

### Trap 4 — Rule of Three in a C++11+ codebase (missing moves)
```cpp
class Buf { ~Buf(); Buf(const Buf&); Buf& operator=(const Buf&); };   // ⚠️ moves fall back to copy. Add the 2 move ops
```

### Trap 5 — non-`noexcept` `= default` move because a member's move can throw
```cpp
class C { SomeType t_; C(C&&) = default; };   // if SomeType's move isn't noexcept, C's isn't either -> vector copies
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Rule of Three is enough in modern C++" | Add the 2 move ops → Rule of Five (or aim for Rule of Zero) |
| "`~C() = default;` is a no-op declaration" | It's user-declared → suppresses implicit move generation |
| "Declaring a move ctor doesn't affect copy" | It `= delete`s the copy ops |
| "`= delete` copy → automatically move-only" | Also deletes the move ops → immovable. Must `= default` moves |
| "`= default`ed special members might be suboptimal" | They're member-wise, `noexcept` where possible — optimal |

---

## Exercises

1. **Fill in Rule of Five:** given `class Buf { size_t n_; int* p_; public:
   Buf(size_t); ~Buf(); Buf(const Buf&); Buf& operator=(const Buf&); };` — add
   the move ctor and move assignment.

   <details><summary>Answer</summary>

   `Buf(Buf&& o) noexcept : n_(o.n_), p_(o.p_) { o.n_ = 0; o.p_ = nullptr; }` and
   `Buf& operator=(Buf&& o) noexcept { if (this != &o) { delete[] p_; n_ = o.n_;
   p_ = o.p_; o.n_ = 0; o.p_ = nullptr; } return *this; }`.
   </details>

2. **Generation quiz:** for each, list which of the 5 the compiler generates.
   (a) `class A {};`, (b) `class B { ~B() {} };`, (c) `class C { C(C&&); };`,
   (d) `class D { D(const D&) = delete; };`, (e) `class E { E(E&&) = default;
   E& operator=(E&&) = default; ~E(); };`.

   <details><summary>Answer</summary>

   (a) all 5. (b) copy ctor + copy assign (deprecated); no moves. (c) dtor;
   copy ops deleted; no move assign. (d) copy assign + dtor; moves not generated
   (copy ctor user-declared as deleted → still "declared"). (e) copy ops deleted
   (move declared); the 2 moves + dtor are what you declared → move-only.
   </details>

3. **Make it move-only:** `class Socket { int fd_; ~Socket(); };` — make it
   non-copyable, movable, correctly.

   <details><summary>Answer</summary>

   `Socket(const Socket&) = delete; Socket& operator=(const Socket&) = delete;
   Socket(Socket&& o) noexcept : fd_(o.fd_) { o.fd_ = -1; } Socket&
   operator=(Socket&& o) noexcept { if (this != &o) { if (fd_ >= 0) close(fd_);
   fd_ = o.fd_; o.fd_ = -1; } return *this; } ~Socket() { if (fd_ >= 0)
   close(fd_); }`
   </details>

4. **Spot the immovable:** `class Config { Config(const Config&) = delete;
   Config& operator=(const Config&) = delete; };  std::vector<Config> v;
   v.push_back(Config{});` — compile error. Why? Fix.

   <details><summary>Answer</summary>

   `= delete`d copy + no declared move → move ops are also implicitly deleted →
   `Config` is immovable → `std::vector<Config>` can't grow (needs to move/copy
   elements on realloc). Fix: add `Config(Config&&) = default; Config&
   operator=(Config&&) = default;` (if `Config`'s members are movable).
   </details>

5. **`= default` to restore:** `class Node { std::unique_ptr<Node> next_;
   public: ~Node() { /* iterative delete to avoid stack overflow */ } };` — the
   destructor kills the move ops. Restore them.

   <details><summary>Answer</summary>

   `Node(Node&&) noexcept = default; Node& operator=(Node&&) noexcept =
   default;` — bring back the member-wise moves (stealing the `unique_ptr`).
   Also `Node(const Node&) = delete; Node& operator=(const Node&) = delete;` (it
   was non-copyable anyway due to `unique_ptr`). Full Rule of Five, explicit.
   </details>

---

## Interview questions

1. The 5 special member functions — naam?
2. Rule of Five — kab, aur kyun paanchon consistent?
3. Destructor declare karne se move ops ka kya (generation)?
4. Move op declare karne se copy ops ka kya?
5. `= delete` copy + no move declared → type ka kya (movable?)?
6. `= default` kab use karo (2 reasons)?

---

## Next
→ [`10-rule-of-zero.md`](10-rule-of-zero.md)
