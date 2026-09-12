# 09 — RAII for other resources (files, locks, sockets)

## Prerequisites
- [`02-raii-idiom.md`](02-raii-idiom.md), [`07-custom-deleters.md`](07-custom-deleters.md)
- Folder 15 (classes), folder 18 preview (move — for movable wrappers)

## Yeh topic abhi kyun
Smart pointers memory ke liye hain. Baaki resources — file descriptors, mutex
locks, sockets, CPU affinity, timers, mmap regions — ke liye aap **apne RAII
wrappers** likhte ho (ya STL ke ready-made use karte). Pattern har jagah same:
ctor acquire, dtor release, non-copyable, movable. Yeh lesson concrete wrappers.

---

## Lock RAII — `std::lock_guard` / `std::scoped_lock` / `std::unique_lock`

STL already provides these (folder 26 mein detail):

```cpp
#include <mutex>
std::mutex mtx;

void f() {
    std::lock_guard<std::mutex> g{mtx};   // ctor -> mtx.lock()
    // critical section
    if (x) return;                          // g dtor -> mtx.unlock()
    doStuff();
}                                            // g dtor -> mtx.unlock()

// C++17 -- multiple mutexes, deadlock-free acquisition order
std::scoped_lock lk{mtxA, mtxB};

// unique_lock -- movable, deferrable, works with condition_variable
std::unique_lock<std::mutex> ul{mtx};
ul.unlock();  /* ... */  ul.lock();
```

**Never** call `mtx.lock()` / `mtx.unlock()` manually — one missed `unlock()` (an
early return, a throw) = **deadlock forever**. Always a guard.

---

## File RAII

STL: `std::ifstream` / `std::ofstream` / `std::fstream` — already RAII (dtor
closes). For a raw `FILE*` (C stdio) or a POSIX `fd`, wrap it.

### `FILE*` — `unique_ptr` with a deleter (file 07)

```cpp
struct FileCloser { void operator()(std::FILE* f) const noexcept { if (f) std::fclose(f); } };
using FilePtr = std::unique_ptr<std::FILE, FileCloser>;

FilePtr f{std::fopen("data.bin", "rb")};
if (!f) throw std::runtime_error("open failed");
std::fread(buf, 1, n, f.get());
// scope end -> fclose
```

### POSIX `fd` — hand-rolled (an `fd` is an `int`, not a pointer)

```cpp
class Fd {
    int fd_ = -1;
public:
    Fd() = default;
    explicit Fd(int fd) noexcept : fd_(fd) {}
    ~Fd() { if (fd_ >= 0) ::close(fd_); }

    // move-only (unique ownership of the fd)
    Fd(Fd&& o) noexcept : fd_(o.fd_) { o.fd_ = -1; }        // steal, null out source
    Fd& operator=(Fd&& o) noexcept {
        if (this != &o) { if (fd_ >= 0) ::close(fd_); fd_ = o.fd_; o.fd_ = -1; }
        return *this;
    }
    Fd(const Fd&)            = delete;
    Fd& operator=(const Fd&) = delete;

    int  get() const noexcept { return fd_; }
    explicit operator bool() const noexcept { return fd_ >= 0; }
    int  release() noexcept { int t = fd_; fd_ = -1; return t; }   // give up ownership
};

Fd sock{::socket(AF_INET, SOCK_STREAM, 0)};
if (!sock) throw std::runtime_error("socket");
// ... use sock.get() ...
// scope end -> ::close
```

This is the **canonical move-only RAII wrapper** shape (folder 18 formalizes the
move ops):
- `int fd_ = -1` — the handle, `-1` = "empty".
- Ctor takes an already-acquired handle (or acquires).
- Dtor releases if valid.
- **Move** = steal + null the source (so the moved-from dtor is a no-op).
- **Copy = deleted** (two owners of one fd → double-close).
- `get()`, `explicit operator bool()`, `release()`.

---

## Socket RAII

A socket is just an `fd` (POSIX) or `SOCKET` (Windows). Same `Fd`-style wrapper,
maybe with `shutdown()` before `close()`:

```cpp
class Socket {
    int fd_ = -1;
public:
    // ... same move-only shape as Fd ...
    ~Socket() {
        if (fd_ >= 0) { ::shutdown(fd_, SHUT_RDWR); ::close(fd_); }
    }
    ssize_t send(const void* p, size_t n) { return ::send(fd_, p, n, MSG_NOSIGNAL); }
    ssize_t recv(void* p, size_t n)       { return ::recv(fd_, p, n, 0); }
};
```

