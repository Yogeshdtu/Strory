# 04 — OOP & design problems

## Prerequisites
- `15-CLASSES/`, `16-OOP/`, `17-RAII/`, `18-COPY-MOVE/`
- `25-OBJECT-MODEL/` (vtable, layout — hard problems ke liye)
- `21-TEMPLATES/11-crtp.md` (static polymorphism)

## Yeh file kya hai
20 problems — class design, Rule of 3/5, virtual dispatch, RAII wrappers,
type erasure, compile-time vs runtime polymorphism. Yeh **"design a class"**
round ki practice hai — API pehle socho, phir implementation.

Har problem: `<details>` mein design + gotchas. Poora code →
[`11-solutions/04-oop-design-solutions.md`](11-solutions/04-oop-design-solutions.md).

Har design ke liye khud se poocho: **ownership kiska? copy semantics? move?
exception safety? kya invariant hai?**

---

## Part A — Easy (~15 min)

### A1. Shape hierarchy
`Shape` abstract base, `area()` pure virtual; `Rectangle`, `Circle`. Ek
`std::vector<std::unique_ptr<Shape>>` ka total area.
`Pattern:` pure virtual + virtual dtor.
<details><summary>Design</summary>

`virtual double area() const = 0; virtual ~Shape() = default;`. Base dtor
**virtual** — warna `delete base_ptr` derived part leak karega (UB). Total:
`std::accumulate` + lambda calling `->area()`. Non-owning `span<Shape*>` bhi
chalega agar lifetime bahar managed hai.
</details>

### A2. Rule of 3 for a `char*` owner
`class Str { char* p; size_t n; }` — ctor from `const char*`, dtor, copy ctor,
copy assign. Leak / double-free / self-assign teeno se bacho.
<details><summary>Design</summary>

Copy ctor: naya buffer alloc + `memcpy`. Copy assign: **copy-and-swap** —
`Str tmp(other); swap(*this, tmp);` — self-assign safe, exception safe
(alloc fail hone pe `*this` unchanged). Dtor: `delete[] p;`. Rule of 3: ek
likha to teeno likho.
</details>

### A3. Rule of 5 + `=default` / `=delete`
A2 waali class mein move ctor + move assign add karo. Kab `=default` kaam karta,
kab nahi?
<details><summary>Design</summary>

Move ctor: `p(std::exchange(o.p, nullptr)), n(std::exchange(o.n, 0))` —
`noexcept` mark karo (warna `vector` reallocation pe copy karega). `=default`
move sirf tab sahi jab members khud move pe "steal + null" karte hon — raw
pointer pe `=default` bas member-wise copy karega (bug). `=delete` copy → move-only
type.
</details>

### A4. Virtual destructor — show the leak
Ek chhota program jo virtual dtor ke bina `delete base` karne pe derived ka
member (jaise `std::string`) leak karta hai — ASan/valgrind output ke saath.
<details><summary>Design</summary>

`Base` bina virtual dtor; `Derived : Base { std::vector<int> big; }`.
`Base* b = new Derived; delete b;` → sirf `~Base` chala, `~Derived` nahi →
`big` ka buffer leak (technically UB). Fix: `virtual ~Base() = default;`.
Guideline: polymorphic base ka dtor virtual **ya** protected non-virtual.
</details>

### A5. Copy vs move call trace
Ek class jisme har special member `std::puts` karta hai. In statements ka output
predict karo: `T a; T b = a; T c = std::move(a); f(T{}); v.push_back(a);
v.push_back(T{});`.
<details><summary>Answer</summary>

`T b = a` → copy ctor. `T c = std::move(a)` → move ctor. `f(T{})` → temporary,
move ctor (ya elided C++17 pe agar by-value). `push_back(a)` → copy. `push_back(T
{})` → move. Reallocation pe existing elements: `noexcept` move → moved, warna
copied. Run karke verify — surprising elisions milenge.
</details>

### A6. PIMPL
`class Widget` ka private data ek `struct Impl` mein chhupao (`unique_ptr<Impl>`).
Header se `<vector>` etc. hata do. Kya cost?
<details><summary>Design</summary>

Header: `class Impl; std::unique_ptr<Impl> pimpl_;` + declared (not defaulted)
dtor. `.cpp` mein `Impl` full definition + `Widget::~Widget() = default;`.
Faayda: ABI stability, compile-time firewall. Cost: ek heap indirection + alloc,
inlining loss — hot-path classes pe mat karo.
</details>

---

## Part B — Medium (~25–35 min)

### B1. LRU cache class
`LruCache<K, V>` — `get(k) -> optional<V>`, `put(k, v)`, fixed capacity, dono
`O(1)`.
`Pattern:` `std::list` (recency order) + `unordered_map<K, list::iterator>`.
<details><summary>Design</summary>

