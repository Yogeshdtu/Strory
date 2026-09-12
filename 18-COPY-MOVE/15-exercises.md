# 15 — Folder 18 Revision + Exercises

## Prerequisites
Lessons 01–14 aur saare 9 examples chalaye hue.

---

## PART A — Concept check

1. Copy ctor kab chalta (4 situations)? `Widget b = a;` — construction ya assignment?
2. Default copy ctor — member-wise, shallow kab problem?
3. Deep vs shallow copy — raw pointer member ke saath?
4. Copy assignment — kya extra karna (release old, self-check, exception-safe)?
5. Copy-and-swap idiom — ek function copy+move dono kaise serve karta?
6. Rule of Three — kaunse 3, kyun ek saath?
7. Value categories — lvalue/prvalue/xvalue, 2-question model?
8. glvalue vs rvalue?
9. "Named rvalue reference is an lvalue" — practical impact?
10. `T&&` kis se bind hota, `const T&` se kaise alag?
11. Forwarding reference vs plain rvalue reference — kaise pehchano?
12. Reference collapsing rules?
13. Move ctor kya steal karta, moved-from state ka guarantee?
14. Move ctor mein har member init pe `std::move` kyun?
15. Move assignment — self-move, release-old?
16. `std::move` actually kya hai? Runtime cost?
17. `std::move` on `const` — kya hota?
18. `return std::move(x)` — kyun bura?
19. Rule of Five — 5 members, generation rules table (dtor declare → moves ka kya)?
20. `= delete` copy, no move declared → type movable?
21. `= default` — 2 reasons to use?
22. Rule of Zero — move semantics ke liye kya deta?
23. `std::mutex`/`std::atomic` member — kya problem?
24. RVO vs NRVO — kaunsa guaranteed (C++17)?
25. Guaranteed prvalue elision — copy/move ctor ki zaroorat?
26. NRVO kab **nahi** apply hota?
27. `std::forward` vs `std::move` — conditional vs unconditional?
28. `std::vector` realloc — move ya copy, kis pe depend (`noexcept`)?
29. `is_move_constructible` vs `is_nothrow_move_constructible`?
30. 5 places move DEFINITELY happens; 5 places it silently copies?

---

## PART B — Output prediction

### B1
```cpp
struct T {
    T()              { puts("ctor"); }
    T(const T&)      { puts("copy"); }
    T(T&&) noexcept  { puts("move"); }
    ~T()             { puts("dtor"); }
};
T make() { return T{}; }
int main() { T a = make(); }
```
<details><summary>Answer</summary>`ctor`, `dtor` — guaranteed prvalue elision: `T{}` constructed directly in `a`. No copy, no move.</details>

### B2
```cpp
std::string s = "a fairly long string value";
auto&& r = std::move(s);
std::cout << s.size() << " ";
std::string t = std::move(s);
std::cout << s.size() << " " << t.size();
```
<details><summary>Answer</summary>`26 0 26` — `std::move(s)` alone changes nothing (26). `std::string t = std::move(s)` runs the move ctor → `s` emptied (0), `t` has the 26 chars.</details>

### B3
```cpp
struct A { std::string s; ~A() {} };
std::cout << std::is_nothrow_move_constructible_v<A> << " "
          << std::is_trivially_copyable_v<A>;
```
<details><summary>Answer</summary>`1 0` — the `~A() {}` suppresses the implicit move ctor, but `is_nothrow_move_constructible_v<A>` is still `true` (satisfied by the `noexcept` copy ctor from the `std::string` member). It's **not** trivially copyable (user dtor). Trap: the trait says "true" but `std::vector<A>` realloc still *copies* (no move ctor exists).</details>

### B4
```cpp
const std::vector<int> src = {1, 2, 3};
std::vector<int> dst = std::move(src);
std::cout << src.size() << " " << dst.size();
```
<details><summary>Answer</summary>`3 3` — `std::move(src)` is `const std::vector<int>&&`; the move ctor can't bind → the **copy** ctor runs. `src` unchanged, `dst` is a deep copy.</details>

