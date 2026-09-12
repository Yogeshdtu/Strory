# 27 — Exercises: the Standard Library

## Prerequisites
- All of folder 19 (files 01–26)

## Yeh file kya hai
Folder 19 ke saare topics ke practice problems — output prediction, "find the
bug", design choices, aur chhote implementation tasks. Har ek ka answer
`<details>` mein. Compile karke check karo jahan code ho: `./build.ps1 file.cpp`.

---

## Part A — Containers: pick the right one

For each, name the container (or "no container — flat structure") and one line of
justification.

1. 50,000 live orders, keyed by a 64-bit id, pure lookup, no ordering needed.
2. Order book: price → aggregated quantity, need best bid/ask and range scans.
3. A fixed set of 12 config flags read once at startup.
4. A work queue where items are added at the back and taken from the front, not
   latency-critical.
5. "Next event to fire" out of thousands, by timestamp.
6. Per-symbol rolling window of the last 256 trade prices, indexed, overwritten
   in a loop.
7. You hold `T*` to some elements and keep inserting/erasing *other* elements
   anywhere; the held pointers must stay valid.

<details><summary>Answers</summary>

1. `std::unordered_map` with `reserve(64k)` (or a flat/open-addressed hash map, or
   direct index on id bits) — O(1) lookup, order irrelevant.
2. Sorted `std::vector<Level>` + `lower_bound` (or a tick-indexed `std::array`) —
   `front()`/`back()` = best bid/ask O(1), range = `[lower_bound, upper_bound)`,
   contiguous.
3. `std::array<Flag, 12>` (or even a `std::map` — N=12, perf irrelevant); linear
   scan is fine.
4. `std::queue` (deque-backed) — FIFO, allocation acceptable here. (Hot path → a
   ring buffer.)
5. `std::priority_queue<Event, std::vector<Event>, cmp>` — min-heap on timestamp,
   O(log n) push/pop, contiguous storage.
6. `std::array<double, 256>` + a head index — no allocation, contiguous,
   overwrite in place.
7. `std::list` (or `std::map`) — insert/erase never invalidates other elements.
   Or `std::vector<std::unique_ptr<T>>` and hold `T*` (the pointees are stable).
</details>

---

## Part B — Output prediction

### B1
```cpp
std::vector<int> v{1, 2, 3};
std::cout << v.size() << " " << v.capacity() << "\n";
v.reserve(10);
std::cout << v.size() << " " << (v.capacity() >= 10) << "\n";
v.push_back(4);
std::cout << v.size() << "\n";
```
<details><summary>Answer</summary>

```
3 3            (or capacity >= 3, impl-defined; libstdc++ gives 3)
3 1            reserve changes capacity, NOT size
4
```
</details>

### B2
```cpp
std::vector<int> v{5, 3, 1, 4, 2};
auto e = std::remove(v.begin(), v.end(), 3);
std::cout << v.size() << " ";
v.erase(e, v.end());
std::cout << v.size() << "\n";
```
<details><summary>Answer</summary>

`5 4` — `std::remove` compacts to `{5,1,4,2, ?}` and returns the new end but
**doesn't** change size (still 5). `erase(e, end())` drops the tail → size 4.
</details>

### B3
```cpp
std::map<std::string, int> m;
std::cout << m["a"] << " " << m.size() << "\n";
m["b"] = 2;
std::cout << (m.count("c") ? "yes" : "no") << " " << m.size() << "\n";
```
<details><summary>Answer</summary>

`0 1` — `m["a"]` **inserts** `{"a", 0}` and returns 0; size is now 1.
`no 2` — `count("c")` doesn't insert; size is 2 (`a`, `b`).
</details>

### B4
```cpp
std::vector<int> v{1,2,3,4,5,6};
auto view = v | std::views::filter([](int x){ return x % 2 == 0; })
              | std::views::transform([](int x){ return x * 10; });
int calls = 0;
for (int x : view) { calls++; std::cout << x << " "; }
std::cout << "| " << calls << "\n";
```
<details><summary>Answer</summary>

`20 40 60 | 3` — lazy: only the 3 even elements flow through; transform runs 3
times.
</details>

