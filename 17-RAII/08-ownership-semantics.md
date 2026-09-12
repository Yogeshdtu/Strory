# 08 — Ownership semantics

## Prerequisites
- [`04-unique-ptr.md`](04-unique-ptr.md), [`05-shared-ptr.md`](05-shared-ptr.md), [`06-weak-ptr.md`](06-weak-ptr.md)

## Yeh topic abhi kyun
"Ownership" = **kaun zimmedaar hai is resource ko free karne ke liye, aur kab.**
Har heap object, file, lock ka ek clear owner hona chahiye. Smart pointers isse
**type mein encode** karte hain — ek `unique_ptr` parameter kehta hai "main
ownership le raha", ek `T*` kehta hai "main sirf dekh raha". Yeh vocabulary
sikhna zaroori — API design ka core.

---

## Ownership ka poora spectrum

| Semantics | Type | "Kaun free karega" | Copy? |
|---|---|---|---|
| **No ownership** (observe) | `T*`, `T&`, `std::span`, `string_view` | koi aur | trivially |
| **Exclusive ownership** | `std::unique_ptr<T>` | is object ka ek owner | move-only |
| **Shared ownership** | `std::shared_ptr<T>` | last owner (refcount) | copy = +1 owner |
| **Non-owning observer of shared** | `std::weak_ptr<T>` | (nobody — via `lock()`) | trivially |
| **Value** (owns by containing) | `T`, `std::vector<T>`, `std::string` | the containing scope | deep copy |

**Default preference (top to bottom):**
1. **Value semantics** (`T`, `std::vector<T>`) — no pointers, no lifetime
   questions. Best when the type is copyable/movable and not huge.
2. **`unique_ptr<T>`** — one owner, heap needed (polymorphism, huge object,
   pImpl, stable address).
3. **`shared_ptr<T>`** — genuinely shared, unclear lifetime. (Rare.)
4. **Raw `T*` / `T&`** — non-owning parameters, "borrow" for the call.

---

## Encoding ownership in function signatures

```cpp
// "I need to READ it for the duration of the call, I don't own it"
void print(const Widget& w);
void scan(const Widget* w);              // (nullable version)
long sum(std::span<const int> data);     // non-owning view

// "I need to MODIFY the caller's object, I don't own it"
void bump(Widget& w);

// "I'm TAKING ownership (I'll destroy it or pass it on)"
void store(std::unique_ptr<Widget> w);   // by value -> caller must std::move
void enqueue(std::vector<Job> jobs);     // by value -> take the vector

// "I'm SHARING ownership (I'll keep a copy)"
void subscribe(std::shared_ptr<Session> s);   // by value -> refcount bump, callee stores it

// "I'm RETURNING ownership to the caller"
std::unique_ptr<Widget> create();
std::vector<Result> compute();
```

Reading a signature should tell you the ownership contract **without looking at
the body**. `store(std::unique_ptr<Widget>)` unambiguously means "you're giving
this away". `store(Widget*)` is ambiguous — does it copy? keep the pointer?
delete it later?

---

## The "sink" parameter idiom

A function that **takes ownership** and stores it → take by value, then `move`:

```cpp
class Registry {
    std::vector<std::unique_ptr<Plugin>> plugins_;
public:
    void add(std::unique_ptr<Plugin> p) {         // sink -- by value
        plugins_.push_back(std::move(p));           // move into storage
    }
};

registry.add(std::make_unique<AuthPlugin>());       // temporary -> moved in, zero copies
auto p = std::make_unique<LogPlugin>();
registry.add(std::move(p));                          // named -> explicit move
```

By-value + `move` handles both cases optimally: a temporary is moved in for free,
a named object requires an explicit `std::move` (which documents the transfer at
the call site).

---

## Non-owning parameters — prefer them

If a function only **uses** an object during the call and doesn't keep it:

```cpp
// ❌ takes shared ownership just to read
double total(std::shared_ptr<Portfolio> p);       // atomic ++/-- per call, 16-byte param

// ✅ borrows
double total(const Portfolio& p);                  // zero overhead, clear "I don't own it"
```