### B5
```cpp
struct Buf {
    int* p; size_t n;
    Buf(size_t n_) : p(new int[n_]), n(n_) {}
    ~Buf() { delete[] p; }
    Buf(Buf&& o) noexcept : p(o.p), n(o.n) { o.p = nullptr; o.n = 0; }
    Buf& operator=(Buf&&) noexcept = default;   // <-- note
};
int main() { Buf a{4}; Buf b{2}; b = std::move(a); }
```
<details><summary>Answer</summary>**Double-free / corruption.** `operator=(Buf&&) = default` does member-wise move assign: `p = std::move(o.p)` (just `p = o.p` for a raw pointer — no null-out!) and `n = o.n`. `b`'s original `p` is **leaked**, and `a.p` is **not nulled** → both `~a` and `~b` `delete[]` the same pointer. The `= default` move assign is wrong for a raw pointer member — write it by hand (release old, steal, null source) or use `std::vector<int>`.</details>

### B6
```cpp
std::string upper(std::string s) {
    for (auto& c : s) c = std::toupper(c);
    return std::move(s);
}
int main() { std::cout << upper("hello"); }
```
<details><summary>Answer</summary>Prints `HELLO`, but the compiler emits `-Wpessimizing-move` on `return std::move(s)`. `s` is a by-value parameter — `return s;` would implicitly move it (params aren't NRVO'd but are treated as rvalues on return). `std::move` here blocks nothing extra but is flagged as a pessimizing habit.</details>

### B7
```cpp
template <class T> void relay(T&& x) { sink(x); }
void sink(const std::string&) { puts("const&"); }
void sink(std::string&&)      { puts("&&"); }
int main() {
    std::string s = "x";
    relay(s);
    relay(std::move(s));
    relay(std::string("y"));
}
```
<details><summary>Answer</summary>`const&`, `const&`, `const&` — inside `relay`, `x` is a **named** parameter → an lvalue → `sink(x)` always picks `sink(const std::string&)`. To preserve the category: `sink(std::forward<T>(x))` → would give `const&`, `&&`, `&&`.</details>

---

## PART C — Find the bug

### C1
```cpp
class Str {
    char* p_;
public:
    Str(const char* s) : p_(new char[strlen(s) + 1]) { strcpy(p_, s); }
    ~Str() { delete[] p_; }
};
Str a{"hello"};
Str b = a;
```
<details><summary>Answer</summary>Rule of Three violated — no copy ctor → the generated one shallow-copies `p_` → `b.p_ == a.p_` → both destructors `delete[]` it → double-free. Fix: add a deep-copy ctor + copy assign (+ move ops for Rule of Five), or use `std::string p_;` (Rule of Zero).</details>

### C2
```cpp
class Buffer {
    std::vector<int> data_;
public:
    explicit Buffer(size_t n) : data_(n) {}
    ~Buffer() { std::cout << "destroyed a Buffer\n"; }
};
std::vector<Buffer> buffers;
for (int i = 0; i < 100000; ++i) buffers.emplace_back(1024);
```
<details><summary>Answer</summary>The `~Buffer()` (added just to log) **suppresses the implicit move ctor**. On each reallocation, `std::vector<Buffer>` **copies** every existing `Buffer` (deep-copying `data_`) instead of moving. Silent ~N-log(N) allocation storm. Fix: remove the logging destructor (Rule of Zero), or `= default` all five special members.</details>

### C3
```cpp
Widget::Widget(Widget&& o) noexcept
    : name_(o.name_),
      tags_(o.tags_) {
    o.id_ = 0;
}
```
<details><summary>Answer</summary>`name_(o.name_)` and `tags_(o.tags_)` — `o.name_`/`o.tags_` are lvalues (named) → these are **copy** constructions, not moves. This "move ctor" deep-copies. Fix: `name_(std::move(o.name_)), tags_(std::move(o.tags_))`.</details>

### C4
```cpp
void process(Config c);
Config cfg = loadConfig();
process(cfg);
process(cfg);
saveConfig(cfg);
```
<details><summary>Answer</summary>Each `process(cfg)` copies the whole `Config` (by-value param, lvalue argument). If `Config` is large this is 2 needless deep copies. Only the *last* use can be moved: `process(cfg); process(std::move(cfg)); /* don't use cfg after */`. But there's a `saveConfig(cfg)` after — so you can't move it; instead change `process` to take `const Config&`.</details>

