# 04 — Passing data to threads

## Prerequisites
- `03-std-thread.md`, `18-COPY-MOVE` (move, `std::ref`), `25-OBJECT-MODEL` file 05 (dangling)
- [`examples/01_first_thread.cpp`](examples/01_first_thread.cpp)

## Yeh topic abhi kyun
`std::thread t(f, arg1, arg2)` — yeh `arg1`, `arg2` ko thread tak kaise pahunchta
hai? Answer mein do traps hain jo naye concurrent code mein bahut common hain:
**arguments decay-copy hote hain** (reference nahi milta bina `std::ref`), aur
**captured references dangle kar sakte hain** agar thread apne creator se zyada
jeeye.

---

## Rule: arguments are decay-copied

`std::thread`'s constructor har argument ki ek **copy** internal storage mein
banata hai (decay-copy: array→pointer, function→pointer, top-level cv/ref stripped),
phir thread function ko woh copies pass karta hai.

```cpp
void f(std::string s);
std::string name = "engine";
std::thread t(f, name);        // `name` COPY hoti hai storage mein, phir f ko move
t.join();                       // `name` yahan bhi valid — copy thread ke paas thi
```

Consequence: **`const T&` parameter ke liye bhi ek copy jaati hai** unless you
opt out with `std::ref`:

```cpp
void g(const std::string& s);   // reference param
std::thread t(g, name);          // ⚠️ `name` copy -> `g` binds to the COPY, not `name`
```

For `void g(const std::string&)` that's harmless (you just have a copy). But if
`g` takes a **non-const reference** and is supposed to mutate the caller's object:

```cpp
void add(long& counter, long k);
long n = 0;
std::thread t1(add, n, 100);            // ❌ compile error (can't bind long& to a copy)
std::thread t2(add, std::ref(n), 100);  // ✅ std::ref -> a reference_wrapper -> real `n`
t2.join();
```

`std::ref(x)` / `std::cref(x)` wrap `x` in a `std::reference_wrapper`, which the
thread stores and unwraps to `T&` / `const T&` when calling.

**⚠️ With `std::ref`, `x` must outlive the thread** (you're passing a real
reference now — dangling risk is back).

---

## Move-only arguments

```cpp
void consume(std::unique_ptr<Job> j);
auto j = std::make_unique<Job>(...);
std::thread t(consume, std::move(j));   // ✅ moved into thread storage, then into `consume`
t.join();
```

`std::move` the argument; the thread stores it by move and moves it into the
function. `j` is null afterward.

---

## Capturing in a lambda — the same rules, explicit

```cpp
int local = 42;
std::thread by_value([local]{ use(local); });    // ✅ copy — safe even if creator returns
std::thread by_ref  ([&local]{ use(local); });   // ⚠️ reference — `local` must outlive the thread
std::thread by_move ([p = std::move(ptr)]() mutable { use(*p); });  // ✅ move-capture
```

**Prefer capturing by value** (or by move) for anything the thread outlives its
creator. Capture by reference **only** when you `join()` before the referent dies.

### The classic dangling bug

```cpp
void spawn_worker() {
    Config cfg = load();
    std::thread t([&cfg]{ run(cfg); });   // ⚠️ captures &cfg
    t.detach();                            // ⚠️ detached — outlives spawn_worker
}                                          // cfg destroyed here; the thread now uses freed memory
```

Fix: capture `cfg` by value/move, or `join()` inside `spawn_worker`, or make `cfg`
outlive the thread (a member, a shared_ptr).

---

## Passing `this` / member functions

```cpp
struct Engine {
    void run();
    void start() {
        worker_ = std::thread(&Engine::run, this);   // member fn: pass &Engine::run + `this`
    }
    std::thread worker_;
};
```

- `std::thread(&Class::method, objptr, args...)` — first "argument" is the object
  (pointer or reference or `shared_ptr`).
- **⚠️ `this` is a raw pointer** — if the `Engine` is destroyed while `worker_` runs
  → use-after-free. Join in `~Engine()` **and** be careful about destruction order
  (join before members the thread uses are destroyed). Or capture a
  `shared_ptr` / `weak_ptr` and `std::enable_shared_from_this`.

---

## Reference / pointer to a local returned by a function

```cpp
std::string make_name();
std::thread t(g, std::ref(make_name()));   // ⚠️ temporary dies at the `;` — dangling ref
std::string name = make_name();
std::thread t2(g, std::ref(name));          // ✅ (and `name` must outlive `t2`)
```

`std::ref` of a temporary is a bug (folder 25 file 05).

---

## Andar kya hota hai

- `std::thread`'s ctor does `std::__invoke(decay-copy(f), decay-copy(args)...)`
  inside the new thread. The decay-copies live in a heap-allocated shared state
  that the thread owns; they're destroyed when the thread function returns.
- `std::reference_wrapper<T>` is basically `T*` with an `operator T&()`; `INVOKE`
  unwraps it, so the callee gets a real `T&` to the *original* object.
- A lambda's captures are members of the closure object; `[x]` copies, `[&x]`
  stores a pointer, `[x = std::move(...)]` move-constructs. The closure is itself
  decay-copied into the thread's shared state.

---

## > **HFT relevance**
> - **Long-lived threads take a pointer to a long-lived owner** (the engine, an
>   arena) set up at startup — no per-call argument passing, no lifetime puzzles.
>   The owner outlives every thread by construction (`join()` in its destructor,
>   in the right order).
> - **Data flows over lock-free queues, not thread arguments** — the hot thread is
>   started once; work arrives via an SPSC ring buffer it polls (folder 28). You
>   don't "pass data to a thread" per event.
> - **Capture by value / move for anything short-lived**; capture by reference
>   only for the startup-owned, outlives-everything state.
> - **`std::ref` sparingly and only for genuinely-shared, long-lived objects** —
>   it re-introduces the dangling risk you get a copy to avoid.

