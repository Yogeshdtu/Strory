# 16 — Exercises: modern C++

## Prerequisites
- All of folder 22 (files 01–15)

## Yeh file kya hai
Practice — output prediction, "find the bug", refactoring, aur discussion. Har
ek ka answer `<details>` mein. Compile: `./build.ps1 file.cpp`. Benchmarks:
`./build.ps1 fast file.cpp`.

---

## Part A — Output / behaviour prediction

### A1
```cpp
int a = 1;
auto byV = [a]{ return a; };
auto byR = [&a]{ return a; };
a = 42;
std::printf("%d %d\n", byV(), byR());
```
<details><summary>Answer</summary>

`1 42`. `[a]` snapshots `a` at creation; `[&a]` reads the live variable.
</details>

### A2
```cpp
std::optional<int> o;
std::printf("%d ", o.value_or(-1));
o = 5;
std::printf("%d ", *o);
o.reset();
std::printf("%d\n", o.has_value());
```
<details><summary>Answer</summary>

`-1 5 0`. Empty → `value_or(-1)` gives -1; `o = 5` engages it; `*o` is 5;
`reset()` empties it; `has_value()` is `false` (0).
</details>

### A3
```cpp
struct V { int a, b; auto operator<=>(const V&) const = default; };
std::printf("%d %d %d\n", V{1,2} < V{1,3}, V{2,0} < V{1,9}, V{1,2} == V{1,2});
```
<details><summary>Answer</summary>

`1 0 1`. Member-wise: `{1,2}<{1,3}` → a tie, b 2<3 → true. `{2,0}<{1,9}` → a 2>1
→ false. `==` is generated too → true.
</details>

### A4
```cpp
double m1 = 1.0, mn = std::nan("");
std::printf("%d %d %d\n", m1 < mn, m1 > mn, m1 == mn);
```
<details><summary>Answer</summary>

`0 0 0`. Any comparison with NaN is false — `partial_ordering::unordered`. This
is why a `= default` `<=>` over a `double` member gives a *partial* order.
</details>

### A5
```cpp
auto g = []() { static int n = 0; return ++n; };
auto h = [n = 0]() mutable { return ++n; };
std::printf("%d %d %d %d\n", g(), g(), h(), h());
```
<details><summary>Answer</summary>

`1 2 1 2`. `g` uses a function-local `static` (one shared counter). `h` uses an
init-capture (per-closure state); `h()`'s two calls give 1 then 2.
</details>

### A6
```cpp
std::string s = std::format("{:*>8}", 42);
std::string t = std::format("{:#x}", 255);
std::printf("[%s] [%s]\n", s.c_str(), t.c_str());
```
<details><summary>Answer</summary>

`[******42] [0xff]`. `*>8` = fill `*`, right-align, width 8. `#x` = hex with the
`0x` prefix.
</details>

---

## Part B — Find the bug

### B1
```cpp
std::function<int()> makeCounter() {
    int n = 0;
    return [&n]{ return ++n; };
}
```
<details><summary>Answer</summary>

`[&n]` captures a reference to the local `n`, which is destroyed when
`makeCounter` returns → the returned closure dangles → UB on call. Fix: `[n]()
mutable` (init-capture / by-value) so the counter lives in the closure.
</details>

### B2
```cpp
std::string_view firstWord(const std::string& line) {
    auto pos = line.find(' ');
    return std::string(line.substr(0, pos));   // ???
}
```
<details><summary>Answer</summary>

`std::string(...)` builds a temporary `std::string`; the returned `string_view`
points into it; the temporary dies at the `return` → dangling view. Fix: return
`std::string_view(line).substr(0, pos)` (a view into `line`, no allocation) — and
document that the result is valid only while `line` lives.
</details>

### B3
```cpp
struct Price {
    long ticks; int size;
    std::strong_ordering operator<=>(const Price& o) const { return ticks <=> o.ticks; }
};
// ... used in std::set<Price> and with p1 == p2
```
<details><summary>Answer</summary>

A **custom** `<=>` does not synthesize `==`. `p1 == p2` won't compile, and
`std::set<Price>` (which needs only `<`, fine) is OK but any equality use breaks.
Add `bool operator==(const Price& o) const { return ticks == o.ticks; }`.
</details>

