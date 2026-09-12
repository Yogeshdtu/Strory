# 14 — Exercises: error handling

## Prerequisites
- Poora folder 23 (files 01–13)

## Yeh file kya hai
Practice — output prediction, "find the bug", design decisions, aur ek challenge.
Har ek ka answer `<details>` mein. Compile: `./build.ps1 file.cpp`. Benchmarks:
`./build.ps1 fast file.cpp`.

---

## Part A — Output / behaviour prediction

### A1
```cpp
struct G { const char* n; ~G() { std::printf("~%s ", n); } };
void f() { G a{"a"}; { G b{"b"}; throw 1; } G c{"c"}; }
int main() { try { G z{"z"}; f(); } catch (int) { std::printf("caught\n"); } }
```
<details><summary>Answer</summary>

`~b ~a ~z caught`. Inner scope's `b`, then `f`'s `a` (unwinding), then `main`'s
`z` (leaving the `try`). `c` is never constructed (throw came first). No `~c`.
</details>

### A2
```cpp
int calls = 0;
int side() { ++calls; return 1; }
int main() {
    assert(side() == 1);
    std::printf("calls=%d\n", calls);
}
```
Compiled once with `-O0`, once with `-O0 -DNDEBUG`.
<details><summary>Answer</summary>

`-O0`: `calls=1`. `-DNDEBUG`: `calls=0` — `assert(...)` is removed entirely, so
`side()` is never called. Never put side effects inside `assert`.
</details>

### A3
```cpp
std::expected<int, const char*> half(int x) {
    if (x % 2) return std::unexpected("odd");
    return x / 2;
}
int main() {
    auto r = half(10).and_then(half).and_then(half);
    if (r) std::printf("%d\n", *r); else std::printf("err: %s\n", r.error());
}
```
<details><summary>Answer</summary>

`half(10)` → 5. `and_then(half)` → `half(5)` → `unexpected("odd")`. Next
`and_then(half)` is skipped (error passthrough). Output: `err: odd`.
</details>

### A4
```cpp
void a() noexcept { b(); }
void b()          { throw std::runtime_error("x"); }
int main() { try { a(); } catch (const std::exception& e) { std::puts(e.what()); } }
```
<details><summary>Answer</summary>

`std::terminate` (abort) — the `catch` in `main` never runs. `b()` throws, the
exception tries to leave `a()` which is `noexcept` → terminate at `a`'s boundary.
Search phase doesn't even reach `main`.
</details>

### A5
```cpp
int foo(int* p) {
    int v = *p;
    if (!p) return -1;
    return v + 1;
}
```
`-O2` pe `foo(nullptr)` — kya hota hai, aur kyun?
<details><summary>Answer</summary>

`*p` on `nullptr` is UB. The compiler is allowed to assume `p != nullptr` (else UB
already happened), so `if (!p)` is dead code and gets **removed** at `-O2`. Result:
`foo(nullptr)` dereferences null → segfault (or arbitrary behaviour). Fix: null-
check *before* the deref.
</details>

### A6
```cpp
std::error_code ec = std::make_error_code(std::errc::timed_out);
std::printf("%d %s | %s\n", ec.value(), ec.category().name(),
            (ec == std::errc::timed_out) ? "match" : "no");
```
<details><summary>Answer</summary>

Something like `110 generic | match` (value is POSIX `ETIMEDOUT` = 110 on Linux;
category `"generic"`). The portable `ec == std::errc::timed_out` comparison is
`true` regardless of the platform's raw number.
</details>

### A7
```cpp
struct Loud { Loud(){ std::puts("ctor"); } ~Loud(){ std::puts("dtor"); } };
struct Bad  { Loud l; Bad() { throw std::runtime_error("boom"); } };
int main() { try { Bad b; } catch (...) { std::puts("caught"); } }
```
<details><summary>Answer</summary>

```
ctor
dtor
caught
```
`Loud l` (member) is fully constructed → its `~Loud()` runs during unwinding.
`~Bad()` does **not** run (the object was never fully constructed).
</details>

