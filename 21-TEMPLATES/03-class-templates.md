# 03 — Class templates

## Prerequisites
- [`02-function-templates.md`](02-function-templates.md)
- Folder 15 (classes), folder 19 file 02 (`std::vector` is a class template)

## Yeh topic abhi kyun
`std::vector<T>`, `std::array<T, N>`, `std::pair<A, B>` — sab class templates
hain. Apna generic container / wrapper likhne ke liye class templates chahiye.
Function templates se ek key difference: **arguments deduce nahi hote** (pre-C++17)
— aapko `MyClass<int>` likhna padta. C++17 CTAD ne yeh badla.

`examples/02_class_templates.cpp` mein ek `FixedStack<T, Cap>` hai.

---

## Declaring

```cpp
template <class T, std::size_t Cap>
class FixedStack {
    T           data_[Cap];       // Cap is a compile-time constant -> inline array, no heap
    std::size_t size_ = 0;
public:
    void push(const T& x)  { if (size_ == Cap) throw std::overflow_error("full"); data_[size_++] = x; }
    void pop()             { --size_; }
    T&       top()         { return data_[size_ - 1]; }
    const T& top() const   { return data_[size_ - 1]; }
    bool empty() const     { return size_ == 0; }
    std::size_t size() const { return size_; }
    static constexpr std::size_t capacity() { return Cap; }
};

FixedStack<int, 4>       s;      // pre-C++17: you MUST write the arguments
FixedStack<std::string, 8> names;
```

Each `FixedStack<int, 4>` and `FixedStack<int, 8>` is a **separate class** — a
separate type, separate `sizeof`, separate instantiated member functions.

---

## Defining members outside the class

```cpp
template <class T, std::size_t Cap>
class FixedStack {
    void push(const T& x);          // declared
};

// definition -- the full template header repeats, and the class name is FixedStack<T, Cap>
template <class T, std::size_t Cap>
void FixedStack<T, Cap>::push(const T& x) {
    if (size_ == Cap) throw std::overflow_error("full");
    data_[size_++] = x;
}
```

Verbose, so small member bodies usually stay inline in the class. All of it lives
in a **header** (definitions must be visible at instantiation — file 15).

---

## Member function templates

A class template's member can itself be a template:

```cpp
template <class T, std::size_t Cap>
class FixedStack {
    template <class It>                              // a member function template
    void push_range(It first, It last) {
        for (; first != last; ++first) push(*first);
    }

    template <class U>                               // converting constructor, like std::vector's
    FixedStack(const FixedStack<U, Cap>& other);     // build a FixedStack<T> from a FixedStack<U>
};
```

`std::vector`'s range constructor, `std::function`'s constructor-from-any-callable,
and allocator rebinding (folder 19 file 23) are all member templates.

---

## CTAD — Class Template Argument Deduction (C++17)

Since C++17 the compiler can deduce a class template's arguments from the
constructor call — just like function templates:

```cpp
std::pair p{42, std::string("x")};     // -> std::pair<int, std::string>
std::vector v{1, 2, 3};                // -> std::vector<int>
std::array a{1, 2, 3};                 // -> std::array<int, 3>
std::lock_guard g{mtx};                // -> std::lock_guard<std::mutex>   (huge ergonomic win)
```

For your own types you sometimes need **deduction guides** to tell the compiler
how constructor arguments map to template parameters:

```cpp
template <class T> struct Box { T value; };
Box(const char*) -> Box<std::string>;   // guide: Box{"hi"} deduces Box<std::string>, not Box<const char*>

Box b1{42};      // Box<int>       (implicit guide from the aggregate/ctor)
Box b2{"hi"};    // Box<std::string> (explicit guide above)
```

CTAD does **not** apply to `FixedStack<int, 4> s;` style declarations without a
constructor that carries the info, and it can't partially deduce (`std::pair<int>
p{...}` is still an error).

---

## Non-type template parameters (NTTPs)

A template parameter can be a **compile-time value**, not just a type:

```cpp
template <class T, std::size_t N>            // std::size_t N
struct Array { T data[N]; };

