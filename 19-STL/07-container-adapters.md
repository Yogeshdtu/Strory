# 07 — Container adapters: `stack`, `queue`, `priority_queue`

## Prerequisites
- [`04-deque-list-forward-list.md`](04-deque-list-forward-list.md), [`02-vector-deep.md`](02-vector-deep.md)
- Folder 20 preview (heaps)

## Yeh topic abhi kyun
Ye "containers" nahi hain — ye **adapters** hain: kisi existing container
(`deque`/`vector`/`list`) ke upar ek **restricted interface** chadhate hain.
`stack` sirf LIFO, `queue` sirf FIFO, `priority_queue` hamesha max nikaalta hai.
Chhote hain, par HFT event loops aur scheduling mein `priority_queue` bahut aata.

---

## `std::stack<T>` — LIFO

```cpp
#include <stack>
std::stack<int> s;                       // default underlying: std::deque<int>
s.push(1); s.push(2); s.push(3);
s.top();                                 // 3   -- reference to the newest
s.pop();                                 // removes 3, returns NOTHING (void)
s.size(); s.empty();
```

- Interface is **only**: `push`, `pop`, `top`, `size`, `empty`, `emplace`. No
  iteration, no `[]`, no `begin()`. That's the point — enforce LIFO.
- `pop()` returns `void` (exception-safety history — a value-returning pop
  couldn't be exception-safe pre-move). Do `auto x = s.top(); s.pop();`.
- Underlying container configurable: `std::stack<int, std::vector<int>>`. Needs
  `back()`, `push_back()`, `pop_back()` → `vector`, `deque`, or `list`.

## `std::queue<T>` — FIFO

```cpp
#include <queue>
std::queue<int> q;                       // default underlying: std::deque<int>
q.push(1); q.push(2); q.push(3);
q.front();                               // 1  -- oldest
q.back();                                // 3  -- newest
q.pop();                                 // removes 1 (the front)
```

- Interface: `push`, `pop`, `front`, `back`, `size`, `empty`, `emplace`.
- Needs `front()`, `back()`, `push_back()`, `pop_front()` from the underlying
  container → `deque` (default) or `list`. **Not `vector`** (no `pop_front`).
- For a real bounded FIFO in perf code you'd use a **ring buffer**, not
  `std::queue` (which allocates deque chunks).

## `std::priority_queue<T>` — a binary heap

```cpp
#include <queue>
std::priority_queue<int> pq;             // MAX-heap by default: top() is the LARGEST
pq.push(3); pq.push(1); pq.push(4); pq.push(1);
pq.top();                                // 4
pq.pop();                                // removes 4; top() now 3
// underlying: std::vector<int>; ordering: std::less<int>  -> max on top

// MIN-heap:
std::priority_queue<int, std::vector<int>, std::greater<int>> minpq;
minpq.push(3); minpq.push(1); minpq.push(4);
minpq.top();                             // 1

// heap of tasks by time:
struct Event { std::int64_t ts; int id; };
auto cmp = [](const Event& a, const Event& b){ return a.ts > b.ts; };   // '>' -> min-ts on top
std::priority_queue<Event, std::vector<Event>, decltype(cmp)> timeline(cmp);
```

- `push` — O(log n) (append to the vector, sift up). `pop` — O(log n) (swap
  root with last, shrink, sift down). `top` — O(1). **No iteration**, no way to
  inspect anything but `top()`, **no** `erase`/decrease-key (unlike a hand-rolled
  heap or `std::multiset`).
- The comparator convention is inverted-feeling: `std::less` → **max**-heap
  (`top()` is the element that is not less than any other). Pass `std::greater`
  for a min-heap.
- Built on `std::make_heap` / `push_heap` / `pop_heap` over a `std::vector` — so
  it's **contiguous** and cache-friendly, unlike a tree.

---

## When to use what

| Need | Use |
|---|---|
| LIFO, no need to iterate | `std::stack` (or just `std::vector` with `push_back`/`pop_back`) |
| FIFO, unbounded, not latency-critical | `std::queue` |
| FIFO, bounded, hot path | hand-rolled **ring buffer** (`std::array` + head/tail), not `std::queue` |
| "Give me the smallest/largest pending item repeatedly" | `std::priority_queue` |
| ...plus erase-arbitrary / decrease-key / iterate | `std::multiset`, or a hand-rolled indexed heap |

Honestly, `std::stack` and `std::queue` earn their keep mostly as
**documentation** — they tell the reader "this is used LIFO/FIFO, nothing else."
Performance-wise a bare `std::vector` (stack) or ring buffer (queue) is what hot
code uses.

---

## Andar kya hota hai

- `std::stack<T>` is literally `struct { Container c; }` with `top()` → `c.back()`,
  `push` → `c.push_back`, `pop` → `c.pop_back`. Zero runtime overhead over the
  underlying container — the adapter compiles away.
- `std::priority_queue` holds a `std::vector` kept in **heap order** (not sorted!
  — `arr[0]` is the max, `arr[2i+1]`/`arr[2i+2]` are its children, each ≤ parent).
  `push` = `c.push_back(x); std::push_heap(c.begin(), c.end(), comp);` — sift the
  new leaf up, O(log n) swaps. `pop` = `std::pop_heap` (moves root to the back,
  restores heap on `[begin, end-1)`) then `c.pop_back()`.
- Because the heap is a `vector`, sift-up/down walk parent↔child indices that are
  **near each other in memory** for the top few levels → far better cache
  behaviour than a pointer-linked tree or `std::multiset` for the same job.

> **HFT relevance:** `std::priority_queue<Event, vector, cmp>` is a common
> **event-loop / simulation timeline** structure — pop the next-earliest event,
> process it, push any it generates. Contiguous storage makes it fast. Its limits
> bite in practice: you can't cancel an event already in the queue (no erase) and
> you can't peek past `top()`. Real engines either use "lazy deletion" (mark
> cancelled, skip on pop) or a hand-rolled **indexed binary heap** (an array
> heap + a `pos[id]` map) that supports O(log n) erase and decrease-key. `std::
> queue`/`std::stack` are avoided on hot paths in favour of preallocated ring
> buffers that never touch the allocator.

