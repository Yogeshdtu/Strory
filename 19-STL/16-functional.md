# 16 — `<functional>`: `std::function`, its cost, `bind`, invocables

## Prerequisites
- [`15-utility-types.md`](15-utility-types.md)
- Folder 18 (move, captures), folder 22 preview (lambdas deep)
- [`08-iterators.md`](08-iterators.md) (template-parameter callables in algorithms)

## Yeh topic abhi kyun
Callbacks, event handlers, strategy objects — ye sab "ek callable ko store /
paas karna" hai. `std::function` iska general answer hai, par uski **cost** (type
erasure → indirect call, no inline, + possible heap allocation) HFT ke liye
maayne rakhti. Ye lesson batata hai kab `std::function` theek hai aur kab template
/ function-pointer / tag chahiye.

`examples/08_std_function_cost.cpp` ne ye sab **measure** kiya.

---

## The callable zoo

```cpp
int free_fn(int x)               { return x + 1; }        // function
struct Functor { int operator()(int x) const { return x + 1; } };   // function object
auto lambda      = [](int x)     { return x + 1; };       // closure (unique unnamed type)
auto capturing   = [k](int x)    { return x + k; };       // closure with state
auto generic     = [](auto x)    { return x + 1; };       // templated operator()
int (*fp)(int)   = &free_fn;                              // function pointer
```

All are **Callable** — `std::invoke(c, args...)` works on any of them (plus
pointer-to-member: `std::invoke(&T::m, obj)`). `std::invocable<F, Args...>` (a
concept) tests it.

The key distinction:
- **A lambda's type is unique and unnamable.** `decltype(lambda)`. Two identical
  lambdas are different types.
- To **store** a callable in a variable / member / container whose type is fixed,
  you need either a **template parameter** (compile-time, keeps the concrete
  type) or **type erasure** (`std::function`, runtime, forgets the type).

---

## `std::function<R(Args...)>` — type-erased callable wrapper

```cpp
#include <functional>

std::function<int(int)> f;             // empty -> calling it throws std::bad_function_call
f = &free_fn;
f = Functor{};
f = [](int x){ return x * 2; };
f = [big](int x){ return x + big.size(); };   // captures -> may allocate (see below)

if (f) int y = f(21);                  // contextual bool: is it non-empty?

std::vector<std::function<void()>> handlers;   // heterogeneous callables in one container -- the classic use
handlers.push_back([]{ log("a"); });
handlers.push_back([n]{ log(n); });
for (auto& h : handlers) h();
```

### Its cost — measured (`examples/08`, `-O2`, this box)

| callable form | ns/call | notes |
|---|---|---|
| templated callable (lambda) | **~1.48** | fully inlined — this is "the add" |
| templated callable (fn ptr) | ~1.48 | inlined |
| raw function pointer param | ~1.48 | indirect but predictable, inlined loop body |
| `std::function` (tiny capture) | **~5.11** | type-erased indirect call, **no inline**; fits small-buffer → no alloc |
| `std::function` (`std::string` capture) | **~6.00** | + **1 heap allocation** on construction |

Two costs:
1. **Indirect call, no inlining** — `std::function::operator()` calls through a
   type-erased pointer. The compiler can't see the target → can't inline it, and
   the call isn't the predictable direct call a function pointer gets. ~3–5 ns
   here vs ~0 for the templated path.
2. **Possible heap allocation** — `std::function` has a **small-buffer
   optimization** (libstdc++: ~16 bytes). A callable that fits (a function
   pointer, a lambda capturing ≤ 2 pointers) → stored inline, no alloc. A bigger
   capture (a `std::string`, several values, anything non-trivially-copyable that
   spills) → **`operator new`** on construction, `operator delete` on
   destruction. `examples/08` shows a `std::string`-capturing lambda forcing 1
   allocation.

## Keeping the concrete type — template parameter

```cpp
template <class F>
void for_each_tick(const std::vector<Tick>& ticks, F&& handler) {   // F is the exact closure type
    for (const auto& t : ticks) handler(t);                          // inlined -> zero call overhead
}
for_each_tick(ticks, [state](const Tick& t){ ... });
```

