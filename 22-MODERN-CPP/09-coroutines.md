# 09 — Coroutines deep

## Prerequisites
- [`08-ranges-deep.md`](08-ranges-deep.md), folder 08 (call stack), folder 14 (heap)
- [`examples/04_coroutines_generator.cpp`](examples/04_coroutines_generator.cpp)

## Yeh topic abhi kyun
Coroutine = ek function jo **beech mein ruk** sakti hai aur baad mein wahi se
**resume** ho sakti — `co_await`, `co_yield`, `co_return`. Isse generators, async
I/O, aur state machines readable code ki tarah likhe jaate. Par C++20 sirf
**machinery** deta — koi ready `std::generator` nahi (woh C++23). Aur ek real
cost hai: **frame allocation**. HFT mein yeh **offline/async structure** ke liye,
tick path ke liye nahi.

---

## What makes a function a coroutine

If a function body contains `co_await`, `co_yield`, or `co_return`, the compiler
turns it into a **state machine**:

```cpp
Generator<int> nums() {
    for (int i = 0; i < 3; ++i)
        co_yield i;          // hand `i` to the caller, SUSPEND here; resume continues the loop
}
```

The compiler needs a **`promise_type`** (you define it, usually nested in the
return type) that customizes:

| hook | when it runs |
|---|---|
| `get_return_object()` | at the start — produces what the caller gets (a `Generator`) |
| `initial_suspend()` | right after the frame is set up — `suspend_always` = lazy, `suspend_never` = eager |
| `yield_value(v)` | on `co_yield v` — store `v`, return an awaitable (`suspend_always` to pause) |
| `await_transform(x)` / `x`'s `await_ready/suspend/resume` | on `co_await x` |
| `return_value(v)` / `return_void()` | on `co_return` |
| `final_suspend()` | when the body finishes — `suspend_always` so the caller can read the last state before the frame is destroyed |
| `unhandled_exception()` | if the body throws |

The caller drives it through a **`std::coroutine_handle<promise_type>`**:
`h.resume()`, `h.done()`, `h.promise()`, `h.destroy()`.

[`examples/04`](examples/04_coroutines_generator.cpp) implements a minimal
pull-based `Generator<T>` with exactly these pieces.

---

## The coroutine frame

When a coroutine is first called, the compiler allocates a **frame** holding:
- the parameters (copied in)
- the local variables that live across a suspension point
- the promise object
- bookkeeping (which suspend point to resume at, the resume/destroy function
  pointers)

