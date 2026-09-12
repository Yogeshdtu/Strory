# 06 — Linked lists

## Prerequisites
- [`01-complexity-analysis.md`](01-complexity-analysis.md)
- Folder 12 (pointers), folder 17 (RAII / ownership), folder 19 file 04 (`std::list`)

## Yeh topic abhi kyun
Linked list interview ka staple hai (reverse, cycle detect, merge, middle) aur
pointer manipulation ka best exercise. Par is folder ka theme yaad rakho:
**linked list aksar galat choice hai** — `examples/03_linked_list.cpp` mein 5M
nodes traverse karna `std::vector` se **~40× slow** hai. Kab genuinely chahiye,
aur kya HFT use karta uski jagah — dono.

---

## The shapes

```
Singly:   [d|•]->[d|•]->[d|•]->null              one `next` per node
Doubly:   null<-[•|d|•]<->[•|d|•]<->[•|d|•]->null  `prev` and `next`
Circular: [d|•]->[d|•]->[d|•]--+                   tail->next points back to head
             ^-----------------+
```

```cpp
struct Node { int value; Node* next; };            // singly
struct DNode { int value; DNode* prev; DNode* next; };   // doubly
```

- **Singly**: smallest overhead (1 pointer). Forward only. `push_front`,
  `insert_after`, `erase_after` are `O(1)`.
- **Doubly**: 2 pointers per node. Bidirectional. `erase(node)` is `O(1)` given
  the node (relink `prev`/`next`); this is what `std::list` provides.
- **Circular**: no null end; useful for round-robin. A **sentinel/dummy head**
  node removes most edge cases (empty list, head insert/delete) — `std::list`
  uses one.

`examples/03_linked_list.cpp` implements an owning singly list with a `tail_`
pointer (so `push_back` is `O(1)`) and a size counter.

---

## The classic operations

### Reverse in place — `O(n)` time, `O(1)` space
```cpp
Node* reverse(Node* head) {
    Node* prev = nullptr;
    while (head) {
        Node* nxt = head->next;   // save
        head->next = prev;        // flip
        prev = head;              // advance prev
        head = nxt;               // advance head
    }
    return prev;                  // new head
}
```
Three-pointer dance. The recursive version is `O(n)` **stack** — avoid for long
lists.

### Cycle detection — Floyd's tortoise and hare, `O(1)` space
```cpp
bool hasCycle(Node* head) {
    Node* slow = head; Node* fast = head;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) return true;   // fast laps slow inside the loop
    }
    return false;
}
```
To find the **start** of the cycle: after they meet, reset one pointer to `head`,
advance both one step at a time; they meet at the cycle entry. (Provable with
modular arithmetic on the meeting point.)

### Middle of the list — same fast/slow
`slow` moves 1, `fast` moves 2; when `fast` hits the end, `slow` is at the middle.
One pass, no length precompute.

### Merge two sorted lists — `O(n + m)`, splice, no allocation
Two pointers; repeatedly attach the smaller head to the result tail. Pure
relinking — this is `list::merge` and the merge step of `list::sort`.

### Detect intersection of two lists
Walk both to their ends, compare tail identity; if equal, align by the length
difference and step together to the first shared node. `O(n + m)`, `O(1)`.

---

## Why linked lists usually lose

`examples/03_linked_list.cpp` (n = 5,000,000, `-O2`, measured):

| operation | time |
|---|---|
| sum a 5M-node singly list | **~70 ms** |
| sum a 5M-int `std::vector` | **~1.7 ms** |
| ratio | **~40× slower** |

Reasons (folder 19 files 04, 25):
1. **A cache miss per node.** `head = head->next` dereferences an unpredictable
   heap address → the prefetcher can't help → ~100–300 cycle stall almost every
   step.
2. **No vectorization.** A pointer-chasing loop can't be SIMD'd; the dependent
   load serializes iterations.
3. **Memory bloat.** `Node{int, Node*}` is 16 bytes (8 padding) for 4 bytes of
   payload → 4× the footprint → 4× the cache pressure.
