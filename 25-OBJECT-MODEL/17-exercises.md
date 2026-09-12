# 17 — Exercises: object model & UB

## Prerequisites
- Poora folder 25 (files 01–16)

## Yeh file kya hai
Practice — output prediction, "find the UB", design, aur ek UB-hunting challenge.
Answers `<details>` mein. Compile: `./build.ps1 file.cpp`. UB hunting (Linux):
`g++ -O1 -g -fsanitize=address,undefined file.cpp`.

---

## Part A — Output / behaviour prediction

### A1
```cpp
struct T { const char* n; ~T(){ std::printf("~%s ", n); } int v(){ return 1; } };
int a = T{"x"}.v();
std::puts("mid");
{ const T& r = T{"y"}; std::puts("in"); }
std::puts("end");
```
<details><summary>Answer</summary>

`~x mid in ~y end`. The `T{"x"}` temporary dies at the `;` after `.v()` (before
`mid`). `const T& r = T{"y"}` extends `T{"y"}` to `r`'s scope, so `~y` runs after
`in`, at the block's `}`.
</details>

### A2
```cpp
alignas(int) unsigned char buf[sizeof(int)];
int* p = ::new (buf) int{7};
::new (buf) int{9};
std::printf("%d %d\n", *p, *std::launder(p));
```
<details><summary>Answer</summary>

Implementation-defined / UB for `*p` (stale pointer after reuse); `*std::launder(p)`
is `9`. In practice GCC often prints `9 9` for a plain `int` (mutable, no
const/vtable), but you should only rely on the `launder`ed access — or on the
pointer returned by the second placement-new.
</details>

### A3
```cpp
struct S { char a; int b; char c; };
std::printf("%zu %zu %zu %zu\n",
            sizeof(S), alignof(S), offsetof(S, b), offsetof(S, c));
```
<details><summary>Answer</summary>

`12 4 4 8`. `a` at 0, 3 pad, `b` at 4, `c` at 8, 3 tail pad → `sizeof` 12,
`alignof` 4.
</details>

### A4
```cpp
int overflow(int x) { return x + 1 > x; }
std::printf("%d\n", overflow(2147483647));
```
Compiled `-O2`, then `-O2 -fwrapv`.
<details><summary>Answer</summary>

`-O2`: `1` (signed overflow is UB → the compiler folds `x+1 > x` to always-true).
`-O2 -fwrapv`: `0` (signed overflow defined as wrap → `INT_MAX + 1` wraps to
`INT_MIN`, which is `< INT_MAX`). GCC actually folds to `1` at every `-O` without
`-fwrapv`.
</details>

### A5
```cpp
struct A { virtual ~A() = default; long x = 0; };
struct B { virtual ~B() = default; long y = 0; };
struct C : A, B {};
C c;
std::printf("%p %p %p\n", (void*)&c, (void*)(A*)&c, (void*)(B*)&c);
```
<details><summary>Answer</summary>

