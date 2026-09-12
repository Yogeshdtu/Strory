# 04 — `std::unique_ptr`

## Prerequisites
- [`02-raii-idiom.md`](02-raii-idiom.md), [`03-why-raii-works.md`](03-why-raii-works.md)
- Folder 16 file 08 (slicing — why polymorphic containers need pointers), folder 13 (move intro)

## Yeh topic abhi kyun
`std::unique_ptr<T>` = RAII for a single heap object. **Exclusive ownership**
(ek owner), **move-only** (ownership transfer hota hai, copy nahi), aur **zero
overhead** (raw pointer jitna hi — `sizeof` 8, dtor inline). Yeh raw owning
`new`/`delete` ka default replacement hai.

---

## Basics

```cpp
#include <memory>

auto p = std::make_unique<Widget>(1, 2, 3);   // Widget{1,2,3} on the heap, p owns it

p->method();          // -> like a pointer
(*p).field;           // deref
Widget* raw = p.get();  // non-owning raw view (don't delete it)
if (p) { }            // explicit operator bool -- p non-null?

// scope end -> p.~unique_ptr() -> delete the Widget
```

- **`std::make_unique<T>(args...)`** — prefer over `unique_ptr<T>(new T(...))`:
  exception-safe (no leak if another arg's evaluation throws), one line, no `new`
  in your code.
- Dtor calls `delete` (or `delete[]` for `T[]`) automatically.
- `sizeof(unique_ptr<T>) == sizeof(T*)` with the default deleter — **zero
  overhead** (file 11).

---

## Move-only — ownership transfer

```cpp
auto a = std::make_unique<Widget>();

// auto b = a;                 // ❌ compile ERROR -- unique_ptr non-copyable (copy ctor deleted)
auto b = std::move(a);         // ✅ ownership a -> b.  a is now nullptr.

// pass ownership INTO a function
void consume(std::unique_ptr<Widget> w);   // by value
consume(std::move(b));                       // b -> w; w's dtor deletes it. b now null.

// return ownership OUT of a function
std::unique_ptr<Widget> make() {
    return std::make_unique<Widget>();       // moved out (implicit move / RVO)
}
```

**Copy nahi hota** — kyunki agar do `unique_ptr` same `Widget` ko own karein, to
dono ke dtors `delete` karenge → **double-free**. Compiler copy ctor / copy
assignment ko `= delete` karke isse **compile-time** rok deta hai. Ownership
sirf **move** se shift hota — aur move ke baad source `nullptr` ho jaata (uska
dtor ab kuch delete nahi karega).

---

## API surface

```cpp
std::unique_ptr<Widget> p = std::make_unique<Widget>();

p.get();              // Widget* -- non-owning; use to pass to APIs that want a raw ptr
p.release();          // Widget* -- GIVE UP ownership; p becomes null; YOU must delete now
p.reset();            // delete current, p = null
p.reset(new Widget);  // delete current, take ownership of the new one
p.swap(q);            // swap ownership with another unique_ptr
static_cast<bool>(p); // p != nullptr
*p, p->m              // deref (UB if null)
```

`release()` vs `reset()`:
- `reset()` — "delete now" (or replace).
- `release()` — "I'm taking the raw pointer out; unique_ptr forgets it; **I** own
  it now" (rare — for handing to a C API that will free it, or converting
  ownership models).

---

## `unique_ptr<T[]>` — arrays

```cpp
auto arr = std::make_unique<int[]>(100);      // int[100] on the heap
arr[7] = 42;                                    // operator[] (no ->/* for the array form)
// dtor -> delete[] (correct array form)
```

- `make_unique<T[]>(n)` **value-initializes** the elements (zeros for POD).
- No `.get()` needed for indexing — `arr[i]` works.
- **But:** `std::vector<T>` is almost always better (knows its size, resizable,
  range-for, algorithms). `unique_ptr<T[]>` only for fixed-size buffers where you
  want to avoid `vector`'s (tiny) size overhead, or for C-array interop.

---

