# 08 — Lock guards: `lock_guard`, `unique_lock`, `scoped_lock`

## Prerequisites
- `07-mutex.md`, `17-RAII`
- [`examples/03_mutex_fix.cpp`](examples/03_mutex_fix.cpp), [`examples/04_deadlock.cpp`](examples/04_deadlock.cpp)

## Yeh topic abhi kyun
Raw `mutex.lock()` / `unlock()` = exception ya early return pe unlock miss →
permanent deadlock. **RAII for locks**: constructor locks, destructor unlocks — har
code path pe, exceptions ke saath bhi. Teen guards hain, har ek apni jagah ke liye.

---

## The three (four) guards

| Guard | Locks | Movable? | Unlock/relock/defer? | Use |
|---|---|---|---|---|
| **`std::lock_guard<M>`** | 1, in ctor | no | no | the default — simplest, cheapest |
| **`std::scoped_lock<M...>`** | 0..N, in ctor, deadlock-safe | no | no | 1 mutex (like `lock_guard`) or **2+** without deadlock |
| **`std::unique_lock<M>`** | 0 or 1, flexible | **yes** | **yes** (`lock`/`unlock`/`try_lock`, `defer_lock`, `adopt_lock`) | condition variables, deferred/conditional locking, transferring ownership |
| `std::shared_lock<M>` | shared (read) lock | yes | yes | with `std::shared_mutex` (file 10) |

**Default choice: `std::lock_guard` (or `std::scoped_lock`).** Reach for
`std::unique_lock` only when you need its flexibility (mainly: condition
variables).

---

## `std::lock_guard` — the workhorse

```cpp
std::mutex m;

void update() {
    std::lock_guard<std::mutex> lk(m);   // lock now
    shared_ += 1;
    // ... any early return / exception here still unlocks via ~lock_guard
}                                          // unlock here
```

- Class template argument deduction (C++17): `std::lock_guard lk(m);` (no `<...>`).
- Zero overhead vs raw lock/unlock — it's just an RAII pair.
- `std::lock_guard lk(m, std::adopt_lock)` — "the mutex is already locked (e.g. by
  `std::lock`), just take responsibility for unlocking it."

## `std::scoped_lock` — `lock_guard` + multi-mutex

```cpp
std::mutex bm, rm;

void transfer() {
    std::scoped_lock lk(bm, rm);   // locks BOTH, deadlock-free (order-independent)
    book_.foo();
    risk_.bar();
}                                   // unlocks both (reverse order)
```

- With **one** mutex: identical to `lock_guard`.
- With **2+**: uses a deadlock-avoidance algorithm (try one, try the next, on
  failure release all and retry in a different order) — so you **don't** need a
  global lock order for these (file 09).
- Prefer `scoped_lock` over `lock_guard` as the default in new code (it's a strict
  superset).

## `std::unique_lock` — the flexible one

```cpp
std::unique_lock<std::mutex> lk(m);       // lock now (like lock_guard)
std::unique_lock<std::mutex> lk(m, std::defer_lock);   // don't lock yet
std::unique_lock<std::mutex> lk(m, std::try_to_lock);  // try_lock now; check lk.owns_lock()
std::unique_lock<std::mutex> lk(m, std::adopt_lock);   // already locked; adopt

lk.lock();          // (re)acquire
lk.unlock();        // release early (while keeping the guard object)
lk.try_lock();
bool have = lk.owns_lock();
std::mutex* raw = lk.release();           // give up ownership without unlocking

// movable:
std::unique_lock<std::mutex> make_locked() { std::unique_lock lk(m); return lk; }  // ✅ moves out
```

**Costs a bit more** than `lock_guard` (a bool flag + branches in the dtor). Use
it where you need it:
- **Condition variables** — `cv.wait(lk, pred)` needs to unlock/relock `lk`
  (file 11). This is the main reason `unique_lock` exists.
- **Unlock partway through** a scope to do non-critical work, then relock.
- **Conditional / deferred** locking.
- **Transfer** lock ownership out of a function.

---

## The classic bug this prevents

