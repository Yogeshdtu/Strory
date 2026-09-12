# 14 — Folder 15 Revision + Exercises

## Prerequisites
Lessons 01–13 aur saare 8 examples chalaye hue.

---

## PART A — Concept check

1. Class vs struct — technical fark + convention?
2. Encapsulation kya, 3 fayde? Invariant kya?
3. Class vs object — analogy + technical?
4. `this` pointer — type, kab available (static?)?
5. Method body mein naam resolution ka order?
6. `return *this` — kyun `T&`, `T` nahi?
7. `public`/`private`/`protected` — kaun kya access karta?
8. Access control runtime pe enforce hota? Ek object doosre same-class ka private?
9. Mixed access aur standard-layout ka relation?
10. Compiler default constructor kab deta, kab nahi?
11. `= default` vs `= delete` — kab kaunsa?
12. Delegating constructor — rules, execution order?
13. Member init list vs body assignment — 2 farak?
14. Members kis order mein init hote? Init-list ka likha order?
15. Destructor kab chalta (4 situations)? Multiple locals — order?
16. Rule of Three/Five — destructor likha to aur kya?
17. `const` method kya guarantee karta? `this` ka type?
18. `mutable` — legit use, misuse?
19. Static data member — storage, `sizeof` pe asar? `inline static` (C++17) kya solve karta?
20. Static method — `this`? Kya access kar sakta? Use case?
21. Operator overload rules (3 constraints)? `+=` member vs `+` free — kyun?
22. `operator<<` free kyun, return type kya kyun?
23. C++20 `<=>` `= default` — kya deta?
24. `explicit` — kya rokta, runtime cost? "by default" rule?
25. `friend` — runtime cost? mutual/inherited/transitive?
26. Nested class — enclosing object se relation? enclosing ke private dekh sakti?
27. Empty class `sizeof`? EBO — kya, kyun?
28. `virtual` add karne se layout/`sizeof`/triviality pe kya asar? (folder 16 preview)

---

## PART B — Output prediction

### B1
```cpp
struct T { T(int i):id(i){std::cout<<"c"<<id<<" ";} ~T(){std::cout<<"d"<<id<<" ";} int id; };
int main() { T a{1}; { T b{2}; T c{3}; } T d{4}; }
```
<details><summary>Answer</summary>`c1 c2 c3 d3 d2 c4 d4 d1` — block ke `}` pe `c`(3),`b`(2) reverse; phir `d`; main end pe `a`.</details>

### B2
```cpp
class C {
    int a_;
    int b_;
public:
    C(int x) : b_(x), a_(b_ * 2) {}
    void p() const { std::cout << a_ << " " << b_; }
};
int main() { C(10).p(); }
```
<details><summary>Answer</summary>`-Wreorder` + `a_` uninitialized. `a_` init first (declared first): `a_(b_*2)` with `b_` garbage → garbage. Then `b_ = 10`. Output: `<garbage> 10`.</details>

### B3
```cpp
class Counter {
    static inline int s_total = 0;
    int local_ = 0;
public:
    void tick() { ++local_; ++s_total; }
    int local() const { return local_; }
    static int total() { return s_total; }
};
int main() {
    Counter a, b;
    a.tick(); a.tick(); b.tick();
    std::cout << a.local() << " " << b.local() << " " << Counter::total();
}
```
<details><summary>Answer</summary>`2 1 3` — `local_` per-object, `s_total` shared.</details>

### B4
```cpp
class Money {
    long long c_;
public:
    explicit Money(long long c) : c_(c) {}
    Money& operator+=(Money o) { c_ += o.c_; return *this; }
    long long c() const { return c_; }
};
Money operator+(Money a, Money b) { a += b; return a; }
int main() {
    Money x{100}, y{50};
    std::cout << (x + y).c() << " ";
    // Money z = 200;   // compile?
    std::cout << (x += y).c() << " " << x.c();
}
```
<details><summary>Answer</summary>`150 150 150`. `Money z = 200;` → compile ERROR (`explicit`). `x += y` returns `x&` → both print 150.</details>

### B5
```cpp
struct Empty {};
struct AsMember { Empty e; int n; };
struct AsBase : Empty { int n; };
int main() { std::cout << sizeof(Empty) << " " << sizeof(AsMember) << " " << sizeof(AsBase); }
```
<details><summary>Answer</summary>`1 8 4` — Empty is 1; as member 1+3pad+4=8; as base EBO → 0+4=4.</details>

