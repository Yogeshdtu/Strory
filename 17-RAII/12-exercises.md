# 12 — Folder 17 Revision + Exercises

## Prerequisites
Lessons 01–11 aur saare 7 examples chalaye hue.

---

## PART A — Concept check

1. "Resource" kya hai — 4 common properties? Memory ke alawa 4 examples?
2. C++ mein `finally` kyun nahi — kya replaces it?
3. RAII wrapper ke 4 elements?
4. Local RAII object ka dtor kaunse exit paths pe chalta, kaunse nahi (5 gaps)?
5. Stack unwinding kya hai, kab, kya karta?
6. Exception safety guarantees — 4 levels? RAII kaunsa free deta?
7. "Zero-cost exceptions" — happy path pe cost? Throw hone pe?
8. `unique_ptr` — ownership, copy/move, `sizeof`, overhead (measured)?
9. `make_unique` vs `unique_ptr(new T)` — 2 reasons?
10. `release()` vs `reset()`?
11. `shared_ptr` — `sizeof`, control block contents, copy ki cost?
12. `make_shared` vs `shared_ptr(new T)` — 3 differences?
13. `shared_ptr` by value kab pass karo?
14. `enable_shared_from_this` — kya problem solve karta?
15. `shared_ptr` cycle leak — kaise banta? `weak_ptr` kaise todta?
16. `weak_ptr` ko use kaise (`lock()`), race-free kyun?
17. Custom deleter ki form → `unique_ptr` size (struct/fn-ptr/lambda)?
18. `shared_ptr` custom deleter — size change kyun nahi?
19. Ownership semantics ke 5 kinds?
20. "Sink parameter" idiom — kya, kyun by-value + move?
21. Non-owning parameter — `const T&` vs `shared_ptr` by value, kab?
22. `int fd` ke liye `unique_ptr` vs hand-rolled class — kyun?
23. Move-only RAII wrapper ke 5 elements? Move ctor mein source null kyun?
24. Rule of 3 / 5 / 0 — kab kaunsa?
25. Rule of Zero kaise achieve karte? "Logging destructor" trap?
26. `unique_ptr` free hai — measured ratio? `shared_ptr` copy vs raw copy?

---

## PART B — Output prediction

### B1
```cpp
struct G { G(){puts("+");} ~G(){puts("-");} };
void f(bool bail) { G g; if (bail) return; puts("work"); }
int main() { f(false); puts("---"); f(true); }
```
<details><summary>Answer</summary>`+`, `work`, `-`, `---`, `+`, `-` — `g`'s dtor runs on both the normal and the early-return path.</details>

### B2
```cpp
auto a = std::make_unique<int>(7);
auto b = std::move(a);
std::cout << (a == nullptr) << " " << *b << " " << sizeof(a);
```
<details><summary>Answer</summary>`1 7 8` — `a` is null after the move; `b` owns `7`; `sizeof(unique_ptr<int>)` == 8.</details>

### B3
```cpp
auto a = std::make_shared<int>(1);
std::cout << a.use_count() << " ";
{
    auto b = a;
    auto c = b;
    std::cout << a.use_count() << " ";
}
std::cout << a.use_count() << " " << sizeof(a);
```
<details><summary>Answer</summary>`1 3 1 16` — count goes 1 → 3 (b, c) → back to 1; `sizeof(shared_ptr)` == 16.</details>

### B4
```cpp
struct Node { std::shared_ptr<Node> other; ~Node() { puts("~Node"); } };
{
    auto a = std::make_shared<Node>();
    auto b = std::make_shared<Node>();
    a->other = b;
    b->other = a;
}
puts("done");
```
<details><summary>Answer</summary>`done` only — no `~Node` prints. The `a↔b` `shared_ptr` cycle keeps both refcounts at 1 forever → leak, destructors never run.</details>

### B5
```cpp
struct FileCloser { void operator()(std::FILE* f) const noexcept { if (f) std::fclose(f); } };
std::cout << sizeof(std::unique_ptr<std::FILE, FileCloser>) << " "
          << sizeof(std::unique_ptr<std::FILE, void(*)(std::FILE*)>) << " "
          << sizeof(std::shared_ptr<std::FILE>);
```
<details><summary>Answer</summary>`8 16 16` — stateless struct deleter → EBO → 8; function-pointer deleter stored → 16; `shared_ptr` always 16 (deleter type-erased).</details>