### A8
```cpp
std::vector<int> v{1, 2, 3};
try {
    std::printf("%d\n", v.at(10));
} catch (const std::out_of_range& e) {
    std::printf("oor: %s\n", e.what());
}
```
Normal build vs `-fno-exceptions` build.
<details><summary>Answer</summary>

Normal: `oor: ...` (the `at()` throws `std::out_of_range`, caught). With
`-fno-exceptions`: doesn't compile (the `try`/`catch` is a compile error). If you
remove the try/catch and just call `v.at(10)` under `-fno-exceptions`: it calls
`std::__throw_out_of_range_fmt` → `std::terminate` → abort.
</details>

---

## Part B — Find the bug

### B1
```cpp
class FileErr : public std::exception {
    std::string msg_;
public:
    FileErr(std::string path) : msg_("cannot open " + std::move(path)) {}
    const char* what() const noexcept override { return msg_.c_str(); }
};
```
<details><summary>Answer</summary>

Deriving straight from `std::exception` with a `std::string` member: the
(implicit) copy constructor copies `msg_`, which allocates, and that copy runs
while an exception is being thrown/propagated — a throwing copy of an exception is
bad (can lead to `terminate`). Fix: derive from `std::runtime_error` and pass the
message to its constructor (`using`/explicit) — it stores the string safely
(ref-counted, `noexcept` copy).
</details>

### B2
```cpp
Buffer& operator=(const Buffer& o) {
    delete[] data_;
    data_ = new char[o.size_];
    std::memcpy(data_, o.data_, o.size_);
    size_ = o.size_;
    return *this;
}
```
<details><summary>Answer</summary>

Two bugs. (1) **Self-assignment**: `b = b` → `delete[] data_` then read `o.data_`
(same, now freed) → UB. (2) **Not strong exception-safe**: if `new char[o.size_]`
throws, `data_` is already deleted → object is now broken (dangling), not just
"unchanged". Fix: copy-and-swap — `Buffer tmp(o); swap(*this, tmp);` (or take the
parameter by value).
</details>

### B3
```cpp
struct Order { std::string sym; std::vector<Leg> legs;
    Order(Order&& o) : sym(std::move(o.sym)), legs(std::move(o.legs)) {} };
std::vector<Order> book;   // push_back a lot
```
<details><summary>Answer</summary>

The hand-written move constructor is not `noexcept`, so it's `noexcept(false)`.
`std::vector` uses `std::move_if_noexcept` on reallocation → since the move isn't
`noexcept`, it **copies** every element instead of moving (deep string/vector
copies) — a big, avoidable perf hit. Fix: `Order(Order&&) noexcept = default;`
(and the move assignment too), plus
`static_assert(std::is_nothrow_move_constructible_v<Order>);`.
</details>

### B4
```cpp
double parse_rate(const char* s) {
    double r = std::strtod(s, nullptr);
    if (errno == ERANGE) throw std::out_of_range("rate");
    return r;
}
```
<details><summary>Answer</summary>