### C5
```cpp
class Socket {
    int fd_ = -1;
public:
    explicit Socket(int fd) : fd_(fd) {}
    ~Socket() { if (fd_ >= 0) ::close(fd_); }
    Socket(Socket&& o) noexcept : fd_(o.fd_) {}
};
Socket a{5};
Socket b = std::move(a);
```
<details><summary>Answer</summary>The move ctor copies `o.fd_` but doesn't null it → `a.fd_` is still `5` → both `~a` and `~b` call `::close(5)` → double-close (may close an unrelated fd reopened as 5). Fix: `Socket(Socket&& o) noexcept : fd_(o.fd_) { o.fd_ = -1; }`. Also declare copy ops `= delete` and add move assignment (Rule of Five).</details>

### C6
```cpp
std::vector<std::unique_ptr<Task>> tasks;
// ... fill tasks ...
for (auto t : tasks)
    scheduler.run(t);
```
<details><summary>Answer</summary>`for (auto t : tasks)` — `auto` (by value) tries to **copy** each `std::unique_ptr<Task>` → compile error (unique_ptr is non-copyable). Fix: `for (const auto& t : tasks) scheduler.run(*t);` (borrow), or if `run` takes ownership: `for (auto& t : tasks) scheduler.run(std::move(t));` (and know `tasks` is now full of nulls).</details>

### C7
```cpp
struct Message {
    std::string topic;
    std::vector<std::byte> body;
    Message(Message&& o) : topic(std::move(o.topic)), body(std::move(o.body)) {}
    Message& operator=(Message&&) = default;
    ~Message() = default;
};
static_assert(std::is_nothrow_move_constructible_v<Message>);
```
<details><summary>Answer</summary>The `static_assert` **fails** — the hand-written move ctor is missing `noexcept`. `std::string`/`std::vector` moves are `noexcept`, so the ctor *could* be `noexcept`, but you didn't say so → `std::vector<Message>` growth will copy. Fix: `Message(Message&& o) noexcept : ...`. (Also: with a user move ctor + user move assign + user dtor, copy ops are deleted — `Message` is move-only, which is probably intended. Simpler: delete all three user declarations → Rule of Zero → correct `noexcept` moves for free.)</details>

### C8
```cpp
std::string join(const std::vector<std::string>& parts) {
    std::string result;
    for (const auto& p : parts) result += p;
    return std::move(result);
}
```
<details><summary>Answer</summary>`return std::move(result)` — `-Wpessimizing-move`. `result` is a named local → `return result;` gets NRVO (constructed in the caller's slot, zero copy/move) or at worst an implicit move. `std::move` here *disables* NRVO, guaranteeing a move ctor call. Fix: `return result;`.</details>

---

## PART D — Write it

### D1 — Rule of Five `String`
A `class String` owning `char* data_` + `size_t len_`. Write all five: ctor from
`const char*`, dtor, deep copy ctor, exception-safe deep copy assign (allocate
first), `noexcept` move ctor (steal + null), `noexcept` move assign (release +
steal + null + self-guard). `static_assert` both `is_nothrow_move_*`.

### D2 — Rule of Zero rewrite
Rewrite D1 with a `std::string` member and **zero** special members. Verify:
still compiles, `is_nothrow_move_constructible_v` is true, now copyable too.

### D3 — value-category probe
Reproduce `examples/03_value_categories.cpp`'s `probe(T&)` / `probe(const T&)` /
`probe(T&&)` overload set. Feed it: a named var, `std::move(var)`, a literal
temp, `a + b`, a `const` var, `std::move(constVar)`. Predict then verify.

### D4 — perfect-forwarding factory
`template <class T, class... Args> T* make_in(void* storage, Args&&... args) {
return new (storage) T(std::forward<Args>(args)...); }`. Test with a type whose
ctor prints "copy" vs "move", passing a temporary and an lvalue.

### D5 — `noexcept` demonstration
Two classes differing only by `noexcept` on the move ctor. `push_back` 100k
elements (each with a ~1KB buffer) into a `std::vector` of each, no `reserve`.
Time both, print the ratio, print `is_nothrow_move_constructible_v` for each.

### D6 — sink-parameter setter
`class Widget { std::string name_; std::vector<int> data_; public: void
setName(std::string); void setData(std::vector<int>); };` — implement the setters
with by-value + `std::move`. Trace allocations for `w.setName("x")`,
`w.setName(namedStr)`, `w.setName(std::move(namedStr))`.

---

## PART E — HFT angle

1. **Element type checklist:** for any type `T` stored in a `std::vector<T>`
   that may grow, list the properties you'd `static_assert` and why (`noexcept`
   move ctor, `noexcept` move assign, maybe `is_trivially_copyable`).

