# 06 — Move constructor

## Prerequisites
- [`05-rvalue-references.md`](05-rvalue-references.md), [`01-copy-constructor.md`](01-copy-constructor.md)
- Folder 10 file 03 (SSO — string copy = alloc + memcpy)

## Yeh topic abhi kyun
Copy ctor: naya buffer allocate + poora data copy → **O(n)**. Move ctor: source
ke pointer/size **steal** karo, source ko empty valid state pe chhodo → **O(1)**.
Yeh modern C++ ki central optimization hai — bade objects ab functions se
"free" mein return hote hain, containers mein "free" mein move hote hain.

---

## Signature

```cpp
class Buffer {
public:
    Buffer(Buffer&& other) noexcept;      // move ctor -- takes `Buffer&&`, marked noexcept
};
```

- Parameter: **`Buffer&&`** (non-const rvalue reference — you need to modify
  `other` to null it out).
- **`noexcept`** — critical (file 13): `std::vector` only moves elements on
  reallocation if the move ctor is `noexcept`; otherwise it copies.
- Runs when the source is an **rvalue** (a temporary, or `std::move(x)`).

---

## What it does — steal, don't copy

```cpp
class Buffer {
    size_t n_    = 0;
    int*   data_ = nullptr;
public:
    explicit Buffer(size_t n) : n_(n), data_(new int[n]) {}

    ~Buffer() { delete[] data_; }

    // COPY ctor -- O(n)
    Buffer(const Buffer& o) : n_(o.n_), data_(new int[o.n_]) {   // allocate
        std::memcpy(data_, o.data_, n_ * sizeof(int));            // copy all bytes
    }

    // MOVE ctor -- O(1)
    Buffer(Buffer&& o) noexcept
        : n_(o.n_), data_(o.data_) {     // 1. take o's pointer + size (no allocation, no copy)
        o.n_    = 0;                     // 2. leave o in a valid empty state
        o.data_ = nullptr;              //    so o's destructor is a harmless no-op
    }
};

Buffer a{1'000'000};        // 1M ints allocated
Buffer b = std::move(a);    // MOVE: b.data_ = a.data_ (steal); a.data_ = nullptr. ~4 instructions.
// a is now {n_=0, data_=nullptr} -- valid, empty, "moved-from"
```

The move ctor **transfers ownership** of the heap buffer from `a` to `b`:
- `b` gets `a`'s pointer and size — no `new`, no `memcpy`.
- `a` is nulled — so when `~a` runs, `delete[] nullptr` (a safe no-op), not a
  double-free.

`examples/04_move_semantics.cpp` shows this with printed "MOVE" lines and buffer
addresses (the same address moves from source to destination).

---

## The moved-from state

After `Buffer b = std::move(a);`, what is `a`?

> **"Valid but unspecified."** You may safely: **destroy it**, or **assign a new
> value to it**. You may **not** rely on its value.

```cpp
Buffer a{100};
Buffer b = std::move(a);

// a.data_ ...             // ⚠️ don't rely -- it's nullptr in OUR impl, but "unspecified" in general
a = Buffer{50};            // ✅ assign a fresh value -- a is usable again
// ~a runs at scope end    // ✅ safe -- our dtor handles data_ == nullptr
```

**Your move ctor MUST leave the source in a state where the destructor is safe.**
The idiomatic way: null out pointers, zero sizes/counts. For standard types
(`std::string`, `std::vector`), moved-from is *usually* empty, but the standard
only guarantees "valid, unspecified" — so `s.clear()` before reuse if you care.

---

## Compiler-generated move constructor

If you don't declare one (and don't declare a destructor, copy ctor, or copy
assign — file 09), the compiler generates:

```cpp
Buffer(Buffer&& o) noexcept(/* if all member moves are noexcept */)
    : member1_(std::move(o.member1_)), member2_(std::move(o.member2_)), ... {}
```

- **Member-wise move** — each member moved via its own move ctor (`std::move` on
  each). `int` → copied (move == copy for trivial types). `std::string`/`std::vector`
  → their move ctors steal. `unique_ptr` → pointer steal.
- `noexcept` iff every member's move is `noexcept`.
- **Not generated** if you declared a destructor, copy ctor, or copy assignment
  (file 09) → then "moves" fall back to the copy ctor (silently slower).