`std::sort`, `std::for_each`, `std::transform` all do this — the comparator /
predicate is a template parameter, so a no-capture lambda inlines to nothing.
That's why `std::sort` with a lambda matches a hand-written sort.

Downsides of the template approach: it must be in a header (or explicitly
instantiated), it's a new instantiation per callable type (code bloat), and it
can't be a non-template virtual / stored in a plain container.

## `std::bind` — mostly superseded by lambdas

```cpp
using namespace std::placeholders;
auto g = std::bind(free_fn, _1);                 // g(x) -> free_fn(x)
auto h = std::bind(sub, 100, _1);                // h(x) -> sub(100, x)

// prefer a lambda -- clearer, no placeholder magic, no bind's copy/ref quirks:
auto g2 = [](int x){ return free_fn(x); };
auto h2 = [](int x){ return sub(100, x); };
```

`std::bind_front(f, args...)` (C++20) is the one worth using — binds leading
args, forwards the rest, less surprising than `bind`:
```cpp
auto cb = std::bind_front(&Session::on_message, this);   // cb(msg) -> this->on_message(msg)
```

## `std::ref` / `std::cref` — pass by reference into things that copy

```cpp
int counter = 0;
std::function<void()> f = [&counter]{ ++counter; };       // captures by ref -- fine here
std::for_each(v.begin(), v.end(), std::ref(statefulFunctor));   // pass functor BY REFERENCE (algorithms take by value)
std::thread t(worker, std::ref(sharedData));              // thread copies args -> ref wrapper to avoid a copy
```

`std::reference_wrapper<T>` is a copyable, rebindable "reference as an object".

---

## Andar kya hota hai

- `std::function` holds: a small aligned buffer (SBO), a pointer to a **manager**
  function (copy/move/destroy the erased object), and a pointer to an **invoker**
  (cast the storage back to the concrete type and call it). `f(args)` → call the
  invoker through that pointer → the invoker calls your callable. One or two
  indirect calls, none inlinable across the boundary.
- Construction: if `sizeof(callable) <= SBO` **and** it's nothrow-move-
  constructible → placement-new into the buffer. Else → `operator new`, store the
  pointer. Destruction mirrors it.
- A capture-less lambda or a function pointer is 8 bytes and trivial → always
  SBO, no alloc — but still an indirect, non-inlined call.
- A template-parameter callable: the compiler stamps out the algorithm with the
  concrete closure type; `operator()` is visible → inlined; the closure's
  captured state often ends up in registers.

> **HFT relevance:** on the hot path, callables are **template parameters** or
> plain **function pointers** / small tag structs — never `std::function` —
> because you want the call inlined and you cannot afford a hidden `operator new`
> when a handler is (re)assigned. `std::function` is fine in the **control
> plane**: rarely-invoked callbacks, config-time strategy selection, an event-
> handler registry built at startup. If you must store heterogeneous handlers
> hot, use a fixed-size `std::variant` of concrete handler types + `std::visit`,
> or an intrusive vtable you control, or `function_ref` (a non-owning view of a
> callable — C++26 / `tl::function_ref`) which is 16 bytes and never allocates.
> `std::bind` is avoided (opaque, can add copies) — use lambdas / `bind_front`.

---

## Hands-on

```bash
./build.ps1 fast 19-STL/examples/08_std_function_cost.cpp
```

Compare the ns/call column: templated ~1.48, `std::function` small ~5.11,
`std::function` with a `std::string` capture ~6.00 **and 1 heap allocation** (the
counted `operator new` prints it). Try shrinking the capture until the allocation
disappears (SBO boundary ~16 bytes).

---

## ⚠️ Traps

### Trap 1 — `std::function` on the hot path
```cpp
std::function<double(const Tick&)> strat = pickStrategy();
for (auto& t : millionTicks) acc += strat(t);   // ⚠️ ~4 ns/call indirect + no inline. Template it, or store a concrete type
```

### Trap 2 — hidden allocation on reassignment
```cpp
callback = [state, buf, name](Event e){ ... };   // ⚠️ capture > SBO -> operator new every time you reassign
```