```cpp
// ❌ raw
void withdraw(long amt) {
    m.lock();
    if (balance_ < amt) return;   // ⚠️ unlock skipped -> mutex held forever
    balance_ -= amt;
    m.unlock();
}
// ✅ RAII
void withdraw(long amt) {
    std::lock_guard lk(m);
    if (balance_ < amt) return;   // ~lock_guard unlocks on the way out
    balance_ -= amt;
}
```

Any exception (`std::bad_alloc` from a `push_back`, a `throw` in a callback) also
unlocks correctly with the guard.

---

## `std::lock` — lock N mutexes deadlock-free (pre-`scoped_lock` / manual)

```cpp
std::lock(a, b);                                  // acquires both, no deadlock
std::lock_guard<std::mutex> la(a, std::adopt_lock);
std::lock_guard<std::mutex> lb(b, std::adopt_lock);
```

`std::scoped_lock(a, b)` is the C++17 one-liner for this. `examples/04` shows both
`scoped_lock` and consistent-order approaches.

---

## Andar kya hota hai

- `std::lock_guard` — holds `M&`, ctor `m.lock()`, dtor `m.unlock()`. No state, no
  branches — compiles to exactly the raw calls at the right points.
- `std::unique_lock` — holds `M*` + a `bool owns_` flag. Dtor: `if (owns_)
  m->unlock()`. The flag enables `unlock()`/`lock()`/move/`release()`.
- `std::scoped_lock<M...>` — variadic; for 1 mutex it's a `lock_guard`; for N it
  calls `std::lock(...)` (the try-and-back-off algorithm) then adopts all.
- CTAD lets you write `std::lock_guard lk(m)` / `std::scoped_lock lk(a, b)` without
  spelling the mutex types.

---

## > **HFT relevance**
> - **`std::scoped_lock` / `std::lock_guard` everywhere a lock is used** on the
>   control plane — never raw `lock()/unlock()`. The zero-overhead RAII pair means
>   there's no reason not to.
> - **`std::unique_lock` only with condition variables** (file 11) — its extra
>   `bool` + dtor branch is negligible there and unavoidable.
> - **Multi-mutex operations → `std::scoped_lock`** so you don't have to maintain a
>   global lock order by hand (though on a hot path you shouldn't be taking two
>   locks at all — folder 28).
> - The hot path takes **no** locks; these matter for setup, config, admin, and
>   the logger/telemetry side.

---

## Hands-on

```bash
./build.ps1 26-CONCURRENCY/examples/03_mutex_fix.cpp   # lock_guard in version (a)
./build.ps1 26-CONCURRENCY/examples/04_deadlock.cpp    # scoped_lock as fix B
```

- Rewrite `examples/03` (a) with raw `m.lock()/m.unlock()` and add an early
  `return` in the loop body under a condition → watch it hang.
- Use `std::unique_lock` + `lk.unlock()` to do a `printf` outside the critical
  section, then `lk.lock()` again.
- `std::scoped_lock(a, b)` vs manually `a.lock(); b.lock();` in two threads with
  opposite order — the scoped_lock version never deadlocks.

---

## ⚠️ Traps

### Trap 1 — raw lock/unlock
Any early return / exception → unlock skipped → permanent deadlock. RAII guard.

### Trap 2 — `std::lock_guard lk(m);` as a statement with no name
```cpp
std::lock_guard<std::mutex>(m);   // ⚠️ a TEMPORARY — locks and immediately unlocks!
std::lock_guard<std::mutex> lk(m); // ✅ named — held for the scope
```
A missing variable name = a temporary destroyed at the `;` = no protection.

### Trap 3 — `unique_lock` where `lock_guard` suffices
The extra flag/branch is tiny but pointless. Default to `lock_guard`/`scoped_lock`.

### Trap 4 — forgetting `std::adopt_lock` after `std::lock`
`std::lock(a, b); std::lock_guard la(a);` — `la` locks `a` **again** → deadlock.
Use `std::adopt_lock` (or just `std::scoped_lock(a, b)`).

### Trap 5 — moving a `unique_lock` that isn't the CV's lock
`cv.wait(lk)` needs `lk` to still own the mutex. Don't `std::move` it away before
the wait.