## As a class member — Rule of Zero (file 10)

```cpp
class Engine {
    std::unique_ptr<Impl> pimpl_;        // owns the Impl
    std::unique_ptr<Logger> logger_;
public:
    Engine();
    ~Engine();                            // pimpl idiom -- define in .cpp where Impl is complete
    // NO copy ctor / copy assign / dtor logic needed for the pointers themselves --
    // unique_ptr's own dtor handles delete. The class becomes move-only automatically.
};
```

A class with `unique_ptr` members:
- Automatically **move-only** (unique_ptr is move-only → the class is too).
- Its **destructor is auto-generated** correctly (deletes the pointees). You
  don't write `delete pimpl_;`.
- This is **Rule of Zero** — you write no special members; the members manage
  themselves.

⚠️ pImpl gotcha: if `Impl` is only forward-declared in the header, the compiler
needs `Impl` complete to generate `~unique_ptr<Impl>` → declare `~Engine();` in
the header and `= default` it in the `.cpp` (folder 15 file 12).

---

## Polymorphism — `unique_ptr<Base>`

```cpp
std::vector<std::unique_ptr<Shape>> shapes;
shapes.push_back(std::make_unique<Circle>(2.0));
shapes.push_back(std::make_unique<Square>(3.0));

for (const auto& s : shapes)
    total += s->area();                   // virtual dispatch -- no slicing (folder 16 file 08)
```

Needs `Shape` to have a **`virtual` destructor** (folder 16 file 07) — else
`delete (Shape*)` skips the derived dtor. `unique_ptr<Base>` correctly calls
through it.

---

## Andar kya hota hai

- `unique_ptr<T>` (default deleter) is literally `struct { T* ptr; };` — one
  pointer. The deleter is `std::default_delete<T>` which is **empty** → EBO
  (folder 15 file 13) → no size cost.
- Move ctor: `new.ptr = old.ptr; old.ptr = nullptr;` — 2 pointer moves. Move
  assign: `delete this->ptr; this->ptr = other.ptr; other.ptr = nullptr;`.
- Dtor: `if (ptr) delete ptr;` — inlined at `-O2`; identical to a manual
  `delete` at end of scope.
- `.get()`, `operator->`, `operator*` — all just return / deref `ptr`, inlined
  to a `mov` / direct access. **Zero abstraction cost** (measured — `examples/
  07_smartptr_benchmark.cpp`: `unique_ptr` ≈ raw, 0.99x).
- Passing `unique_ptr` by value → move (2 pointer ops), not a copy.

> **HFT relevance:** `unique_ptr` is free — use it wherever a heap object has a
> single clear owner (config objects, per-connection state, pImpl, plugin
> instances). Zero size overhead, zero call overhead, and it makes ownership
> **explicit in the type** (a `Widget*` parameter says "I don't own this";
> `unique_ptr<Widget>` says "I'm taking ownership"). In the *hot path*, heap
> objects are usually pooled (folder 14) rather than individually `new`d, so
> `unique_ptr` appears more in setup/control code — but even there it's
> zero-cost, so there's no reason to hand-roll `new`/`delete`.

---

## Hands-on

```bash
./build.ps1 17-RAII/examples/02_unique_ptr.cpp
./build.ps1 fast 17-RAII/examples/07_smartptr_benchmark.cpp   # unique_ptr == raw
```

`02` — make_unique, move (copy = error), pass/return ownership, `reset`/`release`,
`unique_ptr<int[]>`, container, `sizeof` == 8.

---

## ⚠️ Traps

### Trap 1 — copying a `unique_ptr`
```cpp
auto b = a;              // ❌ compile error. auto b = std::move(a);
void f(std::unique_ptr<T> p);  f(a);   // ❌ f(std::move(a));
```

### Trap 2 — using a moved-from `unique_ptr`
```cpp
auto b = std::move(a);  a->method();   // ⚠️ a is nullptr -> null deref
```

### Trap 3 — `release()` without deleting
```cpp
Widget* raw = p.release();   // ⚠️ p forgot it; if you don't `delete raw` -> leak
```

