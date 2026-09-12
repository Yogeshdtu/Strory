# 03 — `std::thread`

## Prerequisites
- `02-process-vs-thread.md`, `17-RAII`
- [`examples/01_first_thread.cpp`](examples/01_first_thread.cpp)

## Yeh topic abhi kyun
`std::thread` C++ ka basic thread type hai. Uska API chhota hai par usme ek sharp
edge hai: **join ya detach zaroori**, warna destructor `std::terminate` bulata hai.
Yeh samajhna (aur file 14 ka `std::jthread` jo ise fix karta) real concurrent code
likhne ki foundation hai.

---

## Basics

```cpp
#include <thread>

void work(int id, const std::string& tag);

std::thread t(work, 1, "hello");   // CONSTRUCT = START. Thread ab chal raha.
// ... main aur t concurrently chal rahe ...
t.join();                           // main yahan BLOCK — t ke khatam hone tak
```

- Constructor: callable + arguments. Thread turant start hota (koi `.start()`
  nahi).
- **`join()`** — caller waits for the thread to finish. Ek baar.
- **`detach()`** — thread ko "chhod do", background mein chalta rahe. `std::thread`
  object usse control kho deta.
- **`joinable()`** — `true` jab tak na `join()` na `detach()` hua (aur default-
  constructed na ho).

---

## The rule: join or detach before the `std::thread` dies

```cpp
{
    std::thread t(work);
    // ⚠️ agar yahan koi join()/detach() nahi
}   // ~std::thread -> joinable() true -> std::terminate()  💥
```

**Kyun `terminate` aur silently-detach nahi?** Silent detach = ek thread jo shayad
destroyed locals ko use kar raha ho → UB. Committee ne kaha: agar aap decide nahi
karte, program crash karo (loud, not silent-UB).

**Isliye:**
- Har code path pe `join()` ya `detach()` — exceptions ke saath (RAII wrapper) ya
  `std::jthread` (file 14, auto-joins).
- `join()` do baar / `join()` after `detach()` → **`std::system_error`** thrown.

### A poor-man's RAII wrapper

```cpp
class JoiningThread {
    std::thread t_;
public:
    template <class... A> explicit JoiningThread(A&&... a) : t_(std::forward<A>(a)...) {}
    ~JoiningThread() { if (t_.joinable()) t_.join(); }
    JoiningThread(JoiningThread&&) = default;
    JoiningThread& operator=(JoiningThread&&) = default;
    JoiningThread(const JoiningThread&) = delete;
    std::thread& get() { return t_; }
};
```

C++20's `std::jthread` **is** this (plus a stop token) — use it.

---

## `std::thread` is move-only

```cpp
std::thread a(work);
std::thread b = std::move(a);    // ✅ ownership moved; a is now not joinable
std::thread c = a;               // ❌ copy deleted
```