---

## Scoped state changes — RAII beyond "handles"

RAII also fits any **"set X, do work, restore X"** pattern:

```cpp
// restore std::cout's format flags after a scope
class IosFlagsGuard {
    std::ostream& os_;
    std::ios::fmtflags flags_;
public:
    explicit IosFlagsGuard(std::ostream& os) : os_(os), flags_(os.flags()) {}
    ~IosFlagsGuard() { os_.flags(flags_); }
};

{
    IosFlagsGuard g{std::cout};
    std::cout << std::hex << std::setw(8) << x;
}   // std::cout's flags restored -- no leaked hex mode

// set + restore CPU affinity for a scope (HFT-ish)
class ScopedAffinity {
    cpu_set_t saved_;
public:
    explicit ScopedAffinity(int cpu) {
        sched_getaffinity(0, sizeof(saved_), &saved_);
        cpu_set_t set; CPU_ZERO(&set); CPU_SET(cpu, &set);
        sched_setaffinity(0, sizeof(set), &set);
    }
    ~ScopedAffinity() { sched_setaffinity(0, sizeof(saved_), &saved_); }
};
```

Any "temporarily change global/thread state" → RAII guard → guaranteed restore
on every exit path.

---

## `std::unique_ptr` with a deleter as a generic guard

For a one-off cleanup you don't want a named class for, `unique_ptr<void, D>` or
a "scope guard" utility:

```cpp
// poor-man's scope guard
auto cleanup = std::unique_ptr<void, void(*)(void*)>(
    reinterpret_cast<void*>(1),                  // non-null sentinel
    [](void*) { std::puts("cleanup ran"); }
);

// libraries (GSL, folly, abseil) provide a proper `scope_exit` / `finally`:
// auto g = gsl::finally([&]{ resource.release(); });
```

`std::experimental::scope_exit` / `gsl::finally` / a 10-line `ScopeGuard`
template is common for ad-hoc RAII where writing a class is overkill.

---

## Andar kya hota hai

- These wrappers compile to **exactly the manual code** — `Fd`'s dtor is `if
  (fd_ >= 0) close(fd_);`, inlined at the `}`. `lock_guard` is `mtx.lock()` in
  the ctor, `mtx.unlock()` at scope end. Zero abstraction cost; the win is
  reliability (every exit path) and one-place cleanup.
- Move-only wrappers: move ctor = a few field copies + null-out (2-3 `mov`s),
  inlined. No allocation, no refcount.
- `-fno-exceptions` builds: dtors still run on scope exit / early return, so RAII
  still works (only the *throw* path is gone).
- A `noexcept` dtor (default) → included in unwind tables cleanly; a throwing
  dtor during unwind → `std::terminate` (file 03) — so wrappers do cleanup that
  can't reasonably fail, or log-and-swallow.

> **HFT relevance:** the trading process's startup touches dozens of OS
> resources — `epoll`/`timerfd`/`eventfd` fds, raw sockets, `mmap`'d shared
> memory and hugepages, PTP/PHC clock fds, `mlock`ed regions, CPU affinity, RT
> scheduling class. Each gets a move-only RAII wrapper (`Fd`, `MmapRegion`,
> `ScopedAffinity`, `ScopedRtPriority`) so that the many rarely-exercised error
> paths in setup can't leak an fd or leave the process pinned to the wrong core.
> Locks in HFT are mostly avoided on the hot path, but where they exist
> (control-plane, logging handoff) they're always `lock_guard`/`scoped_lock` —
> a hand-managed `unlock()` that gets skipped by an early return is a
> system-wide stall. RAII guards for "scoped measurement" (`rdtsc` in ctor,
> record histogram bucket in dtor) are a common profiling pattern.

---

## Hands-on

```bash
./build.ps1 17-RAII/examples/05_custom_deleter.cpp
```

`FILE*` via `unique_ptr<FILE, FileCloser>`, and a fake `fd` via a
function-pointer deleter. Write your own `Fd` class (move-only) and a
`ScopedTimer` guard; test that both clean up on early return.

---

## ⚠️ Traps

### Trap 1 — manual `unlock()`
```cpp
mtx.lock();  if (err) return;  mtx.unlock();   // ⚠️ early return -> deadlock. std::lock_guard
```

### Trap 2 — copyable fd wrapper
```cpp
class Fd { int fd_; ~Fd(){ close(fd_); } };   // ⚠️ no deleted copy -> Fd b = a; -> double-close
```

### Trap 3 — move ctor that doesn't null the source
```cpp
Fd(Fd&& o) : fd_(o.fd_) {}   // ⚠️ o.fd_ still valid -> BOTH dtors close it -> double-close. o.fd_ = -1;
```

### Trap 4 — RAII guard as a member without deciding copy/move
```cpp
struct Server { Fd listen_; };   // Server is move-only now (Fd is). Copy of Server = compile error -- know it
```

### Trap 5 — throwing from a cleanup dtor
```cpp
~Writer() { if (::fsync(fd_) < 0) throw ...; }   // ⚠️ during unwinding -> terminate. Log, don't throw
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "RAII is only for `new`/`delete`" | Files, locks, sockets, affinity, timers, format flags — anything acquire/release |
| "`mtx.lock()`/`unlock()` is fine if I'm careful" | One missed `unlock` (return/throw) = deadlock. Always a guard |
| "An `fd` wrapper can be copyable" | Two owners → double-close. Move-only |
| "Move ctor just copies the handle" | Copy **and null the source**, or double-release |
| "Cleanup dtors can throw on error" | During unwinding → `std::terminate`. Log-and-swallow |

