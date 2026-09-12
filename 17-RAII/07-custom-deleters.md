# 07 — Custom deleters (wrapping C APIs)

## Prerequisites
- [`04-unique-ptr.md`](04-unique-ptr.md), [`05-shared-ptr.md`](05-shared-ptr.md)
- Folder 15 file 13 (EBO), folder 12 file 11 (function pointers)

## Yeh topic abhi kyun
Har C library ka ek `create` / `destroy` (ya `open` / `close`, `alloc` / `free`)
pair hota hai — `FILE*`/`fclose`, `sqlite3*`/`sqlite3_close`, `SSL*`/`SSL_free`,
`addrinfo*`/`freeaddrinfo`. In sabko RAII mein wrap karne ka clean tareeka:
`unique_ptr` (ya `shared_ptr`) with a **custom deleter**. Deleter ki type / form
`unique_ptr` ke size ko affect karti hai — yeh jaanna zaroori.

---

## The problem

```cpp
void useFile() {
    std::FILE* f = std::fopen("data.bin", "rb");
    if (!f) return;
    // ... 10 exit paths, each needs std::fclose(f) ...
    std::fclose(f);
}
```

`std::unique_ptr<std::FILE>` ka default deleter `delete f;` karega → **UB**
(`f` `new` se nahi aaya). Chahiye: deleter jo `std::fclose(f)` kare.

---

## `unique_ptr` with a custom deleter

The deleter type is part of `unique_ptr`'s type: `std::unique_ptr<T, Deleter>`.

### Option A — stateless deleter struct (best, zero overhead)

```cpp
struct FileCloser {
    void operator()(std::FILE* f) const noexcept {
        if (f) std::fclose(f);
    }
};
using FilePtr = std::unique_ptr<std::FILE, FileCloser>;

FilePtr open(const char* path, const char* mode) {
    return FilePtr{std::fopen(path, mode)};
}

// use:
auto f = open("data.bin", "rb");
std::fread(buf, 1, n, f.get());
// scope end -> FileCloser{}(f.get()) -> fclose
```

`FileCloser` is an **empty** type → EBO (folder 15 file 13) → **`sizeof(FilePtr)
== 8`** (just the `FILE*`). Zero overhead. This is the idiomatic form.

### Option B — function pointer deleter (+8 bytes)

```cpp
std::unique_ptr<std::FILE, void(*)(std::FILE*)> f{
    std::fopen("x", "r"),
    [](std::FILE* p) { if (p) std::fclose(p); }   // stateless lambda -> function pointer
};
// sizeof == 16 -- the function pointer must be stored in the object
```

Works, but the deleter (a function pointer) is stored in the `unique_ptr` → 16
bytes, and the call is through a pointer (harder to inline).

### Option C — capturing lambda deleter (8 + capture size)

```cpp
int extra = 5;
auto del = [extra](std::FILE* p) { std::cout << extra; std::fclose(p); };
std::unique_ptr<std::FILE, decltype(del)> f{std::fopen("x","r"), del};
// sizeof == 8 + sizeof(capture)  -- rarely needed; prefer a stateless deleter
```

**Size summary (from `examples/05_custom_deleter.cpp`):**

| Deleter form | `sizeof(unique_ptr<FILE, D>)` |
|---|---|
| stateless struct (`FileCloser`) | **8** (EBO) |
| stateless lambda | **8** (EBO) |
| function pointer `void(*)(FILE*)` | **16** |
| capturing lambda | 8 + capture |

---

## Common C-API wrappers

```cpp
// FILE*
struct FileCloser { void operator()(std::FILE* f) const noexcept { if (f) std::fclose(f); } };
using FilePtr = std::unique_ptr<std::FILE, FileCloser>;

// malloc'd memory
struct FreeDeleter { void operator()(void* p) const noexcept { std::free(p); } };
using CBuf = std::unique_ptr<void, FreeDeleter>;
// or for a typed buffer: std::unique_ptr<char, FreeDeleter>

// POSIX addrinfo
struct AddrinfoDeleter { void operator()(addrinfo* a) const noexcept { ::freeaddrinfo(a); } };
using AddrinfoPtr = std::unique_ptr<addrinfo, AddrinfoDeleter>;

// sqlite3
struct Sqlite3Closer { void operator()(sqlite3* db) const noexcept { sqlite3_close(db); } };
using DbPtr = std::unique_ptr<sqlite3, Sqlite3Closer>;
```