### B4
```cpp
Generator<int> nums(const std::vector<int>& v) {
    for (int x : v) co_yield x;
}
int main() {
    auto g = nums(std::vector<int>{1,2,3});
    while (g.next()) use(g.value());
}
```
<details><summary>Answer</summary>

The coroutine parameter `v` is a `const&` bound to a temporary
(`std::vector<int>{1,2,3}`). The temporary is destroyed after the `nums(...)`
call returns the `Generator`, before the first `next()` pull → the coroutine
iterates freed memory. Fix: take the vector **by value** into the coroutine
(`Generator<int> nums(std::vector<int> v)`) so the frame owns a copy.
</details>

### B5
```cpp
void scale(std::span<float> d, int n) {
    [[assume(n % 8 == 0)]];
    for (int i = 0; i < n; ++i) d[i] *= 2;
}
// caller: scale(buf, buf.size());   where buf.size() can be 100
```
<details><summary>Answer</summary>

`[[assume(n % 8 == 0)]]` is a promise; `n = 100` violates it → **undefined
behaviour**, silently (the compiler may vectorize by 8 with no remainder loop and
walk off the end, or worse). Either enforce `n % 8 == 0` at the boundary
(pad/round the buffer), or drop the `[[assume]]` and let the compiler emit the
scalar tail.
</details>

---

## Part C — Refactor legacy → modern

### C1
```cpp
std::vector<int>* collectEven(const std::vector<int>& in) {
    std::vector<int>* out = new std::vector<int>();
    for (size_t i = 0; i < in.size(); ++i)
        if (in[i] % 2 == 0) out->push_back(in[i]);
    return out;
}
```
<details><summary>Answer</summary>

`std::vector<int> collectEven(const std::vector<int>& in) { std::vector<int> out;
std::ranges::copy_if(in, std::back_inserter(out), [](int x){ return x % 2 == 0;
}); return out; }` — return by value (guaranteed elision / move, no `new`, no
leak), range algorithm.
</details>

### C2
```cpp
int parsePort(const char* s) {
    int p = atoi(s);
    if (p <= 0 || p > 65535) return -1;
    return p;
}
```
<details><summary>Answer</summary>

`std::optional<int> parsePort(std::string_view s) { int p; auto [ptr, ec] =
std::from_chars(s.data(), s.data() + s.size(), p); if (ec != std::errc{} || p < 1
|| p > 65535) return std::nullopt; return p; }` — no UB on overflow, explicit
"invalid" (`nullopt` or, better, `std::expected` with a reason), no allocation.
</details>

### C3
```cpp
struct ByPrice {
    bool operator()(const Order& a, const Order& b) const { return a.px < b.px; }
};
std::sort(orders.begin(), orders.end(), ByPrice());
Order* best = &orders[0];
for (size_t i = 1; i < orders.size(); ++i)
    if (orders[i].qty > best->qty) best = &orders[i];
```
<details><summary>Answer</summary>

`std::ranges::sort(orders, {}, &Order::px);` (projection, no functor struct).
`const Order* best = orders.empty() ? nullptr : &*std::ranges::max_element(orders,
{}, &Order::qty);`.
</details>

---

## Part D — Discussion

1. **Standard-by-standard:** for each of C++11/14/17/20/23, name the *one* feature
   that most changed how you write C++, and why.
   <details><summary>Answer sketch</summary>

   C++11: **move semantics** (return by value cheap, RAII everywhere, `<atomic>`
   + memory model). C++14: **`std::make_unique` + generic lambdas + relaxed
   `constexpr`** (the C++11 gaps). C++17: **vocabulary types** (`optional`/
   `variant`/`string_view`) + structured bindings + `if constexpr` (readable
   generic code). C++20: **concepts + ranges** (constrained, composable generics
   with sane errors). C++23: **`std::expected`** (zero-cost value-or-error
   channel, finally).
   </details>

2. **Hot-path guardrails:** which modern features are a win on the tick path,
   which are neutral, and which are a trap?
   <details><summary>Answer sketch</summary>

   **Win:** `string_view`/`span` (no copy), `from_chars` (~8×), guaranteed copy
   elision, `<bit>`, `constexpr`/`consteval` tables, `if constexpr` fast paths,
   `<=>` (write-once comparison), `std::variant`+`visit` (jump-table dispatch).
   **Neutral:** `auto`, range-for, structured bindings, `enum class`, `using`,
   most attributes — same codegen. **Trap:** `std::function` for a hot callback,
   `std::regex` anywhere fast, coroutines in the innermost loop (frame + suspend
   overhead), `std::any`, a `= default` `<=>` over a `double` (NaN → not a strict
   weak order).
   </details>

