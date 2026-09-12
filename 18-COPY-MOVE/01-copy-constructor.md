# 01 — Copy constructor

## Prerequisites
- Folder 15 (classes, ctors, dtors), folder 17 (RAII, ownership)
- Folder 13 (references — `const T&`), folder 12 (pointers — shallow vs deep)

## Yeh topic abhi kyun
Jab ek object se doosra "same value ka" object banta hai, **copy constructor**
chalta hai. Default wala **member-wise** copy karta hai — jo POD ke liye theek,
par jab class ek raw pointer/resource own karti hai to **shallow copy** →
aliasing → double-free. Yeh Rule of Three (file 03) aur move semantics (file 06)
ka foundation hai.

---

## Kab chalta hai copy constructor

```cpp
Widget a;

Widget b = a;          // copy ctor  (b is NEW -- this is initialization, not assignment)
Widget c(a);           // copy ctor
Widget d{a};           // copy ctor

void f(Widget w);      // by-value parameter
f(a);                  // copy ctor for the parameter (unless the arg is an rvalue -> move)

Widget g() { Widget x; return x; }   // pre-C++17 / NRVO fails -> copy ctor (file 11)

std::vector<Widget> v;
v.push_back(a);        // copy ctor (a is an lvalue)
```

**`b = a` where `b` already exists** → that's **copy assignment** (file 02), a
different function. `Widget b = a;` (in the same statement `b` is declared) →
**copy construction**.

---

## Signature

```cpp
class Widget {
public:
    Widget(const Widget& other);      // the copy constructor -- takes `const Widget&`
};
```

- Parameter is **`const Widget&`** (by reference — a by-value parameter would need
  to copy itself → infinite recursion; `const` so it works with `const` sources
  and temporaries).
- Rarely `Widget(Widget&)` (non-const) — legacy, e.g. old `auto_ptr`. Avoid.

---

## Default (compiler-generated) copy constructor

If you don't declare one, the compiler generates:

```cpp
Widget(const Widget& o)
    : member1_(o.member1_), member2_(o.member2_), ... {}   // member-wise copy
```