### Trap 6 — holding a guard across a long operation
The guard keeps the lock for its whole scope. Scope it tightly — `{ std::lock_guard
lk(m); shared_ = x; }` then do the slow stuff outside (file 06).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "always use `unique_lock`, it's more capable" | Default to `lock_guard`/`scoped_lock`; `unique_lock` only when you need the flexibility (CVs) |
| "`std::lock_guard<std::mutex>(m);` locks for the scope" | That's a temporary — unlocks immediately. Name it |
| "`scoped_lock` and `lock_guard` are the same" | `scoped_lock` also handles 2+ mutexes deadlock-free |
| "RAII locking has overhead" | `lock_guard` is exactly the raw calls; zero overhead |
| "you can transfer a `lock_guard` out of a function" | Not movable — use `unique_lock` for transfer |
| "`std::lock(a,b)` then `lock_guard(a)` is fine" | Double-locks `a`; use `std::adopt_lock` or `scoped_lock` |

---

## Exercises

1. **Bug:** `std::mutex m; void f(){ std::lock_guard<std::mutex>(m); shared_ = 1; }`
   — why doesn't it protect `shared_`?

   <details><summary>Answer</summary>

   `std::lock_guard<std::mutex>(m)` (no variable name) constructs a **temporary**
   that locks `m` and then is immediately destroyed at the `;`, unlocking it. The
   assignment `shared_ = 1` runs with the mutex **unlocked**. Name it: `std::lock_guard
   lk(m);`.
   </details>

2. **Choose:** which guard for (a) a simple `++counter`, (b) `cv.wait` in a
   consumer, (c) locking `book_m_` and `risk_m_` together, (d) unlocking mid-scope
   to do I/O then relocking.

   <details><summary>Answer</summary>

   (a) `lock_guard` / `scoped_lock`. (b) `unique_lock` (CV needs to unlock/relock
   it). (c) `scoped_lock(book_m_, risk_m_)`. (d) `unique_lock` (`lk.unlock()` /
   `lk.lock()`).
   </details>

3. **Adopt:** rewrite `std::scoped_lock lk(a, b);` using `std::lock` + guards
   (pre-C++17 style).

   <details><summary>Answer</summary>

   ```cpp
   std::lock(a, b);
   std::lock_guard<std::mutex> la(a, std::adopt_lock);
   std::lock_guard<std::mutex> lb(b, std::adopt_lock);
   ```
   `std::lock` acquires both deadlock-free; the guards adopt (take over unlocking).
   </details>

4. **Transfer:** write a function that locks `m_` and returns the lock so the
   caller can do more work under it.

   <details><summary>Answer</summary>

   ```cpp
   std::unique_lock<std::mutex> lock_state() {
       return std::unique_lock<std::mutex>(m_);   // moved out (NRVO / move)
   }
   // caller: auto lk = lock_state(); shared_.foo(); // still locked
   ```
   `lock_guard` can't do this — it's not movable.
   </details>

5. **Tight scope:** `void record(const Event& e){ std::lock_guard lk(m_); auto s =
   serialize(e); disk_.write(s); log_.push_back(e); }` — restructure so only the
   shared write is locked.

   <details><summary>Answer</summary>

   ```cpp
   void record(const Event& e) {
       auto s = serialize(e);          // outside
       disk_.write(s);                 // outside (I/O)
       { std::lock_guard lk(m_); log_.push_back(e); }   // only the shared bit
   }
   ```
   (If `disk_` is also shared, it needs its own lock / its own thread.)
   </details>

---

## Interview questions

1. `lock_guard` vs `unique_lock` vs `scoped_lock` — kab kaunsa?
2. Raw `lock`/`unlock` ke bajaye RAII guard kyun (exception safety)?
3. `std::lock_guard<std::mutex>(m);` — kya galat (temporary)?
4. `unique_lock` kis feature ke liye CVs ko chahiye?
5. `std::scoped_lock(a, b)` deadlock se kaise bachata?
6. `std::adopt_lock` kab use hota (`std::lock` ke baad)?
7. Guard ki cost — `lock_guard` zero-overhead kyun?

---

## Next
→ [`09-deadlock.md`](09-deadlock.md)
