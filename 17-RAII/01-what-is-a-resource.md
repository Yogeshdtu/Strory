# 01 — Resource kya hai

## Prerequisites
- Folder 14 (memory, leaks, UAF), folder 15 file 06 (destructors), folder 16 file 07 (virtual dtors)
- Folder 02 file 06 (scope — pehla RAII taste)

## Yeh topic abhi kyun
"RAII" ka **R** = Resource. Isse pehle samajhna zaroori ki **resource kya hota
hai** — memory sirf ek example hai. File handles, sockets, mutex locks, GPU
buffers, database connections, threads — sab **resources** hain jinke saath ek
common pattern hai: **acquire → use → release**. RAII us pattern ko automate
karta hai.

---

## Resource = kuch jo acquire aur release karna padta hai

```
   acquire  ──────►  use  ──────►  release
   (limited, must     (do work)     (MUST happen, exactly once,
    be paired)                       even on error)
```

| Resource | Acquire | Release | Leak = ? |
|---|---|---|---|
| Heap memory | `new` / `malloc` | `delete` / `free` | RSS badhta → OOM (folder 14) |
| File | `fopen` / `open` | `fclose` / `close` | fd table full → "too many open files" |
| Socket | `socket` + `connect` | `close` | fd leak + peer ko pata nahi connection gaya |
| Mutex lock | `lock()` | `unlock()` | **deadlock** — koi aur kabhi lock nahi le payega |
| Thread | `std::thread{...}` | `join()` / `detach()` | `std::terminate` (dtor pe joinable thread) |
| DB connection | `connect()` | `disconnect()` | connection pool exhaust |
| GPU buffer | `cudaMalloc` | `cudaFree` | VRAM leak |

**Common properties:**
1. **Limited** — OS/hardware ke paas fixed number (fds, locks, RAM).
2. **Paired** — har acquire ke liye exactly ek release.
3. **Release mandatory** — chhoot gaya to leak; do baar to UB / crash.
4. **Release must happen on every exit path** — normal return, early return,
   `break`, **exception**.

---

## Manual release kyun fail hota hai

```cpp
void processFile(const char* path) {
    FILE* f = std::fopen(path, "r");
    if (!f) return;                         // ok

    char buf[1024];
    if (std::fread(buf, 1, 1024, f) == 0) {
        return;                             // ⚠️ fclose(f) MISS -> fd leak
    }

    if (!validate(buf)) {
        throw std::runtime_error("bad");    // ⚠️ fclose(f) MISS -> fd leak
    }

    std::fclose(f);                         // sirf happy path pe
}
```

Har `return` / `throw` se pehle `fclose` yaad rakhna:
- Practically **impossible** in real code (10+ exit paths, nested conditions).
- Exceptions to aur bhi — code jo `throw` kar sakta hai woh source mein `throw`
  nahi likhta (`std::vector::push_back` throw kar sakta, `new` throw kar sakta).
- `goto cleanup;` (C style) helps but ugly, aur multiple resources ke saath
  nested cleanup labels.

---

## `try/finally` C++ mein nahi hai — aur kyun

Java/C#/Python mein:
```java
FileReader f = new FileReader(path);
try {
    // use f
} finally {
    f.close();     // guaranteed
}
```

C++ mein `finally` **jaan-boojh kar nahi** diya gaya. Uski jagah:
**destructor**. Ek local object ka destructor har exit path pe **guaranteed**
chalta hai (folder 15 file 06). To "cleanup code" ko ek object ke destructor
mein daalo → woh aapka `finally` ban jaata, aur woh **reusable** hai (ek baar
likho, har jagah use karo). Yehi RAII hai (file 02).

```cpp
void processFile(const char* path) {
    std::ifstream f{path};                  // RAII -- dtor closes the file
    // ... koi bhi return / throw ...
}                                            // f.~ifstream() -> file closed, har path pe
```

---

## Resource lifetime vs object lifetime

RAII ka core insight: **resource ka lifetime ko ek object ke lifetime se
baandh do.**

```
   object banaya  ────►  resource acquired  (constructor)
   object scope mein ─►  resource valid, use karo
   object destroyed ──►  resource released  (destructor)
```

Object kab destroy hota hai — C++ **deterministically** jaanta hai (folder 15
file 06): scope end, `delete`, container clear, stack unwinding. Koi garbage
collector nahi, koi "kabhi baad mein" nahi. Isliye resource release bhi
deterministic — **exactly wahan jahan object khatam hota hai**.

---

## Andar kya hota hai

- Ek resource "handle" aksar ek chhota integer (fd) ya pointer (FILE*, memory
  address) hota hai. RAII wrapper us handle ko ek member mein rakhta hai, ctor
  mein set karta, dtor mein release call karta.
- **Deterministic destruction** hi C++ ki RAII ko GC languages se alag banata:
  wahan `finalize()` / destructor "kabhi" chalta hai (GC ke marzi se) → resource
  leak until GC → isliye unhe explicit `try/finally` / `using` / `with` chahiye.
- Zero runtime overhead (mostly): wrapper ka dtor woh hi code emit karta jo aap
  manually likhte — bas compiler use har exit path pe automatically daalta,
  aur `-O2` pe trivial wrappers inline ho jaate.