Passing an owning smart pointer where a reference would do is the **#1
smart-pointer misuse** — needless refcount traffic (`shared_ptr`) or forced moves
(`unique_ptr`), and it lies about the contract.

**Guideline (Sutter/Stroustrup):**
- Read-only, non-null → `const T&`
- Read-only, nullable → `const T*`
- Modify → `T&`
- Take ownership → `unique_ptr<T>` by value
- Share ownership → `shared_ptr<T>` by value
- Just re-seat a smart pointer → `unique_ptr<T>&` / `shared_ptr<T>&`

---

## Ownership of collections

```cpp
std::vector<Widget>                    owned;   // vector OWNS the Widgets (value)
std::vector<std::unique_ptr<Widget>>   owned2;  // vector OWNS them (heap, polymorphic-safe)
std::vector<Widget*>                   view;    // vector does NOT own -- someone else must keep them alive
std::vector<std::shared_ptr<Widget>>   shared;  // shared ownership of each
```

`std::vector<T*>` is a **non-owning** container — a common source of leaks (who
`delete`s them? — folder 14 file 05) and dangling (`view` outlives the objects).
Use it only for genuine "views into things owned elsewhere", and be explicit.

---

## Andar kya hota hai

- Ownership is a **compile-time/design concept** — the machine code for
  `f(const Widget&)` vs `f(Widget*)` is the same (a pointer in a register). The
  *type* carries the contract; the compiler enforces move-only for `unique_ptr`,
  refcounts for `shared_ptr`.
- A `unique_ptr<T>` by-value parameter → the caller constructs it (or moves an
  existing one) into the parameter slot; at function end the parameter's dtor
  runs → the object is destroyed (unless it was moved out again). Passing a
  temporary → guaranteed move / elision (no copy).
- `shared_ptr` by-value parameter → an atomic increment at the call, an atomic
  decrement at return (unless moved). `const shared_ptr&` → neither.
- Value semantics (`std::vector<T>` by value) → a real copy unless the argument
  is an rvalue (then moved) — this is why "take by value + move" works for sinks
  (folder 18).

> **HFT relevance:** ownership discipline is a correctness *and* performance
> lever. Correctness: every buffer, fd, pool slot, and lock has exactly one
> owner, encoded in the type — so error paths and shutdown don't leak or
> double-free. Performance: non-owning parameters (`const T&`, `T*`,
> `std::span`) everywhere in hot code means zero refcount atomics, zero forced
> copies, zero smart-pointer size overhead — the hot path passes plain
> references into pre-owned, pooled memory. Owning smart pointers live in the
> control plane (who owns a session, a strategy instance, a config). A code
> review question for any hot-path signature: "why isn't this a `const T&`?"

---

## Hands-on

```bash
./build.ps1 17-RAII/examples/02_unique_ptr.cpp       # sink params, return ownership
./build.ps1 17-RAII/examples/03_shared_ptr.cpp       # by-value shared_ptr refcount cost
```

Write: a `class JobQueue { std::vector<std::unique_ptr<Job>> q_; };` with
`void submit(std::unique_ptr<Job>)` (sink) and `Job* peek() const` (non-owning
view). Note which calls need `std::move`.

---

## ⚠️ Traps

### Trap 1 — owning smart pointer parameter for a read
```cpp
void log(std::shared_ptr<Event> e);   // ⚠️ atomic ++/--. void log(const Event& e);
```

### Trap 2 — raw `T*` parameter with unclear ownership
```cpp
void store(Widget* w);   // ⚠️ does it copy? keep? delete later? Ambiguous. unique_ptr<Widget> or const&
```

### Trap 3 — `std::vector<T*>` without documenting who owns
```cpp
std::vector<Node*> nodes;   // ⚠️ owning or not? leak or dangling waiting to happen
```

### Trap 4 — two owners of one resource
```cpp
Widget* raw = new Widget;
std::unique_ptr<Widget> a{raw}, b{raw};   // ⚠️ double-free
cacheA.store(raw); cacheB.store(raw);     // ⚠️ who deletes?
```