`errno` is not cleared before the call, so a stale `ERANGE` from an earlier
library call would trigger a false throw; and if `strtod` itself sets `ERANGE`
but a call between it and the `if` clobbers `errno`, you'd miss it. Fix: `errno =
0;` immediately before `strtod`, read `errno` immediately after. Also consider
`std::from_chars` (no `errno`, returns `std::errc`).
</details>

### B5
```cpp
~Session() {
    if (open_) socket_.close();     // close() can throw on flush error
}
```
<details><summary>Answer</summary>

Destructor is implicitly `noexcept`. If `close()` throws — especially while the
`Session` is being destroyed during stack unwinding from another exception —
you get `std::terminate`. Fix: give the class an explicit `close()` method that
returns an error for callers who care, and in the destructor swallow: `try { if
(open_) socket_.close(); } catch (...) { /* log */ }`.
</details>

### B6
```cpp
for (auto it = levels_.begin(); it != levels_.end(); ++it) {
    if (it->price == px) { levels_.push_back(make_level(px)); break; }
}
```
<details><summary>Answer</summary>

`push_back` can reallocate `levels_`, invalidating `it` **and** the cached
`end()` — the `++it`/`!=` after (well, there's a `break`, so not here) — but even
computing `levels_.end()` once per iteration is fine; the real issue is if the
`break` weren't there. As written the `break` saves it, but it's fragile. Safer:
compute the index, do the `push_back` after the loop, or reserve capacity. Don't
mutate a container's size while iterating it.
</details>

### B7
```cpp
enum class Status { ok, retry, fatal };
Status send(const Msg&);
void pump(const std::vector<Msg>& msgs) {
    for (auto& m : msgs) send(m);
}
```
<details><summary>Answer</summary>

`send`'s return value is ignored — `retry` and `fatal` are silently dropped, so a
failed send looks like success. Mark `[[nodiscard]] Status send(const Msg&);` so
the compiler flags this, and actually handle the result (retry loop / abort on
fatal).
</details>

### B8
```cpp
std::expected<Config, std::string> load(std::string_view path);
// ...
auto cfg = load(p).value();
run(cfg);
```
<details><summary>Answer</summary>

Two issues. (1) `.value()` on an error throws `std::bad_expected_access` (and
under `-fno-exceptions`, `terminate`) — check `if (auto r = load(p)) ... else
...`. (2) `E = std::string` allocates on every error path; prefer a small `enum
class`/struct so the error path is allocation-free.
</details>

---

## Part C — Design decisions

### C1
Ek market-data feed handler ke in errors ke liye mechanism chuno (aur ek line
justification): (a) sequence number gap, (b) unknown instrument id, (c) malloc
failure while growing a buffer, (d) config file has a bad field at startup,
(e) internal check "bid > ask after update".

<details><summary>Answer</summary>

(a) counter + gap-recovery state machine (hot, expected) — not an exception.
(b) `enum`/`error_code`, drop or route to a slow path (hot, expected).
(c) at startup this shouldn't be a runtime concern — pre-allocate pools; if it
truly can happen, `new(nothrow)` + fail-fast (`abort`) since recovery is
hopeless mid-stream. (d) `std::expected<Config, ConfigErr>` with the offending
field — rich, startup, latency irrelevant. (e) `assert` in debug/CI + a
`CHECK`-style always-on guard that `abort`s — a crossed book is a bug/corruption,
better to failover than to trade on it.
</details>

### C2
Aapki team `-fno-exceptions` decide karti hai. Teen concrete cheezein jo ab
change karni padengi codebase mein, aur unka fix.

<details><summary>Answer</summary>

(1) Fallible constructors (e.g. `Socket` that connects) → move to factories
(`static std::expected<Socket, Err> make(...)`) or two-phase init. (2) `v.at(i)`,
`std::stoi`, throwing `std::filesystem`/`std::regex` calls → replace with
bounds-checked `operator[]`, `std::from_chars`, `error_code` overloads. (3) `new`
that can fail → `new(std::nothrow)` + null check, or route through pre-allocated
arenas. Also: audit third-party libs for `-fno-exceptions` support.
</details>

### C3
`std::expected<T, E>` mein `E` ke liye `enum class`, `std::error_code`, aur ek
`struct { Code; SourceLoc; small_string; }` — har ek kab?

<details><summary>Answer</summary>

`enum class` — hottest paths, fixed small reason set, 1–2 bytes, cheapest.
`std::error_code` — when you cross OS/STL boundaries (`errno`, `from_chars`,
`filesystem`) or want portable `== std::errc::...` checks; 16 bytes, still
throw/alloc-free. The `struct` — control-plane / config / protocol parsing where
the operator needs "which field, which line, what value"; costlier to copy, fine
off the hot path.
</details>

### C4
Ek deep pipeline: `fetch → decode → validate → transform → store`, har step fail
kar sakta hai (except `transform` jo pure hai). Return-code style vs
`expected`+monadic style — dono sketch karo, boilerplate compare.

<details><summary>Answer</summary>

Return-code: each step `if (auto e = step(x, out); e != Err::ok) return e;` — 4
such lines plus temporaries, error type threaded through every signature.
`expected`: `return fetch(u).and_then(decode).and_then(validate)
.transform(transform_).and_then(store);` — one expression; `transform` for the
infallible step, `and_then` for the rest; errors auto-propagate; `-O2` compiles
it to the same `if`-ladder. The monadic version has ~0 branching boilerplate.
</details>

### C5
`class RiskEngine` ka `check(const Order&)` — 2M calls/sec. Return type design
karo. Ab requirement badalta hai: caller ko *kaunsa* limit breach hua chahiye
(notional / position / rate / restricted-symbol). Kya badla?

<details><summary>Answer</summary>

First cut: `bool check(const Order&) noexcept;` — hot, one register, predictable.
With the reason: `enum class RiskReject : std::uint8_t { ok, notional, position,
rate, restricted };` returned by value — still one register, still branch-
predictable, still `noexcept`, no allocation. Don't reach for exceptions or a
heavy error struct here; the `enum` carries everything the caller needs.
</details>

---

## Part D — Challenge

### D1 — Build a `Result<T>` and a small pipeline
`-std=c++20` (no `<expected>`). Implement `Result<T, E>` with: `has_value()`,
`operator bool`, `operator*`, `error()`, `value_or()`, and monadic `and_then` /
`transform` / `or_else`. Then write a config-line parser:

```
key = value        -> Entry{key, value}
```

- `parse_line(std::string_view) -> Result<Entry, ParseErr>` where `ParseErr` is an
  `enum class { empty, no_equals, empty_key }`.
- `parse_file(std::span<const std::string_view> lines) -> Result<std::vector<Entry>,
  LineError>` where `LineError { std::size_t line; ParseErr err; }` — stop at the
  first bad line and report which one.
- A `main` that feeds a few good and one bad line and prints either the parsed
  entries or `line N: <reason>`.

Constraints: no exceptions, no allocation on the error path (`E` types are small),
every fallible function `[[nodiscard]]`. Compare your `Result` to
`examples/05_expected.cpp`'s `Expected`.

<details><summary>Hints</summary>

- Storage: `union { T val_; E err_; }; bool has_;` — hand-write the destructor
  (`has_ ? val_.~T() : err_.~E()`), copy ctor with placement-new.
- `and_then(f)`: `if (has_) return f(val_); else return Unexpect<E>{err_};` — `f`
  returns a `Result<U, E>`.
- `transform(f)`: same but wrap: `return Result<U,E>{ f(val_) };`.
- `parse_file`: loop with index, `parse_line(lines[i])` — on error, `return
  Unexpect<LineError>{ {i, r.error()} };`; on success `out.push_back(*r);`.
- Test that ignoring a `[[nodiscard]]` result warns.
</details>

### D2 — Measure your own throw cost
Adapt `examples/04_exception_cost.cpp`: add a **fourth** path that uses your `D1`
`Result<T, E>` for the error, and compare all four (try/catch, return-code,
`error_code`, `Result`) on the 0.1%-error scenario at `-O2`. Predict first, then
measure. Do the value-based mechanisms differ from each other meaningfully?

<details><summary>What you should find</summary>

The three value-based mechanisms (return-code, `error_code`, `Result`) should be
within noise of each other (~1.5–2 ns/iter) and all ~4–5x faster than the
try/catch path at 0.1% errors — because at `-O2` they all compile to essentially
the same branch-on-a-flag. `error_code` copies 16 bytes vs 1–2 for the enum, so
in a *pure* tight loop it can be a hair slower; here it's lost in the work. The
try/catch path pays ~6 µs per actual throw, amortized over 1000 iters ≈ +6 ns
each.
</details>

---

## Next
→ [`../24-COMPILATION-LINKING/00-README.md`](../24-COMPILATION-LINKING/00-README.md)