### B5
```cpp
std::vector<double> v{1.5, 2.5, 3.0};
std::cout << std::accumulate(v.begin(), v.end(), 0) << "\n";
std::cout << std::accumulate(v.begin(), v.end(), 0.0) << "\n";
```
<details><summary>Answer</summary>

`6` — init `0` is `int` → accumulator is `int` → `0+1+2+3 = 6` (each add
truncates). `7` — init `0.0` → `double` accumulator → `7.0`.
</details>

### B6
```cpp
std::priority_queue<int> pq;
for (int x : {3, 1, 4, 1, 5, 9, 2}) pq.push(x);
std::cout << pq.top() << " ";
pq.pop();
std::cout << pq.top() << "\n";
```
<details><summary>Answer</summary>

`9 5` — `std::priority_queue` is a **max**-heap by default.
</details>

---

## Part C — Find the bug

### C1
```cpp
std::vector<int> v{10, 20, 30};
int& first = v[0];
for (int i = 0; i < 100; ++i) v.push_back(i);
first = 99;                       // ???
```
<details><summary>Answer</summary>

`first` is a reference into `v`. The `push_back` loop reallocates `v` several
times → `first` dangles → `first = 99` is use-after-free (UB). Fix: `v.reserve
(103)` before taking the reference, or re-fetch `v[0]` after the loop.
</details>

### C2
```cpp
std::vector<int> v = {1, 2, 3, 4, 5};
for (auto it = v.begin(); it != v.end(); ++it)
    if (*it % 2 == 0) v.erase(it);
```
<details><summary>Answer</summary>

`v.erase(it)` invalidates `it` (and shifts everything after); `++it` then steps a
dangling iterator, and you skip the element that slid into the erased slot. Fix:
`it = v.erase(it);` in the even branch, `++it` in the else — or just
`std::erase_if(v, [](int x){ return x % 2 == 0; });`.
</details>

### C3
```cpp
bool isDate(const std::string& s) {
    std::regex re(R"(\d{4}-\d{2}-\d{2})");
    return std::regex_search(s, re);
}
// called millions of times
```
<details><summary>Answer</summary>

The `std::regex` is **parsed and compiled on every call** (µs each) — thousands
of times the cost of the search. Fix: `static const std::regex re(R"(...)");`
(built once, thread-safe init). Or drop regex for a hand check.
</details>

### C4
```cpp
std::unordered_map<int, std::string> m;
for (int i = 0; i < 1'000'000; ++i) m.emplace(i, "x");
// ... slow build ...
```
<details><summary>Answer</summary>

No `reserve` → ~20 rehashes, each O(current size). Fix: `m.reserve(1'000'000);`
before the loop → one bucket allocation, no rehash (build time roughly halves).
</details>

### C5
```cpp
std::pmr::vector<int> makeScratch() {
    std::byte buf[4096];
    std::pmr::monotonic_buffer_resource pool{buf, sizeof buf};
    std::pmr::vector<int> v{&pool};
    v.assign(100, 7);
    return v;                     // ???
}
```
<details><summary>Answer</summary>

`buf` and `pool` are locals — destroyed at `return`. The returned vector still
references `pool`/`buf` → any later use is UB. Fix: the buffer and resource must
outlive every container using them (caller-owned, or `static`, or heap with
matching lifetime).
</details>

### C6
```cpp
std::vector<int> a{1,2,3}, b{1,2};
if (std::equal(a.begin(), a.end(), b.begin())) std::cout << "equal\n";
```
<details><summary>Answer</summary>

The 3-iterator `std::equal` assumes `b` has at least `a.size()` elements — it
reads `b[2]` which is out of bounds (UB). Fix: the 4-iterator form
`std::equal(a.begin(), a.end(), b.begin(), b.end())` (returns `false` here on the
size mismatch).
</details>

---

## Part D — Algorithm selection

For each task pick the single best algorithm and give its complexity.

1. The p99 latency of 500,000 samples.
2. The 25 cheapest offers out of 10,000, in sorted order.
3. Remove all elements failing a predicate from a `std::vector`, in one pass.
4. Is every element of `v` positive?
5. First position where `a` and `b` differ.
6. Sort trades by symbol, keeping same-symbol trades in arrival order.
7. Sum of `price[i] * qty[i]`.
8. Move all "urgent" items to the front, order among them irrelevant.