`get`: map lookup → `list.splice(list.begin(), list, it)` (node ko front pe le
jao, iterator valid rehta) → value. `put`: exists → update + splice; naya →
push_front, `if (size > cap) { evict list.back(); map.erase; }`. `splice` `O(1)`
aur iterators invalidate nahi karta — yehi crux.
</details>

### B2. Matrix with operator overloading
`Mat` — `operator()(r,c)` element access, `operator+`, `operator*` (matmul),
move ctor. Contiguous storage.
`Pattern:` flat `std::vector<double>`, `data_[r*cols_ + c]`.
<details><summary>Design</summary>

`operator()` do overloads (const / non-const), return `double&`. `operator+`:
free function ya member returning by value (move). `operator*`: dimension assert,
triple loop (ikj order cache-friendly). Bade matrices pe `a + b + c` teen
temporaries banata — expression templates (file 06) fix karte.
</details>

### B3. Bounded blocking queue — interface
`BlockingQueue<T>` ka **public API** design karo: `push` (blocks if full), `pop`
(blocks if empty), `try_push`/`try_pop`, `close()`. Threading contract likho.
<details><summary>Design</summary>

`push(T)`, `bool try_push(T)`, `T pop()`, `optional<T> pop_for(duration)`,
`void close()` (jagaao sab waiters, aage ke `push` fail). Internally: `mutex` +
`not_full` + `not_empty` cv, predicate loops. `close()` ke baad `pop` tab tak
values deta jab tak queue khaali. Implementation → file 07 B1.
</details>

### B4. Observer / Signal-Slot
`Signal<Args...>` — `connect(callback) -> Connection`, `emit(args...)`,
`Connection` scope-exit pe auto-disconnect.
`Pattern:` vector of `std::function` + stable ids; RAII `Connection`.
<details><summary>Design</summary>

`connect` → id, `{id, fn}` push, `Connection{this, id}`. `emit` → iterate
(copy the list first — callback khud disconnect kar sakta hai). `~Connection` →
`disconnect(id)`. Dangling: observer object callback se pehle mar gaya → use
`weak_ptr` capture ya explicit disconnect in its dtor.
</details>

### B5. State machine — two ways
Ek order lifecycle (`New → PendingNew → Working → Filled/Cancelled`) do tareeke
se: (a) `enum` + transition table, (b) virtual `State` classes. Trade-offs.
<details><summary>Design</summary>

(a) `State next[nStates][nEvents]` — data-driven, cache-friendly, `O(1)`,
easy to visualize/validate; logic-per-state thoda scatter. (b) `struct State {
virtual State* on(Event) = 0; }` — logic localized, par vtable indirection +
alloc/lifetime. HFT: table wins (predictable, no indirection). Invalid transition
→ explicit error state, silently ignore mat karo.
</details>

### B6. Self-registering plugin registry
`Registry` — types apne aap ko static-init time pe register karein (`REGISTER
(Foo)` macro), phir `Registry::create("Foo")` factory.
`Pattern:` `static bool reg = (Registry::add("Foo", &make<Foo>), true);`
<details><summary>Design</summary>

`Registry::map()` — Meyers singleton (`static std::map<...>& map()`) taaki
static-init-order fiasco na ho. `add(name, factory_fn)`. Macro har TU mein ek
static object banata jiska ctor register karta. Caveat: linker dead-strip un
objects ko hata sakta jinka koi reference nahi (`--whole-archive` / explicit
force-link).
</details>

### B7. `NonCopyable` and why `Singleton` is usually wrong
`NonCopyable` base likho. Phir Meyers `Singleton<T>`. Phir explain karo HFT
codebase mein Singleton kyun red flag hai.
<details><summary>Answer</summary>