template <int Base>                          // int
struct Counter { int next() { return v++ * Base; } int v = 1; };

template <auto V>                            // C++17: `auto` NTTP -- V is whatever value you pass
struct Constant { static constexpr auto value = V; };
Constant<42>::value;      // 42 (int)
Constant<'x'>::value;     // 'x' (char)

// C++20: floating-point and literal class-type NTTPs
template <double Threshold> struct Filter { /* ... */ };
```

NTTP kinds: integral / enum, pointer / reference to an object or function with
linkage, `nullptr_t`, and (C++20) floating-point and structural class types.
`std::array<T, N>` uses one; `std::bitset<N>`; `FixedStack<T, Cap>`.

Because `N` is known at compile time, `data[N]` is inline (no heap), loops over it
can be **fully unrolled**, and `N` participates in `constexpr` (file 07, 13).

---

## Template template parameters

A parameter that is *itself* a template:

```cpp
template <class T, template <class, class> class Container = std::vector>
class Stack {
    Container<T, std::allocator<T>> data_;   // the container type is chosen by the caller
public:
    void push(const T& x) { data_.push_back(x); }
    void pop()            { data_.pop_back(); }
};

Stack<int> s1;                       // uses std::vector
Stack<int, std::deque> s2;           // uses std::deque
```

Rare in application code (you usually just pass `Container<T>` as a full type),
but it's how some policy designs and adapters are written.

---

## Andar kya hota hai

- `FixedStack<int, 4>` and `FixedStack<int, 8>` are unrelated types — no
  conversion between them, separate vtables (if any), separate `sizeof` (16 vs
  40 here). Member functions are instantiated **lazily**: `FixedStack<T>::pop`
  isn't compiled unless called, so a `FixedStack<NoLessThan>` is fine as long as
  you never call a member needing `<`.
- An NTTP like `Cap` is substituted as a literal → `data_[Cap]` is a fixed-size
  member, the object has no pointer/size overhead beyond `size_`, and the
  compiler can unroll `for (i < Cap)` and prove bounds.
- CTAD works by forming a set of imaginary function templates from the
  constructors (plus explicit guides) and running function-template deduction on
  them.
- `sizeof(FixedStack<int, 4>)` = `4*4 + 8` (padding) = 16 → it's a value type,
  stack/inline, zero indirection. That's why it's the HFT-friendly shape.

> **HFT relevance:** fixed-capacity class templates (`FixedStack<T, N>`,
> `RingBuffer<T, N>`, `FlatMap<K, V, N>`) are the hot-path container pattern —
> the capacity is an NTTP, so storage is **inline / preallocated**, there is **no
> heap allocation ever**, and loops over the storage unroll and vectorize because
> `N` is a constant. Policy parameters (locking, stats, overflow behaviour — file
> 11) are template parameters so each choice inlines with no runtime branch. CTAD
> (`std::scoped_lock lk{m1, m2};`) removes boilerplate. The cost is the usual
> template tax — a distinct instantiation per `<T, N, Policies...>` combination —
> managed by keeping the set of combinations small.

---

## Hands-on

```bash
./build.ps1 21-TEMPLATES/examples/02_class_templates.cpp
```

Write: a `template <class T> class Optional` (a `bool` + aligned storage, no
heap); a `template <class T, std::size_t N> class RingBuffer` with masked
power-of-two wrap; add a deduction guide so `Box{"literal"}` becomes
`Box<std::string>`.

---

## ⚠️ Traps

### Trap 1 — forgetting the template header on an out-of-class definition
```cpp
void FixedStack<T, Cap>::push(const T& x) { ... }   // ❌ needs `template <class T, std::size_t Cap>` above it
```

### Trap 2 — expecting conversion between instantiations
```cpp
FixedStack<int, 4> a; FixedStack<int, 8> b = a;   // ❌ different types, no implicit conversion (unless you write a converting ctor)
```

### Trap 3 — CTAD that doesn't cover a partial spec
```cpp
std::pair<int> p{1, 2.0};   // ❌ can't partially deduce; either full args or none (std::pair p{1, 2.0})
```

### Trap 4 — an NTTP that isn't a constant expression
```cpp
int n = read();
std::array<int, n> a;   // ❌ n isn't constexpr. Use std::vector, or a constexpr n
```

### Trap 5 — instantiating a member that needs an operation T lacks
```cpp
FixedStack<NoCopy, 4> s;
s.push(x);   // ⚠️ push does `data_[i] = x` -> needs T copy-assignable. Fails only when push is instantiated
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`Vec<int>` and `Vec<double>` share code" | Separate instantiations — separate types and machine code |
| "Class templates deduce their arguments (always)" | Only since C++17 (CTAD), and only from constructors; not `Vec<int, 4> v;` |
| "All member functions get instantiated" | Only the ones you actually call (lazy instantiation) |
| "An NTTP can be a runtime value" | It must be a constant expression |
| "`FixedStack<int,4>` converts to `FixedStack<int,8>`" | No — write an explicit converting constructor if you want that |

