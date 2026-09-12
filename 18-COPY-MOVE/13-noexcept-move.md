# 13 — `noexcept` move (and why `std::vector` cares)

## Prerequisites
- [`06-move-constructor.md`](06-move-constructor.md), [`07-move-assignment.md`](07-move-assignment.md)
- Folder 17 file 03 (exception safety guarantees)

## Yeh topic abhi kyun
Ek chhoti si annotation — `noexcept` — jo aapke move ctor/assign par lagti hai,
poore program ki performance badal sakti hai. `std::vector` (aur baaki
containers) reallocation ke waqt elements ko **move karti hain SIRF agar move
`noexcept` ho**; warna **copy** karti hain (strong exception guarantee ke liye).
`examples/08_noexcept_vector.cpp` mein yeh ~3x measure hua.

---

## The problem `std::vector` faces on reallocation

`std::vector::push_back` jab capacity full ho:
1. Allocate a new, bigger buffer.
2. Transfer the existing N elements from old buffer to new.
3. Free the old buffer.

Step 2 — **move or copy?** Move is O(1) per element (steal), copy is O(n) per
element (alloc + memcpy). Move is obviously faster... but:

**If moving element #k throws** (say, its move ctor allocates and `bad_alloc`),
`std::vector` is in a broken state: some elements moved (their sources
half-emptied), the new buffer partially populated, and it can't cleanly recover —
the original elements are damaged. This would violate `push_back`'s **strong
exception guarantee** ("if it throws, the vector is unchanged").

So the standard says: **`std::vector` only moves elements during reallocation if
the move constructor is `noexcept`** (via `std::move_if_noexcept`). If the move
ctor might throw but a copy ctor exists → it **copies** (copies don't damage the
sources; if copy #k throws, destroy the k-1 copies, keep the originals →
unchanged).

---

## `std::move_if_noexcept`

```cpp
// conceptually, what vector does per element during realloc:
new (dest) T( std::move_if_noexcept(src) );

// std::move_if_noexcept(x):
//   returns T&&  (an rvalue -> move)   if T's move ctor is noexcept  OR  T is not copyable
//   returns const T&  (-> copy)        otherwise
```

- Move ctor `noexcept` → move. ✅ fast
- Move ctor **not** `noexcept`, but copyable → **copy**. ⚠️ slow
- Move ctor not `noexcept`, **not** copyable → move anyway (no choice — accepts
  the weaker guarantee).

---

## Measured impact

`examples/08_noexcept_vector.cpp` — two identical classes, one with `noexcept` on
the move ctor, one without. `push_back` 200,000 elements (each owning a 1 KB
buffer), no `reserve` → forces ~18 reallocations:

```
is_nothrow_move_constructible:  WithNoexcept: 1   WithoutNoexcept: 0

push_back 200000 elements:
  noexcept move    -> vector MOVES on realloc   ~152 ms
  non-noexcept move -> vector COPIES on realloc  ~468 ms

  non-noexcept / noexcept : ~3x
```

Same class, one keyword difference → 3x slower, because every reallocation
deep-copies every element instead of stealing its pointer.

(The ratio grows with element payload size and number of reallocations. With
`reserve()` up front → no reallocations → the difference disappears — but you
can't always `reserve`.)

---

## The rule

> **Mark your move constructor and move assignment `noexcept`** — as long as they
> genuinely don't throw (which they shouldn't, since they only transfer
> pointers/ints and null out the source).

```cpp
class Buffer {
public:
    Buffer(Buffer&& o) noexcept;               // ✅
    Buffer& operator=(Buffer&& o) noexcept;    // ✅
    ~Buffer();                                  // (destructors are implicitly noexcept)
};
```

A correct move ctor **has no reason to throw**: no allocation, no user code that
can fail — just field copies and null-outs. If yours does something that can
throw, redesign it so it doesn't (do the fallible work elsewhere).

Compiler-generated move ops (Rule of Zero — file 10) are `noexcept` iff every
member's move op is `noexcept` — and `std::string`, `std::vector`, `unique_ptr`
all have `noexcept` moves → Rule-of-Zero classes get it for free.

---

## Verify it

```cpp
static_assert(std::is_nothrow_move_constructible_v<Buffer>);
static_assert(std::is_nothrow_move_assignable_v<Buffer>);
```