### Trap 3 — calling an empty `std::function`
```cpp
std::function<void()> f;  f();   // ⚠️ throws std::bad_function_call. if (f) f();
```

### Trap 4 — dangling capture outliving the referent
```cpp
std::function<int()> make() { int local = 5; return [&local]{ return local; }; }   // ⚠️ local dead -> UB. capture by value
```

### Trap 5 — passing a stateful functor by value to an algorithm
```cpp
Counter c;
std::for_each(v.begin(), v.end(), c);   // ⚠️ algorithm COPIES c; your c is unchanged. std::ref(c), or use the return value
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::function` is free / just a pointer" | Type-erased: indirect non-inlined call (~3–5 ns) + maybe a heap allocation |
| "`std::function` never allocates" | Only if the callable fits the small buffer (~16 B) — bigger captures → `operator new` |
| "A lambda *is* a `std::function`" | A lambda is a unique compiler-generated class; `std::function` is one thing that can *hold* it |
| "`std::bind` is the way to do partial application" | Lambdas / `std::bind_front` are clearer and avoid `bind`'s copy/nesting quirks |
| "Algorithms take the predicate by reference" | By value — use `std::ref` for a stateful functor you need to observe |

---

## Exercises

1. **Eliminate the allocation:** `std::function<void()> f = [a, b, c, name]{...};`
   where `name` is a `std::string` — it heap-allocates. Two ways to avoid it.

   <details><summary>Answer</summary>

   (a) Capture less / capture a pointer or `int` id instead of the `std::string`,
   so the closure fits the SBO. (b) Don't use `std::function` — make the calling
   code a template on the callable type, or store the closure in a concrete
   `auto` variable and call it directly.
   </details>

2. **Why can `std::sort`'s lambda inline but `std::function`'s can't?**

   <details><summary>Answer</summary>

   `std::sort`'s comparator is a **template parameter** — the concrete closure
   type is known when `sort` is instantiated, so `operator()` is visible and
   inlinable. `std::function` **erases** the type behind an indirect pointer; the
   compiler can't see the target at the call site → no inlining.
   </details>

3. **function_ref shape:** sketch a non-owning `function_ref<R(Args...)>` — what
   two members, why no allocation, what's the lifetime rule?

   <details><summary>Answer</summary>

   `void* obj;` + `R(*call)(void*, Args...);`. Construction stores the address of
   the callable and a thunk that casts `obj` back and calls it. No ownership → no
   allocation, 16 bytes. Rule: the referenced callable must outlive the
   `function_ref` (like `string_view`/`span`).
   </details>

4. **bind vs lambda:** rewrite `std::bind(&Logger::write, &logger, "prefix",
   std::placeholders::_1)` as a lambda.

   <details><summary>Answer</summary>

   `[&logger](const std::string& msg){ logger.write("prefix", msg); }` — or
   `std::bind_front(&Logger::write, &logger, "prefix")`.
   </details>

5. **Storage choice:** you have exactly 4 concrete strategy types and need to
   pick one at runtime and call it millions of times. `std::function`, template,
   or something else?

   <details><summary>Answer</summary>

   `std::variant<StratA, StratB, StratC, StratD>` selected once, then
   `std::visit` (or a switch on `.index()`) in the loop — no allocation, the
   dispatch is one predictable indirect call / jump table, and each strategy's
   `operator()` can inline within the visitor. `std::function` would add
   per-call indirection; a template can't hold "one of 4 chosen at runtime".
   </details>

---

## Interview questions

1. `std::function` ki 2 costs — indirect call aur allocation, dono kab?
2. Small-buffer optimization kya, boundary kahan (~16 B libstdc++)?
3. `std::sort` ka lambda inline kyun hota, `std::function` ka nahi?
4. Lambda aur `std::function` — kya rishta (hold karta, hai nahi)?
5. Hot path pe callable store karne ke `std::function` ke alternatives?
6. Algorithm ko stateful functor — by value ka kya problem, `std::ref` kyun?

---

## Next
→ [`17-chrono.md`](17-chrono.md)
