# 03 — Why RAII works (exception safety, guaranteed destruction)

## Prerequisites
- [`02-raii-idiom.md`](02-raii-idiom.md)
- Folder 15 file 06 (destructors), folder 23 preview (exceptions — bas `throw`/`catch` idea)

## Yeh topic abhi kyun
RAII "works" kyunki C++ **guarantee** karta hai ki ek fully-constructed
automatic object ka destructor chalega — **stack unwinding** ke dauraan bhi. Yeh
guarantee samajhna zaroori hai: exception safety, aur us guarantee ke exceptions
(pun intended) — jab dtor **nahi** chalta.

---

## Stack unwinding — exception ka cleanup mechanism

```cpp
void f() {
    Guard a{"a"};
    Guard b{"b"};
    g();                    // g() throws
    Guard c{"c"};           // kabhi execute nahi hua
}

void caller() {
    try {
        f();
    } catch (const std::exception& e) {
        // handle
    }
}
```

`g()` throws → runtime **stack unwinding** shuru karta:
1. `f()` ke current scope ke **fully-constructed** locals ke destructors, reverse
   order mein: `~b`, phir `~a`. (`c` construct hi nahi hua → skip.)
2. `f()` ka frame pop.
3. `caller()` mein pahunche — matching `catch` mila → us tak ke sab frames ke
   locals ke dtors chal chuke.
4. `catch` block execute.

**Har RAII object ka resource release ho gaya** — bina kisi `catch (...)  {
cleanup; }` likhe. Yehi exception safety hai: exception ke raste pe bhi resources
leak nahi hote.

---

## Without RAII — the exception hole

```cpp
void bad() {
    Res* a = new Res{1};
    Res* b = new Res{2};
    risky();                        // throws
    delete b;                       // ⚠️ MISS
    delete a;                       // ⚠️ MISS
}
```

`risky()` throws → unwinding sirf **objects** ke dtors chalata hai. `a` aur `b`
raw pointers hain — unka koi dtor nahi (pointer ka "dtor" no-op). `Res` objects
heap pe hain, unka koi automatic cleanup nahi. **Leak.**

`examples/06_exception_safety.cpp` yeh counted `operator new`/`delete` se measure
karta hai: raw version throw pe 2 blocks leak karta, RAII (`unique_ptr`) version
0.

### The manual "fix" is fragile

```cpp
void stillBad() {
    Res* a = new Res{1};
    try {
        Res* b = new Res{2};
        try {
            risky();
        } catch (...) { delete b; throw; }
        delete b;
    } catch (...) { delete a; throw; }
    delete a;
}
```

Nested `try`/`catch (...) { cleanup; throw; }` per resource. **Unmaintainable**
with 3+ resources, aur ek `throw;` bhoolo to exception nigal jaata. RAII isse
**structurally** khatam karta.

---

## Exception safety guarantees (folder 23 preview)

RAII se code ye levels achieve karta hai:

| Guarantee | Meaning | RAII role |
|---|---|---|
| **No-throw** (`noexcept`) | operation kabhi throw nahi karti | dtors, `swap`, move (ideally) |
| **Strong** | throw hua to state pehle jaisa (commit-or-rollback) | "copy then swap"; RAII holds the copy |
| **Basic** | throw hua to state valid (invariants hold), par changed | RAII ensures no leak, no half-freed |
| **None** | throw → leak / corruption / UB | raw `new`/`delete`, manual `unlock` |

RAII alone gives you **basic** for free (no leaks). Strong needs a bit more
design (transactions, copy-and-swap — folder 18).

---

## When the destructor does NOT run — RAII ke gaps

RAII guarantee sirf **fully-constructed automatic (stack) objects** ke liye hai.
Yeh cases mein dtor nahi chalta:

### 1. `std::abort()` / `std::terminate()`

```cpp
std::abort();          // process turant khatam -- KOI dtor nahi
```

`std::terminate` (jo `noexcept` violation, uncaught exception, dtor-during-unwind-throw
se call hota) bhi — no cleanup. OS process ke fds/memory reclaim karta, par
in-process cleanup (flush buffers, unlock cross-process locks, send goodbye
packet) skip.

### 2. Uncaught exception (implementation-defined)

```cpp
int main() {
    Guard g;
    throw std::runtime_error("boom");   // no catch anywhere
}
```