### Trap 5 — returning a non-owning pointer to a local
```cpp
Widget* make() { Widget w; return &w; }   // ⚠️ dangling (folder 12/13). Return by value or unique_ptr
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`T*` parameter means the function might delete it" | Raw `T*` = **non-owning** by convention; owning → `unique_ptr` |
| "Pass `shared_ptr` by value to be safe" | Only if the callee takes/shares ownership; else `const T&` |
| "`std::vector<T*>` owns its elements" | No — non-owning; you manage the pointees separately |
| "Ownership is a runtime thing" | Design/compile-time; encoded in types, enforced by the compiler |
| "Every heap object needs a `shared_ptr`" | Most need one clear owner → `unique_ptr` or value |

---

## Exercises

1. **Signature quiz:** what ownership does each imply? `void a(Widget)`, `void
   b(Widget&)`, `void c(const Widget&)`, `void d(Widget*)`, `void
   e(std::unique_ptr<Widget>)`, `void f(std::shared_ptr<Widget>)`.

   <details><summary>Answer</summary>

   a: takes a copy (owns its own). b: borrows, will modify. c: borrows,
   read-only. d: borrows (non-owning), nullable. e: takes ownership (caller must
   `move`). f: shares ownership (callee keeps a copy → refcount bump).
   </details>

2. **Fix the API:** `class Cache { void put(std::string key, Blob* value); Blob*
   get(const std::string& key); };` — the ownership of `value` is unclear.
   Redesign so it's explicit that the cache owns stored blobs.

   <details><summary>Answer</summary>

   `void put(std::string key, std::unique_ptr<Blob> value);` (cache takes
   ownership) and `const Blob* get(const std::string& key) const;` (non-owning
   view — the cache keeps the blob alive). Or `std::shared_ptr<Blob> get(...)`
   if callers might outlive cache entries.
   </details>

3. **Sink idiom:** write `void Engine::registerHandler(std::unique_ptr<Handler>
   h)` storing into a `std::vector`. Show a call with a temporary and a call with
   a named `unique_ptr`. Which needs `std::move`?

   <details><summary>Answer</summary>

   `void registerHandler(std::unique_ptr<Handler> h) { handlers_.push_back(std::move(h));
   }`. `registerHandler(std::make_unique<FooHandler>());` — temporary, moved
   implicitly. `auto h = std::make_unique<BarHandler>(); registerHandler(std::move(h));`
   — named, needs explicit `std::move`.
   </details>

4. **Non-owning container:** you have `std::vector<std::unique_ptr<Order>>
   allOrders;` and want a `std::vector<...>` of just the *buy* orders without
   copying or transferring ownership. What element type?

   <details><summary>Answer</summary>

   `std::vector<Order*>` (or `std::vector<std::reference_wrapper<Order>>`) — raw
   non-owning pointers into `allOrders`. Valid only while `allOrders` is
   unchanged and alive (no realloc / erase). Document that lifetime dependency.
   </details>

5. **Re-seat parameter:** a function that reallocates a caller's `unique_ptr`
   (deletes the old, points it at a new object). Signature?

   <details><summary>Answer</summary>

   `void grow(std::unique_ptr<Buffer>& p) { p = std::make_unique<Buffer>(p->size()
   * 2); }` — takes `unique_ptr<Buffer>&` (a reference to the caller's smart
   pointer) so it can re-seat it. (`unique_ptr<Buffer>` by value would take
   ownership, not re-seat.)
   </details>

---

## Interview questions

1. Ownership semantics ke 5 kinds — type aur "kaun free karega"?
2. Default preference order (value / unique / shared / raw)?
3. Function signature se ownership contract kaise pata chalta — 4 examples?
4. "Sink parameter" idiom — kya, kaise, kyun by-value + move?
5. Non-owning parameter kab prefer karo — `const T&` vs `shared_ptr` by value?
6. `std::vector<T*>` — owning ya not? Common bug?

---

## Next
→ [`09-raii-for-other-resources.md`](09-raii-for-other-resources.md)
