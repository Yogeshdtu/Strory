# 14 — Move in practice — when it happens, when it doesn't

## Prerequisites
- [`08-std-move.md`](08-std-move.md) … [`13-noexcept-move.md`](13-noexcept-move.md)

## Yeh topic abhi kyun
Theory done. Ab practical: **kaunse expressions actually move karte hain, kaunse
chup-chaap copy karte hain, aur common mistakes** jo "maine to move socha tha
par copy ho gaya" (ya ulta) type bugs dete hain. Yeh ek checklist-style lesson
hai.

---

## Move HAPPENS here

```cpp
std::string s = "long string past SSO";
std::vector<int> v = {1, 2, 3};

// 1. Initializing from an rvalue
std::string a = std::move(s);            // xvalue -> move ctor
std::string b = get() + "!";             // prvalue -> move ctor (or elision)
std::string c = std::string(100, 'x');   // prvalue -> elision (C++17), else move

// 2. Assigning an rvalue
a = std::move(b);                        // move assignment
a = makeString();                        // rvalue -> move assignment

// 3. Passing an rvalue to a by-value parameter
void take(std::string p);
take(std::move(s));                      // s moved into p
take(makeString());                      // prvalue moved (or elided) into p

// 4. Returning a local / by-value param / prvalue
std::string f() { std::string x; return x; }        // NRVO, else implicit move
std::string g(std::string x) { return x; }          // x is implicitly moved (param, no NRVO)
std::string h() { return std::string("x"); }        // elision

// 5. Container operations with rvalue arguments
v.push_back(std::move(otherVec));        // wait, wrong type -- but: elem moves
vecOfString.push_back(std::move(s));     // s moved into the vector's storage
vecOfString.emplace_back(std::move(s));  // same
m.insert({key, std::move(value)});       // value moved into the map node

// 6. std::vector / container reallocation (if element move is noexcept -- file 13)
vecOfMsg.push_back(...);                 // on regrow: each existing Msg MOVED

// 7. Algorithms that relocate: std::sort, std::remove, std::rotate, erase-remove
std::sort(v.begin(), v.end());          // moves elements around (move-assign)
```

---

## Move does NOT happen here (silent copy)

```cpp
std::string s = "long string past SSO";

// 1. Source is an lvalue and you didn't std::move
std::string a = s;                       // COPY -- s is an lvalue
sink.push_back(s);                       // COPY
take(s);                                 // COPY into the by-value parameter

// 2. Source is const (even with std::move)
const std::string cs = "x";
std::string b = std::move(cs);           // COPY -- const rvalue -> move ctor can't bind -> copy ctor

// 3. Named rvalue-reference / forwarding-reference parameter used without std::move / std::forward
void f(std::string&& p)   { g(p); }               // COPY -- p is an lvalue inside f. g(std::move(p))
template <class T> void h(T&& p) { g(p); }         // COPY (or ref) -- g(std::forward<T>(p))

// 4. return std::move(local) that BLOCKED elision -- you forced a move where you might have had ZERO
std::string bad() { std::string x; return std::move(x); }   // move (not copy), but pessimized vs `return x;`

// 5. Type has no move ctor (Rule of Three, or logging destructor -- file 9) -> "moves" fall back to copy
class Legacy { ~Legacy(); Legacy(const Legacy&); Legacy& operator=(const Legacy&); };
std::vector<Legacy> vl; vl.push_back(...);        // realloc COPIES (no move ctor)

// 6. Trivially-copyable type -> "move" == "copy" (both a memcpy). Not a bug, just no benefit
struct POD { int a, b; };
POD p2 = std::move(p1);                  // bit-copy; p1 unchanged

// 7. std::initializer_list elements are const -> can't move out of a braced-init-list
std::vector<std::string> vs = { s1, s2, s3 };     // COPIES (initializer_list<string> holds const string)
```

---

## The classic mistakes

### Mistake 1 — copy where a move was intended

```cpp
std::vector<Row> rows = loadRows();
process(rows);                           // ⚠️ copies the whole vector if process takes it by value/const& stored
// If `rows` is dead after this:
process(std::move(rows));                // ✅
```

### Mistake 2 — move where a copy was needed (use-after-move)

