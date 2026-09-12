# 08 — Intrusive order lists (fixing V2's regression)

## Prerequisites
- [`07-flat-array-book.md`](07-flat-array-book.md)
- [`06-measuring-v2.md`](06-measuring-v2.md) (yaad rakho: V2's linear-scan
  regression yahi lesson fix karta)

## Yeh topic abhi kyun
07 ne price-level lookup O(1) banaya. Ab **order-within-level** ka
06's linear-scan problem fix karna hai — permanently, na ki V2 jaisa
half-fix.

---

## Intrusive list kya hai

> **Intrusive linked list = ek list jahan "next"/"prev" pointers KHUD
> element ke andar hote (external node-wrapper nahi), aur elements ek
> ARENA (pre-allocated array) mein rehte, heap pe individually nahi.**

```cpp
struct OrderSlot {
    OrderId id;
    Qty qty;
    std::uint32_t prev, next;   // <- arena INDICES, pointers NAHI
    std::uint32_t level_idx;
    bool is_buy;
};

std::vector<OrderSlot> arena_;   // saare orders EK jagah, contiguous
```

**Do design choices jo yeh fast banate:**
1. **`std::vector<OrderSlot>` ek baar allocate hota** (construction pe,
   `max_orders` size) — koi per-order heap allocation kabhi nahi (V1's
   `std::list` ke against, jo har order ke liye ek naya heap node
   allocate karta).
2. **Links `uint32_t` INDICES hain, `OrderSlot*` pointers NAHI** — 8 bytes
   (64-bit pointer) ki jagah 4 bytes; aur arena_ agar kabhi resize/move
   ho (yahan nahi hota, fixed size hai), indices still valid rehte
   (pointers dangle ho jaate).

---

## Free list — allocation/deallocation bina `new`/`delete`

```cpp
std::vector<std::uint32_t> free_list_;   // available slot indices
std::size_t free_top_ = 0;

// "allocate":
const std::uint32_t slot = free_list_[--free_top_];

// "deallocate":
free_list_[free_top_++] = slot;
```

Yeh bilkul 36-LOW-LATENCY-CPP/06 (memory pools) ka pattern hai, order-book
context mein — ek fixed arena + intrusive free-list, `operator new`/
`delete` kabhi call nahi hota hot path pe.

---

## Insert (append to level's list)

```cpp
LevelV3& level = ...;
const std::uint32_t slot = free_list_[--free_top_];
arena_[slot] = OrderSlot{id, qty, level.tail, NULL_SLOT, idx, is_buy};
if (level.tail != NULL_SLOT) arena_[level.tail].next = slot;
else level.head = slot;
level.tail = slot;
```

Bilkul standard doubly-linked-list append — bas pointers ki jagah
array-indices manipulate ho rahe. **O(1), no allocation, no scan.**

---

## Remove (unlink from anywhere in the list) — YEH V2's problem ka fix hai

```cpp
void unlink(OrderSlot& order, LevelV3& level) {
    if (order.prev != NULL_SLOT) arena_[order.prev].next = order.next;
    else level.head = order.next;
    if (order.next != NULL_SLOT) arena_[order.next].prev = order.prev;
    else level.tail = order.prev;
}
```

**Isko call karne ke liye tumhe pehle se `order` (aur uska `level`) chahiye
— DIRECTLY, koi scan nahi.** Yeh `09-order-id-lookup.md` ka kaam hai:
order-id se seedha arena-slot tak O(1) pahunchna, taaki `unlink()` ko koi
"kaunsa element hai" search na karna pade.

```
V1: index_[id] -> list::iterator          -- O(1) direct, par per-node heap alloc
V2: index_[id] -> {price} -> LINEAR SCAN  -- O(level size), 06's regression
V3: index_[id] -> arena slot -> DIRECT     -- O(1), NO heap alloc, NO scan
```

V3 dono problems fix karta EK saath: **koi per-order heap allocation
(V1's weakness)**, aur **koi order-within-level scan (V2's weakness)**.