2. **Return by value is free:** show that `std::vector<Order> parse(std::span<const
   std::byte>)` returning by value costs zero copies (guaranteed elision / NRVO),
   vs a C-style `void parse(std::span<...>, std::vector<Order>& out)`.

3. **`shared_ptr` vs move:** a `Snapshot` handed to 3 threads. Copying a
   `shared_ptr<Snapshot>` per thread (3 atomic incs) vs moving a
   `unique_ptr<Snapshot>` to one owner + `const Snapshot*` to the others. When is
   each right?

4. **Logging-destructor regression:** you add `~OrderBook() { dumpStats(); }`.
   `std::vector<OrderBook>` was moving on realloc; now it copies. How would you
   catch this in CI (a `static_assert`, a test, a compiler flag)?

5. **Move in a hot loop?** the innermost market-data loop rarely moves objects.
   Explain why (pre-owned pooled memory, in-place updates) and where move *does*
   show up (batch handoff to a queue, `std::vector` of parsed messages returned
   from a decode function).

---

## PART F — Challenge

**"`SmallVector<T, N>` — a move-aware small-buffer-optimized vector"**

Build a `SmallVector<T, N>`: stores up to `N` elements inline (in a
`std::aligned_storage`-like buffer), spills to the heap beyond that. This forces
you to get every special member right:

- **Storage:** `union`-like: `alignas(T) std::byte inline_[N * sizeof(T)]` OR a
  heap pointer; a `size_` and `capacity_`; a flag / `capacity_ > N` to know
  which.
- **Rule of Five, by hand** (you can't Rule-of-Zero this — raw storage):
  - dtor: destroy `size_` elements, free heap buffer if spilled.
  - copy ctor / copy assign: element-wise copy into the right storage; deep.
  - move ctor / move assign, **`noexcept` if `T`'s move is noexcept**: if the
    source spilled → steal its heap pointer (O(1)); if inline → **element-wise
    move** each `T` into our inline buffer (O(size)) + destroy the source's. Null
    the source's size / pointer.
- `push_back(const T&)` and `push_back(T&&)`; `emplace_back(Args&&...)` with
  perfect forwarding + placement new. Grow: inline → heap on the (N+1)th element.
- `static_assert(std::is_nothrow_move_constructible_v<SmallVector<T,N>> == std::is_nothrow_move_constructible_v<T>)`.
- Benchmark (`-O2`): `SmallVector<int, 16>` vs `std::vector<int>` for
  workloads that fit inline (no allocation) vs spill. Show the inline case does
  zero allocations.
- `#ifdef BUG`: a move ctor that forgets to null the source's spilled pointer →
  show the double-free (a counted `operator new`/`delete`, or a debug assert).
- Discuss: why is `SmallVector`'s move ctor **not** always `noexcept` even when
  `std::vector`'s is? (Element-wise move of inline elements can call `T`'s move,
  which for a non-`noexcept`-move `T` could throw.)

Yeh folder 14 (placement new, aligned storage), 15/16 (classes, layout, RAII), 17
(RAII), aur is folder ki har cheez ko jodta hai — aur `folly::small_vector` /
`llvm::SmallVector` jaise real-world types ka miniature hai.

---

## Next
→ [`../19-STL/00-README.md`](../19-STL/00-README.md)
