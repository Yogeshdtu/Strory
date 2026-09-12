# 07 — Stacks and queues

## Prerequisites
- [`06-linked-lists.md`](06-linked-lists.md)
- Folder 19 file 07 (container adapters), folder 09 (arrays)

## Yeh topic abhi kyun
Stack (LIFO) aur queue (FIFO) sabse simple ADTs hain — par unke **implementation
choices** (array vs linked, fixed vs growing, circular buffer) HFT ke liye
central hain. Aur inpe bane algorithms (monotonic stack, BFS queue, expression
parsing) interview mein aksar aate.

---

## Stack — LIFO

Operations: `push`, `pop`, `top`, `empty`, `size`. All `O(1)`.

```cpp
// array-backed stack (the good default) -- just a vector with a restricted interface
template <class T>
class Stack {
    std::vector<T> data_;
public:
    void push(const T& x) { data_.push_back(x); }
    void pop()            { data_.pop_back(); }        // UB if empty -- caller checks
    T&   top()            { return data_.back(); }
    bool empty() const    { return data_.empty(); }
    std::size_t size() const { return data_.size(); }
};
```

`std::stack<T>` is exactly this adapter over a `std::deque` by default (folder 19
file 07). Use `std::stack<T, std::vector<T>>` for contiguous storage. A linked
stack (`push_front`/`pop_front` on a list) works but adds a node allocation per
push and a cache miss per access — no reason to.

### Algorithms on a stack
- **Balanced brackets**: push openers, pop+match on closers, empty at end.
- **Expression evaluation**: shunting-yard (operator stack + output queue) →
  RPN → evaluate with a value stack.
- **Monotonic stack**: keep the stack sorted (increasing or decreasing) by
  popping violators before each push. Solves "next greater element", "largest
  rectangle in histogram", "daily temperatures" in `O(n)` total (each element
  pushed and popped once).
- **DFS iterative** (`examples/06_graph_algorithms.cpp`): an explicit stack
  replaces the call stack.
- **Undo/redo**, function call frames, backtracking.

---

## Queue — FIFO

Operations: `push`/`enqueue` (back), `pop`/`dequeue` (front), `front`, `back`,
`empty`, `size`. All `O(1)`.

```cpp
std::queue<int> q;         // std::deque-backed by default
q.push(1); q.push(2);
q.front();                 // 1
q.pop();                   // removes 1
```

A `std::vector` **cannot** back a queue (no `pop_front` — `erase(begin())` is
`O(n)`). `std::deque` and `std::list` can. For a *bounded* FIFO, the right answer
is a **circular buffer**.

### Circular buffer (ring buffer) — the important one

A fixed-size array with `head` and `tail` indices that wrap around.

```cpp
template <class T, std::size_t CAP>       // CAP a power of two -> wrap is a mask
class Ring {
    std::array<T, CAP> buf_{};
    std::size_t head_ = 0;                 // next read
    std::size_t tail_ = 0;                 // next write
    std::size_t count_ = 0;
public:
    bool full()  const { return count_ == CAP; }
    bool empty() const { return count_ == 0; }
    bool push(const T& x) {
        if (full()) return false;
        buf_[tail_] = x;
        tail_ = (tail_ + 1) & (CAP - 1);   // wrap: no % , CAP is a power of two
        ++count_;
        return true;
    }
    bool pop(T& out) {
        if (empty()) return false;
        out = buf_[head_];
        head_ = (head_ + 1) & (CAP - 1);
        --count_;
        return true;
    }
};
```

- **No allocation ever** — the storage is one fixed array.
- **`O(1)`, branch-light, cache-resident** — head/tail chase a small contiguous
  region.
- **Bounded** — `push` fails (or overwrites the oldest, for a "latest N" buffer)
  when full. Bounding is a feature: back-pressure instead of unbounded memory
  growth.
- Power-of-two capacity → index wrap is `& (CAP-1)` (1 cycle) instead of `%`
  (~20 cycles). `std::bit_ceil` to round a requested size up (folder 19 file 22).

This is the backbone of SPSC/MPSC queues, log buffers, and market-data intake
(folders 41, 44). "Distinguish full from empty": either keep a `count_`, or
leave one slot unused, or use free-running 64-bit head/tail and mask on access
(the Disruptor approach).

---

## Deque — double-ended

`push_front`, `push_back`, `pop_front`, `pop_back`, `[i]` all `O(1)`.
`std::deque` (folder 19 file 04) is a chunked array — not contiguous. Backs
`std::stack` and `std::queue` by default. Use it when you genuinely need cheap
operations at *both* ends and don't need `data()`.

`std::deque` also powers the **sliding-window maximum** algorithm: a
`deque<index>` kept decreasing; push each new index (popping smaller values off
the back), pop the front when it falls out of the window → `O(n)` total, `O(1)`
per step.

---

## Andar kya hota hai

- `std::stack`/`std::queue` are `struct { Container c; }` — the adapter compiles
  away; `push`/`pop` forward to `c.push_back`/`c.pop_back`/`c.pop_front` and
  inline (folder 19 file 07).
- `std::deque`'s `push_back` allocates a new 512-byte chunk when the tail chunk
  fills; `push_front` similarly at the head. So `std::queue<T>` touches the
  allocator periodically — fine for throughput, not for a jitter budget.
- The ring buffer's `push`/`pop`: an index compare, a store/load, a masked
  increment. ~5–8 instructions, no branch mispredict (the `full`/`empty` check
  is predictable in steady state), no cache miss (small contiguous `buf_`).
