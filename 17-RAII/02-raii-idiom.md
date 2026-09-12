# 02 — The RAII idiom

## Prerequisites
- [`01-what-is-a-resource.md`](01-what-is-a-resource.md)
- Folder 15 files 04–06 (ctors, init lists, dtors), folder 16 file 07

## Yeh topic abhi kyun
Ab hum RAII ko exactly define karte hain aur ek wrapper likhte hain. Yeh pattern
itna central hai ki iske bina modern C++ likhna hi nahi chahiye — `std::string`,
`std::vector`, `std::unique_ptr`, `std::lock_guard`, `std::fstream` — sab RAII
hain.

---

## RAII = Resource Acquisition Is Initialization

Naam thoda confusing hai. Better mental model:

> **Ek object ka constructor resource acquire karta hai.
> Uska destructor resource release karta hai.
> Resource ka lifetime = object ka lifetime.**

```cpp
class FileHandle {
    std::FILE* f_ = nullptr;
public:
    explicit FileHandle(const char* path, const char* mode)
        : f_(std::fopen(path, mode)) {                 // ACQUIRE in ctor
        if (!f_) throw std::runtime_error("open failed");
    }

    ~FileHandle() {                                     // RELEASE in dtor
        if (f_) std::fclose(f_);
    }

    std::FILE* get() const { return f_; }              // controlled access

    // non-copyable (ek FILE* ka ek owner) -- folder 18 mein move add karenge
    FileHandle(const FileHandle&)            = delete;
    FileHandle& operator=(const FileHandle&) = delete;
};
```

Use:
```cpp
void readConfig() {
    FileHandle fh{"config.txt", "r"};       // file opened
    parse(fh.get());
    // ... koi bhi return / throw ...
}                                            // fh.~FileHandle() -> fclose, GUARANTEED
```

Caller ko `fclose` yaad rakhna **nahi** — woh `FileHandle` ke dtor ka kaam hai,
aur woh scope end pe automatically chalta.

---

## RAII wrapper ke 4 elements

```cpp
class Guard {
    Handle h_;                              // 1. RESOURCE HANDLE as a member
public:
    Guard(args) : h_(acquire(args)) { }     // 2. ACQUIRE in constructor
    ~Guard() { if (valid(h_)) release(h_); }  // 3. RELEASE in destructor (idempotent-safe check)
    Guard(const Guard&) = delete;           // 4. OWNERSHIP policy (copy/move -- folder 18)
    Guard& operator=(const Guard&) = delete;
};
```

1. **Handle as member** — the fd / pointer / lock.
2. **Ctor acquires** — aur agar acquire fail ho to `throw` (object banega hi
   nahi, dtor nahi chalega — folder 15 file 04).
3. **Dtor releases** — `if` check taaki null/moved-from state pe double-release
   na ho.
4. **Ownership policy** — do owners same resource ko delete na karein
   (double-free). Default: `= delete` copy. Better: move-only (unique ownership)
   — folder 18.

---

## Scope-based lifetime = automatic

```cpp
{
    LockGuard g{mtx};        // lock acquired
    if (x) return;           // g.~LockGuard() -> unlock
    doStuff();
    throw ...;               // g.~LockGuard() -> unlock  (stack unwinding)
}                            // g.~LockGuard() -> unlock  (normal)
```

Ek local RAII object → uska destructor **har** control-flow exit pe chalta:
- Normal fall-through past the `}`.
- `return` (kisi bhi jagah se).
- `break` / `continue` (loop se bahar).
- **Exception** (stack unwinding — file 03).
- Even `std::exit`? **Nahi** — `std::exit` locals ke dtors nahi chalata (`return
  from main` chalata). `std::abort` bhi nahi. (Edge cases — file 03.)

---

## Nested resources — automatic reverse cleanup

```cpp
void transaction() {
    DbConnection conn{"localhost"};          // acquire 1
    Transaction  txn{conn};                  // acquire 2
    FileHandle   log{"txn.log", "a"};        // acquire 3

    doWork(conn, txn, log);

    txn.commit();
}   // dtors: ~log -> ~txn (rollback if not committed) -> ~conn   -- REVERSE order
```