---

## Hands-on

```bash
./build.ps1 26-CONCURRENCY/examples/01_first_thread.cpp
```

Section 4 shows `std::ref`. Then:
- `void add(long&, long)` — call `std::thread(add, n, 100)` (compile error) vs
  `std::thread(add, std::ref(n), 100)` (works).
- Capture a local by `[&]`, `detach()`, and (Linux) run under ASan — see the
  use-after-scope.
- `std::thread(&Struct::method, &obj)` — then destroy `obj` before joining → UAF.

---

## ⚠️ Traps

### Trap 1 — expecting pass-by-reference without `std::ref`
```cpp
std::thread t(mutate, x);            // ⚠️ mutates a copy (or won't compile for `T&`)
std::thread t(mutate, std::ref(x));  // ✅
```

### Trap 2 — `std::ref` to a temporary / soon-to-die object
```cpp
std::thread t(g, std::ref(build()));   // ⚠️ dangling
```

### Trap 3 — capturing a local by reference in a detached thread
The local dies when the creator returns; the detached thread uses freed memory.
Capture by value/move, or join.

### Trap 4 — `this` captured, object destroyed while the thread runs
Join in the destructor, mind destruction order, or use `shared_from_this` /
`weak_ptr`.

### Trap 5 — passing a `std::unique_ptr` by copy
Doesn't compile. `std::move(ptr)` (thread stores it by move).

### Trap 6 — big object passed by value
`std::thread(f, huge_vector)` copies `huge_vector` into thread storage. Pass by
`std::move`, or `std::cref` (if it outlives the thread), or a `shared_ptr`.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "thread args are passed by reference" | Decay-copied; `std::ref` for a real reference |
| "`const T&` param means no copy" | A copy is still made and bound to the param |
| "`std::ref` is always the fix" | Only for long-lived shared objects — it re-adds the dangling risk |
| "capturing `[&]` is fine, it's fast" | Fine only if you `join()` before the referents die |
| "`this` in a member-fn thread is safe" | It's a raw pointer — UAF if the object dies first |
| "move-only args can't be passed" | `std::move(arg)` — the thread stores it by move |

---

## Exercises

1. **Compile or not:** `void inc(int& x); int n = 0; std::thread t(inc, n);` — and
   the fix.

   <details><summary>Answer</summary>

   Doesn't compile — the decay-copied `int` can't bind to `int&`. Fix:
   `std::thread t(inc, std::ref(n));` (and `n` must outlive `t`).
   </details>

2. **Dangle:** which are safe if the thread is `detach()`ed?
   ```cpp
   int a = 1; std::string b = "x"; auto c = std::make_unique<int>(3);
   std::thread t1([a]{ f(a); });
   std::thread t2([&b]{ f(b); });
   std::thread t3([c = std::move(c)]{ f(*c); });
   ```

   <details><summary>Answer</summary>

   `t1` safe (copy of `a`). `t2` **unsafe** — `b` dies when the enclosing scope
   returns; the detached thread dangles. `t3` safe (owns the `unique_ptr` via
   move).
   </details>

3. **Member thread:** `struct S { std::thread w; void go(){ w = std::thread(&S::loop,
   this); } void loop(); ~S(){ ??? } };` — fill in `~S()` and name the ordering
   hazard.

   <details><summary>Answer</summary>

   `~S() { if (w.joinable()) w.join(); }` — but this runs **after** `S`'s other
   members are... no, members are destroyed *after* the body. The hazard: if
   `loop()` uses another member of `S`, and you don't join in the body / early,
   that member could be destroyed while `loop()` still runs. Safest: an explicit
   `stop()` that signals and joins, called before destruction, or `std::jthread` +
   a `stop_token`, joined first in member order.
   </details>

4. **Big arg:** `std::vector<Tick> ticks` (100 MB). `std::thread t(process, ticks)`
   — what happens, three better options.

   <details><summary>Answer</summary>

   `ticks` is copied (100 MB) into the thread's storage. Better: `std::move(ticks)`
   (thread owns it), `std::cref(ticks)` (if `ticks` outlives `t`), or pass a
   `std::shared_ptr<std::vector<Tick>>` (shared ownership, cheap to pass).
   </details>

5. **`std::ref` temporary:** why is `std::thread t(log, std::ref(std::string("msg")))`
   a bug?

   <details><summary>Answer</summary>

   `std::string("msg")` is a temporary destroyed at the end of the `std::thread`
   constructor's full-expression. `std::ref` stores a reference to it; the thread
   then uses a dangling reference. Pass by value (`std::thread t(log,
   std::string("msg"))`), or keep the string in a named variable that outlives
   the thread.
   </details>

---

## Interview questions

1. `std::thread(f, arg)` — `arg` kaise pass hota (decay-copy)?
2. `const T&` parameter ke liye bhi copy kyun jaati?
3. `std::ref` / `std::cref` — kya karte, kab use, kya risk?
4. Move-only argument (`unique_ptr`) kaise pass karein?
5. Lambda capture `[&]` in a detached thread — kya khatra?
6. Member function ko thread mein chalana — `this` ka lifetime issue?
7. `std::ref` of a temporary — kyun bug?

---

## Next
→ [`05-race-conditions.md`](05-race-conditions.md)
