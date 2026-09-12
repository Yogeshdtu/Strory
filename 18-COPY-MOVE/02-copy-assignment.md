# 02 — Copy assignment operator

## Prerequisites
- [`01-copy-constructor.md`](01-copy-constructor.md)
- Folder 15 file 09 (operator overloading), folder 17 file 03 (exception safety)

## Yeh topic abhi kyun
Copy **constructor** ek naya object banata hai. Copy **assignment** ek
**already-existing** object ki value ko doosre se replace karta hai. Extra
challenges: **purana resource release karo**, **self-assignment** handle karo,
aur ideally **exception-safe** raho. Rule of Three ka doosra member.

---

## Kab chalta hai copy assignment

```cpp
Widget a, b;

b = a;          // copy assignment  (b already exists -- replace its value with a's)
b = getWidget();// copy assignment... unless getWidget() returns an rvalue -> MOVE assignment (file 07)

// vs
Widget c = a;   // copy CONSTRUCTION (c is new -- file 01)
```

If the left side already exists → assignment. If it's being declared in the same
statement → construction.

---

## Signature

```cpp
class Widget {
public:
    Widget& operator=(const Widget& other);   // takes const&, returns Widget& (for chaining: a = b = c)
};
```

- Parameter: `const Widget&`.
- **Returns `Widget&`** (`*this`) — so `a = b = c;` works (right-to-left: `b = c`
  returns `b&`, then `a = b`).
- A member function (must be).

---

## What copy assignment must do

For a class owning a resource:

1. **Guard against self-assignment** (`a = a;`) — else you might free your own
   resource then copy from freed memory.
2. **Release the old resource** (or reuse it).
3. **Acquire/copy the new value** (deep copy).
4. **Return `*this`**.

### Naive version (buggy on self-assignment)

```cpp
Buffer& operator=(const Buffer& o) {
    delete[] data_;                    // ⚠️ if &o == this, we just freed o's data too
    data_ = new int[o.n_];             // ...and o.n_ is fine but o.data_ is dangling
    std::memcpy(data_, o.data_, o.n_ * sizeof(int));   // ⚠️ reads freed memory
    n_ = o.n_;
    return *this;
}
```

### With a self-assignment check

```cpp
Buffer& operator=(const Buffer& o) {
    if (this != &o) {                  // self-assignment guard
        delete[] data_;
        data_ = new int[o.n_];
        std::memcpy(data_, o.data_, o.n_ * sizeof(int));
        n_ = o.n_;
    }
    return *this;
}
```

Works, but **not exception-safe**: if `new int[o.n_]` throws, `data_` is already
`delete`d → `*this` is now broken (dangling `data_`, but the destructor will
`delete[]` it → double-free or crash).

### Exception-safe: allocate before you free

```cpp
Buffer& operator=(const Buffer& o) {
    if (this != &o) {
        int* fresh = new int[o.n_];                        // 1. acquire NEW first (may throw -> *this untouched)
        std::memcpy(fresh, o.data_, o.n_ * sizeof(int));
        delete[] data_;                                     // 2. release OLD (no-throw)
        data_ = fresh;
        n_    = o.n_;
    }
    return *this;
}
```

If `new` throws, we haven't touched `*this` → **strong exception guarantee**
(folder 17 file 03). This ordering also happens to make the self-assignment
check optional (if `&o == this`, you copy your own data into `fresh`, free the
old, point at `fresh` — correct, just wasteful) — but keep the check for the
common no-op case.

---

## The copy-and-swap idiom (cleanest)

```cpp
class Buffer {
    int* data_ = nullptr;
    size_t n_  = 0;

    friend void swap(Buffer& a, Buffer& b) noexcept {   // non-throwing member-wise swap
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.n_,    b.n_);
    }

public:
    // ... ctor, dtor, copy ctor ...

    Buffer& operator=(Buffer o) {        // ⚠️ NOTE: BY VALUE -- the copy happens here
        swap(*this, o);                  // swap our guts with the copy's
        return *this;
    }                                    // `o` (holding our OLD guts) is destroyed here
};
```