Kai resources → destructors **reverse acquisition order** mein chalte (folder 15
file 06). Isliye baad wala resource pehle wale par safely depend kar sakta
(`Transaction` needs `DbConnection` alive) — cleanup mein `Transaction` pehle
jaata.

**Koi manual cleanup ladder nahi** (`if fail3: cleanup2; cleanup1;` — C style).
Compiler karta hai.

---

## The standard library is RAII

| Type | Resource | Acquire | Release (dtor) |
|---|---|---|---|
| `std::string` | heap buffer (if > SSO) | ctor / grow | `~string` → `delete[]` |
| `std::vector<T>` | heap array | ctor / grow | `~vector` → destroy elems + `delete[]` |
| `std::unique_ptr<T>` | one `T` | `make_unique` | `~unique_ptr` → `delete` |
| `std::shared_ptr<T>` | shared `T` + ctrl block | `make_shared` | refcount 0 → `delete` |
| `std::lock_guard<M>` | a lock | ctor → `m.lock()` | `~lock_guard` → `m.unlock()` |
| `std::fstream` | a file | ctor / `open()` | `~fstream` → `close()` |
| `std::jthread` (C++20) | a thread | ctor | `~jthread` → request stop + `join()` |

Aap `new`/`delete`, `fopen`/`fclose`, `lock`/`unlock` **manually likhte hi
nahi** — yeh types karte hain. Aapka kaam: apne domain resources ke liye aise
hi wrappers likhna (file 09).

---

## Andar kya hota hai

- Compiler har scope ke end par (aur har early-exit / throw path par) local
  objects ke destructors ki calls **inject** karta hai — construction ke ulte
  order mein. Yeh `.eh_frame` / unwind tables mein bhi record hota (exception
  path ke liye — file 03).
- Trivial RAII wrapper (`LockGuard` = ek pointer member + `lock`/`unlock` calls)
  `-O2` pe **fully inline** — generated code bilkul waisa jaise aap manually
  `m.lock()` ... `m.unlock()` likhte, bas har exit path pe automatically.
- Ctor throw → jo members already construct hue unke dtors chalte, phir
  exception propagate; **`~Guard()` khud nahi chalta** (object incomplete) —
  isliye "acquire in ctor, and if it fails, throw" clean hai (partial state
  nahi bacha).
- Non-trivial dtors (loop over elements, syscall) — real cost, par woh cost aap
  kisi bhi tareeke se pay karte; RAII bas usse reliable + one-place banata.

> **HFT relevance:** RAII zero-cost abstraction ka poster child hai — ek
> `ScopedAffinity` (sets CPU affinity in ctor, restores in dtor), ek
> `TimestampScope` (rdtsc in ctor, records delta in dtor), ek `PoolHandle`
> (takes a slot in ctor, returns it in dtor) — sab `-O2` pe inline hoke woh
> exact code ban jaate jo aap manually likhte, minus the bugs. Hot path pe
> RAII use hota hai jahan koi resource-jaisa lifetime ho (scratch buffer from a
> pool, a scoped measurement); jahan nahi (pure arithmetic), RAII ka sawaal hi
> nahi.

---

## Hands-on

```bash
./build.ps1 17-RAII/examples/01_raii_basics.cpp
./build.ps1 17-RAII/examples/05_custom_deleter.cpp    # FILE* wrapped in unique_ptr
```

Khud likho: ek `TimerScope` jo ctor mein `steady_clock::now()` save kare aur
dtor mein elapsed ms print kare. `{ TimerScope t; doWork(); }` — kaam ke aage
peeche timing automatic.

---

## ⚠️ Traps

### Trap 1 — acquire in ctor body instead of init list, forgetting the `if` guard in dtor
```cpp
~Guard() { release(h_); }   // ⚠️ agar ctor ne throw kiya ya moved-from -> h_ invalid -> double/invalid release
~Guard() { if (h_) release(h_); }   // ✅
```

### Trap 2 — copyable RAII wrapper (double release)
```cpp
class F { FILE* f_; public: ~F(){ fclose(f_); } };   // ⚠️ F b = a; -> dono fclose same f_ -> double-close
```