4. **Allocation cost.** Building the list is `N` separate `new` calls
   (`examples/03` build is dominated by this).

Even "insert in the middle a lot" — the supposed linked-list win — usually loses
to `std::vector` for `n` up to thousands: the vector's `memmove` is
cache-friendly and there's no per-node allocation (folder 19 file 04).

---

## When a linked list is genuinely right

All three must hold:
1. You need **`O(1)` splice / erase** at arbitrary positions **and you already
   hold the node** (no search).
2. You need **iterator / pointer stability** — inserting/erasing elsewhere must
   not invalidate references you hold.
3. You **rarely traverse** the whole thing (traversal is where the cache cost
   lands).

Examples: an LRU cache's recency chain (move-to-front on access, given the node),
a free list, an intrusive "active timers" list.

---

## The HFT answer: intrusive list over a contiguous arena

Keep the `O(1)` splice **and** cache locality by putting the nodes in a
`std::vector` (or a fixed slab) and using **indices** instead of pointers:

```cpp
struct Slot { Order data; std::uint32_t next; std::uint32_t prev; };
std::vector<Slot> pool;                 // contiguous -- nodes are cache-adjacent
std::uint32_t head = NIL, freeHead = 0; // "pointers" are indices into `pool`
```

- Splice/erase = a few index writes → `O(1)`, no allocation.
- Traversal walks `pool[i].next` — still index-chasing, but the slots are
  contiguous and often prefetchable, and there's no per-node `malloc`.
- "Intrusive" = the `next`/`prev` links live *inside* the payload struct, so no
  separate node allocation and the object can be on multiple lists at once.

This is the standard pattern for order queues at a price level, timer wheels, and
pool free-lists in a matching engine (folders 39–41).

---

## Andar kya hota hai

- `head->next` is a load with an address the CPU only learns *after* the previous
  load completes → a **dependency chain**; out-of-order execution can't hide it.
  Contrast a `vector` loop where `&v[i+1]` is known immediately → the prefetcher
  streams ahead and multiple iterations overlap.
- `new Node` goes through the general allocator (folder 14) — a few hundred ns
  worst case, and the returned blocks are wherever the allocator's free list had
  space → scattered.
- `std::list` adds an allocator-header per node on top of `prev`+`next`+payload;
  libstdc++'s `_List_node` is ~`24 + sizeof(T)` bytes.
- The intrusive-arena version keeps the links but the slots are one allocation →
  `pool[i]` and `pool[i+1]` share cache lines for small slots, and `pool.data()`
  can be `madvise`/prefetched.

> **HFT relevance:** `std::list` / `std::forward_list` are effectively banned on
> the hot path — a cache miss per element is the opposite of low latency (folder
> 19 file 04). Where the *algorithm* wants a linked list (LRU chain, order queue
> at a level, free list, timer list), HFT uses an **intrusive doubly-linked list
> over a preallocated arena** with `uint32` indices as links: `O(1)` splice/erase
> (the reason to want a list), contiguous storage (kills the cache cost), zero
> allocation in steady state. The classic interview ops (reverse, cycle detect,
> merge) are pointer-manipulation practice, not production patterns.

---

## Hands-on

```bash
./build.ps1 fast 20-ALGORITHMS-DSA/examples/03_linked_list.cpp
```

Implement: reverse (iterative), Floyd cycle detect + find cycle start, middle
node, merge two sorted lists. Then rebuild the "sum" benchmark with an
**index-based arena list** (`std::vector<Slot>`) and see how much of the 40× gap
closes.

---

## ⚠️ Traps

### Trap 1 — losing the rest of the list during reverse
```cpp
head->next = prev;  head = head->next;   // ⚠️ head->next was just set to prev -> you go backwards.
// Save `nxt = head->next` BEFORE flipping.
```

### Trap 2 — recursive reverse / traversal on a long list
```cpp
Node* rev(Node* n) { if (!n || !n->next) return n; Node* h = rev(n->next); ... }   // ⚠️ O(n) stack -> overflow at ~100k+
```