### B6
```cpp
class R {
    std::vector<int> v_;
    mutable int calls_ = 0;
public:
    explicit R(std::vector<int> v) : v_(std::move(v)) {}
    int sum() const { ++calls_; int s = 0; for (int x : v_) s += x; return s; }
    int calls() const { return calls_; }
};
int main() {
    const R r{{1, 2, 3}};
    r.sum(); r.sum();
    std::cout << r.sum() << " " << r.calls();
}
```
<details><summary>Answer</summary>`6 3` — `sum()` const via `mutable calls_`; called 3x on a `const` object (allowed — const method).</details>

---

## PART C — Find the bug

### C1
```cpp
class Buffer {
    int* data_;
    std::size_t n_;
public:
    Buffer(std::size_t n) : data_(new int[n]), n_(n) {}
    ~Buffer() { delete[] data_; }
};
int main() { Buffer a{10}; Buffer b = a; }
```
<details><summary>Answer</summary>Default copy ctor → `b.data_ == a.data_` (shallow). Both destructors `delete[]` same pointer → double-free. Fix: `= delete` copy, ya deep-copy ctor + copy-assign (Rule of 3), ya `std::vector<int> data_;` (Rule of Zero).</details>

### C2
```cpp
class Logger {
public:
    Logger(std::string file);
};
void f() { Logger log("app.log"); }
class Config { public: Config(int); };
Config c;
```
<details><summary>Answer</summary>`Config c;` → no default ctor (a ctor was declared). Fix: `Config() = default;` ya `Config() : ... {}`. Also `Logger(std::string)` should be `explicit` (single-arg) to avoid `Logger l = "x";`.</details>

### C3
```cpp
class Temperature {
    double celsius_;
public:
    Temperature(double c) : celsius_(c) {}
    void raise(double d) { celsius_ += d; }
};
void warm(Temperature t);
int main() { warm(37.5); }
```
<details><summary>Answer</summary>`warm(37.5)` compiles via implicit `double`→`Temperature` — unit/intent unclear. Add `explicit Temperature(double)`; call `warm(Temperature{37.5})`. (Also no negative-clamp invariant, but the flagged bug is the implicit conversion.)</details>

### C4
```cpp
class Widget {
    int x_;
public:
    void set(int x_) { x_ = x_; }
    int  get() const { return x_; }
};
```
<details><summary>Answer</summary>Parameter `x_` shadows member `x_`; `x_ = x_;` assigns param to itself (`-Wself-assign`), member unchanged. Fix: rename param, or `this->x_ = x_;`. (Repo convention: member `x_`, param `x` — no clash.)</details>

### C5
```cpp
class Query {
    std::string sql_;
public:
    Query select(const std::string& c) { sql_ += "SELECT " + c; return *this; }
    Query from  (const std::string& t) { sql_ += " FROM " + t; return *this; }
    const std::string& str() const { return sql_; }
};
int main() {
    Query q;
    q.select("*").from("orders");
    std::cout << q.str();
}
```
<details><summary>Answer</summary>Methods return `Query` **by value** → `.from(...)` runs on a temporary copy, not `q`. `q.str()` == `"SELECT *"` only. Fix: return `Query&`.</details>

### C6
```cpp
class Registry {
    static std::vector<std::string> names_;
public:
    static void add(std::string n) { names_.push_back(std::move(n)); }
};
int main() { Registry::add("a"); }
```
<details><summary>Answer</summary>`static std::vector<std::string> names_;` is only a **declaration** — no definition → linker error `undefined reference to Registry::names_`. Fix: `static inline std::vector<std::string> names_;` (C++17), or define in a `.cpp`.</details>

### C7
```cpp
struct WireMsg {
public:
    std::uint32_t seq;
private:
    std::uint16_t type;
public:
    std::uint16_t len;
};
static_assert(offsetof(WireMsg, len) == 6);
```
<details><summary>Answer</summary>Mixed access (`seq`/`len` public, `type` private) → `WireMsg` is **not standard-layout** → `offsetof` is `-Winvalid-offsetof` / UB, and the compiler may reorder members. Fix: all members same access (all public for a wire struct).</details>