### Trap 3 — RAII object ko `new` karna
```cpp
auto* g = new LockGuard{mtx};   // ⚠️ ab dtor tabhi jab tum `delete g` -> RAII ka point khatam. Local banao
```

### Trap 4 — resource ko wrapper ke bahar leak karna
```cpp
FILE* leak() { FileHandle fh{"x","r"}; return fh.get(); }   // ⚠️ fh dtor -> fclose -> returned FILE* dangling
```

### Trap 5 — dtor se throw
```cpp
~Guard() { if (fsync(fd_) < 0) throw ...; }   // ⚠️ unwinding ke dauraan -> std::terminate (file 03)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "RAII bas smart pointers hai" | Any resource — files, locks, threads, handles |
| "RAII ka runtime overhead hai" | Trivial wrappers inline → zero; you'd pay the release cost anyway |
| "Ctor mein acquire fail ho to dtor cleanup karega" | Dtor nahi chalta (object incomplete); ctor khud throw + partial cleanup |
| "RAII object `new` se banao" | Local banao — scope hi cleanup trigger hai |
| "Ek wrapper copyable hona chahiye" | Default non-copyable (double release); move-only (folder 18) |

---

## Exercises

1. **Write a wrapper:** `class SocketGuard` — ctor takes an `int fd` (already
   opened), dtor does `::close(fd)`. Non-copyable. Add a `get()` and an
   `explicit operator bool()`.

   <details><summary>Answer</summary>

   `class SocketGuard { int fd_ = -1; public: explicit SocketGuard(int fd) :
   fd_(fd) {} ~SocketGuard() { if (fd_ >= 0) ::close(fd_); } int get() const {
   return fd_; } explicit operator bool() const { return fd_ >= 0; }
   SocketGuard(const SocketGuard&) = delete; SocketGuard& operator=(const
   SocketGuard&) = delete; };`
   </details>

2. **Reverse order:** `{ A a; B b; C c; }` where each prints in ctor/dtor —
   output? Which resource is released first, and why is that the safe order?

   <details><summary>Answer</summary>

   Ctor: A B C. Dtor: C B A (reverse). Safe because `c` may hold a reference to
   `b` / `a`; releasing `c` first means `b`/`a` are still valid during `~C()`.
   </details>

3. **Ctor throws:** `class G { Res r1_; Res r2_; public: G() : r1_(), r2_() {
   throw 1; } ~G() { puts("~G"); } };  try { G g; } catch(...) {}` — which
   destructors run?

   <details><summary>Answer</summary>

   `~r2_` then `~r1_` (constructed members, reverse). `~G()` does **not** run —
   `G` never completed construction.
   </details>

4. **Spot the leak:** `FILE* dangerous() { FileHandle fh{"a.txt","r"}; return
   fh.get(); }` — what's wrong? Fix (2 ways).

   <details><summary>Answer</summary>

   `fh` is destroyed at function return → `fclose` → the returned `FILE*`
   dangles. Fix A: return the `FileHandle` itself (make it movable — folder 18).
   Fix B: don't leak the raw handle; do the file work inside and return the
   result.
   </details>

5. **jthread:** why does `std::thread`'s destructor call `std::terminate` if the
   thread is still joinable, while `std::jthread`'s destructor joins? Which is
   more "RAII-correct"?

   <details><summary>Answer</summary>

   `std::thread` (C++11) chose `terminate` to force an explicit decision
   (join/detach) — but that means forgetting → crash, not RAII-clean.
   `std::jthread` (C++20) destructor requests stop + joins → truly RAII (cleanup
   on every path). Prefer `jthread`.
   </details>

---

## Interview questions

1. RAII ko ek line mein define karo. Naam kyun misleading?
2. RAII wrapper ke 4 elements?
3. Ek local RAII object ka dtor kaunse exit paths pe chalta (aur kaunse nahi)?
4. Multiple resources → cleanup order? Kyun reverse safe hai?
5. Ctor mein acquire fail ho → kya karna, dtor ka kya?
6. STL ke 5 RAII types + unka resource?

---

## Next
→ [`03-why-raii-works.md`](03-why-raii-works.md)