```cpp
auto config = buildConfig();
applyConfig(std::move(config));          // config is now moved-from
logConfig(config);                       // ⚠️ config is empty / unspecified -> wrong log / crash
```

Only `std::move` an object on its **last use**.

### Mistake 3 — `std::move` on a member you still need

```cpp
Widget::Widget(Widget&& o) noexcept
    : name_(std::move(o.name_)) {
    validate(o.name_);                   // ⚠️ o.name_ was just moved-from -> empty
}
```

### Mistake 4 — moving into a container element vs moving the container

```cpp
std::vector<std::string> a, b;
// ...
b = std::move(a);                        // ✅ move the whole vector (steal 3 pointers)
for (auto& s : a) b.push_back(std::move(s));   // ⚠️ if you meant to move the vector, this is N moves + a is a mess
```

### Mistake 5 — `std::move` in a range-for over a container you'll reuse

```cpp
for (auto s : names) consume(std::move(s));   // ✅ s is a per-iteration COPY; moving it is fine
for (auto& s : names) consume(std::move(s));  // ⚠️ moves the ELEMENTS out of `names` -> names now full of empties
```

---

## When to `std::move` — the rules

1. **Last use of a named lvalue** that you want transferred, not copied:
   `q.push(std::move(batch));` when `batch` is dead after.
2. **Members in a hand-written move ctor/assign**: `name_(std::move(o.name_))`.
3. **Into a member from a by-value "sink" parameter**: `void set(std::string s)
   { name_ = std::move(s); }`.
4. **Never** in a `return` of a local (`return x;`), never on a `const`, never on
   something you'll use again.

## When to `std::forward` — the rules

1. **Only** inside a function with a **forwarding reference** parameter (`T&&`
   with deduced `T`, or `auto&&`).
2. Pass each such parameter onward exactly once: `f(std::forward<T>(x))`.
3. Use `std::forward<decltype(x)>(x)` for `auto&&`.

---

## Andar kya hota hai

- The compiler picks copy vs move purely from the **value category** of the
  source expression (file 04) and the available constructors. `std::move` /
  `std::forward` are compile-time casts that change the category → zero runtime
  cost, but they steer which constructor runs.
- A "move" of a trivially-copyable type is a `memcpy` (== copy). A move of a
  heap-owning type is a few pointer transfers. A "move" that falls back to copy
  (const source, missing move ctor) is a full deep copy — and the only signal is
  a profiler showing an unexpected allocation, or `is_nothrow_move_constructible`
  being false.