---

## Exercises

1. **sizeof:** `FixedStack<char, 10>` and `FixedStack<double, 10>` — give each
   `sizeof` (assume `size_` is `size_t` = 8 bytes) and explain.

   <details><summary>Answer</summary>

   `<char,10>`: `10` bytes of `data_` + `8` for `size_`, aligned to 8 → 24 (or
   16 if `data_` packs before `size_` with 6 padding — layout-dependent; likely
   16). `<double,10>`: `80` + `8` = 88. NTTP `Cap` makes the array inline; the
   struct is a value type.
   </details>

2. **CTAD guide:** `template <class T> struct Wrapper { T v; };`. Make
   `Wrapper w{"hello"}` deduce `Wrapper<std::string>` instead of `Wrapper<const
   char*>`.

   <details><summary>Answer</summary>

   Add `Wrapper(const char*) -> Wrapper<std::string>;` after the class. Now the
   deduction guide overrides the implicit `const char*` deduction.
   </details>

3. **Member template use:** why does `std::vector<int> v(mylist.begin(),
   mylist.end());` work even though `mylist` is a `std::list`?

   <details><summary>Answer</summary>

   `std::vector` has a **member constructor template** `template <class It>
   vector(It first, It last)` — it accepts any iterator pair, `std::list`'s
   included, and copies the range.
   </details>

4. **NTTP unrolling:** `template <class T, std::size_t N> T sum(const
   std::array<T, N>& a);` for `N = 4` at `-O2` — what does the loop compile to?

   <details><summary>Answer</summary>

   `N` is a constant → the compiler fully unrolls to 4 adds (or a single SIMD
   `haddps`-style reduction), no loop counter, no branch. That's the payoff of a
   compile-time size vs a runtime `std::vector::size()`.
   </details>

5. **Lazy instantiation:** `FixedStack<std::mutex, 4> s;` — `std::mutex` is
   non-copyable. Does this compile? When would it fail?

   <details><summary>Answer</summary>

   The class instantiates fine (data members only). It fails the moment you call
   `push` (which does `data_[i] = x`, needing copy-assignment) or copy the
   `FixedStack`. Members are instantiated on use.
   </details>

---

## Interview questions

1. Class template vs function template — argument deduction ka fark (CTAD kab aaya)?
2. Out-of-class member definition ka syntax — kya repeat hota?
3. `FixedStack<int,4>` aur `FixedStack<int,8>` ka rishta (koi nahi) — conversion?
4. Non-type template parameter — kaunse types allowed, `N` compile-time hone se kya milta?
5. CTAD deduction guide kab chahiye?
6. Lazy member instantiation — `std::vector<NonCopyable>` kab fail hota?

---

## Next
→ [`04-template-parameters.md`](04-template-parameters.md)