Pattern: for each `X* create(...)` / `void destroy(X*)` pair, a 3-line deleter
struct + a `using` alias. Then RAII everywhere.

### Wrapping a non-pointer handle (fd, HANDLE)

`unique_ptr` wants a *pointer*. For an `int fd`, either:
- Write a small hand-rolled RAII class (`class FdGuard { int fd_; ~FdGuard(){ if
  (fd_>=0) ::close(fd_); } };` — file 09), **or**
- Use a deleter that takes the pointer-ish type and abuse `unique_ptr<int,
  Deleter>` with `&fd` (works but awkward — the pointer is to a stack `int`).

The hand-rolled class is cleaner for `int fd` / `HANDLE`. `unique_ptr` shines
when the resource *is* a pointer.

---

## `shared_ptr` with a custom deleter — size unchanged

```cpp
std::shared_ptr<std::FILE> f(std::fopen("x", "r"),
                             [](std::FILE* p) { if (p) std::fclose(p); });
// sizeof(shared_ptr<FILE>) == 16  -- ALWAYS, regardless of deleter
```

`shared_ptr` **type-erases** the deleter into the control block → the deleter can
be anything (lambda, functor, function pointer) and `shared_ptr<T>`'s size never
changes. (The control block may allocate a bit more for a fat deleter.) The
deleter is passed to the **constructor**, not the type.

This is why `shared_ptr<FILE>` "just works" with any deleter, while
`unique_ptr<FILE, D>` bakes `D` into the type. Trade-off: `shared_ptr`'s
flexibility costs the type erasure (an indirect call to the deleter) + the always-
16-byte size + refcount atomics.

---

## Andar kya hota hai

- `unique_ptr<T, D>` layout: `{ [[no_unique_address-ish]] D deleter; T* ptr; }`.
  If `D` is empty → compressed-pair / EBO → the `D` takes 0 bytes → `sizeof == 8`.
  If `D` has state (function pointer, capture) → it's stored → bigger.
- Dtor: `if (ptr) deleter(ptr);`. For a stateless struct deleter, `deleter(ptr)`
  inlines to `fclose(ptr)` (or whatever) — identical to a manual call.
- `shared_ptr`'s deleter lives in the control block behind a type-erased
  interface (a virtual call or a function pointer) → the deleter call is an
  indirect call, not inlined. Fine for a `close`/`free` (called once), irrelevant
  perf-wise.
- `make_unique` / `make_shared` **can't** take a custom deleter (they use the
  default) → for custom deleters you use the `unique_ptr<T,D>{ptr}` /
  `shared_ptr<T>(ptr, del)` constructors directly.

> **HFT relevance:** at startup / config time, HFT code touches plenty of C APIs
> (kernel-bypass NIC libraries, `epoll`/`timerfd`, `mmap`, `hugetlbfs`, PTP clock
> handles, shared-memory segments) — each gets a `unique_ptr<T, StatelessCloser>`
> (8 bytes, zero overhead) or a tiny hand-rolled `FdGuard`. This makes the
> error-handling paths in setup (which are numerous and rarely tested) leak-free
> and stuck-resource-free. Hot-path resources are pools/arenas, not C handles.
> Never a function-pointer or capturing-lambda deleter where a stateless struct
> works — keep the wrapper at pointer size.

---

## Hands-on

```bash
./build.ps1 17-RAII/examples/05_custom_deleter.cpp
```

`FILE*` wrapped in `unique_ptr<FILE, FileCloser>`; the size table (8 / 8 / 16 /
16); `shared_ptr<FILE>` with a lambda deleter; a fake `fd` wrapped with a
function-pointer deleter.

---

## ⚠️ Traps

### Trap 1 — `unique_ptr<FILE>` with the default deleter
```cpp
std::unique_ptr<std::FILE> f{std::fopen("x","r")};   // ⚠️ dtor does `delete f` -> UB. Custom deleter needed
```

### Trap 2 — function-pointer / capturing-lambda deleter where a struct works
```cpp
std::unique_ptr<FILE, void(*)(FILE*)> f{...};   // ⚠️ 16 bytes. Stateless struct -> 8
```

### Trap 3 — forgetting the null check in the deleter
```cpp
struct D { void operator()(FILE* f) const { std::fclose(f); } };   // ⚠️ fclose(nullptr) if fopen failed. `if (f)`
```