<details><summary>Answers</summary>

1. `std::nth_element(v.begin(), v.begin() + n*99/100, v.end())` then read that
   element — **O(n)**.
2. `std::partial_sort(v.begin(), v.begin() + 25, v.end())` — **O(n log 25)**.
3. `std::erase_if(v, pred)` (C++20) / erase-remove idiom — **O(n)**, one pass.
4. `std::all_of(v.begin(), v.end(), [](int x){ return x > 0; })` — **O(n)**.
5. `std::mismatch(a.begin(), a.end(), b.begin(), b.end())` — **O(min(|a|,|b|))**.
6. `std::stable_sort(v.begin(), v.end(), bySymbol)` — **O(n log n)**.
7. `std::transform_reduce(price.begin(), price.end(), qty.begin(), 0.0)` —
   **O(n)**, vectorized.
8. `std::partition(v.begin(), v.end(), isUrgent)` — **O(n)**.
</details>

---

## Part E — Small implementation tasks

### E1 — Sorted vector as a set
Implement `insert`, `contains`, and `range(lo, hi)` for a `std::vector<int>` kept
sorted. Give complexities.

<details><summary>Answer</summary>

```cpp
struct SortedVec {
    std::vector<int> v;
    void insert(int x) { v.insert(std::lower_bound(v.begin(), v.end(), x), x); }  // O(log n) find + O(n) shift
    bool contains(int x) const {
        auto it = std::lower_bound(v.begin(), v.end(), x);
        return it != v.end() && *it == x;                                          // O(log n)
    }
    auto range(int lo, int hi) const {                                             // [lo, hi], O(log n) to locate
        return std::ranges::subrange(std::lower_bound(v.begin(), v.end(), lo),
                                     std::upper_bound(v.begin(), v.end(), hi));
    }
};
```
</details>

### E2 — Bitmask slot iteration
Given `std::uint64_t free_mask` (1 = slot free), write a loop that claims the
lowest free slot and returns its index, or -1 if none.

<details><summary>Answer</summary>

```cpp
int claim(std::uint64_t& free_mask) {
    if (free_mask == 0) return -1;
    int i = std::countr_zero(free_mask);   // lowest set bit
    free_mask &= free_mask - 1;            // clear it
    return i;
}
```
O(1), two instructions on modern x86 (`tzcnt`, `blsr`).
</details>

### E3 — Zero-heap scratch
Write a function that fills a `std::pmr::vector<int>` with up to 1000 values on a
stack buffer and provably never heap-allocates.

<details><summary>Answer</summary>

```cpp
void work() {
    std::byte buf[1000 * sizeof(int) + 64];
    std::pmr::monotonic_buffer_resource pool{buf, sizeof buf,
                                             std::pmr::null_memory_resource()};   // no spill to heap
    std::pmr::vector<int> v{&pool};
    v.reserve(1000);                    // from buf
    for (int i = 0; i < 1000; ++i) v.push_back(i);
    // ... use v ...
}   // buf reclaimed automatically; any overflow would have thrown std::bad_alloc
```
</details>

### E4 — VWAP with one pass each
Given `std::vector<double> px, qty` (equal length), compute VWAP = Σ(px·qty) /
Σ(qty).

<details><summary>Answer</summary>

```cpp
double num = std::transform_reduce(px.begin(), px.end(), qty.begin(), 0.0);
double den = std::reduce(qty.begin(), qty.end(), 0.0);
double vwap = den != 0.0 ? num / den : 0.0;
```
(For a reproducible result use `std::inner_product` / `std::accumulate` — fixed
addition order.)
</details>

### E5 — `variant` message dispatch
`using Msg = std::variant<Add, Cancel, Trade>;`. Write `apply(const Msg&,
Book&)` that dispatches exhaustively with a compile error if a new alternative is
added and unhandled.

<details><summary>Answer</summary>