3. **Coroutine placement:** you're designing a backtest engine and a live trading
   engine. Where do coroutines fit in each?
   <details><summary>Answer sketch</summary>

   Backtest: a `generator` over the historical event stream is a great fit — lazy
   pull of the next event, composes with ranges, latency irrelevant. Live: **not**
   the tick path (frame allocation + suspend/resume + no vectorization). Coroutines
   can structure the **async gateway / admin I/O** (`co_await socket.read()`) with
   pooled frames, but the market-data → decision → order loop stays a plain
   contiguous pipeline.
   </details>

4. **Adoption plan:** your team is on C++17 with GCC 13. What C++20 features do
   you turn on first, and what do you wait on?
   <details><summary>Answer sketch</summary>

   Turn on now: `concepts`, `<=>`, `<bit>`, `<span>`, `<numbers>`, designated
   init, `consteval`/`constinit`, `[[likely]]`, calendar `<chrono>`, `std::format`
   (GCC 13 has it), ranges (solid). Wait: **modules** (build-system integration
   still rough), **coroutines** (no `std::generator` until C++23 / GCC 14, need a
   library), heavy `std::ranges::to` / new C++23 adaptors. `std::expected` needs
   GCC 12+ libstdc++ — check `__cpp_lib_expected`, else `tl::expected`.
   </details>

---

## Challenge

Open-ended — koi answer key nahi. Har stage ke baad **same output** hona chahiye; jo naapo
wahi likho, chahe modern version slow nikle (Rule 2).

### Challenge 1 — C++03 order router → C++23, stage by stage
~200 lines ka ek **purane style** ka program likho: order lines parse karna, symbol-wise
aggregate karna, report print karna — raw `for` loops, functor structs, owning raw pointers,
`printf`, `NULL`, manual `std::map` find/insert (bilkul `examples/08_legacy_to_modern.cpp`
ke `namespace legacy` jaisa, par bada). Phir chaar stages mein modernize karo:
1. **C++11/14:** `auto`, range-for, lambdas, `unique_ptr`, `nullptr`
2. **C++17:** structured bindings, `std::optional`, `string_view`, `if`-with-initializer
3. **C++20:** ranges pipelines, concepts, `<=>`, `std::format`, designated initializers
4. **C++23:** `std::print`, `ranges::to`, `views::enumerate`, deducing `this`, `std::expected`
   (file 15; `*.cpp23.cpp` naam do)

Har stage ke baad: (a) golden output file se **byte-for-byte** compare, (b) `-O2` pe runtime
(best of 5), (c) binary size, (d) lines of code. Ek table banao aur har row ke saath ek line:
"is stage mein kya badla, aur kya kuch slow/bada hua?"

### Challenge 2 — lazy tick replay aur allocations ginna
`std::generator<Tick>` (file 15) se ek CSV ke 1M ticks replay karo; `views::filter` se sirf
ek symbol; `views::chunk(1000)` se batches; har batch ka VWAP `std::print` karo. Phir:
- Global `operator new` ko ek counter ke saath replace karo (folder 14 file 05 wali
  "counted `operator new`" technique) aur gino **kitne allocations** hue — generator frame kitne?
- Wahi pipeline plain `for` loop se likho, `-O2` pe ns/tick compare karo.
- Jo aaya wahi likho: ranges + generator ki "ergonomics" ki kimat is workload pe kitni hai?

### Challenge 3 — modules ka break-even point apni machine pe
Ek 3-header chhoti library (`price.hpp`, `book.hpp`, `util.hpp`) ko ek named module mein
badlo (file 10), aur consumers mein `import std;` use karo (file 15 section 10 ka setup).
1, 5, aur 20 translation units ke saath **clean build time** naapo — headers vs modules.
Lesson 15 mein ek file pe ~1.7× aur ~5 files pe break-even aaya tha; aapki machine pe kitne
TUs pe modules jeete? Jo mile, wahi likho.

---

## Next
→ [`../23-ERROR-HANDLING/00-README.md`](../23-ERROR-HANDLING/00-README.md)