### Trap 4 — `make_unique` with a custom deleter
```cpp
auto p = std::make_unique<FILE, FileCloser>(...);   // ❌ doesn't exist. unique_ptr<FILE,FileCloser>{fopen(...)}
```

### Trap 5 — mixing allocation and deleter
```cpp
std::unique_ptr<T, FreeDeleter> p{new T};   // ⚠️ `new` + `free` mismatch -> UB. Match alloc/dealloc
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Custom deleter always makes `unique_ptr` bigger" | Stateless struct/lambda → EBO → still 8 bytes |
| "`shared_ptr` size changes with a custom deleter" | Never — deleter is type-erased into the control block |
| "`make_unique` can take a deleter" | It can't — use the `unique_ptr<T,D>{ptr}` constructor |
| "Function-pointer deleter is fine" | Works, but +8 bytes + indirect call; prefer a stateless struct |
| "Deleter doesn't need a null check" | It might be called with null (failed acquire) — guard it |

---

## Exercises

1. **Wrap it:** given `sqlite3* sqlite3_open(...)` and `int sqlite3_close(sqlite3*)`,
   write the deleter struct + `using DbPtr = ...`. What's `sizeof(DbPtr)`?

   <details><summary>Answer</summary>

   `struct Sqlite3Closer { void operator()(sqlite3* db) const noexcept {
   sqlite3_close(db); } };  using DbPtr = std::unique_ptr<sqlite3,
   Sqlite3Closer>;` — `sizeof(DbPtr) == 8` (empty deleter → EBO).
   </details>

2. **Size experiment:** for `std::FILE*`, make `unique_ptr`s with (a) a stateless
   struct deleter, (b) `void(*)(FILE*)`, (c) a lambda capturing an `int`. Predict
   each `sizeof`.

   <details><summary>Answer</summary>

   (a) 8 (EBO). (b) 16 (function pointer stored). (c) 8 + `sizeof(int)` rounded →
   16 (the captured `int` is stored alongside the `FILE*`).
   </details>

3. **shared vs unique deleter:** `std::shared_ptr<FILE> a(fopen(...), del1);` and
   `std::shared_ptr<FILE> b(fopen(...), del2);` where `del1` and `del2` are
   *different* lambda types. Can `a` and `b` be the same type? For `unique_ptr`?

   <details><summary>Answer</summary>

   `shared_ptr<FILE>` — yes, both are `std::shared_ptr<FILE>` (deleter type-erased
   in the control block). `unique_ptr` — no: `unique_ptr<FILE, decltype(del1)>`
   and `unique_ptr<FILE, decltype(del2)>` are distinct types.
   </details>

4. **fd wrapper:** why is a hand-rolled `class FdGuard { int fd_; ... }` cleaner
   than `std::unique_ptr<int, CloseFd>` for a POSIX file descriptor?

   <details><summary>Answer</summary>

   An `fd` is an `int`, not a pointer. `unique_ptr` needs a pointer type; you'd
   have to store `&someInt` and the deleter would take `int*` — awkward, and the
   "null" state is `-1` not `nullptr`. A small class with `int fd_ = -1` and a
   `~FdGuard` that does `if (fd_ >= 0) ::close(fd_)` is direct and idiomatic
   (file 09).
   </details>

5. **null-safe deleter:** `open()` returns `FilePtr{std::fopen(bad, "r")}` where
   `fopen` fails (returns null). Does the `FileCloser` deleter get called? With
   what? Is it a problem?

   <details><summary>Answer</summary>

   `unique_ptr` with a null pointer — its dtor does `if (ptr) deleter(ptr);`, so
   the deleter is **not** called for null. (Even if it were, `FileCloser`'s `if
   (f)` guard handles it.) No problem — a `FilePtr` holding null is just an empty
   RAII wrapper.
   </details>

---

## Interview questions

1. `unique_ptr<FILE>` default deleter kyun galat? Fix?
2. Deleter ki form → `unique_ptr` size: struct / fn-ptr / capturing lambda?
3. Empty deleter struct 0 bytes kaise (EBO)?
4. `shared_ptr` custom deleter — size kyun change nahi hota (type erasure)?
5. `make_unique` custom deleter le sakta? Nahi to kaise?
6. `int fd` ke liye `unique_ptr` vs hand-rolled class — kyun latter?

---

## Next
→ [`08-ownership-semantics.md`](08-ownership-semantics.md)
