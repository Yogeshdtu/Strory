# 09 — Placement new & manual lifetime management

## Prerequisites
- `02-object-lifetime.md`, `08-alignment-deep.md`, `17-RAII`
- [`examples/05_placement_new.cpp`](examples/05_placement_new.cpp)

## Yeh topic abhi kyun
Placement new = **storage aur object construction ko alag karna**. Aap pehle se
allocated memory (stack buffer, arena, pool slot, mmap'd region) mein ek object ka
**lifetime shuru** karte ho, bina `operator new`/heap ke. Iske upar `std::vector`,
`std::optional`, small-buffer optimization, aur har HFT memory pool bana hai.

---

## Syntax

```cpp
#include <new>       // <-- placement new + std::launder

alignas(T) unsigned char buf[sizeof(T)];       // raw storage — koi T object nahi

T* p = ::new (buf) T(args...);                  // construct a T IN buf (no allocation)
p->method();
p->~T();                                        // destruct (explicit — no delete!)
```

- `::new (address) T(args...)` — "yeh address diya, uspe `T` construct karo."
  Returns a `T*` to the new object.
- `address` `alignof(T)` ke liye suitably aligned hona chahiye (file 08).
- **No `delete`.** Placement new ne `operator new` call nahi kiya, toh `operator
  delete` bhi nahi. Cleanup = **explicit `p->~T()`**.
- `<new>` include karo (`::new` global scope se, taaki class ka `operator new`
  override na aaye).

Overloaded placement forms bhi hain (`std::nothrow`, custom), par sabse common yeh
"pointer" form hai.

---

## Kyun — 4 real uses

### 1. Object in a stack buffer (no heap, no default-construct requirement)

```cpp
alignas(Big) unsigned char slot[sizeof(Big)];
if (should_build) {
    Big* b = ::new (slot) Big(config);   // construct only when needed
    use(*b);
    b->~Big();
}
```

`std::optional<T>` is exactly this: `alignas(T) storage + bool engaged`. `examples/05`
mein ek `FixedOptional<T>` yehi karta.

### 2. Separate allocation from construction (containers)

`std::vector<T>` `reserve(100)` → `operator new(100 * sizeof(T))` — raw storage.
Phir `push_back` → **placement new** into the next slot. `pop_back` → `back().~T()`.
Growth → new buffer, move/copy-construct each element into it (placement new),
destruct the old ones.

```cpp
// vector::push_back ka core (simplified):
::new (data_ + size_) T(std::forward<Args>(args)...);
++size_;
```

### 3. Memory pool / arena

```cpp
template <class T, std::size_t N>
class Pool {
    alignas(T) unsigned char storage_[N * sizeof(T)];
    std::size_t used_ = 0;
public:
    template <class... A>
    T* create(A&&... a) {
        if (used_ == N) return nullptr;
        T* p = ::new (storage_ + used_ * sizeof(T)) T(std::forward<A>(a)...);
        ++used_;
        return p;
    }
    // destroy: p->~T();  (+ track slots for reuse — a free list)
};
```

Hot path allocation = a bump + a placement new. No `malloc`, no lock, no page
fault. (Folder 36 goes deep.)

### 4. Reuse storage for a different type / value

```cpp
alignas(std::max_align_t) unsigned char box[64];
auto* s = ::new (box) std::string("hi");
s->~basic_string();
auto* n = ::new (box) std::int64_t{42};   // same bytes, new object of a new type
```

---

## `std::launder` — the subtle part

Jab aap **usi storage** mein ek naya object banao jahan purana tha, aur purana
pointer/reference reuse karo:

```cpp
struct Cfg { const int version; };        // const member!
alignas(Cfg) unsigned char buf[sizeof(Cfg)];
Cfg* p = ::new (buf) Cfg{1};
int a = p->version;                        // 1
::new (buf) Cfg{2};                        // new Cfg, same bytes
int b = p->version;                        // ⚠️ UB — compiler may still think it's 1
int c = std::launder(p)->version;          // ✅ 2 — "read what's there NOW"
```

- `std::launder(p)` — "is pointer ki provenance/history ke baare mein kuch mat
  maano; is address pe jo *ab* hai usse access karo."
- Zaroori jab type mein: **`const` members**, **reference members**, ya
  **virtual functions** (compiler purani value / vptr ko constant-fold / cache kar
  sakta).
- **Simple mutable POD** ke liye technically bhi chahiye per standard, par
  practically GCC/Clang usually theek. Best practice: **placement new ka return
  value use karo** (naya `T*`), tab `std::launder` ki zaroorat hi nahi.
- `examples/05` ka `FixedOptional<T>` har access pe `std::launder` use karta — safe
  by construction.

---

## Aligned storage — the right way

```cpp
// ✅ modern (C++11+):
alignas(T) unsigned char storage[sizeof(T)];

// ❌ std::aligned_storage<Size, Align> — DEPRECATED in C++23 (was fiddly, error-prone)
// ❌ char storage[sizeof(T)];   // may not be aligned for T -> UB on use
```

`unsigned char` / `std::byte` array with `alignas(T)` — that's it. `std::byte`
(C++17) is the "this is raw storage" type.

For a **buffer of N objects**: `alignas(T) unsigned char storage[N * sizeof(T)]`,
and slot `i` is `storage + i * sizeof(T)`.

---

## Andar kya hota hai

- Placement `::new (p) T(args)` → **no `operator new` call**; the compiler emits
  the constructor call with `this = p`. That's the whole runtime cost — just the
  ctor.
- `p->~T()` → a direct call to the destructor (virtual dispatch if `T` is
  polymorphic and you call through a base pointer — use the most-derived type or
  a virtual dtor).
- `std::launder(p)` → `__builtin_launder(p)` — an optimization barrier on that
  pointer's value; no code generated, but the optimizer can no longer assume it
  points at the *old* object.
- `std::vector`, `std::optional`, `std::variant`, `std::deque`, SSO in
  `std::string` — all built on `alignas` storage + placement new + explicit dtor.

---

## > **HFT relevance**
> - **Every hot-path allocator** — object pools, slab allocators, arena/bump
>   allocators, intrusive free lists (folder 20 file 06, folder 36) — is
>   "pre-allocate a big buffer, then placement-new / explicit-dtor per object."
>   Zero `malloc` on the tick path.
> - **Ring buffers / SPSC queues of non-trivial types** — placement-new the
>   element into the slot on produce, `->~T()` on consume (for trivially-copyable
>   types you can just `memcpy` and skip the dtor — folder 06).
> - **`std::optional`-style "maybe a result"** without heap — `FixedOptional<T>`
>   pattern (`examples/05`).
> - **`std::launder` discipline** — when reusing a slot for a type with
>   `const`/reference members or a vtable, either launder or (better) use the
>   fresh pointer from placement new.
> - **Trivially-destructible pool** — skip the dtor loop on teardown, just reset
>   the cursor (folder 06 exercise).

---

## Hands-on

```bash
./build.ps1 25-OBJECT-MODEL/examples/05_placement_new.cpp
```

Sections: manual construct/destruct in `alignas` storage, storage reuse, the
`std::launder` note, and `FixedOptional<Widget>` (no heap, `alive` count → 0).
Then:
- Build the `Pool<T, N>` above; `create()` 5 objects, watch `alive`; destroy them;
  confirm `alive == 0`.
- Reuse a slot for `std::string` then `std::int64_t`; forget the `->~T()` on the
  string first — run under a leak checker (or count with a wrapper).
- Add a `const` member to `Widget`, access through a stale pointer after re-emplace
  vs `std::launder` — compare at `-O2`.

---

## ⚠️ Traps

### Trap 1 — using `delete` after placement new
```cpp
T* p = ::new (buf) T{};
delete p;   // ⚠️ UB — calls operator delete on memory operator new never gave out
p->~T();     // ✅
```

### Trap 2 — forgetting the explicit destructor
```cpp
::new (buf) std::string("big");   // ⚠️ no ~string() -> the heap buffer leaks
```

### Trap 3 — double destruction
Placement-new'd object + an automatic object of the same name/scope → the dtor runs
twice. Only placement-new into storage with **no** automatic dtor.

### Trap 4 — misaligned placement address
```cpp
char raw[sizeof(T)];                 // ⚠️ not aligned for T
::new (raw) T{};                      // UB on use
alignas(T) unsigned char raw2[sizeof(T)];   // ✅
```

### Trap 5 — stale pointer after reuse without `std::launder`
```cpp
T* p = ::new (buf) T{1};
::new (buf) T{2};
p->x;                    // ⚠️ UB for const/ref/vtable members — use launder or the new pointer
```

### Trap 6 — `new (buf) T[n]` array placement form
The array placement form (`new (buf) T[n]`) may request extra bytes for a cookie
on some ABIs → your buffer overflows. Prefer a loop of scalar placement news.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "placement new allocates" | It doesn't — you supply the storage; it just runs the ctor |
| "`delete` cleans up a placement-new'd object" | No — explicit `p->~T()`; `delete` on that memory is UB |
| "`char buf[sizeof(T)]` is fine for placement new" | Only with `alignas(T)`; else misaligned → UB |
| "the old pointer works after re-emplacing" | UB for `const`/ref/vtable members — `std::launder` or a new pointer |
| "`std::aligned_storage` is the tool" | Deprecated (C++23); use `alignas(T) unsigned char[sizeof(T)]` |
| "you must call the dtor for every type" | Trivially-destructible types: optional (no side effects) |

---

## Exercises

1. **Fix it:** `unsigned char b[sizeof(std::string)]; auto* s = new (b)
   std::string("x"); return;` — two things wrong.

   <details><summary>Answer</summary>

   (1) `b` isn't aligned for `std::string` — needs `alignas(std::string)`.
   (2) No `s->~basic_string()` before `b` goes out of scope → the string's heap
   allocation leaks.
   </details>

2. **Cleanup:** for `::new (buf) int{5}` — do you need `->~int()` / anything?

   <details><summary>Answer</summary>

   `int` is trivially destructible — no dtor call needed (it would be a no-op /
   pseudo-destructor). You *can* write `p->~/*int alias*/()` for symmetry in
   generic code, but nothing leaks either way.
   </details>

3. **`std::launder`:** `struct S { int& r; };` in a slot. After re-emplacing `S`
   over an old one, why must you launder (or use the new pointer)?

   <details><summary>Answer</summary>

   `S` has a reference member. The compiler may assume `oldPtr->r` still binds to
   the old referent (it can propagate/cache that). Accessing the new `S` through
   the stale pointer is UB. `std::launder(oldPtr)->r`, or just use the `S*` that
   placement new returned.
   </details>

4. **Pool teardown:** a `Pool<T, N>` is destroyed with 30 live objects. What must
   its destructor do, and when can it skip work?

   <details><summary>Answer</summary>

   Call `->~T()` on each of the 30 live slots (track which are live). It can skip
   the dtor loop entirely when `std::is_trivially_destructible_v<T>` — just let
   the buffer's storage go away.
   </details>

5. **`optional` from scratch:** sketch the storage + `emplace` + `reset` of a
   `Maybe<T>`.

   <details><summary>Answer</summary>

   ```cpp
   alignas(T) unsigned char s_[sizeof(T)]; bool has_ = false;
   template<class...A> T& emplace(A&&...a){ reset();
       T* p = ::new(static_cast<void*>(s_)) T(std::forward<A>(a)...); has_=true; return *p; }
   void reset(){ if(has_){ std::launder(reinterpret_cast<T*>(s_))->~T(); has_=false; } }
   ~Maybe(){ reset(); }
   ```
   (Access via `std::launder`; delete copy/move or implement carefully.)
   </details>

---

## Interview questions

1. Placement new — kya karta, kya NAHI karta (no allocation)?
2. Placement-new'd object ka cleanup — `delete` kyun galat, kya sahi?
3. Aligned storage kaise banao (modern way)? `std::aligned_storage` ka kya hua?
4. `std::launder` — kis liye, kab zaroori (const/ref/vtable + reuse)?
5. `std::vector::push_back` internally placement new kaise use karta?
6. Trivially-destructible type ke liye explicit dtor call zaroori?
7. Memory pool ka core — allocation = kya (bump + placement new)?

---

## Next
→ [`10-strict-aliasing.md`](10-strict-aliasing.md)