- Parameter **by value** → the copy ctor (or move ctor for an rvalue arg!) makes
  `o`. If the argument was an rvalue, `o` is *moved* → this one `operator=`
  handles **both** copy and move assignment.
- `swap(*this, o)` — a no-throw swap of the internals.
- `o`'s destructor cleans up our old resource.
- **Automatically self-assignment-safe** (you copy first, then swap) and
  **strong exception-safe** (if the copy throws, `*this` is untouched — the
  parameter never got constructed).

Trade-off: for a non-rvalue argument it always does a full copy even if you could
have reused the existing buffer (a hand-written version could `memcpy` into the
existing allocation if sizes match). Usually the clarity wins.

`examples/01_copy_semantics.cpp` uses the allocate-before-free form;
`examples/04_move_semantics.cpp` shows a class with both copy and move assign.

---

## Default (compiler-generated) copy assignment

```cpp
Widget& operator=(const Widget& o) {
    member1_ = o.member1_;      // member-wise assignment (each member's operator=)
    member2_ = o.member2_;
    return *this;
}
```

- Member-wise. For `std::string`/`std::vector` members → their (deep, self-safe,
  exception-safe) `operator=` runs → correct. For raw pointer members → pointer
  bit-copy → aliasing → same shallow-copy disaster as file 01.
- Deleted if any member is non-assignable (`const` member, reference member,
  `unique_ptr` for copy-assign).
- Deleted if there's a user-declared move op (file 09).

---

## Andar kya hota hai

- Compiler-generated copy assignment: a sequence of member `operator=` calls,
  inlined. For all-trivial members it can be a `memcpy`.
- A hand-written deep-copy assignment on a heap-owning member = free + `operator
  new` + `memcpy` — the same O(n) + allocation cost as the copy ctor, paid every
  assignment.
- Copy-and-swap: the by-value parameter is one copy (or move), then 2-3 `swap`s
  (pointer exchanges, inlined) — the extra function-call boundary usually inlines
  away at `-O2`.
- `-Wdeprecated-copy`: fires if you declared a destructor or the copy ctor but
  rely on the implicit copy assignment (Rule-of-Three smell).

> **HFT relevance:** copy assignment on an owning type = a free + alloc + memcpy
> per `=`. On hot paths, owning objects are either not assigned at all (they're
> constructed once, in a pool) or move-assigned (O(1)). POD messages assign via
> `memcpy` (trivial, fine). A hand-written copy assignment on a hot type is
> reviewed the same as a copy ctor — usually the type should be non-assignable
> or Rule-of-Zero. The copy-and-swap idiom is fine for control-plane types
> (config, session) where clarity + exception safety matter more than shaving an
> allocation.

---

## Hands-on

```bash
./build.ps1 18-COPY-MOVE/examples/01_copy_semantics.cpp
```

`Str::operator=` uses allocate-before-free + a self-assignment guard; watch
`d = a;` and `d = d;` (self-assignment → guarded no-op body).

---

## ⚠️ Traps

### Trap 1 — `delete` before `new` (not exception-safe)
```cpp
operator=(const T& o) { delete[] p_; p_ = new T[o.n_]; ... }   // ⚠️ new throws -> *this broken. Allocate first
```

### Trap 2 — no self-assignment handling in the naive form
```cpp
operator=(const T& o) { delete[] p_; p_ = new T[o.n_]; memcpy(p_, o.p_, ...); }   // ⚠️ a = a; -> reads freed o.p_
```

### Trap 3 — forgetting `return *this;`
```cpp
T& operator=(const T& o) { ...; }   // ⚠️ no return -> UB. `a = b = c;` breaks
```

### Trap 4 — copy assignment but no copy ctor / dtor (Rule of Three)
```cpp
class C { int* p_; public: C& operator=(const C&); };   // ⚠️ where's the dtor? the copy ctor? Triad
```