### B6
```cpp
class C {
    std::vector<int> v_;
public:
    ~C() { /* log */ }
};
std::cout << std::is_move_constructible_v<C> << " "
          << std::is_copy_constructible_v<C>;
```
<details><summary>Answer</summary>`0 1` (roughly) — the user-declared destructor suppresses the implicit move ctor; copy is still generated (deprecated). Note: `is_move_constructible_v` may report `1` because it can fall back to the copy ctor — the point is there's **no dedicated move**, so "moves" copy the vector.</details>

### B7
```cpp
std::weak_ptr<int> w;
{
    auto s = std::make_shared<int>(5);
    w = s;
    std::cout << (bool)w.lock() << " ";
}
std::cout << (bool)w.lock() << " " << w.expired();
```
<details><summary>Answer</summary>`1 0 1` — inside the scope `lock()` yields a valid `shared_ptr`; outside, the object is gone → `lock()` empty, `expired()` true.</details>

---

## PART B2 — What happens next?

Yahan sawaal ek khaas pal ka hai — **`}` pe, ya exception ke waqt, AGLA kaunsa constructor /
destructor chalega?** Pehle order likho, phir chalao. Neeche ke jawab GCC 16.2 pe is class
ke saath chala ke liye gaye hain:

```cpp
struct Logger {
    std::string n;
    explicit Logger(std::string s) : n(std::move(s)) { std::cout << "+" << n << " "; }
    ~Logger() { std::cout << "~" << n << " "; }
};
```

### N1
```cpp
{
    Logger a("A");
    Logger b("B");
    std::cout << "| ";
}                             // <- ab kaunsa destructor pehle?
```
<details><summary>Answer</summary>

`+A +B | ~B ~A` — destructors construction ke **ulte order** mein chalte hain. `b` baad mein
bana tha, isliye pehle marta hai. Isi wajah se RAII mein "jo baad mein acquire hua, woh pehle
release" apne aap hota hai (file 02) — jaise pehle lock, phir file: pehle file band, phir unlock.
</details>

### N2
```cpp
struct Two {
    Logger x{"X"};
    Logger y{"Y"};
    Two()  { std::cout << "[Two body] "; throw 1; }   // <- body mein exception
    ~Two() { std::cout << "~Two "; }
};
try { Two t; } catch (int) { std::cout << "caught"; }
```
<details><summary>Answer</summary>

`+X +Y [Two body] ~Y ~X caught` — `~Two` **kabhi nahi chala**. Constructor poora nahi hua, to
`Two` object bana hi nahi (file 02), isliye uska destructor nahi chalta. Par jo **members**
poore ban chuke the (`x`, `y`), unke destructors ulte order mein chal gaye. Yahi wajah hai ki
resources ko members (RAII objects) mein rakhte hain, constructor body mein raw `new` karke
nahi — raw pointer ka koi destructor nahi hota, leak ho jaata. (Stack unwinding ki poori detail:
folder 23 file 04.)
</details>

### N3
```cpp
{
    auto p = std::make_unique<Logger>("P");
    std::cout << "| ";
    p = std::make_unique<Logger>("Q");   // <- P pehle marega ya Q pehle banega?
    std::cout << "| ";
}
```
<details><summary>Answer</summary>

`+P | +Q ~P | ~Q` — assignment ka **right side pehle poora evaluate** hota hai, isliye `Q`
pehle bana. Phir `unique_ptr` ne purana `P` delete kiya aur `Q` ko pakad liya. Scope ke end
pe `Q` gaya. Matlab replace karte waqt ek pal ke liye dono objects zinda the — bade objects
ke saath yeh memory peak ka kaaran ban sakta hai.
</details>

---

## PART C — Find the bug

### C1
```cpp
void readFile(const char* path) {
    FILE* f = std::fopen(path, "r");
    char buf[256];
    if (std::fgets(buf, 256, f) == nullptr) return;
    process(buf);
    std::fclose(f);
}
```
<details><summary>Answer</summary>Two bugs: (1) no null check on `f` after `fopen` → if it fails, `fgets(nullptr)` = UB. (2) the early `return` skips `fclose(f)` → fd leak. Fix: `FilePtr f{std::fopen(path,"r")}; if (!f) return;` — RAII closes on every path.</details>