**This is why Rule of Zero (file 10) works:** all-RAII members → the generated
move ctor member-wise-moves them → O(1), correct, `noexcept`.

---

## `std::move` inside the move ctor

```cpp
class Widget {
    std::string name_;
    std::vector<int> data_;
public:
    Widget(Widget&& o) noexcept
        : name_(std::move(o.name_)),      // ⚠️ std::move needed -- o.name_ is an lvalue (it has a name)
          data_(std::move(o.data_)) {     //    without std::move -> name_ COPIED, not moved
    }
};
```

`o` is a named rvalue reference → the expression `o.name_` is an **lvalue** (file
04/05). Without `std::move(o.name_)`, the member's **copy** ctor runs. Every
member init in a hand-written move ctor needs `std::move`.

(The compiler-generated move ctor does this automatically — another reason to
prefer Rule of Zero.)

---

## When move happens

```cpp
Buffer makeBuffer();              // returns by value

Buffer a = makeBuffer();          // prvalue -> guaranteed elision (C++17) OR move ctor
Buffer b = std::move(a);          // xvalue -> move ctor
Buffer c{Buffer{50}};             // prvalue -> elision / move

std::vector<Buffer> v;
v.push_back(Buffer{10});          // rvalue arg -> move ctor into the vector's storage
v.push_back(a);                   // lvalue arg -> COPY ctor
v.push_back(std::move(a));        // xvalue -> move ctor

// vector reallocation -> moves each element IF move ctor is noexcept (file 13)
```

Move is chosen when the source expression is an **rvalue** (file 04). For lvalues
you must `std::move` to opt in.

---

## Andar kya hota hai

- Hand-written move ctor for `{ptr, size}`: 2 field copies + 2 nulls = ~4
  `mov` instructions, inlined at `-O2`. Compare copy: `operator new` (~50-200 ns)
  + `memcpy(n)` (bandwidth-bound).
- `std::string` move: 3 word copies (ptr, size, capacity) + 3 zeros in the
  source (for libstdc++'s layout) — or for an SSO string, a small `memcpy` of
  the inline buffer. Still O(1) bounded, vs O(n) + alloc for copy.
- Move ctor `noexcept` → `std::vector` grow uses `std::move_if_noexcept` which
  returns an rvalue → move ctor called per element. Not `noexcept` → returns a
  `const&` → **copy** ctor called (strong exception guarantee — file 13).
  `examples/08_noexcept_vector.cpp` measures this: 3x slower without `noexcept`.
- `examples/09_copy_vs_move_bench.cpp`: copy 1M `std::string`s = ~177 ms, move
  the vector = ~0.0001 ms (steal 3 pointers).

> **HFT relevance:** move semantics is why a function can `return
> std::vector<Order>` or a big `Book` snapshot by value with no copy — the
> result is constructed once and its buffer handed to the caller. In setup /
> control paths this makes value-semantic APIs cheap and safe. Hot inner loops
> usually don't move objects (they operate on pre-owned pooled memory in place),
> but the moment a `std::vector`/`std::string`/message payload crosses a
> function boundary or goes into a container, `noexcept` move ctors make it O(1).
> A hot type whose move ctor isn't `noexcept` (or is missing → falls back to
> copy) is a latent allocation storm during `vector` growth — caught by
> `static_assert(std::is_nothrow_move_constructible_v<T>)`.

---

## Hands-on

```bash
./build.ps1 18-COPY-MOVE/examples/04_move_semantics.cpp
./build.ps1 fast 18-COPY-MOVE/examples/09_copy_vs_move_bench.cpp
```

`04` — copy vs move ctor traces, moved-from `size()==0`, moved-from object
reassigned. `09` — measured copy vs move of a big `vector<string>`.

---

## ⚠️ Traps

### Trap 1 — move ctor without `noexcept`
```cpp
Buffer(Buffer&& o) : data_(o.data_) { o.data_ = nullptr; }   // ⚠️ vector realloc will COPY. Add noexcept
```

### Trap 2 — not nulling the source
```cpp
Buffer(Buffer&& o) noexcept : data_(o.data_) {}   // ⚠️ o.data_ still set -> BOTH dtors delete[] -> double-free
```