---

## Hands-on

```cpp
// timeline.cpp -- min-heap event loop
#include <queue>
#include <vector>
#include <cstdio>

struct Event { long ts; int id; };

int main() {
    auto later = [](const Event& a, const Event& b){ return a.ts > b.ts; };   // earliest ts on top
    std::priority_queue<Event, std::vector<Event>, decltype(later)> pq(later);

    pq.push({50, 1}); pq.push({10, 2}); pq.push({30, 3}); pq.push({20, 4});

    while (!pq.empty()) {
        Event e = pq.top(); pq.pop();
        std::printf("t=%ld  event %d\n", e.ts, e.id);       // 10, 20, 30, 50 in order
        if (e.id == 2) pq.push({45, 99});                   // events can schedule new events
    }
}
```
```bash
g++ -std=c++20 -O2 timeline.cpp -o timeline && ./timeline
```

---

## ⚠️ Traps

### Trap 1 — `pop()` returns void
```cpp
int x = s.pop();   // ❌ pop() is void. auto x = s.top(); s.pop();
```

### Trap 2 — comparator direction on `priority_queue`
```cpp
std::priority_queue<int> pq;                 // this is a MAX-heap (top() = largest), surprising to many
std::priority_queue<int, std::vector<int>, std::greater<>> minpq;   // min-heap needs std::greater
```

### Trap 3 — expecting to iterate / erase
```cpp
for (auto x : pq) ...          // ❌ no begin()/end(). No way to erase a non-top element either
```

### Trap 4 — `std::queue` on `std::vector`
```cpp
std::queue<int, std::vector<int>> q;   // ❌ vector has no pop_front(). Use deque (default) or list
```

