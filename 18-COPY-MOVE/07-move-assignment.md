# 07 — Move assignment

## Prerequisites
- [`06-move-constructor.md`](06-move-constructor.md), [`02-copy-assignment.md`](02-copy-assignment.md)

## Yeh topic abhi kyun
Move **constructor** ek naya object banata hai stealing se. Move **assignment**
ek **existing** object ki value ko replace karta hai stealing se — plus usko
**apna purana resource release** karna padta hai, aur **self-move** handle karna
(edge case, par matter karta). Rule of Five ka paanchwa member.

---

## Signature

```cpp
class Buffer {
public:
    Buffer& operator=(Buffer&& other) noexcept;   // takes Buffer&&, returns Buffer&, noexcept
};
```

- Parameter: `Buffer&&`.
- Returns `Buffer&` (`*this`) — for chaining.
- `noexcept` — same importance as the move ctor (`std::vector` growth, file 13).

---

## What it must do

`b = std::move(a)` where `b` already holds a resource:

1. **Release `b`'s current resource** (else leak).
2. **Steal `a`'s resource** (pointer transfer).
3. **Null out `a`** (so `~a` doesn't double-free).
4. **Handle self-move** (`a = std::move(a)` — rare but legal).
5. **Return `*this`**.

```cpp
Buffer& operator=(Buffer&& o) noexcept {
    if (this != &o) {              // self-move guard
        delete[] data_;            // 1. release our old buffer
        data_ = o.data_;           // 2. steal
        n_    = o.n_;
        o.data_ = nullptr;         // 3. null the source
        o.n_    = 0;
    }
    return *this;                  // 5.
}
```

`examples/04_move_semantics.cpp` has this exact form (`d = std::move(b)` frees
`d`'s old buffer, steals `b`'s, nulls `b`).

---

## Self-move — `a = std::move(a)`

```cpp
Buffer a{100};
a = std::move(a);        // legal C++. Without the `this != &o` guard:
                         //   delete[] data_;   -> frees a's own buffer
                         //   data_ = o.data_;  -> o IS a -> data_ = (freed pointer)
                         //   o.data_ = nullptr;-> now a.data_ = nullptr
                         // -> you've lost the buffer (leaked? no -- freed) but a is now empty.
                         // Actually here it happens to be "safe-ish" but the object lost its value
                         // for no reason. With other layouts -> UB.
```

The standard says self-move must leave the object in a "valid but unspecified"
state — it need not preserve the value. The `if (this != &o)` guard is the
simplest way to make self-move a no-op. (`std::swap`-based move assignment is
also self-move-safe automatically.)

Self-move is rare in direct code but happens indirectly (`std::sort`,
`std::remove`, algorithms that move elements around a container may momentarily
self-move).

---

## The swap-based move assignment (simplest, self-safe)

```cpp
class Buffer {
    friend void swap(Buffer& a, Buffer& b) noexcept {
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.n_,    b.n_);
    }
public:
    Buffer& operator=(Buffer&& o) noexcept {
        swap(*this, o);            // give o our old guts; take o's
        return *this;
    }                             // o (holding our OLD buffer) is... NOT destroyed here (it's a reference)
};
```

Wait — `o` is a `Buffer&&` (a reference to the caller's object), not a local. So
after `swap`, the caller's object holds *our* old buffer, and *it* will be
destroyed by the caller eventually. This works: our old resource ends up in the
moved-from source, which the caller then destroys. Self-move → `swap(*this,
*this)` → no-op. Clean.

(The **unified assignment** — `operator=(Buffer o)` by value — handles copy AND
move AND self in one function; file 02. Trade-off: always makes the parameter,
even when reuse was possible.)

---

## Compiler-generated move assignment

```cpp
Buffer& operator=(Buffer&& o) noexcept(/* if all member move-assigns are noexcept */) {
    member1_ = std::move(o.member1_);      // member-wise move assignment
    member2_ = std::move(o.member2_);
    return *this;
}
```

- Member-wise: each member's `operator=(T&&)` runs (`std::string`/`std::vector`
  steal, `int` copies, `unique_ptr` releases old + steals).
- Generated only if you declared **none** of: destructor, copy ctor, copy assign,
  move ctor (file 9).
- Deleted if any member is non-move-assignable (`const` member, reference member).

For a Rule-of-Zero class, the generated move assignment is correct, `noexcept`,
and O(1) — no reason to write one.

---

## `noexcept` — same as move ctor

`std::vector` (and other containers) reallocation moves elements only if the
type's move operations are `noexcept`. If the move **assignment** isn't
`noexcept` but the move **ctor** is, growth still moves (it uses the ctor). But
for `std::vector::erase`, `std::remove`, `std::sort` etc. that shuffle elements
via **move assignment**, `noexcept` move assignment keeps those O(1) per element.

**Rule: mark both move ctor and move assignment `noexcept`** (if they genuinely
don't throw — which they shouldn't, since they only move pointers/ints).

---

## Andar kya hota hai

- Hand-written move assignment for `{ptr, size}`: 1 `delete[]` + 2 field copies +
  2 nulls, inlined. The `delete[]` of the old buffer is the only non-trivial
  cost, and it's unavoidable (something has to free it).
- swap-based: 2-3 `swap`s (register exchanges) + deferring the `delete[]` to the
  source's later destruction. Same total work, different timing.
- `std::string`/`std::vector` move assignment: `delete[]` (or free) the old
  buffer, then steal the 3 words, null the source. O(1) + one deallocation.
- Compiler-generated: sequence of member `operator=(T&&)` calls, inlined; for
  all-RAII members it's correct and `noexcept`.

> **HFT relevance:** move assignment matters for containers that shuffle elements
> — `std::vector::erase`, `std::sort`, `std::rotate`, the erase-remove idiom
> (folder 19) all move-assign elements around. If your element type's move
> assignment isn't `noexcept` (or is a copy because move ops weren't declared),
> those operations copy — a hidden allocation storm on a book/order container.
> `static_assert(std::is_nothrow_move_assignable_v<T>)` on hot element types.
> In the innermost loop, objects are assigned in place (POD `=` is a memcpy) or
> not at all; move assignment shows up at the container-mutation layer.