First two addresses are equal (`&c` and the `A` subobject at offset 0). The `B*`
address is `&c + 16` (after `A`'s vptr + `x`) — casting to a secondary base
*changes* the pointer value. `sizeof(C) == 32`.
</details>

### A6
```cpp
float f = 3.14f;
std::uint32_t viaCast   = *reinterpret_cast<std::uint32_t*>(&f);
std::uint32_t viaBitCst = std::bit_cast<std::uint32_t>(f);
std::printf("%08x %08x\n", viaCast, viaBitCst);
```
<details><summary>Answer</summary>

Both usually print `4048f5c3` at `-O0`. But `viaCast` is UB (strict-aliasing
violation) — in a larger function, with `-O2 -flto`, the optimizer can produce a
wrong/stale value. `viaBitCst` is always correct. Use `bit_cast`.
</details>

---

## Part B — Find the UB

### B1
```cpp
const std::string& first_word(const std::string& s) {
    return s.substr(0, s.find(' '));
}
```
<details><summary>Answer</summary>

`s.substr(...)` returns a **temporary `std::string`**; returning a `const&` to it
means the temporary is destroyed at the end of the `return` statement → the caller
gets a dangling reference. Return by value (`std::string`), or take a
`std::string_view` and return a `std::string_view` into the *caller's* string.
</details>

### B2
```cpp
struct Widget { int id; };
Widget* make() {
    Widget w{42};
    return &w;
}
```
<details><summary>Answer</summary>

Returns a pointer to a local (`w`), whose storage/lifetime ends at the `return`.
Any use of the returned pointer is UB (use-after-return). `-Wreturn-stack-address`
warns. Return by value, or allocate.
</details>

### B3
```cpp
int sum(const int* a, int n) {
    int s = 0;
    for (int i = 0; i <= n; ++i) s += a[i];
    return s;
}
```
<details><summary>Answer</summary>

Two UBs. (1) `i <= n` reads `a[n]` — one past the intended `[0, n)` → out-of-bounds
read. (2) If `n == INT_MAX`, `++i` overflows `i` → signed-overflow UB (and the
loop's termination relies on it). Use `i < n` and `std::size_t`/`int64_t` for the
counter if `n` can be huge.
</details>

### B4
```cpp
union U { float f; std::uint32_t bits; };
std::uint32_t float_bits(float x) {
    U u; u.f = x; return u.bits;
}
```
<details><summary>Answer</summary>

Reading `u.bits` after writing `u.f` reads the **inactive** union member — UB in
C++ (a GCC/Clang extension makes it "work"). Use `std::bit_cast<std::uint32_t>(x)`
or `std::memcpy`.
</details>

### B5
```cpp
struct Cfg { const int version; };
void bump(void* storage) {
    Cfg* p = static_cast<Cfg*>(storage);
    int old = p->version;
    ::new (storage) Cfg{old + 1};
    use(p->version);          // <-- ?
}
```
<details><summary>Answer</summary>

`p->version` after re-placement-new is UB — `p` is a stale pointer to the *old*
`Cfg`, and because `version` is `const`, the compiler may have cached/propagated
the old value. Use the pointer returned by placement new (`Cfg* q = ::new
(storage) Cfg{old+1}; use(q->version);`), or `std::launder(p)->version`.
</details>

### B6
```cpp
std::vector<int> v{1, 2, 3, 4, 5};
for (auto it = v.begin(); it != v.end(); ++it)
    if (*it % 2 == 0) v.push_back(*it * 10);
```
<details><summary>Answer</summary>

`push_back` can reallocate `v`, invalidating `it` **and** the `v.end()` recomputed
each iteration — actually `end()` is re-evaluated, but `it` becomes dangling after
a realloc → `++it` / `*it` are UB. Collect the values first, or `reserve` enough,
or iterate by index and append after the loop.
</details>

### B7
```cpp
bool ordered(const std::vector<Trade>& v) {
    return std::is_sorted(v.begin(), v.end(),
                          [](const Trade& a, const Trade& b){ return a.ts <= b.ts; });
}
```
<details><summary>Answer</summary>

`<=` is not a strict weak order (it's reflexive). `is_sorted` / `sort` with a
non-strict-weak-order comparator is UB. Use `a.ts < b.ts`.
</details>

---

## Part C — Design / explain

### C1
You're building a fixed-capacity `SmallVector<T, N>` with inline storage. Describe
the storage member, how `push_back` and `pop_back` manage element lifetimes, and
when the destructor can skip work.

<details><summary>Answer</summary>

Storage: `alignas(T) unsigned char buf[N * sizeof(T)];` + `std::size_t size_;`.
`push_back(x)`: if `size_ < N`, `::new (buf + size_ * sizeof(T)) T(x); ++size_;`.
`pop_back()`: `--size_; std::launder(reinterpret_cast<T*>(buf + size_ *
sizeof(T)))->~T();`. Access via `std::launder`. `~SmallVector()`: loop `pop_back()`
for the live elements — but can skip the loop entirely if
`std::is_trivially_destructible_v<T>`.
</details>

### C2
A wire message `struct MdQuote { std::uint64_t ts; std::int64_t bid, ask;
std::uint32_t bid_sz, ask_sz; std::uint8_t flags; };` must be `memcpy`'d in/out of
a byte buffer and `memcmp`'d for dedup. What properties must it have, what's wrong
as written, and how do you fix both?

<details><summary>Answer</summary>

Needs `is_trivially_copyable` (✓ as written) and `is_standard_layout` (✓) for the
`memcpy`. For `memcmp` dedup it needs `has_unique_object_representations` — **fails**
because of 3 padding bytes after `flags`. Fix: add `std::uint8_t _pad[3] = {};` (or
reorder), then `static_assert(sizeof(MdQuote) == 40)` and
`static_assert(std::has_unique_object_representations_v<MdQuote>)`. Also
`static_assert` `offsetof` for each field and endianness if relied on. (Better for
dedup: hash the fields individually — no padding concern.)
</details>

### C3
Your engine's hot dispatch does `if (auto* m = dynamic_cast<Market*>(o)) ...` per
tick. Give three faster designs and their trade-offs.

<details><summary>Answer</summary>

(1) **Tag enum + `switch` + checked `static_cast`** — 1 branch, but you own
correctness (assert the tag in debug). (2) **`std::variant<Market, Limit, ...>` +
`std::visit`** — compile-time dispatch, no RTTI, inlinable; trade-off: closed set,
all types must be known at that point. (3) **Virtual method on `Order`** — one
indirect call, no cast; trade-off: back to a vtable indirection (~2-20 ns) and an
ABI-sensitive interface. All three work with `-fno-rtti`; `dynamic_cast` doesn't.
</details>

### C4
Explain why `-O0` "working" is not evidence that code is UB-free, using two of the
cases from file 16.

<details><summary>Answer</summary>

`-O0` mostly doesn't propagate the "UB can't happen" assumptions. (1)
Null-check-after-deref: at `-O0` the `if (!p)` is still emitted and taken for
`nullptr`; at `-O2` the deref implies non-null and the check is deleted →
`foo(nullptr)` now crashes. (2) `x + 1 > x`: at `-O0` GCC may still fold it (it's a
frontend simplification), but a subtler overflow check like `a + b < a` is honoured
at `-O0` and folded at `-O2`. The behaviour *changes with optimization* precisely
because it was UB.
</details>

### C5
When is it correct to skip calling destructors on the objects in an arena at
teardown, and what goes wrong if you skip it when you shouldn't?

<details><summary>Answer</summary>

Correct when every type stored is `std::is_trivially_destructible` — the dtor is a
no-op, so resetting the arena pointer reclaims everything. If you skip it for a
non-trivially-destructible type (e.g. one holding a `std::string`, a file handle,
a lock, a `shared_ptr`), you leak whatever it owned (heap memory, fd, refcount) —
and for RAII types with side effects (a `Transaction` that rolls back in its dtor)
you skip the side effect.
</details>

---

## Part D — Challenge

### D1 — Write a UB-free `TaggedValue`

Implement a `TaggedValue` that holds either an `std::int64_t` or a `double`
(8 bytes each), with:
- `set(std::int64_t)` / `set(double)` and `as_int()` / `as_double()` /
  `tag()` accessors — **no strict-aliasing UB, no inactive-union-read UB**.
- Correct for `constexpr` use? (bonus)
- A `raw_bytes()` returning `std::span<const std::byte, 8>` of the payload.

Then write a small test: round-trip both types, dump the raw bytes of `double
1.0` and `std::int64_t 1`, confirm they differ, and confirm `as_int()` after
`set(double)` gives you the bit pattern (via a deliberate cross-read through
`raw_bytes()` + `bit_cast`, not through the wrong accessor).

Constraints: use `std::memcpy` or `std::bit_cast` for all payload access; the
storage is `alignas(std::int64_t) std::byte payload_[8];` + an `enum class Tag`.
No `union` inactive reads, no `reinterpret_cast` + deref.

<details><summary>Hints</summary>

- `set(std::int64_t v){ tag_ = Tag::I; std::memcpy(payload_, &v, 8); }`
- `as_int() const { std::int64_t v; std::memcpy(&v, payload_, 8); return v; }`
- For `constexpr`: `std::bit_cast` is `constexpr`; `std::memcpy` is not — so a
  `constexpr` version would store a `std::variant<std::int64_t,double>` or use
  `bit_cast` on a fixed-size array. Simplest: don't chase `constexpr`, note why.
- `raw_bytes()`: `return std::span<const std::byte, 8>(payload_);`
- Compare with `examples/06_strict_aliasing.cpp` and `11-type-punning.md`'s
  tagged-struct example.
</details>

### D2 — Hunt the UB in a mini order book

Here's a deliberately broken snippet. Find **all** the UB (there are at least 5),
classify each by category (memory / integer / lifetime / sequencing / aliasing /
library), and fix them.

```cpp
struct Level { int px_ticks; int qty; };

class Book {
    Level* levels_;
    int    n_ = 0, cap_ = 0;
public:
    Book() { levels_ = (Level*)std::malloc(sizeof(Level) * 8); cap_ = 8; }
    ~Book() { std::free(levels_); }

    const Level& best() const { return levels_[0]; }          // (a)

    void add(int px, int qty) {
        if (n_ >= cap_) {
            cap_ *= 2;
            levels_ = (Level*)std::realloc(levels_, sizeof(Level) * cap_);
        }
        levels_[n_] = Level{px, qty};
        n_ = n_++ + 1;                                          // (b)
    }

    int total_qty() const {
        int t = 0;
        for (int i = 0; i <= n_; ++i) t += levels_[i].qty;      // (c)
        return t;
    }

    std::uint64_t checksum() const {
        return *reinterpret_cast<const std::uint64_t*>(levels_); // (d)
    }
};

// caller:
const Level& top = Book{}.best();                                // (e)
use(top);
```

<details><summary>Answer</summary>

(a) + empty book: `levels_[0]` when `n_ == 0` reads an uninitialized `Level`
(`malloc`'d, no ctor — `Level` is trivial so the *object* exists in C++20, but the
value is indeterminate) → **library/logic** (guard with `n_ > 0`). If `n_ == 0`
and you return `best()`, callers act on garbage.

(b) `n_ = n_++ + 1;` — modifies `n_` twice unsequenced → **sequencing** UB. Just
`++n_;`.

(c) `i <= n_` reads `levels_[n_]` — **memory** (out-of-bounds). Use `i < n_`.

(d) `reinterpret_cast<const std::uint64_t*>(levels_)` + deref — **aliasing** UB
(`Level` object, `uint64_t` glvalue). Use `std::memcpy` of `sizeof(Level)` bytes,
or hash fields.

(e) `Book{}.best()` — `best()` returns a `const Level&` into a **temporary
`Book`** that's destroyed at the `;` → `top` is a **lifetime** dangling reference
(and `~Book` freed `levels_`, so it's also use-after-free). Materialize the `Book`.

Bonus: `realloc` of `malloc`'d memory for a trivial type is OK in C++20
(implicit-lifetime), but if `Level` had a non-trivial member this whole
`malloc`/`realloc`/`free` scheme would be UB — use `std::vector<Level>` or an
allocator + placement new.
</details>

---

## Next
→ [`../26-CONCURRENCY/00-README.md`](../26-CONCURRENCY/00-README.md)