- Use-after-move is not UB *per se* for standard types (they're "valid,
  unspecified") but reading the value is a logic bug; for your own types it can
  be UB if your move ctor leaves the object in a state your other methods don't
  expect.

> **HFT relevance:** the practical rules matter more than the theory. Common
> real bugs: (1) a big `std::vector`/`std::string` passed by value or captured by
> value in a lambda without `std::move` → hidden copy on a setup path; (2) a
> `for (auto& x : container) sink(std::move(x));` that empties a container the
> code reuses next tick; (3) a hot element type missing `noexcept` move (file 13)
> → `push_back` growth deep-copies; (4) `return std::move(local)` disabling
> elision. Reviewers grep hot/warm code for `std::move(` and check each: is it
> the last use? is the source non-const? for `for (auto&`, is emptying the
> container intended? And `static_assert(is_nothrow_move_constructible_v<T>)` on
> every container element type.

---

## Hands-on

```bash
./build.ps1 18-COPY-MOVE/examples/05_std_move_demo.cpp     # move vs copy, const, return, container
./build.ps1 fast 18-COPY-MOVE/examples/09_copy_vs_move_bench.cpp
```

Add to `05`: a `for (auto& s : names) v.push_back(std::move(s));` then print
`names` — see the elements are now empty.

---

## ⚠️ Traps

### Trap 1 — pass big object by value without `std::move`
```cpp
void store(std::vector<Row>);  std::vector<Row> rows = load();  store(rows);   // ⚠️ copy. store(std::move(rows))
```

### Trap 2 — use after move
```cpp
sink(std::move(x));  log(x);   // ⚠️ x moved-from
```

### Trap 3 — `for (auto& x : c) f(std::move(x))` and then reuse `c`
```cpp
for (auto& e : events) queue.push(std::move(e));  replay(events);   // ⚠️ events now full of moved-from husks
```

### Trap 4 — `std::move` inside a `return` of a local
```cpp
Big f() { Big b; ...; return std::move(b); }   // ⚠️ pessimizing. return b;
```

### Trap 5 — capturing a big object by value in a lambda
```cpp
auto job = [data]() { use(data); };   // ⚠️ copies `data`. [data = std::move(data)]() { ... }  (init-capture)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Passing by value is a move" | Only if the argument is an rvalue; an lvalue is copied |
| "`std::move` guarantees a move" | Not on `const`, not if there's no move ctor — silent copy |
| "`for (auto& x : c) f(std::move(x))` is harmless" | It empties the container's elements |
| "After moving, I can still read the source's value" | Valid but unspecified — assign or destroy only |
| "Braced-init-list elements can be moved into a vector" | `initializer_list` holds `const` — they're copied |

---

## Exercises

1. **Move or copy:** for each, does a move happen? `std::string a = b;`,
   `std::string a = std::move(b);`, `std::string a = b + c;`, `std::string a =
   std::move(constB);`, `v.push_back(x);`, `v.push_back(std::move(x));`, `return
   local;`, `return std::move(local);`.

   <details><summary>Answer</summary>

   `a = b` copy. `a = std::move(b)` move. `a = b + c` move (prvalue). `a =
   std::move(constB)` **copy** (const). `push_back(x)` copy. `push_back(std::move(x))`
   move. `return local;` NRVO/implicit move. `return std::move(local)` move but
   pessimized (elision blocked).
   </details>

2. **Fix the reuse bug:** `for (auto& row : rows) db.insert(std::move(row));
   rows.clear();` — is the `std::move` a problem here?

   <details><summary>Answer</summary>

   Not a bug — `rows` is `clear()`ed right after, so emptying the elements first
   is harmless (they'd be destroyed anyway). This is actually the *right* pattern
   for "drain a container into a sink": `std::move` each element, then `clear()`.
   The bug would be reusing `rows`'s contents after the loop.
   </details>

3. **Sink parameter:** `void Cache::put(std::string key, Blob value) { map_.emplace(std::move(key),
   std::move(value)); }` — for `cache.put("k", makeBlob())`, count copies of the
   blob.

   <details><summary>Answer</summary>

   Zero copies of the blob. `makeBlob()` is a prvalue → moved (or elided) into
   the `value` parameter → `std::move(value)` → moved into the map node. The
   `std::string` key: `"k"` → temporary → moved into `key` → moved into the node.
   </details>

4. **Lambda capture:** `std::vector<int> data = load();  pool.submit([data] {
   process(data); });` — the lambda copies `data`. Rewrite with C++14
   init-capture to move it.

   <details><summary>Answer</summary>

   `pool.submit([data = std::move(data)] { process(data); });` — the init-capture
   `data = std::move(data)` moves the vector into the lambda's closure. (Outer
   `data` is now moved-from — fine if unused after.)
   </details>

5. **Missing move ctor:** `class Old { ~Old(); Old(const Old&); Old&
   operator=(const Old&); };  std::vector<Old> v; v.push_back(Old{});  v.push_back(Old{});`
   — what happens on the second `push_back` if it triggers realloc?

   <details><summary>Answer</summary>

   `Old` has no move ctor (Rule of Three, no move ops) → `std::move_if_noexcept`
   picks the **copy** ctor → the existing element(s) are **copied** into the new
   buffer, not moved. Deep-copy per element per reallocation. Fix: add `noexcept`
   move ctor/assign, or Rule of Zero.
   </details>

---

## Interview questions

1. 5 places move DEFINITELY happens?
2. 5 places you think move happens but it's a silent copy?
3. `for (auto& x : c) f(std::move(x))` — kya problem, kab OK?
4. Use-after-move — kya bug, standard types ka kya guarantee?
5. Lambda mein bade object ko move kaise capture karo (init-capture)?
6. "sink parameter" (`void f(std::string s)`) + `std::move` — zero-copy kaise?

---

## Next
→ [`15-exercises.md`](15-exercises.md)