The frame goes on the **heap** via `operator new` — **unless** the compiler can
prove its lifetime is bounded and inline it (HALO: "heap allocation elision
optimization"), which happens when the coroutine is created and fully consumed in
the same scope with no escape. Don't rely on it.

`h.destroy()` (or the RAII wrapper's destructor) frees the frame.

---

## Two shapes

### Generators (pull) — `co_yield`
The consumer asks for the next value; the coroutine runs until the next
`co_yield`, then suspends. `examples/04`: `fib(n)`, `split_lines(text)`. Lazy —
`fib(1'000'000)` only computes as many values as you pull.

### Async tasks (push/await) — `co_await`
`co_await someAwaitable` suspends until the awaitable is ready (an I/O completion,
a timer, another task), then resumes — usually on a different thread or via an
event loop. This is the "async without callback hell" use: `co_await
socket.read()` reads like blocking code but doesn't block the thread.

C++20 gives no library awaitables or executors — you use a library (cppcoro,
libcoro, Asio's coroutine support, folly, seastar) or write your own.

---

## C++23: `std::generator`

C++23 finally ships the standard generator:

```cpp
#include <generator>
std::generator<int> fib() {
    int a = 0, b = 1;
    while (true) { co_yield a; std::tie(a, b) = std::pair{b, a + b}; }
}
for (int x : fib() | std::views::take(10)) std::print("{} ", x);   // composes with ranges
```

It models `input_range`, so range-`for` and range adaptors just work. The frame
cost is unchanged — it's the missing *type*, not a performance fix.

---

## Andar kya hota hai

- The compiler splits the body at each suspend point into resumable chunks and
  generates a `resume()` that `switch`es on a stored state index to jump to the
  right chunk. Locals that straddle a suspend point are promoted into the frame;
  ones that don't stay on the real stack.
- `co_yield v` → `co_await promise.yield_value(v)`. `yield_value` stashes `v` in
  the promise and returns `suspend_always{}` → `resume()` returns to the caller.
  The caller reads `h.promise().current_` then calls `resume()` again to continue.
- The frame `operator new`: a coroutine call has a **hidden allocation** and a
  hidden `delete` on destroy — µs-scale worst case (the general allocator, folder
  14), and it's per coroutine *instance*, not per resume. HALO removes it only in
  the simple "create + consume locally" case.
- Each `resume()` is a function call + the state `switch` + restoring frame
  locals — cheap (ns), but not free, and it's an optimization barrier around the
  suspend point.
- `co_await` on a custom awaitable: `await_ready()` (skip suspension if already
  done), `await_suspend(handle)` (register the resume, return control), `await_
  resume()` (produce the awaited value).

> **HFT relevance:** coroutines are for **structure, off the tick path**. Good
> fits: a **backtest / replay event stream** as a `generator` (lazy pull of the
> next market event, composes with ranges), an **async gateway / admin I/O**
> layer where `co_await socket.read()` replaces a callback state machine, a
> **parser** expressed as a resumable generator over a byte stream in tooling.
> **Not** the market-data hot loop: the frame allocation (even amortized) and the
> suspend/resume + state-switch per element are real overhead versus a plain
> contiguous scan, and the suspend points block vectorization. If you do use a
> generator near a fast path, pool the frames (a custom `promise_type::operator
> new` drawing from an arena) and verify HALO or the pool actually removes the
> per-instance allocation.

---

## Hands-on

```bash
./build.ps1 22-MODERN-CPP/examples/04_coroutines_generator.cpp
```

`examples/04`: a hand-rolled `Generator<T>`, `fib(n)`, lazy pull (`fib(1e6)`,
take 5), a line splitter. Extend it: a `range_gen(lo, hi, step)`; a generator
that filters as it yields; measure the per-element cost of pulling from a
generator vs a plain `for` loop over a vector (expect the generator to be
several× slower).

---

## ⚠️ Traps

### Trap 1 — a coroutine is not a "free" abstraction
```cpp
// Every coroutine instance has a heap frame (unless HALO) + suspend/resume bookkeeping.
// Fine for structure; wrong for the innermost hot loop.
```

### Trap 2 — dangling reference in a coroutine parameter
```cpp
Generator<int> g(const std::vector<int>& v) { for (int x : v) co_yield x; }
auto gen = g(makeVec());   // ⚠️ makeVec()'s temporary is destroyed before you pull -> UB. Pass by value into the coroutine
```

### Trap 3 — reading state after the coroutine is done
```cpp
// final_suspend() must be `suspend_always` so the frame survives until you've read the last value / handled the exception,
// and YOU must h.destroy() it. `suspend_never` final_suspend -> the frame self-destructs -> use-after-free.
```

### Trap 4 — forgetting `initial_suspend` semantics
```cpp
// suspend_always initial_suspend -> lazy (nothing runs until first resume). suspend_never -> the body runs up to the first
// suspend immediately on call. Pick deliberately.
```

### Trap 5 — expecting C++20 to have `std::generator`
```cpp
#include <generator>   // ❌ C++23. In C++20 hand-roll it (examples/04) or use cppcoro / libcoro
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Coroutines are a zero-cost abstraction" | Each instance has a (usually heap) frame + suspend/resume bookkeeping |
| "C++20 has `std::generator`" | C++23 — C++20 gives only the language machinery |
| "A generator loop is as fast as a `for` over a vector" | Several× slower — frame + resume + state switch per element, no vectorization |
| "The frame is always on the heap" | Usually; HALO can elide it when create + consume are local and non-escaping |
| "`co_await` blocks the thread" | It suspends the *coroutine*; the thread is free to run other work (with an executor) |

---

## Exercises

1. **Which hooks:** for a pull generator, what must `initial_suspend`,
   `final_suspend`, and `yield_value` return, and why?

   <details><summary>Answer</summary>

   `initial_suspend` → `suspend_always` (lazy — don't run until the first
   `next()`). `yield_value` → `suspend_always` (pause after producing each value
   so the consumer can read it). `final_suspend` → `suspend_always` (keep the
   frame alive after the body ends so the consumer sees "done" and can read the
   last value; the consumer/RAII wrapper then `destroy()`s it).
   </details>

2. **Frame cost:** you pull 1,000,000 values from a `Generator<int>`. How many
   `operator new` calls for the frame, and how many resume+state-switch
   operations?

   <details><summary>Answer</summary>

   One `operator new` (the frame is allocated once at creation, freed once at
   destroy) — unless HALO elides it. But ~1,000,000 `resume()` calls, each a
   function call + a state `switch` + restoring the frame's live locals. That
   per-element overhead is why a generator loses to a plain vector scan.
   </details>

3. **Async shape:** sketch how `co_await socket.async_read(buf)` avoids callback
   hell.

   <details><summary>Answer</summary>

   `async_read` returns an awaitable whose `await_suspend(h)` registers `h` to be
   resumed when the read completes (by the event loop / io_uring / completion
   port) and returns control. The coroutine suspends; the thread runs other
   work. On completion the loop calls `h.resume()` and execution continues right
   after the `co_await` with the bytes available — linear code, no nested
   callbacks.
   </details>

4. **HALO:** when can the compiler elide the coroutine frame allocation?

   <details><summary>Answer</summary>

   When the coroutine handle doesn't escape the enclosing scope — it's created,
   fully iterated/awaited, and destroyed all within one function, so the compiler
   can prove the frame's lifetime is bounded and place it on the caller's stack.
   Storing the handle in a member, returning it, or passing it elsewhere defeats
   HALO.
   </details>

5. **Pooling:** you must use a generator on a warm path. How do you remove the
   per-instance heap allocation?

   <details><summary>Answer</summary>

   Give the `promise_type` a custom `static void* operator new(size_t n)` (and
   matching `operator delete`) that draws the frame from a preallocated arena /
   fixed-block pool sized for the frame. Then every coroutine of that type takes
   its frame from the pool — O(1), no `malloc`. Verify the frame size with
   `sizeof` / a probe.
   </details>

---

## Interview questions

1. Coroutine kya banati (`co_await`/`co_yield`/`co_return`) — compiler kya generate karta?
2. `promise_type` ke hooks — `initial_suspend` / `yield_value` / `final_suspend` kya control karte?
3. Coroutine frame — kahan allocate, HALO kab elide karta?
4. Generator loop vector scan se slow kyun?
5. C++20 vs C++23 — `std::generator`, kya missing tha?
6. HFT mein coroutine kahan (offline/async), tick path pe kyun nahi?

---

## Next
→ [`10-modules.md`](10-modules.md)