Put these on hot element types. If they fail, you either forgot the `noexcept`
keyword, or a member's move can throw, or (subtle) you declared a destructor and
the move ops silently became copies (`std::is_move_constructible` is still true —
via the copy ctor! — so check the **`nothrow`** variant, or better
`std::is_nothrow_move_constructible`).

---

## `noexcept` beyond move

- **Destructors** are implicitly `noexcept` — a throwing destructor during stack
  unwinding → `std::terminate` (folder 17 file 03).
- **`swap`** should be `noexcept` — used in copy-and-swap and by algorithms; a
  throwing swap breaks the strong guarantee.
- **`noexcept` on other functions** — a promise; violating it (an exception
  escapes) → `std::terminate` (no unwinding). It also enables some optimizations
  (smaller unwind tables, and callers can skip exception-handling setup) — but
  the move-ops case is where it has a *dramatic, measurable* effect.
- **`noexcept(expr)`** — conditional: `noexcept(noexcept(m1_.f()) && ...)` for
  generic code that should be `noexcept` only when its parts are.

---

## Andar kya hota hai

- `noexcept` is part of the function **type** (since C++17) — `void f() noexcept`
  and `void f()` are different types. `std::move_if_noexcept` inspects it via
  `std::is_nothrow_move_constructible`.
- If a `noexcept` function *does* throw, the runtime calls `std::terminate`
  directly (it doesn't unwind out of a `noexcept` function). The compiler may
  emit a small "call terminate" landing pad but no full unwind machinery for
  that function.
- `std::vector` realloc with `noexcept` move: a loop of `new (dest++) T(std::move(*src++));`
  — no try/catch needed, N O(1) steals. Without: `new (dest++) T(*src++);` (copy)
  wrapped so that on throw it destroys the partial copies and rethrows, leaving
  the source vector untouched.

> **HFT relevance:** this is one of the highest-leverage single keywords in C++.
> Every hot element type (`Order`, `Quote`, a message struct, anything stored in
> a `std::vector` that might grow) must have `noexcept` move ops — otherwise
> `push_back` growth silently deep-copies every element, an allocation storm
> exactly where you didn't want one. `static_assert(std::is_nothrow_move_constructible_v<T>
> && std::is_nothrow_move_assignable_v<T>)` is standard on such types. POD types
> pass trivially (trivial move == trivial copy, `noexcept`). The trap: a
> "logging destructor" (file 9) turns the implicit moves into copies, and
> `is_nothrow_move_constructible` still returns true (it's true via the copy
> ctor) — so people also `static_assert(!std::is_trivially_copyable... )` or use
> explicit tests, or just `-Werror=deprecated-copy` + Rule of Zero.

---

## Hands-on

```bash
./build.ps1 fast 18-COPY-MOVE/examples/08_noexcept_vector.cpp
```

`WithNoexcept` vs `WithoutNoexcept` — `is_nothrow_move_constructible` 1 vs 0,
and ~3x time difference on `push_back` growth. Add a `reserve()` before the loop
in both → the difference vanishes (no reallocations).

---

## ⚠️ Traps

### Trap 1 — move ctor without `noexcept`
```cpp
Buffer(Buffer&& o) : data_(o.data_) { o.data_ = nullptr; }   // ⚠️ vector<Buffer> realloc COPIES. Add noexcept
```

### Trap 2 — `noexcept` on a move that CAN throw
```cpp
Buffer(Buffer&& o) noexcept : data_(new int[o.n_]) { ... }   // ⚠️ this "move" allocates -> can throw -> terminate.
                                                             //    A real move doesn't allocate