- Power-of-two `& (CAP-1)` vs `% CAP`: the mask is a single `and`; `%` by a
  runtime value is a `div` (~20–40 cycles). If `CAP` is a compile-time constant
  power of two, even `%` is optimized to a mask — but people write `CAP` as a
  runtime field, so use the mask explicitly.

> **HFT relevance:** the **circular buffer is the queue** in HFT — a preallocated
> `std::array` (or slab) with power-of-two capacity, masked head/tail indices,
> cache-line-aligned and padded to avoid false sharing between producer and
> consumer (folders 28, 41). It never allocates, never blocks, and its latency
> distribution has no tail. `std::queue`/`std::deque` allocate chunks and are
> avoided on the hot path. Monotonic stacks / sliding-window-max show up in
> signal computation (rolling max/min over the last N ticks in `O(1)`
> amortized). `std::stack` is fine anywhere it's just documenting LIFO intent.

---

## Hands-on

```bash
./build.ps1 20-ALGORITHMS-DSA/examples/06_graph_algorithms.cpp   # BFS queue, iterative-DFS stack
```

Write: a bracket-matcher with `std::stack`, a fixed-capacity `Ring<int, 1024>`
(power-of-two, masked wrap) and confirm it does zero allocations, and
sliding-window-maximum with a `std::deque<std::size_t>` of indices.

---

## ⚠️ Traps

### Trap 1 — `pop()` returns void
```cpp
int x = s.pop();   // ❌ std::stack::pop() is void. auto x = s.top(); s.pop();
```

### Trap 2 — `std::vector` as a queue
```cpp
std::queue<int, std::vector<int>> q;   // ❌ vector has no pop_front. deque or list, or a ring buffer
```

### Trap 3 — ring buffer: can't tell full from empty
```cpp
// head == tail means BOTH empty and full if you don't track count / leave a gap. Pick one convention.
```

### Trap 4 — `% CAP` on a runtime capacity in the hot path
```cpp
tail = (tail + 1) % cap;   // ⚠️ ~20-40 cycle div. Make cap a power of two -> & (cap - 1)
```

### Trap 5 — unbounded `std::queue` as an intake buffer
```cpp
// a slow consumer + fast producer -> the deque grows without limit -> OOM / GC-like stalls.
// A bounded ring gives back-pressure instead.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::stack` adds overhead" | It's a thin adapter — inlines to the underlying container's ops |
| "A queue can be a `std::vector`" | No `O(1)` `pop_front` — use `deque`, `list`, or a ring buffer |
| "Ring buffer needs `%` to wrap" | Power-of-two capacity → `& (cap-1)`, one instruction |
| "`std::queue` is fine for a hot-path intake buffer" | It allocates and is unbounded — use a bounded ring |
| "Monotonic stack is `O(n²)`" | Each element is pushed and popped at most once → `O(n)` total |

---

## Exercises

1. **Two stacks → queue:** implement a FIFO queue using two stacks. Amortized
   cost per operation?

   <details><summary>Answer</summary>

   `in` and `out` stacks. `push` → `in.push`. `pop` → if `out` empty, pour all of
   `in` into `out` (reversing order), then `out.pop`. Each element is moved
   between stacks at most once → **amortized `O(1)`** (worst-case `O(n)` on the
   pour).
   </details>

2. **Monotonic stack:** "next greater element" for each entry of `[2,1,2,4,3]`.
   Walk through the stack.

   <details><summary>Answer</summary>

   Keep a decreasing stack of indices. i0(2): stack[0]. i1(1): 1<2, push →
   [0,1]. i2(2): 2>1 pop1 (ans[1]=2), 2==2 push → [0,2]. i3(4): pop2 (ans[2]=4),
   pop0 (ans[0]=4), push → [3]. i4(3): push → [3,4]. Leftover 3,4 → -1.
   Result: `[4,2,4,-1,-1]`. `O(n)`.
   </details>

3. **Ring capacity:** you want a ring that holds ≥ 1000 items. What capacity, and
   why, and what's the wrap expression?

   <details><summary>Answer</summary>

   `std::bit_ceil(1000) = 1024`. Power of two → wrap is `idx & 1023` (one `and`
   instruction) instead of `idx % 1024` (which the compiler would also optimize,
   but the mask is explicit and works for a runtime capacity too).
   </details>

4. **Full vs empty:** three ways a ring buffer distinguishes full from empty.

   <details><summary>Answer</summary>

   (a) a separate `count_` field; (b) leave one slot always unused → `full` when
   `(tail+1)&mask == head`; (c) free-running 64-bit `head`/`tail` counters
   (never wrapped), `size = tail - head`, mask only when indexing.
   </details>

5. **Sliding window max:** why is the deque-of-indices approach `O(n)` and not
   `O(nk)`?

   <details><summary>Answer</summary>

   Each index is pushed to the back exactly once and removed exactly once (either
   popped from the back by a larger later value, or popped from the front when it
   exits the window). Total push+pop work is `2n` → `O(n)`.
   </details>

---

## Interview questions

1. Stack/queue ke `O(1)` ops — array vs linked implementation trade-off?
2. `std::vector` queue kyun nahi ban sakta?
3. Circular buffer — full vs empty kaise distinguish, power-of-two kyun?
4. Monotonic stack `O(n)` kaise (har element push+pop ek baar)?
5. Two stacks se queue — amortized `O(1)` kaise?
6. HFT mein `std::queue` ki jagah kya, kyun (ring buffer, back-pressure)?

---

## Next
→ [`08-hash-tables.md`](08-hash-tables.md)