- Move-only (owns an OS resource — folder 18).
- Store in containers by moving: `std::vector<std::thread> pool; pool.emplace_back(work, i);`
- Moving into an already-joinable `std::thread` (via assignment) → `std::terminate`
  (you'd be dropping a running thread). Join/detach the target first.

---

## Identity & introspection

```cpp
std::thread t(work);
std::thread::id id = t.get_id();               // thread's id (non-joinable -> id{})
std::this_thread::get_id();                     // current thread's id
std::thread::hardware_concurrency();            // logical cores (hint; 0 = unknown)

std::this_thread::sleep_for(std::chrono::milliseconds(10));
std::this_thread::sleep_until(deadline);
std::this_thread::yield();                       // "I have nothing to do; reschedule me"
```

`std::thread::id` is comparable, hashable (`std::hash<std::thread::id>`), and
streamable — use it as a map key for per-thread data (or prefer `thread_local`).

---

## Native handle (platform escape hatch)

```cpp
pthread_t h = t.native_handle();               // for pthread_setaffinity_np, sched_setscheduler, ...
```

Standard C++ has **no** API for affinity, priority, real-time scheduling, or
naming a thread — you drop to `native_handle()` + platform calls (folder 41 for
HFT pinning).

---

## Andar kya hota hai

- `std::thread`'s constructor: **decay-copies** each argument into internal
  storage (so a `const std::string&` parameter still gets a copy passed unless you
  `std::ref` it — file 04), then calls `pthread_create` with a trampoline that
  invokes your callable with those stored arguments.
- The `std::thread` object holds the `pthread_t` (or a Windows `HANDLE`). Its
  destructor: `if (joinable()) std::terminate();`.
- `join()` → `pthread_join` (blocks, reaps the thread's resources). `detach()` →
  `pthread_detach` (resources auto-reclaimed on exit; you lose the handle).
- Exceptions escaping the thread function → `std::terminate` (an uncaught
  exception on any thread, including the top of a `std::thread`). Use
  `std::async`/`packaged_task` (file 12) or `std::exception_ptr` to move errors
  across threads.

---

## > **HFT relevance**
> - **Long-lived, pinned threads created once at startup** — never per-event. A
>   fixed set: hot loop, timer, logger, telemetry, admin. Each pinned to a core
>   via `native_handle()` + `pthread_setaffinity_np`, hot ones on isolated cores
>   with `SCHED_FIFO` (folder 41).
> - **`std::jthread`** (file 14) for the RAII join + `stop_token` shutdown — no
>   `terminate` land-mines, clean cooperative stop on exit.
> - **No exceptions on worker threads** — an uncaught exception is an instant
>   `terminate`. Hot threads are `noexcept` end-to-end (folder 23); errors are
>   values pushed to a queue.
> - **Name your threads** (`pthread_setname_np`) — shows up in `perf` / `top -H` /
>   `gdb`, invaluable for diagnosing which thread stalled.
> - `hardware_concurrency()` for sizing pools, but the hot threads' core
>   assignments are explicit, not "whatever the scheduler picks."

---

## Hands-on

```bash
./build.ps1 26-CONCURRENCY/examples/01_first_thread.cpp
```

Single thread + join, lambda thread, a `vector<std::thread>` of workers,
`std::ref` for by-reference args, `detach` + `joinable()`. Then:
- Remove a `join()` → run → watch `terminate called without an active exception`.
- Throw inside a thread function (no try/catch) → `terminate`.
- `std::thread b = std::move(a); a.join();` → `std::system_error` (a not joinable).

---

## ⚠️ Traps

### Trap 1 — forgetting join/detach
```cpp
void f() { std::thread t(work); }   // ⚠️ ~thread -> std::terminate
```
`std::jthread`, or a RAII wrapper, or an explicit `join()` on every path.

### Trap 2 — `join()` twice / after `detach()`
Throws `std::system_error`. Guard with `if (t.joinable())`.

### Trap 3 — exception escaping a thread function
Instant `std::terminate`. Catch at the top of the thread body, or use
`std::async` / `packaged_task` (the exception is stored in the future).

### Trap 4 — dangling reference captured by the thread
```cpp
void spawn() {
    int local = 5;
    std::thread t([&local]{ use(local); });   // ⚠️ if t outlives spawn(), local is gone
    t.detach();
}
```
Capture by value, or ensure the thread joins before the referent dies.

### Trap 5 — moving over a joinable thread
```cpp
std::thread a(work), b(work2);
a = std::move(b);   // ⚠️ a was joinable -> std::terminate (dropping a running thread)
```
`a.join(); a = std::move(b);`.

### Trap 6 — assuming `get_id()` on a finished thread is meaningful
After `join()`/`detach()`, `t.get_id() == std::thread::id{}`. Capture the id
before joining if you need it.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "the thread starts when I call `.start()`" | It starts in the constructor — no `.start()` |
| "if I forget to join, the thread just detaches" | `~std::thread` with `joinable() == true` calls `std::terminate` |
| "arguments are passed by reference" | Decay-copied into the thread; use `std::ref` for a reference |
| "an exception in a thread propagates to `main`" | Uncaught on any thread → `std::terminate`; use `async`/`packaged_task` |
| "`std::thread` is copyable" | Move-only (owns an OS thread) |
| "C++ has an API for pinning / priority" | No — `native_handle()` + platform calls |

---

## Exercises

1. **Predict:**
   ```cpp
   { std::thread t([]{ std::puts("hi"); }); }
   std::puts("after");
   ```

   <details><summary>Answer</summary>

   Undefined ordering of `"hi"` vs the crash, but the program **terminates** —
   `~std::thread` runs with `joinable() == true` → `std::terminate` (prints
   `terminate called without an active exception` and aborts). Add `t.join();`.
   </details>

2. **Fix:** `std::vector<std::thread> v; for (int i=0;i<4;++i) v.push_back(worker,
   i);` — doesn't compile; also, what's missing after the loop?

   <details><summary>Answer</summary>

   `push_back` doesn't take constructor args — use `v.emplace_back(worker, i);`.
   After the loop: `for (auto& t : v) t.join();` (or use `std::jthread` and the
   vector's destructor joins them).
   </details>

3. **By-reference arg:** `long n = 0; std::thread t(add, n, 100); t.join();` where
   `void add(long& x, long k)`. Why doesn't `n` change?

   <details><summary>Answer</summary>

   `std::thread` decay-copies `n` into internal storage and passes *that* by
   reference to `add` — the caller's `n` is untouched. Use `std::thread t(add,
   std::ref(n), 100);`.
   </details>

4. **Exception:** `std::thread t([]{ throw std::runtime_error("x"); }); t.join();`
   — what happens, and how do you get the error to `main`?

   <details><summary>Answer</summary>

   The uncaught exception on the thread → `std::terminate` (the `t.join()` never
   sees it). To propagate: `auto f = std::async(std::launch::async, []{ throw
   std::runtime_error("x"); }); try { f.get(); } catch (...) { ... }` — the future
   re-throws on `get()`.
   </details>

5. **RAII wrapper:** write the minimal `~JoiningThread()` and explain why the move
   assignment must join the existing thread first.

   <details><summary>Answer</summary>

   `~JoiningThread() { if (t_.joinable()) t_.join(); }`. Move assignment must
   `if (t_.joinable()) t_.join();` before taking over the source's thread —
   otherwise you'd drop a running `std::thread` (its destructor would
   `std::terminate`). `std::jthread` handles all of this.
   </details>

---

## Interview questions

1. `std::thread` kab start hota? Join vs detach.
2. Join/detach bhoolne pe kya, aur kyun `terminate` (silent detach nahi)?
3. `std::thread` copyable hai? Kyun / kyun nahi?
4. Arguments kaise pass hote (decay-copy), `std::ref` kab?
5. Thread function se exception nikle to kya, kaise handle?
6. `native_handle()` kis liye — standard C++ mein kya missing hai?
7. `std::jthread` `std::thread` ke kaunse do problems fix karta?

---

## Next
→ [`04-passing-data-to-threads.md`](04-passing-data-to-threads.md)