### C8
```cpp
class Pool {
    char storage_[1024];
    std::size_t used_ = 0;
public:
    template <class T, class... A>
    T* make(A&&... a) { void* p = storage_ + used_; used_ += sizeof(T); return new (p) T(std::forward<A>(a)...); }
    ~Pool() {}   // ???
};
```
<details><summary>Answer</summary>`make` placement-constructs objects but `~Pool()` never calls their destructors (and doesn't check `used_ + sizeof(T) <= 1024`, and ignores alignment). For non-trivial `T` → resource leaks. Also no way to know which destructors to run. Fix: restrict to trivially-destructible `T` (`static_assert`), add bounds + alignment, or track constructed objects. (Arena semantics — folder 14 file 10.)</details>

---

## PART D — Write it

### D1 — `Fraction` class
Invariant: `den != 0`, always stored reduced (gcd), sign on numerator. Ctor
validates/reduces. `operator+`, `operator*`, `operator==`, `operator<<`.
`explicit Fraction(long)` for integers.

### D2 — `Stopwatch` class
`start()`, `stop()`, `elapsedNs() const`, `reset()`. Invariant: can't `stop()`
before `start()`. Use `std::chrono::steady_clock`. `mutable` not needed —
`elapsedNs` is genuinely const if you store start/stop points.

### D3 — `BoundedInt<Lo, Hi>` (non-type template preview OK, or hardcode)
A class wrapping an `int` that clamps to `[Lo, Hi]` on construction and on every
mutation. `explicit` ctor, `operator int() const` (explicit or not — justify),
`operator+=`.

### D4 — `Matrix2x2`
`double m_[4]` (or 4 named members). Ctor from 4 values. `operator*` (matrix
multiply), `determinant() const`, `transpose() const`, `operator<<`. All
read-only ops `const`. `static Matrix2x2 identity()`.

### D5 — `IntrusiveList` with nested `Node`
`class IntrusiveList { struct Node { int v; Node* next; }; Node* head_; };` —
nested `public` `Iterator`, `push_front`, `begin`/`end`, range-`for`. Destructor
frees nodes. Rule of 3/5 (or `= delete` copy).

### D6 — `Handle` (RAII sketch, no real resource)
`class Handle` wrapping an `int id_` (-1 = invalid). Ctor "acquires" (assigns an
id from a static counter), dtor "releases" (prints). `explicit operator bool()`.
Copy `= delete`. `int id() const`. (Move ops → folder 18.)

---

## PART E — HFT angle

1. **Zero-cost accessor:** `class Book { std::int64_t bestBid_, bestAsk_; public:
   std::int64_t bestBid() const { return bestBid_; } ... };` — `-O2 -S` se dekho
   ki `book.bestBid()` ek raw field load hai (koi call overhead nahi jab inline).

2. **`explicit` strong types:** `void sendOrder(Price, Qty, Side);` — teenon
   strong types `explicit` ctors ke saath. Kaunse bugs compile-time pakde jaate
   (list 3)?

3. **Trivially copyable class:** `class Quote` — private data, ctors, `const`
   accessors, par `static_assert(std::is_trivially_copyable_v<Quote>)`. Kaunse
   members / features yeh todenge (virtual, `std::string`, user dtor, ...)?

4. **`const`-correctness chain:** ek hot-path `onMarketData(const FeedMsg& m)` —
   har method jo `m` pe call ho `const` honi chahiye. Ek non-`const` method
   chain mein kya break karta?

5. **Static counter contention:** `static inline std::atomic<long> s_orders`
   incremented on every order across 4 threads. False-sharing / contention
   risk? `alignas(64)` ya `thread_local` counters + periodic aggregate — kab
   kaunsa?

---

## PART F — Challenge

**"`FixedPriceLevel` — a cache-tuned order-book level class"**

Ek `class PriceLevel` design karo jo ek order-book price level represent kare:

Requirements:
- **Private data:** `std::int64_t priceTicks_`, `std::uint64_t totalQty_`,
  `std::uint32_t orderCount_`, aur ek intrusive singly-linked list of order
  handles (`std::uint32_t headOrderIdx_`, `-1`/`UINT32_MAX` = empty) into an
  external order pool (folder 14).
- **Invariants (enforced):** `orderCount_ == 0  <=>  totalQty_ == 0  <=>  list
  empty`; `priceTicks_ > 0`.
- **Interface:** `addOrder(idx, qty)`, `removeOrder(idx, qty)`, `bool empty()
  const`, `std::int64_t price() const`, `std::uint64_t qty() const`,
  `std::uint32_t count() const`, iterator over order indices.
- **Layout:** `static_assert(std::is_trivially_copyable_v<PriceLevel>)` (so it
  can live in a `std::vector<PriceLevel>` / pool and be `memcpy`-relocated),
  `static_assert(sizeof(PriceLevel) <= 32)` (half a cache line — fit two per
  line), no `virtual`, all-private data but layout still tight.
- **`operator<=>`** by `priceTicks_` (for sorting the book), `operator<<` for
  logging (hidden friend).
- **A `#ifdef BUG` variant:** make `addOrder` update `totalQty_` but forget
  `orderCount_` — write an assertion / invariant-check method that catches it.
- Micro-bench: build a book of 100 levels, 1M add/remove ops, `-O2`, rdtsc
  per-op. Compare `PriceLevel` methods inlined vs `[[gnu::noinline]]`.

Yeh folder 11 (layout), 12 (pointers/indices), 14 (pools), aur is folder ko
jodta hai — aur folder 39 (order book) ka direct seed hai.

---

## Next
→ [`../16-OOP/00-README.md`](../16-OOP/00-README.md)