### Trap 5 — `operator=` returning `void` or `T` (by value)
```cpp
void operator=(const T&);   // ⚠️ a = b = c breaks
T    operator=(const T&);   // ⚠️ extra copy on every assignment. `T&`
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Copy assignment and copy ctor are the same" | Ctor builds new; assignment replaces existing (+ release old, self-check) |
| "`if (this != &o)` is enough" | Also need `new` before `delete` for exception safety |
| "Default copy assignment deep-copies" | Member-wise — deep for `string`/`vector`, shallow for raw pointers |
| "`operator=` can return `void`" | Return `T&` (`*this`) for chaining |
| "Copy-and-swap is slower so avoid it" | Handles copy+move+self+exceptions in one small function; inlines well |

---

## Exercises

1. **Fix the self-assignment bug:** `T& operator=(const T& o) { delete[] data_;
   data_ = new int[o.n_]; std::copy_n(o.data_, o.n_, data_); n_ = o.n_; return
   *this; }` — what breaks on `x = x;`? Two fixes.

   <details><summary>Answer</summary>

   `x = x;` → `delete[] data_` frees `o.data_` too (same object) → `std::copy_n`
   reads freed memory → UB. Fix A: `if (this != &o) { ... }`. Fix B: allocate
   `fresh` first, copy into it, then `delete[] data_`, then `data_ = fresh` —
   correct even for self-assignment.
   </details>

2. **Copy-and-swap:** implement `Buffer& operator=(Buffer o) { swap(*this, o);
   return *this; }` with a `friend swap`. Why does this one function also serve
   as the *move* assignment?

   <details><summary>Answer</summary>

   The parameter `o` is by value. If the argument is an lvalue → copy ctor makes
   `o`. If it's an rvalue → **move ctor** makes `o` (cheap). Either way,
   `swap(*this, o)` swaps guts and `o`'s dtor cleans the old ones. So `b = a;`
   (copy) and `b = std::move(a);` / `b = makeBuffer();` (move) both route through
   it.
   </details>

3. **Exception safety:** with `delete[] data_; data_ = new int[BIG];` and `new`
   throwing `std::bad_alloc` — what state is `*this` in? What does the eventual
   destructor do?

   <details><summary>Answer</summary>

   `data_` was `delete`d, then `new` threw before reassigning → `data_` is a
   dangling pointer. The destructor later does `delete[] data_` on the dangling
   pointer → double-free / crash. The "allocate first" ordering avoids this
   entirely.
   </details>

4. **Return type:** `T operator=(const T&)` (by value) vs `T& operator=(const
   T&)` — what does `a = b = c;` cost with each?

   <details><summary>Answer</summary>

   `T&` → `b = c` returns a reference to `b`, `a = b` assigns from it — no extra
   copies. `T` (by value) → `b = c` returns a **copy** of `b`, `a = b` assigns
   from that copy — one needless full copy per chained assignment.
   </details>

5. **Rule of Zero:** `class Config { std::string name_; std::vector<int> ports_;
   };` — what does the compiler-generated copy assignment do? Is it self-safe?
   Exception-safe?

   <details><summary>Answer</summary>

   Member-wise: `name_ = o.name_;` (string's `operator=` — deep, self-safe,
   strong-ish), `ports_ = o.ports_;` (vector's `operator=` — deep, self-safe).
   Both are self-safe and at least basic-exception-safe. So `Config`'s generated
   copy assignment is correct with zero hand-written code (Rule of Zero).
   </details>

---

## Interview questions

1. Copy assignment vs copy ctor — kya extra karna padta (3 things)?
2. Self-assignment kyun handle karna, kaise?
3. Exception-safe copy assignment — `new` aur `delete` ka order kyun matter?
4. Copy-and-swap idiom — kaise kaam karta, ek function copy+move dono kaise serve karta?
5. `operator=` ka return type kyun `T&`?
6. Default copy assignment — member-wise, kab galat?

---

## Next
→ [`03-rule-of-three.md`](03-rule-of-three.md)