Ek uncaught exception → `std::terminate`. Standard **allow karta hai** ki
unwinding is case mein **na ho** (implementation-defined). GCC/Clang typically
**don't unwind** past `main` for an uncaught exception → `g`'s dtor may not run.
Isliye: top-level `catch (...)` in `main` rakho agar cleanup matter karta.

### 3. `std::exit()` / `quick_exit()`

```cpp
void f() { Guard g; std::exit(0); }   // ⚠️ g's dtor does NOT run
```

`std::exit` **automatic** objects ke dtors nahi chalata (sirf `static`/global +
`atexit` handlers). `return` from `main` chalata hai. `quick_exit` to `atexit`
bhi skip karta.

### 4. `longjmp` past C++ objects

```cpp
setjmp(buf); ... longjmp(buf, 1);   // ⚠️ C mechanism -- C++ dtors ke beech se jump -> UB / skip
```

### 5. Constructor threw (object never fully built)

```cpp
struct G { Res r_; G() { throw 1; } ~G() { cleanup(); } };
try { G g; } catch (...) {}   // ~G() NAHI -- but ~r_ (constructed member) DOES run
```

`~G()` nahi chalta (G incomplete), par jo **members** construct ho chuke unke
dtors chalte. Isiliye "acquire in ctor, throw if fail" — jo acquire ho chuka
(members) woh RAII-clean, jo nahi hua woh acquire hi nahi hua.

### 6. Object leaked (never destroyed)

```cpp
Guard* g = new Guard{};   // never deleted -> ~Guard() never runs
```

RAII guarantee **automatic** storage ke liye hai. Heap object ka dtor tabhi jab
`delete` (ya `unique_ptr` — RAII on the RAII object).

### 7. Destructor itself throws during unwinding

```cpp
~Guard() { if (fsync(fd_)) throw ...; }   // ⚠️ already-unwinding + naya throw -> std::terminate
```

Ek unhandled exception unwinding kar rahi hai, aur ek dtor **naya** exception
throw karta → `std::terminate`. Isliye **dtors ko `noexcept` rakho** (default
hain).

---

## Andar kya hota hai

- Compiler har function ke liye **unwind tables** (`.eh_frame` / `.gcc_except_table`)
  generate karta: "is PC range mein yeh locals live hain, unwind pe inke dtors
  call karo (yeh addresses)". Runtime library (`libgcc_s` / `libunwind`) throw pe
  in tables ko walk karta — frame by frame — dtors call karta jab tak matching
  handler na mile ("two-phase unwinding": phase 1 finds the handler, phase 2
  runs cleanups).
- **Zero cost when no exception** ("zero-cost exceptions" / table-based model):
  the happy path has no extra instructions — the cost is only paid *when* an
  exception is thrown (unwinding is slow — µs range — but rare).
- `noexcept` function → compiler assumes no unwind out of it → smaller tables,
  and a `noexcept` violation calls `std::terminate` (no unwinding).

> **HFT relevance:** the "zero-cost" model means RAII + exceptions add **nothing**
> to the hot path when nothing throws — this is why RAII is free to use
> everywhere. The catch: **actually throwing** is very slow (unwinding walks
> tables, µs+), so hot paths are designed to **never throw** (error codes /
> `std::expected` / pre-validation instead), and often built `-fno-exceptions`
> for a smaller binary + guaranteed no-unwind. But RAII still works with
> `-fno-exceptions` (dtors still run on scope exit / early return). Some HFT
> shops keep exceptions for startup/config and forbid them in the trading loop.
> `std::terminate`-on-uncaught means a top-level `catch` in `main` for graceful
> shutdown (flush logs, cancel resting orders).

---

## Hands-on

```bash
./build.ps1 17-RAII/examples/06_exception_safety.cpp
```

Raw `new`/`delete` vs `unique_ptr`, with and without a `throw` mid-function.
Counted allocations: raw+throw leaks 2, RAII+throw leaks 0. Try: add a
`std::exit(0)` in the RAII version → notice the dtors *don't* run (RAII gap #3).

---

## ⚠️ Traps

### Trap 1 — `std::exit` in RAII code
```cpp
void f() { LockGuard g{mtx}; std::exit(1); }   // ⚠️ g's dtor skipped -> mutex left locked
```