### C2
```cpp
class Buffer {
    char* data_;
    size_t size_;
public:
    Buffer(size_t n) : data_(new char[n]), size_(n) {}
    ~Buffer() { delete[] data_; }
};
Buffer a{1024};
Buffer b = a;
```
<details><summary>Answer</summary>Rule of Three violated — default copy ctor does `b.data_ = a.data_` (shallow) → both destructors `delete[]` the same buffer → double-free. Fix: `std::vector<char> data_;` member (Rule of Zero), or write deep copy ctor + copy assign + move ops.</details>

### C3
```cpp
struct S : std::enable_shared_from_this<S> {
    std::shared_ptr<S> getSelf() { return std::shared_ptr<S>(this); }
};
auto a = std::make_shared<S>();
auto b = a->getSelf();
```
<details><summary>Answer</summary>`std::shared_ptr<S>(this)` creates a **second, independent** control block. Now `a` and `b` each think they own the object with count 1 → when both drop, the object is deleted **twice**. Fix: `return shared_from_this();` (uses the existing control block).</details>

### C4
```cpp
void handle(std::shared_ptr<Connection> c) {
    for (int i = 0; i < 1'000'000; ++i)
        c->send(makeHeartbeat());
}
```
<details><summary>Answer</summary>`c` is passed **by value** → one atomic refcount increment on entry, one decrement on return. Not per-iteration (that would be worse), but still needless — and it lies about ownership (the function doesn't keep `c`). Fix: `void handle(Connection& c)` or `const std::shared_ptr<Connection>& c`.</details>

### C5
```cpp
class Fd {
    int fd_;
public:
    explicit Fd(int fd) : fd_(fd) {}
    ~Fd() { ::close(fd_); }
    Fd(Fd&& o) : fd_(o.fd_) {}
};
Fd a{open("x", O_RDONLY)};
Fd b = std::move(a);
```
<details><summary>Answer</summary>The move ctor copies `o.fd_` but **doesn't null it** → after `Fd b = std::move(a);`, both `a.fd_` and `b.fd_` hold the same descriptor → both destructors `::close()` it → double-close (may close an unrelated fd reopened with the same number). Fix: `Fd(Fd&& o) noexcept : fd_(o.fd_) { o.fd_ = -1; }` and guard the dtor `if (fd_ >= 0)`.</details>

### C6
```cpp
std::vector<Widget*> widgets;
for (int i = 0; i < 10; ++i)
    widgets.push_back(new Widget(i));
// ... use widgets ...
widgets.clear();
```
<details><summary>Answer</summary>`std::vector<Widget*>` is non-owning — `clear()` destroys the pointers, not the `Widget`s → 10 leaks. Fix: `std::vector<std::unique_ptr<Widget>>` (or `std::vector<Widget>` if no polymorphism) — then `clear()`/scope-end deletes them.</details>

### C7
```cpp
class Session {
    std::unique_ptr<Impl> pimpl_;
public:
    Session() : pimpl_(std::make_unique<Impl>()) {}
    ~Session() = default;   // in session.hpp, where Impl is only forward-declared
};
```
<details><summary>Answer</summary>`~Session() = default;` in the header instantiates `~unique_ptr<Impl>()` there, which needs `Impl` **complete** to `delete` it — but `Impl` is only forward-declared → compile error ("invalid application of sizeof to incomplete type" / "deleting incomplete type"). Fix: declare `~Session();` in the header, `Session::~Session() = default;` in `session.cpp` where `Impl` is defined.</details>

### C8
```cpp
std::shared_ptr<Texture> load(const std::string& name) {
    static std::map<std::string, std::shared_ptr<Texture>> cache;
    if (auto it = cache.find(name); it != cache.end()) return it->second;
    auto t = std::make_shared<Texture>(name);
    cache[name] = t;
    return t;
}
```
<details><summary>Answer</summary>The cache holds `shared_ptr` → it keeps **every** texture alive forever (unbounded growth; textures never freed even when no real user holds them). Fix: `std::map<std::string, std::weak_ptr<Texture>> cache;` — store `weak_ptr`, `cache[name].lock()` to check, so the cache doesn't own the textures.</details>

---

## PART D — Write it

### D1 — `Fd` (move-only RAII)
Full `class Fd` per lesson 09: ctor from `int`, dtor `::close` (guarded), move
ctor + move assign (steal + null), deleted copy, `get()`, `explicit operator
bool`, `release()`. Test double-move, self-move-assign.

### D2 — `ScopeGuard`
`template <class F> class ScopeGuard { F f_; bool active_ = true; public: explicit
ScopeGuard(F f); ~ScopeGuard(); void dismiss() noexcept; };` — non-copyable,
movable. Demo the "rollback unless committed" pattern.

### D3 — `SharedSnapshot` reader/writer
A `Publisher` holding `std::shared_ptr<const Config>`; `read()` returns `const
Config*` (borrow); `publish(std::unique_ptr<Config>)` swaps in a new snapshot.
Readers must not touch the refcount.

### D4 — `ObjectPool<T>` with RAII handles
A fixed-size pool (folder 14) that hands out a `Pool::Handle` — a move-only RAII
type whose destructor returns the slot to the pool. `auto h = pool.acquire();`
→ slot auto-returned at scope end. `h->method()` accesses the object.

### D5 — `Timer` guard
`class ScopedTimer { const char* name_; steady_clock::time_point start_; public:
... ~ScopedTimer() { print elapsed µs }; };` — non-copyable. `{ ScopedTimer
t{"parse"}; parse(); }` times the block automatically.

### D6 — Rule of Zero refactor
Take a given `class NetworkBuffer` with `char* + size_t + FILE* logFile` and
Rule-of-Five boilerplate; refactor to `std::vector<char>` + `FilePtr` members
with an empty class body (beyond the constructor). Show the line-count drop and
that copy/move are still correct.

---

## PART E — HFT angle

1. **Where smart pointers live:** for a trading process, list which objects use
   `unique_ptr`, which use `shared_ptr` (if any), and which use neither (pool +
   raw). Justify each.

2. **`shared_ptr` profile hit:** a profiler shows 8% of hot-loop time in
   `__shared_ptr::_M_release` (atomic dec + branch). What's the fix — tune or
   redesign? Sketch it.

3. **RAII for OS resources:** name 5 OS resources a low-latency process
   acquires at startup and the RAII wrapper for each (`Fd`, `MmapRegion`,
   `ScopedAffinity`, ...). Why does each error path need RAII?

4. **`-fno-exceptions` + RAII:** does RAII still work with exceptions disabled?
   What changes? Why do some HFT shops build the trading loop `-fno-exceptions`?

5. **Snapshot lifetime:** a market-data `Snapshot` is read by 4 strategy
   threads. `shared_ptr<Snapshot>` copied into each thread vs one
   `shared_ptr` + `const Snapshot*` per epoch — cost difference on the read
   path, and how the old snapshot is safely reclaimed.

---

## PART F — Challenge

**"`PooledPtr<T>` — a zero-atomic owning smart pointer backed by a pool"**

Design a smart pointer for the hot path that has `unique_ptr`-like ergonomics
but never calls `new`/`delete` or touches an atomic:

- `class FixedPool<T, N>` (folder 14 style): `N` pre-allocated, aligned slots +
  an intrusive free list. `PooledPtr<T> acquire(args...)` placement-constructs a
  `T` in a free slot; returns a move-only `PooledPtr<T>`.
- `PooledPtr<T>`: holds `T* obj_` + `FixedPool* pool_`. `operator->`, `operator*`,
  `get()`, `explicit operator bool`, `release()`, `reset()`. Move-only (steal +
  null). Destructor: `obj_->~T(); pool_->free(obj_);` — **no `delete`, no
  atomic**.
- `sizeof(PooledPtr<T>)` — measure (16: two pointers). Discuss shrinking to 8
  (store a slot index + a global pool, or a tagged pointer).
- Benchmark (`-O2`, rdtsc): `PooledPtr` acquire+release vs `make_unique` vs
  `make_shared`, in a hot loop. Expect `PooledPtr` ≈ pool alloc (~1-5 ns) vs
  `make_unique` ~100+ ns.
- `#ifdef BUG` variant: forget to null `obj_` in the move ctor → show the
  double-free-into-pool corruption (a debug `assert` in `pool_->free` catches a
  double free of the same slot).
- A `PooledPtr<Base>` variant for polymorphic pooled objects — needs the pool to
  know how to destroy + which slot; discuss the design (per-type pools, or a
  virtual `~Base()` + a slot-size check).

Yeh folder 14 (pools), 15/16 (classes, virtual), 18 (move — you'll formalize the
move ops), aur is folder ko jodta hai — aur folder 44 (HFT projects: object
pool) ka direct seed hai.

---

## Next
→ [`../18-COPY-MOVE/00-README.md`](../18-COPY-MOVE/00-README.md)