---

## Cache behavior — kyun yeh V1's `std::list` se bhi behtar hai

```
std::list<Order>:  har node ALAG heap allocation -- traversal ke waqt
                    memory mein RANDOM jagah jump karta (allocator ne
                    kahin bhi rakha ho sakta).

Intrusive arena:    saare orders EK contiguous std::vector mein.
                    Traversal (agar kabhi poori list scan karni pade)
                    memory mein RELATIVELY close hoti (arena locality) --
                    guaranteed sequential NAHI (order allocation-order pe
                    depend karta, list-order pe nahi), par std::list se
                    kahin behtar (32-CACHE ka principle).
```

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `prev`/`next` update karna bhoolna kisi ek side pe
`unlink()` mein DONO directions update karna zaroori hai (agar `order`
list ka head/tail hai, `level.head`/`level.tail` bhi update karna) — ek
missed update se list corrupt ho jaati (dangling reference to a freed
slot).

### Trap 2 — freed slot ko dobara use karna bina reset kiye
Agar `arena_[slot]` ke purane fields (jaise `prev`/`next`) reset nahi
hote naye order allocate karte waqt, stale data se subtle bugs aa sakte.
Is folder ka `add()` poora `OrderSlot{...}` struct fresh assign karta
(overwrite, na ki partial update) — is trap ko naturally avoid karta.

### Trap 3 — arena ko "unlimited" samajhna
`free_top_ == 0` (arena full) case handle karna zaroori hai — `add()`
`false` return karta agar koi free slot nahi bacha. 06-memory-pools
(36) ka "fixed cap = overflow policy chahiye" principle yahan bhi.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Intrusive list = `std::list` jaisa hi | `std::list` har node heap-allocate karta; intrusive ek arena mein pre-allocated slots reuse karta |
| Index-based links pointer-based se slow hote | Zyada compact (4 bytes vs 8), aur arena stability guarantee karte |
| `unlink()` khud order dhoondh sakta | Nahi — caller ko slot pehle se pata hona chahiye (09 ka index deta) |
| Arena "unlimited" hai | Fixed capacity — overflow (`free_top_==0`) handle karna zaroori |

---

## Exercises

1. Kyun `uint32_t` index `OrderSlot*` pointer se BETTER hai is design
   mein, sirf size (4 vs 8 bytes) ke alawa?
   <details><summary>Answer</summary>
   Agar arena (`std::vector<OrderSlot>`) kabhi resize/reallocate hota
   (yahan nahi hota — fixed size hai, par general principle), sab
   POINTERS invalid ho jaate (nayi memory location), jabki INDICES
   valid rehte (relative position preserved). Isliye index-based links
   inherently zyada resize-safe hain, size-saving ek bonus hai.
   </details>

2. `unlink()` order ke `prev`/`next` ko khud NULL_SLOT set nahi karta
   (sirf level ke head/tail aur neighbors update karta). Kya yeh bug hai?
   <details><summary>Answer</summary>
   Bug nahi hai IS design mein — kyunki unlink() ke turant baad slot
   free-list mein chala jaata (`free_list_[free_top_++] = slot`), aur
   agli baar allocate hone pe `add()` poora `OrderSlot{...}` struct
   FRESH overwrite karta (Trap 2 se). Stale `prev`/`next` values kabhi
   read nahi hote is beech mein. Agar design change ho (jaise "freed
   slots debug-inspect karna"), yeh explicit reset karna safer hota.
   </details>

---

## Interview questions

1. Intrusive linked list `std::list` se kaise alag hai (mechanism-level)?
2. Free-list allocation pattern batao — 36-LOW-LATENCY-CPP se connection.
3. `unlink()` ko O(1) banane ke liye kya PRECONDITION chahiye (order ka
   slot pehle se pata hona)?
4. Index-based vs pointer-based links ka trade-off.

---

## Next
→ [`09-order-id-lookup.md`](09-order-id-lookup.md)
