# 14 — `std::function` cost and alternatives

## Prerequisites
- **`19-STL`** (`std::function` cost, `08_std_function_cost.cpp`),
  **`22-MODERN-CPP`** (lambdas, closure size)
- `13-virtual-dispatch-elimination.md`
- `examples/08_std_function_cost.cpp`

## Yeh topic abhi kyun
`std::function` bahut convenient hai callbacks ke liye — aur hot path pe woh
teen costs laata: (1) a type-erased **indirect call** (no inline), (2) a
**heap allocation** in its constructor if the callable/capture is bigger
than its small-buffer (~16 B), (3) an extra indirection + likely cache miss
to reach a heap target. Yeh lesson: the alternatives, measured.

---

## The 4 ways to pass/store a callable

| | Cost | Alloc? | Inline? | Owns callable? |
|---|---|---|---|---|
| **template parameter** (`template<class F> void run(F f)`) | zero — resolved at compile time | no | **yes** (fully) | n/a (by value/ref) |
| **raw function pointer** (`R(*)(Args...)`) | one indirect call | no | no | no |
| **`function_ref`** (non-owning: `{void* ctx, R(*)(void*, Args...)}`) | one indirect call | **never** | no | **no** (caller keeps it alive) |
| **`std::function`** | one indirect call + type erasure | **yes if > SBO** | no | **yes** |

`function_ref` is C++26's `std::function_ref`; trivially hand-rolled today
(30 lines — see `08_std_function_cost.cpp`):
```cpp
template <class R, class... Args>
class function_ref<R(Args...)> {
    void* ctx_;
    R (*call_)(void*, Args...);
public:
    template <class F> function_ref(F&& f) noexcept
        : ctx_(std::addressof(f)),
          call_([](void* c, Args... a) -> R {
              return (*static_cast<std::remove_reference_t<F>*>(c))(std::forward<Args>(a)...); }) {}
    R operator()(Args... a) const { return call_(ctx_, std::forward<Args>(a)...); }
};
```
Two pointers, no allocation, handles captures (via `ctx_`). The caller must
keep the callable alive for the duration of the call — perfect for "pass a
callback down one or two levels".

---

## Measured (`08_std_function_cost.cpp`, is box — a carried-hash loop, 4M calls)

```
  templated (inlined)   : 1.50 ns/call
  raw fn pointer         : 1.50 ns/call
  function_ref           : 1.50 ns/call     (2 words, no alloc ever)
  std::function (small)   : 3.13 ns/call     [ctor heap bytes: 0]   (SBO)
  std::function (fat cap) : 3.13 ns/call     [ctor heap bytes: 64]  (SBO overflow -> heap)
```

- **templated / fn-ptr / function_ref are equal here** — because this loop
  is **latency-bound** on a carried `h` (a dependency chain), so the OoO
  engine overlaps the indirect-call overhead behind the chain. **On a
  throughput loop** (independent items) `templated` pulls ahead — it inlines,
  which lets the loop vectorize; the others can't inline. *What the loop is
  bound by decides whether inlining matters* (Rule 2).
- **`std::function` is ~2× (3.13 ns)** — its call path (type-erased indirect
  + the invoker) is heavy enough that even the dependency chain doesn't hide
  it.
- **Fat capture → 64 heap bytes in the constructor** — a construction-time
  spike (allocator tail — lesson 04) + an extra pointer chase + a cache miss
  per call to reach the heap-stored callable. The `[ctor heap bytes]` was
  measured with a global `operator new` counter.

---

## Which to use

| Situation | Use |
|---|---|
| a callback consumed **now**, in the same or a nested call | **`function_ref`** (or a template param) |
| a hot inner loop calling a user-supplied op | **template parameter** (inlines, vectorizes) |
| a C API callback (`void(*)(void*)`) | **raw fn pointer + a `void* user` context** |
| you must **store** the callable, lifetime outlives the call, set is heterogeneous | **`std::function`** — and keep the capture small (< SBO), or store a `unique_ptr` to a bigger state and capture that pointer |
| an event/observer list, hot | a small `std::vector<function_ref>` refreshed each dispatch, or `std::vector<{void*, fn}>` |

**Hot path default: template or `function_ref`.** `std::function` only when
you genuinely need owned storage of an unknown callable — and then watch the
capture size (measure with a `new` hook — lesson 05).

---

## Closure size — the SBO cliff

```cpp
auto a = []            { ... };          // sizeof 1  (empty)
auto b = [x]            { ... };          // sizeof sizeof(x)
auto c = [&]            { ... };          // sizeof (captured refs)  (usually small)
auto d = [arr8]         { ... };          // sizeof 64  -> std::function heap-allocates
std::function<void()> f = d;              // <-- ctor does `operator new(64)`
```
libstdc++/libc++ `std::function` SBO is ~16 bytes. Capture ≤ ~2 pointers →
no alloc. A captured array, a big struct by value, several captured
objects → over the cliff → heap. Capture a **pointer** to the state instead
(and own the state elsewhere — a pool/arena).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `std::function` in a hot loop
Indirect call + no inline + maybe a heap-target chase. Template it or
`function_ref` it.