### Trap 4 — `make_unique<T>(new U)` / double management
```cpp
Widget* raw = new Widget;
std::unique_ptr<Widget> a{raw};
std::unique_ptr<Widget> b{raw};   // ⚠️ two owners of the same raw -> double-free
```

### Trap 5 — `unique_ptr<T[]>` with `->` / `delete` mismatch
```cpp
std::unique_ptr<int> p{new int[10]};   // ⚠️ dtor does `delete` not `delete[]` -> UB. unique_ptr<int[]>
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Smart pointers are slower than raw" | `unique_ptr` == raw (measured 0.99x) |
| "`unique_ptr` can be copied" | Move-only; copy is a compile error (by design) |
| "After `std::move`, the source still works" | Source is `nullptr`; deref = crash |
| "`release()` deletes the object" | `release()` gives up ownership WITHOUT deleting |
| "`unique_ptr<int>` works for `new int[10]`" | Wrong deleter (`delete` vs `delete[]`) — use `unique_ptr<int[]>` |

---

## Exercises

1. **Move semantics:** `auto a = std::make_unique<int>(5); auto b = std::move(a);
   std::cout << (a == nullptr) << " " << *b;` — output?

   <details><summary>Answer</summary>

   `1 5` — `a` is null after the move, `b` owns the `int(5)`.
   </details>

2. **Ownership transfer:** write `std::unique_ptr<Widget> pipeline(std::unique_ptr<Widget>
   w)` that does something to `*w` and returns ownership. Call it 3 times in a
   chain: `p = pipeline(pipeline(pipeline(std::move(p))))`.

   <details><summary>Answer</summary>

   `std::unique_ptr<Widget> pipeline(std::unique_ptr<Widget> w) { w->process();
   return w; }` — takes ownership by value, returns it by value (move out). The
   chain moves the same object through 3 stages, one owner at a time.
   </details>

3. **Rule of Zero:** `class Session { std::unique_ptr<Socket> sock_;
   std::unique_ptr<Buffer> buf_; };` — is `Session` copyable? Movable? Does it
   need a destructor? What does the compiler-generated one do?

   <details><summary>Answer</summary>

   Not copyable (unique_ptr isn't), automatically movable. No destructor needed —
   the compiler-generated one destroys `buf_` then `sock_` (reverse decl order),
   each `unique_ptr` dtor `delete`s its pointee.
   </details>

4. **release for C API:** a C function `void takeOwnership(Widget* w)` that will
   `free`/`delete` `w` itself. You have a `std::unique_ptr<Widget> p`. How do you
   hand it over without a double-free?

   <details><summary>Answer</summary>

   `takeOwnership(p.release());` — `release()` makes `p` forget the pointer (its
   dtor won't delete), and the C function now owns it. (Only valid if the C
   function frees it compatibly with how it was allocated.)
   </details>

5. **Polymorphic vector:** `std::vector<std::unique_ptr<Animal>> zoo;` — add a
   `Dog` and a `Cat`, call `speak()` on each. What must `Animal` have? What
   happens on `zoo.clear()`?

   <details><summary>Answer</summary>

   `Animal` needs `virtual ~Animal()` (and `virtual void speak()`). `zoo.clear()`
   destroys each `unique_ptr`, each calling `delete (Animal*)` → virtual dtor →
   correct `~Dog()` / `~Cat()`.
   </details>

---

## Interview questions

1. `unique_ptr` — ownership model, copy vs move, `sizeof`?
2. `make_unique` vs `unique_ptr(new T)` — 2 reasons to prefer `make_unique`?
3. `release()` vs `reset()`?
4. `unique_ptr` member → class ke special members ka kya (Rule of Zero)?
5. `unique_ptr<Base>` polymorphism ke liye — `Base` mein kya chahiye?
6. `unique_ptr` ka runtime overhead — measured?

---

## Next
→ [`05-shared-ptr.md`](05-shared-ptr.md)