---

## Hands-on

```bash
./build.ps1 18-COPY-MOVE/examples/04_move_semantics.cpp
```

`d = std::move(b)` (move assign — `d`'s old buffer freed, `b`'s stolen, `b`
emptied); `e = makeBuffer(8)` (rvalue → move assign).

---

## ⚠️ Traps

### Trap 1 — leak: not releasing the old resource
```cpp
Buffer& operator=(Buffer&& o) noexcept { data_ = o.data_; o.data_ = nullptr; return *this; }
// ⚠️ our OLD data_ leaked. delete[] data_; first
```

### Trap 2 — no self-move guard
```cpp
Buffer& operator=(Buffer&& o) noexcept { delete[] data_; data_ = o.data_; ... }
// ⚠️ a = std::move(a); -> delete[] then read freed pointer. `if (this != &o)` or swap-based
```

### Trap 3 — not nulling the source
```cpp
Buffer& operator=(Buffer&& o) noexcept { delete[] data_; data_ = o.data_; return *this; }
// ⚠️ o.data_ still set -> ~o double-frees. o.data_ = nullptr;
```

### Trap 4 — move assign not `noexcept`
```cpp
Buffer& operator=(Buffer&&) { ... }   // ⚠️ erase/sort/remove will copy instead of move. Add noexcept
```

### Trap 5 — returning `void` / by value
```cpp
void operator=(Buffer&&);   // ⚠️ a = b = c; breaks. Return Buffer& (*this)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Move assignment just steals like the move ctor" | Also must **release the existing resource** and handle self-move |
| "Self-move (`a = std::move(a)`) never happens" | Algorithms can trigger it; guard with `this != &o` or swap-based |
| "Only the move ctor needs `noexcept`" | Move assignment too — erase/sort/remove use it |
| "`operator=(T&&)` can return `void`" | Return `T&` for chaining |
| "Compiler move assignment might leak" | For RAII members it correctly releases-then-steals |

---

## Exercises

1. **Write it:** `class Str { char* p_; size_t len_; ... };` — write the move
   assignment (release old, steal, null source, self-guard, return `*this`).

   <details><summary>Answer</summary>

   `Str& operator=(Str&& o) noexcept { if (this != &o) { delete[] p_; p_ = o.p_;
   len_ = o.len_; o.p_ = nullptr; o.len_ = 0; } return *this; }`
   </details>

2. **Spot the leak:** `Buffer& operator=(Buffer&& o) noexcept { data_ = o.data_;
   n_ = o.n_; o.data_ = nullptr; return *this; }` — what leaks?

   <details><summary>Answer</summary>

   `*this`'s **original** `data_` buffer — it's overwritten without being freed.
   Add `delete[] data_;` before `data_ = o.data_;` (and a self-move guard).
   </details>

3. **Self-move:** trace `Buffer a{10}; a = std::move(a);` with (a) a guarded
   version, (b) an unguarded `delete[] data_; data_ = o.data_; o.data_ =
   nullptr;`.

   <details><summary>Answer</summary>

   (a) `this == &o` → skip body → `a` unchanged. (b) `delete[] data_` frees
   `a`'s buffer; `data_ = o.data_` → `data_` = the just-freed pointer; `o.data_ =
   nullptr` → `a.data_ = nullptr`. `a` ends up empty (its buffer gone) — "valid
   but unspecified", technically allowed, but pointless data loss (and UB-prone
   with richer layouts).
   </details>

4. **swap-based:** implement `Buffer& operator=(Buffer&& o) noexcept { swap(*this,
   o); return *this; }`. Where does `*this`'s old buffer get freed?

   <details><summary>Answer</summary>

   After `swap`, `o` (the caller's moved-from source) holds `*this`'s old
   buffer. When the caller destroys that source object (scope end / temporary
   end), its destructor frees the old buffer. Self-move → `swap(a, a)` → no-op.
   </details>

5. **noexcept + erase:** `std::vector<T> v; v.erase(v.begin());` — how does the
   `noexcept`-ness of `T`'s move assignment affect this operation's cost?

   <details><summary>Answer</summary>

   `erase` shifts elements left via **move assignment**. If `T`'s move assign is
   `noexcept` → each shift is an O(1) steal. If not → the shifts still use move
   assign (erase doesn't need the strong guarantee like realloc does), but if
   move ops were never declared, it falls back to **copy** assignment → O(n) per
   shifted element. `noexcept` + declared move ops keep it cheap.
   </details>

---

## Interview questions

1. Move assignment vs move ctor — kya extra (release old, self-move)?
2. Self-move kaise handle karo, kyun matter karta?
3. swap-based move assignment — old resource kab free hota?
4. `noexcept` move assignment — kaunse operations (erase/sort) pe asar?
5. `operator=(T&&)` return type kyun `T&`?
6. Compiler-generated move assignment — kya karta, kab banta?

---

## Next
→ [`08-std-move.md`](08-std-move.md)