### Trap 2 — a fat lambda into `std::function`
Silent `operator new` in the ctor (measure!). Capture a pointer to
pool/arena-owned state.

### Trap 3 — `function_ref` outliving its callable
`function_ref` holds a `void*` to the callable — if the callable (a local
lambda) is destroyed before the `function_ref` is called → dangling. Only
for synchronous "call it now" use.

### Trap 4 — `std::function` for a closed set
If it's Quote/Trade/Cancel handlers, that's a `variant`/tag-switch (lesson
13), not a `std::function` per type.

### Trap 5 — assuming templated always wins
On a latency-bound loop it ties fn-ptr/`function_ref` (OoO hides the call).
The win is on throughput loops (inline → vectorize). Know your bound.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::function` is just a function pointer" | + type erasure + maybe a heap alloc; ~2× a raw call |
| "small lambda → `std::function` is free" | SBO yes for tiny; a captured array/struct → heap in the ctor |
| "`function_ref` can be stored for later" | non-owning — only for synchronous use |
| "template callbacks are always fastest" | tie on latency-bound loops; win on throughput (inline→SIMD) |
| "flexible = use `std::function` everywhere" | hot path: template / `function_ref`; `std::function` only to own an unknown callable |

---

## Exercises

1. Ek event bus: `void subscribe(EventType t, std::function<void(const Event&)>
   cb);` aur hot path `for (auto& cb : subs_[t]) cb(ev);`. Subscribers ke
   lambdas `[this, cfg = big_config_struct]` capture karte hain. Do problems,
   redesign.

   <details><summary>Answer</summary>

   Problems:
   (1) **`std::function` per subscriber** — each `cb(ev)` is a type-erased
   indirect call, no inline. For a hot event bus firing millions of times/sec
   that's the dominant cost.
   (2) **`[this, cfg = big_config_struct]`** — `cfg` is a struct captured
   *by value* → if it's bigger than the SBO (~16 B), `subscribe` does a
   heap allocation when constructing the `std::function` (once per subscribe
   — cold path, tolerable) — **but** it also means each `std::function` now
   points at a heap block, so every `cb(ev)` call chases a pointer into
   possibly-cold memory → a cache miss per call on the hot path.
   Redesign:
   - Subscribers register `{void* ctx, void(*)(void*, const Event&)}` (a
     `function_ref`-style pair) where `ctx` points at the subscriber object
     (which owns `cfg`). Store `std::vector<Sub>` — no `std::function`, no
     heap, the callback pointer is a direct function that the compiler
     knows, and `ctx` is the subscriber's own (warm, since it's processing)
     memory.
   - Or, if the set of subscriber *types* is closed, dispatch via a
     `variant`/switch (lesson 13).
   - Capture `cfg` **by pointer/reference to subscriber-owned storage**, never
     by value into the callback wrapper.
   - Consider whether the bus even needs indirection: if there are 2-3 known
     consumers, call them directly.
   </details>

2. `sizeof(std::function<void()>)` typically 32 bytes, SBO ~16. Ek lambda
   `[a, b]` where `a`, `b` are `int64_t` — heap allocate hoga ya nahi? Aur
   `[a, b, c]`?

   <details><summary>Answer</summary>

   `[a, b]` with two `int64_t` = 16 bytes of capture. libstdc++/libc++ SBO
   is ~16 bytes (implementation-defined — libstdc++ stores up to a
   `sizeof(void*[2])` = 16 B *and* requires the callable to be nothrow-move-
   constructible to use SBO). Two `int64_t` fits exactly → **no heap
   allocation** (borderline — depends on the implementation's exact SBO
   size and alignment/trivially-copyable checks).
   `[a, b, c]` = 24 bytes → **exceeds SBO → `operator new` in the ctor.**
   The safe assumption for portable code: **capture ≤ 1-2 pointers' worth
   → probably SBO; anything more → assume heap.** Don't rely on the exact
   boundary — verify with a `new` counter (like `08_std_function_cost.cpp`)
   or avoid `std::function` on the hot path entirely. If you need to carry
   `a, b, c`, put them in a struct owned by a pool and capture `&that`.
   </details>

---

## Interview questions

1. `std::function`'s 3 hot-path costs (indirect call, maybe heap, target chase).
2. `function_ref` — what it is, its two words, its lifetime rule.
3. The SBO cliff — what capture sizes trigger a heap allocation.
4. When templated callbacks tie vs beat a fn-pointer (latency vs throughput bound).
5. An event bus with fat-capture `std::function` subscribers — the redesign.

---

## Next
→ [`15-ring-buffers.md`](15-ring-buffers.md)