> **HFT relevance:** deterministic resource management hi wajah hai ki HFT C++
> mein likha jaata, GC language mein nahi — koi unpredictable GC pause nahi,
> cleanup exactly scope end pe. Resources jo HFT mein matter karte: memory
> (pools/arenas — folder 14), file descriptors (sockets, epoll fds, timerfds),
> locks (jo hot path mein avoid hote, par jahan hain — RAII), CPU affinity
> masks, mmap'd regions, hardware timestamping handles. Har ek ko ek RAII
> wrapper milta (file 09) taaki error paths pe bhi leak / stuck-lock na ho.

---

## Hands-on

```bash
./build.ps1 17-RAII/examples/01_raii_basics.cpp
```

`TraceGuard` — ctor "acquire", dtor "release". Dekho: normal exit, early return,
nested scopes, loop — har case mein release automatic aur reverse order mein.

---

## ⚠️ Traps

### Trap 1 — "resource sirf memory hai"
Files, sockets, locks, threads — sab. Har ek ka leak alag disaster.

### Trap 2 — manual release har path pe yaad rakhna
```cpp
if (err) { fclose(f); return; }   // 5 exit paths -> 5 jagah fclose -> ek bhoola -> leak
```

### Trap 3 — exception path bhoolna
```cpp
FILE* f = fopen(...);  mayThrow();  fclose(f);   // ⚠️ throw -> fclose miss
```

### Trap 4 — `try/finally` dhoondhna
C++ mein nahi hai. Destructor hi finally hai.

### Trap 5 — double release
```cpp
fclose(f);  ...  fclose(f);   // ⚠️ double-close (UB, jaise double-free)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Resource = heap memory" | Memory is one kind; files/sockets/locks/threads/... |
| "Manual `fclose`/`delete` har path pe likh dunga" | Real code mein exit paths bahut — RAII automate karta |
| "C++ mein `finally` hona chahiye" | Destructor hi finally — reusable + guaranteed |
| "Lock leak bas ek warning hai" | Deadlock — poora system stuck |
| "GC language mein bhi RAII hota hai" | Wahan finalizer non-deterministic — explicit `using`/`with` chahiye |

---

## Exercises

1. **Classify:** in mein se kaunse resources hain (acquire/release paired)?
   `int x = 5;`, `std::fopen`, `std::string s`, `pthread_mutex_lock`,
   `std::vector<int> v(100)`, `new int[10]`, `SDL_CreateWindow`.

   <details><summary>Answer</summary>

   Resources: `std::fopen` (fclose), `pthread_mutex_lock` (unlock), `new int[10]`
   (delete[]), `SDL_CreateWindow` (SDL_DestroyWindow). `std::string`/`std::vector`
   own a resource internally but **manage it themselves** (RAII already). `int x`
   — not a resource.
   </details>

2. **Count exit paths:** ek function likho jo ek `FILE*` open kare aur 4
   alag-alag conditions pe early-return kare. Manual `fclose` kitni jagah likhna
   padega? Ek galti = ?

   <details><summary>Answer</summary>

   `fclose` 5 jagah (4 early returns + 1 normal end). Ek bhoola → fd leak har us
   path pe. RAII: 0 jagah `fclose` — wrapper ka dtor har path pe.
   </details>

3. **Lock leak:** `mtx.lock(); if (cond) return; doWork(); mtx.unlock();` — agar
   `cond` true ho to kya hota? Agar `doWork()` throw kare?

   <details><summary>Answer</summary>

   `cond` true → `return` → `unlock()` miss → mutex locked forever → next
   `lock()` (any thread) deadlocks. `doWork()` throws → same, `unlock()` miss.
   Fix: `std::lock_guard<std::mutex> g{mtx};` (RAII — file 09).
   </details>

4. **Deterministic vs GC:** Java mein `FileReader` ka `finalize()` kab chalta
   hai? C++ mein `std::ifstream` ka destructor kab? Fark ka practical impact?

   <details><summary>Answer</summary>

   Java: whenever GC runs (could be never before program exit) → file stays open
   → "too many open files" under load → hence `try-with-resources`. C++:
   deterministically at scope end → file closed immediately, predictable.
   </details>

5. **Pairing:** `cudaMalloc(&p, n)` / `cudaFree(p)`, `mmap(...)` / `munmap(...)`,
   `std::thread t{f}` / `t.join()` — teenon ke liye ek RAII wrapper class ka
   skeleton likho (ctor acquires, dtor releases).

   <details><summary>Answer</summary>

   `class GpuBuf { void* p_; public: GpuBuf(size_t n) { cudaMalloc(&p_, n); }
   ~GpuBuf() { cudaFree(p_); } /* delete copy */ };` — same shape for
   `mmap`/`munmap` and `std::thread` (dtor: `if (t_.joinable()) t_.join();`).
   </details>

---

## Interview questions

1. "Resource" ki definition — 4 common properties?
2. Memory ke alawa 4 resources, har ek ka leak = kya?
3. Manual release (`fclose`/`unlock`) kyun unreliable — 2 reasons?
4. C++ mein `finally` kyun nahi — kya replaces it?
5. RAII ka core idea — resource lifetime kis se baandha jaata?
6. GC language ka finalizer C++ destructor se kaise alag (determinism)?

---

## Next
→ [`02-raii-idiom.md`](02-raii-idiom.md)
