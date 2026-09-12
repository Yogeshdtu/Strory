# 06 — `std::weak_ptr`

## Prerequisites
- [`05-shared-ptr.md`](05-shared-ptr.md)
- Folder 14 file 05 (leaks — cyclic `shared_ptr`)

## Yeh topic abhi kyun
`shared_ptr` ka ek fatal flaw: **cycles**. Do objects ek doosre ko `shared_ptr`
se rakhein → refcount kabhi 0 nahi → dono leak. `std::weak_ptr` iska fix hai: ek
**non-owning** observer jo strong count nahi badhata. Cycles todne, aur caches /
observer lists ke liye.

---

## The cycle leak

```cpp
struct Node {
    std::shared_ptr<Node> other;
    ~Node() { std::cout << "~Node\n"; }
};

{
    auto a = std::make_shared<Node>();     // a: strong count 1
    auto b = std::make_shared<Node>();     // b: strong count 1
    a->other = b;                           // b's count -> 2
    b->other = a;                           // a's count -> 2
}   // a, b (locals) destroyed -> counts 2 -> 1.  NEVER 0.  ~Node NEVER runs.  LEAK.
```

`a` local gaya → `a`'s count 2→1 (because `b->other` still holds one). `b` local
gaya → `b`'s count 2→1 (because `a->other` still holds one). Now `a` and `b`
objects are only kept alive by **each other** — unreachable, but count > 0 →
never freed. Classic reference-counting weakness (a garbage collector would
collect this; `shared_ptr` won't).

`examples/04_weak_ptr.cpp` shows this with a counted `operator new`: the bad
version leaves 2 blocks outstanding.

---

## The fix — `weak_ptr` for the back-edge

```cpp
struct Node {
    std::shared_ptr<Node> next;      // forward: OWNS
    std::weak_ptr<Node>   prev;      // back: OBSERVES (doesn't own)
    ~Node() { std::cout << "~Node\n"; }
};

{
    auto a = std::make_shared<Node>();
    auto b = std::make_shared<Node>();
    a->next = b;                      // b's strong count -> 2
    b->prev = a;                      // a's strong count STAYS 1 (weak doesn't count)
}   // a gone -> a's strong count 0 -> ~a -> a->next reset -> b's count 0 -> ~b.  CLEAN.
```

`weak_ptr` increments the **weak** count, not the strong. So the cycle is broken:
one direction owns, the other just watches.

**Rule:** in a parent↔child / prev↔next / owner↔observer relationship, **one
direction is `shared_ptr` (owning), the other is `weak_ptr` (non-owning)**.
Typically: parent owns child (`shared_ptr`), child points back to parent
(`weak_ptr`).

---

## Using a `weak_ptr` — `lock()`

A `weak_ptr` can't be dereferenced directly (the object might be gone). You
`lock()` it to get a `shared_ptr`:

```cpp
std::weak_ptr<Node> w = someShared;

if (auto s = w.lock()) {            // s is a shared_ptr<Node>, or empty if object gone
    s->doSomething();               // safe -- s keeps it alive for this scope
} else {
    // the object has been destroyed
}

w.expired();                        // true if the object is gone (== w.use_count() == 0)
w.use_count();                      // strong count of the referred object (0 if gone)
w.reset();                          // stop observing
```

`lock()` is **atomic and race-free**: it either returns a valid `shared_ptr`
(strong count bumped, object guaranteed alive for the returned pointer's scope)
or an empty one. You can't have a "checked, then it died before I used it" race.
Always use `lock()`, never assume.

---

## Common uses

### 1. Break `shared_ptr` cycles

Parent→child owning, child→parent weak. Doubly-linked lists, trees with parent
pointers, observer patterns.

### 2. Non-owning cache

```cpp
class TextureCache {
    std::unordered_map<std::string, std::weak_ptr<Texture>> cache_;
public:
    std::shared_ptr<Texture> get(const std::string& name) {
        if (auto sp = cache_[name].lock()) return sp;      // still alive -> reuse
        auto sp = loadTexture(name);                        // load fresh
        cache_[name] = sp;                                  // store WEAK -> cache doesn't keep it alive
        return sp;
    }
};
```

The cache holds `weak_ptr`s → it doesn't keep textures alive; when the last real
user drops its `shared_ptr`, the texture frees, and the cache's `weak_ptr`
`expired()`s (clean it up lazily).

### 3. Async callbacks that shouldn't extend lifetime

```cpp
timer.setCallback([weakSelf = weak_from_this()] {
    if (auto self = weakSelf.lock()) self->onTimer();   // fire only if still alive
});
```

Capture `weak_ptr` (not `shared_ptr`) → the callback doesn't keep the object
alive; if the object is gone by the time the timer fires, the callback safely
no-ops.

---

## `weak_ptr` and `make_shared` interaction

Recall (file 05): `make_shared` puts the object **and** the control block in one
allocation. That allocation is freed only when **both** strong AND weak counts
hit 0. So:

```cpp
auto sp = std::make_shared<HugeObject>();   // one big allocation
std::weak_ptr<HugeObject> wp = sp;
sp.reset();                                  // object destroyed (~HugeObject runs)...
                                            // ...but the HugeObject-sized memory is NOT freed
                                            //    because wp still holds a weak ref (control block alive)
```

For big objects with long-lived `weak_ptr`s, `shared_ptr(new HugeObject)`
(separate allocations) frees the object memory immediately on strong-count 0.
Niche case, but know it.

---

## Andar kya hota hai

- `sizeof(weak_ptr<T>) == 2 * sizeof(void*)` — same as `shared_ptr` (object ptr +
  control-block ptr). It just doesn't touch the strong count.
- Copy/destroy a `weak_ptr` → atomic inc/dec of the **weak count** (still an
  atomic, but weak-count contention is rarer).
- `lock()` → an atomic **compare-and-increment** on the strong count: "if strong
  > 0, bump it and return a `shared_ptr`; else return empty". Lock-free, single
  atomic RMW.
- Control block lifetime: destroyed (freed) when strong == 0 **and** weak == 0.
  While strong > 0, there's an implicit +1 on the weak count. So the control
  block outlives the object as long as any `weak_ptr` exists.

> **HFT relevance:** `weak_ptr` is a cold-path tool — it's about *lifetime
> correctness* for shared objects (breaking cycles, safe async callbacks, non-
> owning caches), not performance. In the hot path there's no `shared_ptr`, so no
> `weak_ptr` either. Where it matters: control-plane objects (sessions,
> subscriptions, strategy instances) that outlive individual requests and get
> referenced by async work — capturing `weak_ptr` in a callback avoids both a
> use-after-free (if you'd captured a raw pointer) and a lifetime leak (if you'd
> captured a `shared_ptr`). The `lock()`-then-check is the safe pattern.

---

## Hands-on

```bash
./build.ps1 17-RAII/examples/04_weak_ptr.cpp
```

`bad::run()` (cycle → 2 blocks leaked) vs `good::run()` (`weak_ptr` back-edge →
clean). Plus a `weak_ptr` outliving its object: `expired()` true, `lock()` empty.

---

## ⚠️ Traps

### Trap 1 — cycle with two `shared_ptr`s
```cpp
parent->child = c;  c->parent = parent;   // ⚠️ both shared -> leak. child->parent should be weak_ptr
```

### Trap 2 — using `weak_ptr` without `lock()`
```cpp
weakPtr->method();   // ❌ weak_ptr has no operator-> . auto s = weakPtr.lock(); if (s) s->method();
```

### Trap 3 — `lock()` result not checked
```cpp
weakPtr.lock()->method();   // ⚠️ if expired -> null deref. Check `if (auto s = weakPtr.lock())`
```

### Trap 4 — cache holding `shared_ptr` instead of `weak_ptr`
```cpp
std::map<Key, std::shared_ptr<T>> cache;   // ⚠️ cache keeps everything alive forever -> unbounded growth. weak_ptr
```

### Trap 5 — `make_shared` + long-lived `weak_ptr` keeping big memory reserved
```cpp
auto sp = std::make_shared<Huge>();  weak_ptr<Huge> w = sp;  sp.reset();
// object destroyed but Huge-sized memory still held by control block until `w` is gone
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`shared_ptr` handles cycles (like a GC)" | It doesn't — refcount never hits 0. Use `weak_ptr` |
| "`weak_ptr` can be dereferenced" | No — `lock()` to get a `shared_ptr` first |
| "`lock()` might succeed then the object dies mid-use" | No — the returned `shared_ptr` keeps it alive for its scope |
| "A cache of `shared_ptr` is fine" | Keeps entries alive forever → use `weak_ptr` for non-owning caches |
| "`weak_ptr` costs nothing" | Atomic weak-count ops on copy/destroy; `lock()` is a CAS |

---

## Exercises

1. **Break the cycle:** `struct Employee { std::shared_ptr<Company> employer; };
   struct Company { std::vector<std::shared_ptr<Employee>> staff; };` — this
   leaks. Which pointer should be `weak_ptr` and why?

   <details><summary>Answer</summary>

   `Company` **owns** its staff (`shared_ptr`); `Employee::employer` should be
   `std::weak_ptr<Company>` (an employee observes but doesn't own the company).
   Then when the `Company` `shared_ptr` drops, staff drop, no cycle.
   </details>

2. **lock() pattern:** write a `Subject` that holds `std::vector<std::weak_ptr<Observer>>`
   and a `notify()` that calls `observer->update()` only for observers still
   alive, and removes expired ones.

   <details><summary>Answer</summary>

   `void notify() { std::erase_if(obs_, [](auto& w){ return w.expired(); });
   for (auto& w : obs_) if (auto o = w.lock()) o->update(); }` — lock each, skip
   empties, prune expired.
   </details>

3. **Cache correctness:** `weak_ptr` cache — after all real users drop a
   `Texture`, what does `cache_[name].lock()` return? When would you clean the
   map entry?

   <details><summary>Answer</summary>

   `lock()` returns an **empty** `shared_ptr` (the `Texture` is gone). Clean the
   entry lazily on the next `get(name)` miss, or periodically sweep
   `erase_if(cache_, [](auto& kv){ return kv.second.expired(); })`.
   </details>

4. **Async safety:** `timer.async_wait([self = shared_from_this()] { self->tick();
   });` vs `[weakSelf = weak_from_this()] { if (auto s = weakSelf.lock())
   s->tick(); }` — behaviour difference when the object is destroyed before the
   timer fires?

   <details><summary>Answer</summary>

   `shared_ptr` capture → the callback **keeps the object alive** until it runs
   (may be undesirable — the object "lingers"). `weak_ptr` capture → if the
   object was destroyed, `lock()` returns empty → the callback no-ops. Choose
   based on whether the pending work *should* extend lifetime.
   </details>

5. **expired vs lock:** why prefer `if (auto s = w.lock())` over `if
   (!w.expired()) w.lock()->use()`?

   <details><summary>Answer</summary>

   `expired()` then `lock()` is a TOCTOU race in MT — the object can die between
   the check and the `lock()`. `if (auto s = w.lock())` does it atomically: one
   operation, and `s` (if non-null) guarantees the object lives for `s`'s scope.
   </details>

---

## Interview questions

1. `shared_ptr` cycle leak — kaise banta, kyun refcount 0 nahi hota?
2. `weak_ptr` cycle kaise todta (strong vs weak count)?
3. `weak_ptr` ko use kaise karte (`lock()`), aur kyun woh race-free hai?
4. `weak_ptr` ke 3 use cases?
5. `make_shared` + long-lived `weak_ptr` — memory ka kya (control block lifetime)?
6. `if (auto s = w.lock())` vs `expired()` check — kaunsa sahi, kyun?

---

## Next
→ [`07-custom-deleters.md`](07-custom-deleters.md)