- Copies each member using **its** copy constructor: `int` → bit copy,
  `std::string` → deep copy (string's own copy ctor), `Widget*` → **pointer
  bit-copy** (both now point to the same object — shallow!).
- Generated when: no user-declared copy ctor, **and** (in modern C++) no
  user-declared move ctor/assign (a user move op → copy ctor is `= delete`d).
- If a member is non-copyable (`unique_ptr`, `std::mutex`) → the generated copy
  ctor is `= delete`d → the class is non-copyable.

---

## Shallow vs deep copy

```cpp
class Buffer {
    int* data_;
    size_t n_;
public:
    Buffer(size_t n) : data_(new int[n]), n_(n) {}
    ~Buffer() { delete[] data_; }
    // NO user copy ctor -> compiler generates a SHALLOW one
};

Buffer a{10};
Buffer b = a;      // ⚠️ b.data_ == a.data_  (same pointer!)
// scope end: ~b -> delete[] data_ ;  ~a -> delete[] SAME data_ again -> DOUBLE-FREE, UB
```

**Shallow copy** — copies the pointer value, not the pointee. Two objects, one
buffer, two destructors → double-free (folder 14 file 06).

**Deep copy** — allocate a new buffer, copy the contents:

```cpp
Buffer(const Buffer& o) : data_(new int[o.n_]), n_(o.n_) {
    std::memcpy(data_, o.data_, n_ * sizeof(int));   // independent copy
}
```

Now `a` and `b` have separate buffers → modifying one doesn't affect the other,
and each destructor frees its own → no double-free.

`examples/01_copy_semantics.cpp` shows a `Str` class doing deep copy, with
printed copy ctor calls and separate buffer addresses.

---

## When you must write it (Rule of Three preview — file 03)

**If your class has a user-declared destructor that releases a raw resource →
you almost certainly need a user copy constructor (deep copy) and copy
assignment too.** The three go together. If you don't, `T b = a;` shallow-copies
→ double-release.

Better: **don't own raw resources** — use `std::vector` / `std::string` /
`std::unique_ptr` members → the compiler-generated copy ctor is correct (deep for
`vector`/`string`, deleted for `unique_ptr`) → Rule of Zero (file 10).

---

## Copy ctor and `explicit`

```cpp
class Widget {
public:
    explicit Widget(const Widget&);   // ⚠️ almost always WRONG -- breaks pass-by-value,
                                      //    return-by-value, container operations
};
```

The copy constructor should **never** be `explicit` — copy-initialization
(`Widget b = a;`, passing/returning by value) relies on it being implicit.

---

## Andar kya hota hai

- Compiler-generated copy ctor is `inline`; for an all-trivial-members class it's
  a `memcpy`-equivalent (or elided entirely). For members with their own copy
  ctors, it's a sequence of member copy-ctor calls in declaration order.
- A **deep copy** of a heap-owning member = `operator new` + `memcpy(n)` — O(n)
  and an allocation. This is exactly the cost move semantics (file 06)
  eliminates for rvalue sources.
- Passing by value → the copy ctor runs in the **caller's** context to
  materialize the parameter (or a move if the argument is an rvalue).
- `-Wdeprecated-copy` (in `-Wextra`) warns if you declare a destructor OR one
  copy op but rely on the implicit generation of the other — a Rule-of-Three
  smell.

> **HFT relevance:** an unintended deep copy (a `Book`, an `OrderVector`, a
> `std::string` payload copied where a `const&` or a move would do) is a hidden
> allocation + memcpy on a path that shouldn't allocate. Copy constructors on
> hot types are scrutinized: POD messages are trivially copyable (a `memcpy`,
> fine — and pool-relocatable); owning types are either non-copyable
> (`= delete`, forcing `unique_ptr` management) or move-only. A user-written copy
> ctor on a hot type is a red flag — usually the type shouldn't be copied at
> all, or should be Rule-of-Zero. `-Wdeprecated-copy` is treated as an error in
> many HFT builds.

---

## Hands-on

```bash
./build.ps1 18-COPY-MOVE/examples/01_copy_semantics.cpp
```

`Str` deep-copies; watch the "COPY ctor" lines and the distinct buffer
addresses. `examples/02_rule_of_three.cpp` has the full correct triad + a
commented broken "destructor-only" version.

---

## ⚠️ Traps

### Trap 1 — raw-owning class with no user copy ctor
```cpp
class Buf { int* p_; public: ~Buf() { delete[] p_; } };   // ⚠️ Buf b = a; -> shallow -> double-free
```

### Trap 2 — `Widget b = a;` called "assignment"
That's **construction**. `b = a;` (b pre-existing) is assignment (file 02).

### Trap 3 — `explicit` copy ctor
```cpp
explicit Widget(const Widget&);   // ⚠️ breaks return-by-value / pass-by-value
```

### Trap 4 — copy ctor by value (infinite recursion)
```cpp
Widget(Widget o);   // ❌ to construct the parameter you'd call the copy ctor... forever. `const Widget&`
```

### Trap 5 — declaring a dtor silently keeps copy but drops move (file 09/10)
```cpp
class C { std::vector<int> v_; ~C(){ log(); } };   // copy still generated (deprecated), move NOT -> silent copies
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`Widget b = a;` calls `operator=`" | Calls the **copy constructor** (b is new) |
| "Default copy ctor is a deep copy" | Member-wise — deep for `string`/`vector` members, **shallow** for raw pointers |
| "A class with `new` in the ctor is fine without a copy ctor" | Shallow copy → double-free. Rule of Three, or don't own raw |
| "Copy ctor can be `explicit`" | Never — copy-init needs it implicit |
| "Copy ctor takes `Widget`" | `const Widget&` — by value would recurse |

---

## Exercises

1. **Shallow → double-free:** `class Str { char* p_; public: Str(const char* s)
   : p_(strdup(s)) {} ~Str() { free(p_); } };  Str a{"hi"}; Str b = a;` — what
   happens at end of scope? Fix with a deep copy ctor.

   <details><summary>Answer</summary>

   `b.p_ == a.p_` (shallow) → `~b` frees `p_`, `~a` frees the same `p_` again →
   double-free (crash / corruption). Fix: `Str(const Str& o) : p_(strdup(o.p_))
   {}` (+ copy assign — Rule of Three).
   </details>

2. **Count the copies:** `void f(Widget w); Widget g();` — for `Widget a; f(a);
   Widget b = g(); f(g());` — where does the copy ctor run (assuming no move, no
   elision)?

   <details><summary>Answer</summary>

   `f(a)` → copy ctor for the parameter (`a` is an lvalue). `Widget b = g()` →
   copy ctor for the return (pre-elision). `f(g())` → the return value could be
   used directly as the parameter (elision), else 1-2 copies. With C++17
   guaranteed elision for prvalues, `b = g()` and `f(g())` do **zero** extra
   copies; only `f(a)` copies.
   </details>

3. **Rule of Zero it:** `class Packet { char* buf_; size_t len_; public:
   Packet(size_t n) : buf_(new char[n]), len_(n) {} ~Packet() { delete[] buf_;
   } Packet(const Packet& o) : buf_(new char[o.len_]), len_(o.len_) { memcpy(buf_,
   o.buf_, len_); } /* + copy assign */ };` — rewrite so you write no copy ctor.

   <details><summary>Answer</summary>

   `class Packet { std::vector<char> buf_; public: explicit Packet(size_t n) :
   buf_(n) {} };` — `std::vector<char>`'s copy ctor deep-copies; the compiler-
   generated `Packet` copy ctor calls it. Zero hand-written special members.
   </details>

4. **Non-copyable member:** `class Session { std::unique_ptr<Socket> sock_;
   int id_; };` — is `Session` copyable? Why? What's the compiler-generated copy
   ctor?

   <details><summary>Answer</summary>

   Not copyable — `unique_ptr` is non-copyable, so the compiler-generated
   `Session` copy ctor is implicitly `= delete`d. `Session b = a;` → compile
   error. (`Session` is automatically move-only.)
   </details>

5. **const source:** why must the copy ctor take `const Widget&` and not `Widget&`?
   What breaks with `Widget&`?

   <details><summary>Answer</summary>

   `Widget&` can't bind to a `const Widget` or a temporary → `Widget b = a;`
   where `a` is `const`, or `Widget b = makeWidget();`, or passing a temporary by
   value would all fail to compile. `const Widget&` binds to everything a copy
   should copy from.
   </details>

---

## Interview questions

1. Copy ctor kab chalta hai (kam se kam 4 situations)?
2. `Widget b = a;` — construction ya assignment?
3. Default copy ctor kya karta (member-wise), shallow kab problem?
4. Deep vs shallow copy — raw pointer member ke saath kya galat?
5. Copy ctor `const Widget&` kyun, `Widget` (by value) kyun nahi?
6. Copy ctor kabhi `explicit` — kyun nahi?

---

## Next
→ [`02-copy-assignment.md`](02-copy-assignment.md)