### Trap 3 — `fast->next->next` without checking `fast->next`
```cpp
while (fast) { fast = fast->next->next; }   // ⚠️ null deref when fast->next is null. Condition: while (fast && fast->next)
```

### Trap 4 — memory leak / double free in a hand-rolled list
```cpp
// erase a node -> you must delete it. clear() must walk-and-delete. Copying the list needs a deep copy or = delete.
```

### Trap 5 — reaching for `std::list` "because I insert a lot"
```cpp
// For n up to thousands, std::vector.insert (a memmove) beats std::list.insert (node malloc + cache-cold relink). Measure.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Linked lists are cache-friendly" | The opposite — a likely cache miss per `->next` |
| "`std::list` is faster for middle inserts" | Only with iterator held + large n + rare traversal; usually `vector` wins |
| "Reverse needs extra space" | Iterative 3-pointer reverse is `O(1)` space |
| "Cycle detection needs a visited set" | Floyd's tortoise/hare is `O(1)` space |
| "HFT uses linked lists for order queues" | It uses **intrusive lists over an arena** (index links, contiguous) |

---

## Exercises

1. **Reverse:** reverse a singly list iteratively. Dry-run on `1->2->3`.

   <details><summary>Answer</summary>

   `prev=null`. h=1: nxt=2, 1->null, prev=1, h=2. h=2: nxt=3, 2->1, prev=2, h=3.
   h=3: nxt=null, 3->2, prev=3, h=null. Return prev=3 → `3->2->1`.
   </details>

2. **Cycle start:** prove that resetting one pointer to `head` after the
   tortoise/hare meet, then stepping both by 1, lands them at the cycle entry.

   <details><summary>Answer</summary>

   Let the non-cycle prefix be `μ`, cycle length `λ`, meeting point `k` steps
   into the cycle. Slow travelled `μ + k`; fast travelled `2(μ + k)` and is an
   integer number of laps ahead: `2(μ+k) - (μ+k) = μ + k ≡ 0 (mod λ)`, so
   `μ ≡ -k (mod λ)`. From `head`, `μ` steps reach the entry; from the meeting
   point, `μ` steps advance `k + μ ≡ 0` → also the entry. They coincide.
   </details>

3. **Merge sorted:** merge `1->3->5` and `2->4->6` by relinking (no new nodes).

   <details><summary>Answer</summary>

   Dummy head; pointer `t`. Compare heads, attach smaller, advance that list and
   `t`. `1,2,3,4,5,6`. Attach the remaining tail at the end. `O(n+m)`, `O(1)`.
   </details>

4. **Arena list:** design `struct Slot { T data; uint32_t next, prev; };` +
   `std::vector<Slot> pool` + a free list. How do allocate / free / splice work?

   <details><summary>Answer</summary>

   `allocate`: pop `freeHead` (`freeHead = pool[freeHead].next`), or `push_back`
   a new slot. `free(i)`: `pool[i].next = freeHead; freeHead = i;`. `splice`:
   rewrite `next`/`prev` indices of the 3–4 involved slots. All `O(1)`, no
   `malloc`, slots contiguous.
   </details>

5. **When list wins:** give one concrete scenario where `std::list` genuinely
   beats `std::vector`, and one where people *think* it does but it doesn't.

   <details><summary>Answer</summary>

   Wins: you hold `T*` to many elements elsewhere and frequently erase arbitrary
   ones — `list::erase` doesn't move/invalidate other elements. Doesn't win:
   "I insert in the middle a lot" for n in the thousands — `vector`'s `memmove`
   + no allocation beats `list`'s node malloc + cache-cold relink.
   </details>

---

## Interview questions

1. Singly vs doubly vs circular — overhead aur kaunsa op `O(1)`?
2. Iterative reverse — 3 pointers, kya save karna zaroori?
3. Floyd's cycle detection — kaise kaam karta, cycle start kaise?
4. Linked list traversal `vector` se 40× slow kyun?
5. Linked list genuinely kab chahiye (teen conditions)?
6. HFT order queue ke liye kya use hota (intrusive arena list)?

---

## Next
→ [`07-stacks-and-queues.md`](07-stacks-and-queues.md)