```

### Trap 3 — checking `is_move_constructible` instead of `is_nothrow_...`
```cpp
static_assert(std::is_move_constructible_v<T>);   // ⚠️ true even if "move" is actually a copy. Use is_nothrow_...
```

### Trap 4 — logging destructor silently disabling `noexcept` moves
```cpp
class C { std::vector<int> v_; ~C() { log(); } };   // ⚠️ no implicit move -> vector<C> copies. Rule of Zero or = default all 5
```

### Trap 5 — assuming `reserve` isn't needed if moves are `noexcept`
`noexcept` moves make realloc *cheaper*, but realloc still happens (~log2(N)
times), each moving all elements. `reserve(finalSize)` avoids reallocation
entirely — still worth it in hot code.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`noexcept` on move is just documentation" | `std::vector` growth moves vs copies based on it — measured ~3x |
| "If a move can throw, mark it `noexcept` anyway for speed" | Then a throw → `std::terminate`. A real move can't throw — fix the design |
| "`is_move_constructible` tells me it moves" | It's true even when 'move' resolves to the copy ctor — check `is_nothrow_move_constructible` |
| "Rule of Zero classes might not have `noexcept` moves" | They do if all members' moves are `noexcept` (string/vector/unique_ptr are) |
| "`noexcept` moves mean I don't need `reserve`" | Realloc still happens ~log2(N) times; `reserve` removes it |

---

## Exercises

1. **Add the keyword:** `class Msg { std::string topic_; std::vector<std::byte>
   body_; Msg(Msg&& o) : topic_(std::move(o.topic_)), body_(std::move(o.body_))
   {} };` — one change makes `std::vector<Msg>` growth 3x faster. What?

   <details><summary>Answer</summary>

   `Msg(Msg&& o) noexcept : ...` — add `noexcept`. Now `std::move_if_noexcept`
   returns rvalues → `std::vector<Msg>` reallocation **moves** each `Msg`
   (stealing the string/vector buffers) instead of deep-copying them. (Also add a
   `noexcept` move assignment.)
   </details>

2. **Predict:** `struct A { std::string s; };` (Rule of Zero) vs `struct B {
   std::string s; ~B() {} };` — `std::is_nothrow_move_constructible_v` for each?
   Which does `std::vector<...>` reallocation move?

   <details><summary>Answer</summary>

   `A`: `true` — generated `noexcept` move ctor (string's is `noexcept`). `B`:
   the `~B() {}` suppresses the implicit move ctor → `B` has only a copy ctor →
   `is_nothrow_move_constructible_v<B>` is... `true` (satisfied by the *copy*
   ctor, which is `noexcept` for a `std::string` member!). But `std::move_if_noexcept`
   still picks the copy ctor because there's no move ctor at all. So `A` moves,
   `B` copies. (This is why the trap is nasty — the trait says `true`.)
   </details>

3. **Terminate risk:** `Buffer(Buffer&& o) noexcept { data_ = new int[o.n_];
   std::memcpy(data_, o.data_, ...); }` — what's wrong? What happens if `new`
   throws?

   <details><summary>Answer</summary>

   This "move ctor" actually **copies** (allocates + memcpy) — it's mislabeled.
   Marked `noexcept`, but `new` can throw `bad_alloc` → a `noexcept` violation →
   `std::terminate` (crash). A real move ctor steals `o.data_`, doesn't allocate.
   </details>

4. **Verify:** add `static_assert(std::is_nothrow_move_constructible_v<T> &&
   std::is_nothrow_move_assignable_v<T>)` to a class. It fails. List 3 possible
   causes.

   <details><summary>Answer</summary>

   (1) Forgot the `noexcept` keyword on the move ctor/assign. (2) A member's move
   op isn't `noexcept` (or the member is non-movable). (3) You declared a
   destructor / copy op → move ops weren't generated → the assert on
   `is_nothrow_move_assignable` may fail (no move assign) even though the ctor
   variant passes via copy.
   </details>

5. **reserve vs noexcept:** `std::vector<BigMsg> v; v.reserve(1'000'000); for
   (...) v.push_back(std::move(m));` — does `BigMsg`'s move `noexcept`-ness matter
   here?

   <details><summary>Answer</summary>

   Not for reallocation (there is none — `reserve` pre-sized it). The `push_back(std::move(m))`
   itself uses the move ctor regardless of `noexcept`. So here `noexcept` doesn't
   affect the growth cost — but you should still mark it (other operations
   `erase`/`sort`/`insert` and any un-`reserve`d growth elsewhere depend on it).
   </details>

---

## Interview questions

1. `std::vector` reallocation — elements move ya copy? Kis pe depend?
2. `std::move_if_noexcept` — kya return karta, kab move kab copy?
3. Move ctor ko `noexcept` na banane ka measured impact?
4. Move ctor `noexcept` par throw kar de — kya hota?
5. `is_move_constructible` vs `is_nothrow_move_constructible` — kaunsa check karo, kyun?
6. Logging destructor + `noexcept` move — kya silently break hota?

---

## Next
→ [`14-move-in-practice.md`](14-move-in-practice.md)
