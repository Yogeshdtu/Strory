# 10 — Rule of Zero

## Prerequisites
- [`09-raii-for-other-resources.md`](09-raii-for-other-resources.md)
- Folder 15 file 06 (destructors, Rule of 3/5 preview), folder 18 preview

## Yeh topic abhi kyun
"Rule of Zero" = **best case**: aapki class ke liye aapko **koi** special member
function (destructor, copy ctor, copy assign, move ctor, move assign) likhni hi
nahi padti — kyunki har member khud RAII hai aur khud ko manage karta hai.
Compiler-generated special members correct hote hain, aur aapka code chhota +
bug-free. Yeh modern C++ ka default target hai. (Rule of 3/5 ki poori depth
folder 18 mein — yeh uska "0" case hai.)

---

## Rule of 3 / 5 / 0 — quick map

| Rule | Kab | Kya likhna padta |
|---|---|---|
| **Rule of Three** (C++98) | class ek raw resource own karti hai (raw `new`, `FILE*`, `fd`) | destructor + copy ctor + copy assign (teenon) |
| **Rule of Five** (C++11) | same, plus move | + move ctor + move assign (paanchon) |
| **Rule of Zero** (best) | class ke saare members khud RAII hain | **kuch nahi** — compiler sab generate karta |

Insight: **agar aap ek special member likh rahe ho, aap kuch galat kar rahe ho
(ya at least kuch low-level).** Ideally, wrap the raw resource in a tiny RAII
type (`unique_ptr`, `Fd`, `std::vector`) and then the *outer* class is Rule of
Zero.

---

## Rule of Zero — the pattern

```cpp
// ❌ Rule of Five -- manual, error-prone
class Session {
    Socket* sock_;                       // raw
    char*   buf_;                        // raw
    size_t  bufLen_;
public:
    Session(...) : sock_(new Socket(...)), buf_(new char[4096]), bufLen_(4096) {}
    ~Session() { delete sock_; delete[] buf_; }               // + copy ctor + copy assign
    Session(const Session& o) : sock_(new Socket(*o.sock_)),  // + move ctor + move assign
                                buf_(new char[o.bufLen_]), bufLen_(o.bufLen_) {
        std::memcpy(buf_, o.buf_, bufLen_);
    }
    // ... 40 more lines of copy/move boilerplate, easy to get wrong ...
};

// ✅ Rule of Zero -- members are RAII, class writes NOTHING
class Session {
    std::unique_ptr<Socket> sock_;       // RAII
    std::vector<char>       buf_;         // RAII
public:
    Session(...) : sock_(std::make_unique<Socket>(...)), buf_(4096) {}
    // NO destructor -- compiler-generated: destroys buf_ then sock_ (each cleans itself)
    // NO copy ctor  -- deleted (unique_ptr non-copyable) -> Session is move-only, automatically
    // NO move ctor  -- compiler-generated: moves each member (unique_ptr steal, vector steal)
    // NO copy/move assign -- ditto
};
```

The `Session` class body has **zero** special members and is **completely
correct**:
- Destruction: `~buf_()` then `~sock_()` (reverse decl order) → each RAII member
  frees its resource.
- Copy: implicitly `= delete`d because `unique_ptr` is non-copyable → `Session`
  is move-only. (If you wanted it copyable, use `shared_ptr` or a value member.)
- Move: implicitly generated, member-wise (steal each member).

---

## Kaise achieve karein Rule of Zero

**Har raw resource ko ek RAII member se replace karo:**

| Raw | RAII replacement |
|---|---|
| `T* p; ~C(){ delete p; }` | `std::unique_ptr<T> p;` |
| `T* p; ~C(){ delete[] p; }` (dynamic array) | `std::vector<T> p;` |
| `char* s; ~C(){ delete[] s; }` (string) | `std::string s;` |
| `FILE* f; ~C(){ fclose(f); }` | `std::unique_ptr<FILE, FileCloser> f;` / `std::fstream` |
| `int fd; ~C(){ close(fd); }` | a move-only `Fd` member (file 09) |
| shared resource | `std::shared_ptr<T> p;` |
| fixed buffer | `std::array<T, N> buf;` (no dtor needed anyway) |

Once **every** member is either a trivial type or a self-managing RAII type, the
compiler-generated destructor / copy / move are all correct → write none.

---

## The subtle gotcha — declaring one affects the others

The special members are **interdependent** (folder 18 file 09). If you declare
**any** of them (even `= default`), it can suppress the auto-generation of
others:

```cpp
class Widget {
    std::vector<int> data_;
public:
    ~Widget() { std::cout << "bye\n"; }   // ⚠️ user-declared destructor
    // -> move ctor and move assign are NO LONGER auto-generated!
    // -> Widget is now COPYABLE (deprecated) but NOT movable -> silently slower
    //    (every "move" falls back to a copy of data_)
};
```

Adding a destructor just to log → you lost the implicit move operations →
`std::vector<Widget>` reallocation now **copies** instead of moving.

**Rule of Zero really means: declare NONE of the five.** If you must declare one,
you're back to Rule of Five (declare all five, `= default` the ones you don't
customize). This is why "just add a quick destructor for logging" is a trap.

```cpp
// If you truly need the destructor, be explicit about the rest:
class Widget {
    std::vector<int> data_;
public:
    ~Widget() { log(); }
    Widget(const Widget&)            = default;
    Widget& operator=(const Widget&) = default;
    Widget(Widget&&)                 = default;   // bring back the move ops
    Widget& operator=(Widget&&)      = default;
};
```

---

## Rule of Zero + non-copyable

Common case: a class that owns a `unique_ptr` (or an `Fd`, or a `std::mutex`)
member → automatically **move-only**, no boilerplate:

```cpp
class Engine {
    std::unique_ptr<Impl> pimpl_;    // non-copyable member
public:
    Engine();
    ~Engine();                        // pImpl: declared here, `= default` in .cpp (Impl complete there)
    // Engine is move-only, correctly, with almost no code.
    Engine(Engine&&) noexcept;               // often needs to be in .cpp too (pImpl)
    Engine& operator=(Engine&&) noexcept;
};
// engine.cpp:
Engine::Engine() : pimpl_(std::make_unique<Impl>()) {}
Engine::~Engine() = default;
Engine::Engine(Engine&&) noexcept = default;
Engine& Engine::operator=(Engine&&) noexcept = default;
```