```cpp
template <class... Ts> struct Overloaded : Ts... { using Ts::operator()...; };

void apply(const Msg& m, Book& b) {
    std::visit(Overloaded{
        [&](const Add& a)    { b.add(a); },
        [&](const Cancel& c) { b.cancel(c); },
        [&](const Trade& t)  { b.trade(t); },
    }, m);
}
// add a 4th alternative to Msg and there's no matching operator() -> std::visit fails to compile
// (do NOT add a [](auto&){} catch-all — that would silence the check)
```
</details>

---

## Part F — Design / discussion

1. **The 27x:** `std::list<int>` and `std::vector<int>` both iterate O(n).
   Explain, with cache misses, why `examples/11` measures list at ~18 ms and
   vector at ~0.67 ms for 1M elements.

2. **map vs sorted vector:** both O(log n) for lookup; `examples/02` measures the
   sorted vector 3x faster. Why?

3. **`std::function` on the hot path:** name its two costs and give three
   alternatives for storing a callable that's invoked millions of times.

4. **When is the STL right in HFT?** describe the three system tiers and which
   STL usage is acceptable in each.

5. **Preallocation:** why is "reserve/preallocate everything at startup" a design
   principle, not a micro-optimization, for a hot path?

<details><summary>Answers (sketch)</summary>

1. `vector`: contiguous → hardware prefetch + SIMD, ~1 miss per 16 ints, adjacent
   iterations independent. `list`: `++it` follows `next` to an unpredictable heap
   address → a cache miss (~200+ cycles) almost every element, no prefetch, no
   vectorization, dependent loads serialize. ~27x.
2. Same comparison count; the vector's binary search touches elements sharing a
   couple of cache lines (especially the last steps) → ~2–3 misses, while the
   tree's ~18 compared nodes are separate heap allocations → ~18 independent
   misses.
3. Costs: (a) type-erased indirect call that can't inline (~3–5 ns), (b) a heap
   allocation on construction when the closure exceeds the small buffer (~16 B).
   Alternatives: template the calling code on the callable type; a plain function
   pointer + a `void* ctx`; a `std::variant<...>` of concrete callables +
   `std::visit`; `function_ref` (non-owning, 16 B, no alloc).
4. Control plane (startup/config/admin) — full STL, allocation fine. Warm path
   (per-second housekeeping) — STL with `reserve`, no per-op allocation. Hot path
   (packet→order, ~1 µs) — almost no STL containers; flat preallocated
   structures; keep only the iterator/algorithm model and vocabulary types.
5. Any steady-state allocation risks a page fault, an allocator lock, an `mmap`,
   or an O(n) container move/rehash — each a latency spike in the tail. If all
   capacity is reserved at startup, steady state does none of that and the
   latency distribution has almost no tail. That property *is* the design goal.
</details>

---

## Part G — Challenge

**Build a minimal flat order book** (types + operations only, no networking):

- `std::array<Level, 4001> bids, asks;` indexed by `tick - ref + 2000`, where
  `Level { std::uint64_t qty; }`.
- `best_bid_idx`, `best_ask_idx`.
- `void apply_add(Side, int tick, std::uint64_t qty)` — O(1), no allocation, no
  search; update `best_*_idx` if this improves the top.
- `void apply_cancel(Side, int tick, std::uint64_t qty)` — O(1); if it empties
  the current best, advance `best_*_idx` to the next non-empty level.
- `std::optional<std::pair<int,std::uint64_t>> best_bid() const` — O(1).

Then write a benchmark (`examples/11` discipline: `-O2`, warm-up, min of reps,
sink) that applies 10,000,000 random add/cancel ops and reports ns/op. Compare
against the same workload on a `std::map<int, std::uint64_t>`.

<details><summary>Hints</summary>

- Re-centering (when a price moves outside ±2000 ticks) is rare — handle it off
  the hot path (shift the arrays or rebuild), don't branch for it every op.
- `best_bid_idx` advances downward past empty levels only on a cancel that
  empties the top; an add above the current best just sets it directly.
- The `std::map` version will be ~10–50x slower and will allocate per new price —
  that's the lesson.
- Sink: accumulate `best_bid()->second` into a `volatile` so the optimizer keeps
  the work.
</details>

---

## Next
→ [`../20-ALGORITHMS-DSA/00-README.md`](../20-ALGORITHMS-DSA/00-README.md)