`NonCopyable`: `protected` ctor/dtor, `= delete` copy. `Singleton`: `static T&
instance() { static T t; return t; }` — thread-safe init (C++11), lazy. **Problem:**
hidden global state → testing hard (can't reset / mock), lifetime/order issues at
shutdown, hidden contention (the one instance's lock), no DI. HFT prefers explicit
wiring: config ek object banao, use uske members ko pass karo.
</details>

### B8. `ScopeGuard`
`ScopeGuard` — arbitrary callable ko destructor pe run kare; `dismiss()` se
cancel. Move-only.
`Pattern:` `template<class F> class ScopeGuard { F f; bool active; };`
<details><summary>Design</summary>

`~ScopeGuard() { if (active_) f_(); }`. `dismiss()` → `active_ = false`. Move
ctor source ko dismiss kar de. Helper: `auto g = make_scope_guard([&]{ fclose(fp);
});`. `f_()` ko `noexcept` maano (dtor se throw = terminate). C++ mein
`std::experimental::scope_exit` / Andrei Alexandrescu ka classic.
</details>

### B9. Type-erased `Function<Ret(Args...)>`
`std::function` ka mini version — koi bhi callable store kare, small-buffer
optimization ke saath.
`Pattern:` abstract `Callable` base + templated concrete holder + inline buffer.
<details><summary>Design</summary>

`struct Base { virtual Ret call(Args...) = 0; virtual void moveTo(void*) = 0;
virtual ~Base(); };` `template<class F> struct Holder : Base { F f; … };`.
`alignas(max) std::byte buf[32]` — chhota callable inline, bada heap. `operator()`
→ `reinterpret_cast<Base*>(storage)->call(...)`. Empty call → throw
`bad_function_call`.
</details>

---

## Part C — Hard (~40+ min)

### C1. Order book — public API design
Sirf **interface** + har operation ki complexity. `add_limit(side, price, qty)
-> OrderId`, `cancel(OrderId)`, `modify(OrderId, newQty)`, `best_bid()`,
`best_ask()`, `depth(n)`. Data structure choice defend karo.
<details><summary>Design</summary>

Price → `Level` (FIFO of orders). Options: `std::map<Price, Level>` (`O(log)`,
ordered, simple); sorted `vector` (`O(log)` find, `O(n)` insert); **price-indexed
flat array** `Level[MAX_TICKS]` + cached BBO (`O(1)` add/cancel, bounded rewalk
on level-empty). HFT choice: flat array. `OrderId → {side, tick, slot}` index for
`O(1)` cancel. `depth(n)` → walk from cached BBO. Details → folder 39, `10-hft-problems.md`.
</details>

### C2. Matching engine ownership model
`Order`, `Level`, `Book` — kaun kiska owner hai? `shared_ptr` hot path pe kyun
nahi? Order ke resting aur filled hone pe memory kya hoti hai?
<details><summary>Answer</summary>

`Book` owns everything. `Order` objects ek **pool** (`ObjectPool<Order>`) se
aate hain — `Book` ke andar. `Level` ek intrusive FIFO of `Order*` (order ke
andar `next`/`prev` hooks — no per-link alloc). Cancel/fill → `Level` se unlink
(`O(1)`), `Order` ko pool mein return. `shared_ptr` = atomic refcount traffic +
cache-line bouncing + non-deterministic `delete` → hot path pe zeher. Raw
non-owning pointers + clear single owner.
</details>

### C3. Lock-free-friendly object lifetime
Ek object jo lock-free structure mein hai — reader use kar raha ho tab writer
usse "remove" kar de. Safe reclamation ke 3 schemes name + trade-off.
<details><summary>Answer</summary>

(1) **Hazard pointers** — reader publishes what it's reading; reclaimer skips
those. `O(1)` read, bounded memory. (2) **RCU** — readers cost nothing (just
disable preemption / mark epoch); writer waits a grace period then frees. Great
read:write ratio. (3) **Epoch-based reclamation** — global epoch, threads pin an
epoch while active, objects freed once all threads past their retire epoch.
Simpler than HP, unbounded memory if a thread stalls. (Folder 28.)
</details>

### C4. Testable **and** zero-overhead
Ek `Strategy` component jo production mein virtual-call-free ho, par unit test
mein mock inject ho sake. Kaise?
<details><summary>Answer</summary>

**Compile-time polymorphism**: `template<class MarketData> class Strategy` — prod
mein `Strategy<LiveFeed>`, test mein `Strategy<MockFeed>`. Zero indirection,
fully inlinable, `if constexpr` for branches. Cost: longer compiles, template
error messages, code bloat if many instantiations. Alternative: virtual interface
+ `-fdevirtualize` / `final` + LTO — compiler often devirtualizes when the
concrete type is visible. CRTP (`Strategy<Derived>`) for a shared base without
vtable.
</details>

### C5. `Result<T, E>` / expected
`Result<T, E>` — ya `T` (success) ya `E` (error), exceptions ke bina. `map`,
`and_then`, `value_or`.
`Pattern:` tagged union (`std::variant`-like) + monadic ops.
<details><summary>Design</summary>

`union { T ok; E err; }; bool has_value_;` + placement new / manual dtor (ya
`std::variant<T,E>` wrap). `map(f)` → `Result<U,E>` (ok pe `f` apply, err
passthrough). `and_then(f)` → `f` khud `Result` deta hai (flatten). C++23 mein
`std::expected`. HFT: hot path error handling bina stack-unwind cost ke; `E`
chhota rakho (enum / error code).
</details>

---

## Next
→ [`05-stl-problems.md`](05-stl-problems.md)