(The pImpl idiom forces the destructor / move ops into the `.cpp` because
`unique_ptr<Impl>` needs `Impl` complete to be destroyed — folder 15 file 12. So
strictly it's "Rule of Five with `= default`", but the *logic* is zero.)

---

## Andar kya hota hai

- Compiler-generated destructor: calls each member's destructor in reverse
  declaration order (then base classes). For all-RAII members, this frees
  everything correctly. For trivial members, it's a no-op → the whole dtor may
  be trivial (`is_trivially_destructible`).
- Compiler-generated move ctor/assign: member-wise move (`std::move` each
  member). `unique_ptr` move = pointer steal + null; `vector`/`string` move =
  steal the 3 pointers + null. **O(1)**, no allocation.
- Compiler-generated copy: member-wise copy. `vector`/`string` copy = allocate +
  element copy (**O(n)**); `unique_ptr` copy = doesn't exist → the class's copy
  is `= delete`d.
- The generated functions are `inline` and `-O2` optimizes them to the minimal
  moves/copies — often better than hand-written (no missed member, no wrong
  order).

> **HFT relevance:** Rule of Zero is the target for *every* non-primitive type —
> control-plane objects (sessions, subscriptions, strategy state) and hot-path
> POD (which are trivially copyable, the ultimate Rule of Zero). It means: no
> hand-written copy/move to get wrong (a mis-written move ctor that forgets a
> member = a subtle corruption bug), correct and optimal generated code, and
> `is_trivially_copyable` / `is_trivially_destructible` hold for the POD types so
> they can be `memcpy`'d into pools and `std::vector`-relocated cheaply. The
> "quick logging destructor" trap is real — it silently disables moves and makes
> a `std::vector<T>` of that type reallocate by copying. Reviewers watch for any
> user-declared special member and ask "why isn't this Rule of Zero?".

---

## Hands-on

```bash
./build.ps1 17-RAII/examples/02_unique_ptr.cpp    # Rule of Zero via unique_ptr members
```

Write a `class Buffer` two ways: (a) raw `char* + size_t` with Rule of Five
boilerplate, (b) `std::vector<char>` member with Rule of Zero (empty class body
beyond the constructor). Compare line count and correctness.

---

## ⚠️ Traps

### Trap 1 — "quick logging destructor" kills the move ops
```cpp
class C { std::vector<int> v_; public: ~C() { log(); } };   // ⚠️ no more implicit move -> copies. `= default` all five
```

### Trap 2 — raw resource member, forgetting Rule of Five
```cpp
class C { int* p_; public: ~C() { delete[] p_; } };   // ⚠️ C b = a; -> double-free. Wrap p_ in vector -> Rule of Zero
```

### Trap 3 — thinking `= default` on one is free
```cpp
class C { ...; C(const C&) = default; };   // still "user-declared" -> may suppress move generation
```

### Trap 4 — Rule of Zero class that "should" be copyable but has a `unique_ptr`
```cpp
class Config { std::unique_ptr<Section> s_; };   // ⚠️ move-only. Wanted a copyable config? shared_ptr or value
```

### Trap 5 — pImpl without moving dtor/move to the `.cpp`
```cpp
// widget.hpp: class Widget { std::unique_ptr<Impl> p_; public: ~Widget() = default; };
// ⚠️ Impl incomplete here -> can't instantiate ~unique_ptr<Impl>. Declare ~Widget(); define in .cpp
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Rule of Zero = write a `= default` destructor" | Write **none** of the five; `= default` still counts as declared |
| "Adding a destructor for logging is harmless" | Suppresses implicit move ops → silent copies |
| "Rule of Zero classes are always copyable" | Depends on members — a `unique_ptr` member → move-only |
| "Compiler-generated special members might be wrong" | For all-RAII members they're correct and optimal |
| "I need copy/move ctors for a class with vector/string members" | No — the generated ones handle it (Rule of Zero) |

---

## Exercises

1. **Rule of Zero it:** `class Packet { char* data_; size_t len_; public:
   Packet(size_t n) : data_(new char[n]), len_(n) {} ~Packet() { delete[]
   data_; } /* + copy ctor + copy assign + move ctor + move assign */ };` —
   rewrite with Rule of Zero. How many special members now?

   <details><summary>Answer</summary>

   `class Packet { std::vector<char> data_; public: explicit Packet(size_t n) :
   data_(n) {} };` — **zero** special members. Copy = deep copy (vector's),
   move = O(1) (vector's), dtor = frees the buffer. All generated, all correct.
   </details>

2. **Spot the regression:** a `class Metrics { std::vector<Sample> samples_; };`
   used in `std::vector<Metrics>`. Someone adds `~Metrics() {
   flushToDisk(samples_); }`. What silently changed for the `std::vector<Metrics>`?

   <details><summary>Answer</summary>

   The user-declared destructor suppressed the implicit **move** ctor/assign. Now
   `std::vector<Metrics>` reallocation **copies** each `Metrics` (deep-copying
   `samples_`) instead of moving (O(1) steal) → much slower growth. Fix: `=
   default` all five, or move `flushToDisk` out of the destructor.
   </details>

3. **Copyable or move-only:** for each member set, is the Rule-of-Zero class
   copyable, movable, both, neither? (a) `std::vector<int>`, (b)
   `std::unique_ptr<T>`, (c) `std::shared_ptr<T>`, (d) `std::mutex`, (e) `int`
   + `std::string`.

   <details><summary>Answer</summary>

   (a) both. (b) move-only. (c) both. (d) neither (`std::mutex` is non-copyable
   AND non-movable). (e) both.
   </details>

4. **pImpl Rule of Zero-ish:** why can't you write `~Widget() = default;` in the
   *header* for `class Widget { std::unique_ptr<Impl> p_; };` when `Impl` is only
   forward-declared? Where does it go?

   <details><summary>Answer</summary>

   `= default` in the header instantiates `~unique_ptr<Impl>()` there, which needs
   `Impl` **complete** to call `delete` on it — it's only forward-declared →
   error. Declare `~Widget();` in the header, `Widget::~Widget() = default;` in
   the `.cpp` where `Impl` is a complete type.
   </details>

5. **Force copyable:** you have `class Doc { std::unique_ptr<Body> body_; };` and
   you *need* `Doc` to be copyable (deep copy). Two ways?

   <details><summary>Answer</summary>

   (a) Write a copy ctor / copy assign that deep-copies: `Doc(const Doc& o) :
   body_(std::make_unique<Body>(*o.body_)) {}` (+ assign) — now Rule of Five. (b)
   Change the member to `std::shared_ptr<Body>` (shared, not deep) or a value
   `Body body_;` (deep, Rule of Zero) if `Body` is copyable.
   </details>

---

## Interview questions

1. Rule of 3 / 5 / 0 — kab kaunsa, kya likhna?
2. Rule of Zero kaise achieve karte (raw → RAII members)?
3. Ek special member declare karne se baaki par kya asar?
4. "Logging destructor" trap — kya silently breaks?
5. `unique_ptr` member wali class — copyable? movable? Kyun?
6. pImpl class Rule of Zero kyun nahi (technically), aur uska workaround?

---

## Next
→ [`11-smart-pointer-performance.md`](11-smart-pointer-performance.md)