---

## Exercises

1. **Write `Fd`:** the full move-only `Fd` class (ctor from `int`, dtor `close`,
   move ctor + move assign, deleted copy, `get()`, `explicit operator bool`,
   `release()`). Test: `Fd a{5}; Fd b = std::move(a);` — `a.get()`? `b.get()`?

   <details><summary>Answer</summary>

   See the lesson's `Fd`. After `Fd b = std::move(a);` → `a.get() == -1` (nulled),
   `b.get() == 5`. `a`'s dtor is now a no-op; `b`'s closes fd 5.
   </details>

2. **lock_guard equivalence:** rewrite `void f() { m.lock(); work(); m.unlock();
   }` with `std::lock_guard`. Then add an early `return` and a `throw` — does the
   guard version stay correct?

   <details><summary>Answer</summary>

   `void f() { std::lock_guard<std::mutex> g{m}; work(); }`. Early `return` →
   `g`'s dtor unlocks. `throw` → stack unwinding runs `g`'s dtor → unlock. The
   manual version leaks the lock on both.
   </details>

3. **IosFlagsGuard:** implement it. Use it around `std::cout << std::hex << x;`
   and verify that after the scope, `std::cout << 255` prints `255` (decimal),
   not `ff`.

   <details><summary>Answer</summary>

   See the lesson. `{ IosFlagsGuard g{std::cout}; std::cout << std::hex << 255;
   } std::cout << " " << 255;` → prints `ff 255` — the guard restored decimal.
   </details>

4. **mmap wrapper:** sketch a move-only `MmapRegion` (ctor `mmap`s, dtor
   `munmap`s, holds `void* addr_` and `size_t len_`). What does the move ctor
   null out?

   <details><summary>Answer</summary>

   `MmapRegion(MmapRegion&& o) : addr_(o.addr_), len_(o.len_) { o.addr_ =
   MAP_FAILED; o.len_ = 0; }` — null `addr_` (to `MAP_FAILED`/`nullptr`) so the
   moved-from dtor's `if (addr_ != MAP_FAILED) munmap(addr_, len_)` is a no-op.
   </details>

5. **ScopeGuard:** write a minimal `template <class F> class ScopeGuard { F f_;
   bool active_ = true; public: ~ScopeGuard() { if (active_) f_(); } void
   dismiss() { active_ = false; } };` — when is `dismiss()` useful?

   <details><summary>Answer</summary>

   `dismiss()` cancels the cleanup — useful for "rollback unless we commit":
   acquire a resource, set up a `ScopeGuard` that releases it, do risky work, and
   on success call `guard.dismiss()` so the resource is kept. If anything throws
   before `dismiss()`, the guard rolls back.
   </details>

---

## Interview questions

1. `mtx.lock()`/`unlock()` manually kyun mana? `lock_guard` kya karta?
2. `FILE*` vs POSIX `fd` — RAII wrapper mein fark kyun (`unique_ptr` vs class)?
3. Move-only RAII wrapper ke 5 elements?
4. Move ctor mein source ko null kyun karna?
5. "Scoped state change" RAII (affinity, format flags) — pattern?
6. `ScopeGuard` / `finally` — kab, `dismiss()` kyun?

---

## Next
→ [`10-rule-of-zero.md`](10-rule-of-zero.md)