### Trap 5 — `priority_queue` is "sorted"
```cpp
// The underlying vector is in HEAP order, not sorted. Only top() is guaranteed.
// To drain in order: pop() repeatedly (that's heapsort, O(n log n))
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`s.pop()` gives me the element" | Returns `void`; read `top()`/`front()` first |
| "`std::priority_queue<int>` is a min-heap" | Max-heap by default — `top()` is the largest; use `std::greater` for min |
| "The `priority_queue`'s array is sorted" | Heap-ordered — only `top()`/`arr[0]` is meaningful |
| "I can remove an arbitrary element from a `priority_queue`" | No — only `pop()` the top. Use `multiset` or an indexed heap |
| "`std::queue` is a good bounded FIFO for hot code" | It allocates (deque chunks) — use a ring buffer on the hot path |

---

## Exercises

1. **k largest:** given `std::vector<int> v` and `k`, find the `k` largest values
   using a `priority_queue`. Min-heap or max-heap? Complexity?

   <details><summary>Answer</summary>

   Keep a **min-heap of size k** (`priority_queue<int, vector<int>, greater<>>`):
   push each element; if `size() > k`, `pop()` (drops the current smallest). End:
   the heap holds the k largest. O(n log k), O(k) space. (A max-heap of all n +
   k pops is O(n + k log n).)
   </details>

2. **Custom priority:** a `priority_queue` of `Order{price, qty}` where the
   *highest price* has priority; ties broken by *lowest qty* first. Write the
   comparator.

   <details><summary>Answer</summary>

   `auto cmp = [](const Order& a, const Order& b){ if (a.price != b.price) return
   a.price < b.price; return a.qty > b.qty; };` — remember `priority_queue` puts
   the element for which `cmp(x, other)` is *false* on top, so "less" ⇒ lower
   priority. Higher price → not-less → on top; among equal prices, larger qty →
   "less" → lower priority, so smaller qty rises.
   </details>

3. **Cancel an event:** your `priority_queue` timeline needs to cancel event `id`
   before it fires. `priority_queue` can't erase. Two approaches?

   <details><summary>Answer</summary>

   (a) **Lazy deletion**: keep a `std::unordered_set<int> cancelled`; on `pop()`,
   if `cancelled.count(e.id)` skip it. Simple, wastes space until the event would
   have fired. (b) **Indexed heap**: hand-roll an array heap plus `pos[id] →
   heap index`; erase = swap with last, pop, sift — O(log n).
   </details>

4. **Adapter overhead:** does wrapping a `std::vector<int>` in `std::stack`
   make `push`/`pop` slower at `-O2`?

   <details><summary>Answer</summary>

   No. `std::stack` forwards `push`→`push_back`, `pop`→`pop_back`, `top`→`back`;
   the adapter is a thin `struct` and inlines to nothing. Same code as calling
   the vector directly.
   </details>

5. **Drain in order:** you have a `priority_queue` of 1000 ints and want them
   sorted ascending in a `vector`. Method and complexity?

   <details><summary>Answer</summary>

   If it's a min-heap: `while (!pq.empty()) { out.push_back(pq.top()); pq.pop(); }`
   → ascending, O(n log n) (this is heapsort). If max-heap: do the same and
   `std::reverse(out)`, or push into the vector and read back-to-front.
   </details>

---

## Interview questions

1. Adapter vs container — kya fark, adapter kya restrict karta?
2. `stack`/`queue`/`priority_queue` ke default underlying containers?
3. `pop()` void kyun return karta (history)?
4. `std::priority_queue<int>` max ya min heap? Min-heap kaise banate?
5. `priority_queue` andar — heap-ordered vector, `push`/`pop` O(log n) kaise?
6. Event cancel karna ho to `priority_queue` ke saath kya karte?

---

## Next
→ [`08-iterators.md`](08-iterators.md)