### Trap 2 — uncaught exception, expecting cleanup
```cpp
int main() { Guard g; throw 1; }   // ⚠️ g's dtor may NOT run (impl-defined). catch(...) in main
```

### Trap 3 — dtor throws during unwinding
```cpp
~Buffer() { flush(); }   // ⚠️ if flush() throws AND we're unwinding -> terminate. Make dtor noexcept-safe
```

### Trap 4 — `catch (...) { cleanup; throw; }` per resource
Fragile, unmaintainable — that's what RAII replaces.

### Trap 5 — thinking exceptions have runtime cost on the happy path
Zero-cost model: no cost unless thrown. But *throwing* is slow — avoid in hot paths.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Exception → resources definitely leak" | RAII objects' dtors run during unwinding |
| "RAII destructor ALWAYS runs" | Not on `abort`/`exit`/`longjmp`/uncaught-at-main/never-`delete`d |
| "`std::exit` runs local dtors" | Only static/global + `atexit`; `return from main` runs locals |
| "Exceptions cost on every call" | Zero-cost model — only when thrown (then slow) |
| "Dtor can throw safely" | During unwinding → `std::terminate`. Keep dtors non-throwing |

---

## Exercises

1. **Unwinding order:** `void f() { A a; B b; g(); C c; }` where `g()` throws.
   Which dtors run, in what order? Does `~C()` run?

   <details><summary>Answer</summary>

   `~b` then `~a` (reverse, constructed-only). `~C()` does NOT run — `c` was
   never constructed (`g()` threw first).
   </details>

2. **Leak or not:** for each, does the `FILE*` leak on `throw`?
   ```cpp
   (a) FILE* f = fopen(...); risky(); fclose(f);
   (b) FileHandle f{...}; risky();
   (c) auto f = std::unique_ptr<FILE, Closer>{fopen(...)}; risky();
   ```

   <details><summary>Answer</summary>

   (a) leaks — no dtor to close it. (b) safe — `FileHandle::~FileHandle` runs
   during unwind. (c) safe — `unique_ptr`'s dtor runs the `Closer`.
   </details>

3. **exit trap:** `void shutdown() { std::lock_guard g{gMutex}; std::exit(0); }`
   — what happens to `gMutex`? Does it matter (process ending anyway)?

   <details><summary>Answer</summary>

   `g`'s dtor is skipped → `gMutex` stays locked. In-process it doesn't matter
   (process dies), BUT if it's a cross-process / robust mutex, or if `atexit`
   handlers try to take it → deadlock at shutdown. Prefer `return` from `main`.
   </details>

4. **Strong guarantee:** you have `void update(Config& c)` that modifies `c` in
   several steps and might throw halfway. How does "copy, modify the copy, then
   `swap`" (RAII holds the copy) give the strong guarantee?

   <details><summary>Answer</summary>

   Make a local copy `Config tmp = c;` (RAII — auto-cleaned). Modify `tmp`. If
   any step throws, `tmp` is destroyed, `c` untouched → strong guarantee. If all
   steps succeed, `std::swap(c, tmp)` (noexcept) commits atomically.
   </details>

5. **Top-level catch:** why do robust programs write `int main() { try { real();
   } catch (const std::exception& e) { log(e.what()); return 1; } catch (...) {
   return 2; } }` instead of letting exceptions escape `main`?

   <details><summary>Answer</summary>

   An exception escaping `main` → `std::terminate` → the standard permits **no
   stack unwinding** → local dtors may not run → no graceful shutdown (unflushed
   logs, un-cancelled orders, unreleased cross-process locks). The top-level
   catch guarantees unwinding happened and lets you do controlled cleanup.
   </details>

---

## Interview questions

1. Stack unwinding kya hai, kab hota hai, kya karta hai?
2. RAII exception-safe kaise banata hai (vs raw new/delete + throw)?
3. Exception safety guarantees — 4 levels? RAII kaunsa deta free mein?
4. 5 cases jahan RAII destructor **nahi** chalta?
5. "Zero-cost exceptions" — happy path pe cost? Throw hone pe?
6. Dtor se throw kyun mana, kya hota unwinding ke dauraan?

---

## Next
→ [`04-unique-ptr.md`](04-unique-ptr.md)