### Trap 3 — forgetting `std::move` on members
```cpp
Widget(Widget&& o) noexcept : name_(o.name_) {}   // ⚠️ o.name_ is an lvalue -> COPY. std::move(o.name_)
```

### Trap 4 — relying on moved-from value
```cpp
auto b = std::move(a);  useValue(a);   // ⚠️ a is "valid but unspecified". Assign or destroy only
```

### Trap 5 — move ctor throwing
```cpp
Buffer(Buffer&& o) noexcept { validate(); }   // ⚠️ if validate() throws -> noexcept violation -> std::terminate
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Move ctor copies then clears the source" | It *steals* (pointer transfer) — no copy at all |
| "Moved-from object is invalid / must not be touched" | Valid but unspecified — assign or destroy is fine |
| "The compiler-generated move ctor is enough always" | Only if you didn't declare dtor/copy ops (file 9) |
| "Hand-written move ctor doesn't need `std::move` on members" | It does — `o.member` is an lvalue |
| "`noexcept` on the move ctor is optional polish" | `std::vector` growth copies instead of moves without it (3x slower measured) |

---

## Exercises

1. **Write it:** `class Str { char* p_; size_t len_; ... };` — write the move
   ctor. What must the source `p_`/`len_` become, and why?

   <details><summary>Answer</summary>

   `Str(Str&& o) noexcept : p_(o.p_), len_(o.len_) { o.p_ = nullptr; o.len_ = 0;
   }`. Source `p_` → `nullptr` so `~Str()` does `delete[] nullptr` (no-op) instead
   of freeing the buffer `this` now owns (double-free).
   </details>

2. **Copy vs move trace:** `Buffer a{4}; Buffer b = a; Buffer c = std::move(a);`
   — with printed ctors, what prints? What is `a.size()` after?

   <details><summary>Answer</summary>

   `Buffer(4) ctor`, `COPY ctor` (for `b`, new buffer), `MOVE ctor` (for `c`,
   steals `a`'s pointer). `a.size() == 0` (moved-from, nulled).
   </details>

3. **Missing `std::move`:** `Widget(Widget&& o) noexcept : name_(o.name_),
   tags_(o.tags_) {}` where `name_` is `std::string`, `tags_` is
   `std::vector<std::string>`. What actually happens? Fix.

   <details><summary>Answer</summary>

   `o.name_` and `o.tags_` are lvalues → `name_` and `tags_` are **copy**-
   constructed (alloc + deep copy of the vector of strings). It's a "move ctor"
   that copies. Fix: `name_(std::move(o.name_)), tags_(std::move(o.tags_))`.
   </details>

4. **noexcept effect:** two identical `Buffer` classes, one with `noexcept` move,
   one without. `std::vector<Buffer> v; for (i<100000) v.push_back(Buffer{256});`
   — which is faster and why (measured in `08_noexcept_vector.cpp`)?

   <details><summary>Answer</summary>

   The `noexcept` one — on each reallocation, `std::vector` **moves** the
   existing elements (O(1) pointer steals). The non-`noexcept` one → `std::vector`
   **copies** them (alloc + memcpy per element) to preserve the strong exception
   guarantee. Measured ~3x slower.
   </details>

5. **Compiler-generated:** `class C { std::string s_; std::unique_ptr<T> p_;
   int n_; };` — does the compiler generate a move ctor? What does it do? Is it
   `noexcept`?

   <details><summary>Answer</summary>

   Yes (no user dtor/copy ops declared). Member-wise: `s_(std::move(o.s_))`,
   `p_(std::move(o.p_))`, `n_(o.n_)`. `std::string` and `unique_ptr` moves are
   `noexcept` → the generated `C` move ctor is `noexcept`. This is Rule of Zero
   working.
   </details>

---

## Interview questions

1. Move ctor vs copy ctor — kya steal karta, complexity?
2. Moved-from object ka state — kya guarantee?
3. Move ctor mein source ko null kyun karna zaroori?
4. Hand-written move ctor ke har member init pe `std::move` kyun?
5. `noexcept` move ctor — `std::vector` growth pe kya asar (aur kyun)?
6. Compiler-generated move ctor kab banta, kya karta?

---

## Next
→ [`07-move-assignment.md`](07-move-assignment.md)
