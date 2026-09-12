# 05 — Version 2: sorted vector implementation

## Prerequisites
- [`04-measuring-v1.md`](04-measuring-v1.md)
- `examples/orderbook_v2_vector.hpp`, `examples/03_orderbook_v2_vector.cpp`

## Yeh topic abhi kyun
04 ne dikhaya V1 ka bottleneck **structural** hai — tree traversal,
per-node allocation. Obvious fix (32-CACHE-MEMORY-PERFORMANCE se): price
levels ko ek **contiguous, sorted array** mein rakho. Yeh V2.

---

## Design

```cpp
struct PriceLevel { Price price; Qty total_qty; std::deque<Order> orders; };
using Side = std::vector<PriceLevel>;

Side bids_;   // sorted descending, best = front()
Side asks_;   // sorted ascending,  best = front()
std::unordered_map<OrderId, Location> index_;   // Location = {is_buy, price} -- SIRF price, NOT an iterator
```

Do badlaav V1 se:
1. `std::map<Price, PriceLevel>` → sorted `std::vector<PriceLevel>` +
   `std::lower_bound` (binary search).
2. `std::list<Order>` → `std::deque<Order>` (chunks-contiguous, V1's
   per-node-heap-allocation se behtar cache behavior).

---

## `add` — binary search + possible shift

```cpp
bool add(OrderId id, bool is_buy, Price price, Qty qty) {
    Side& side = is_buy ? bids_ : asks_;
    auto it = locate(side, price, is_buy);        // O(log n), CONTIGUOUS binary search
    if (it == side.end() || it->price != price) {
        it = side.insert(it, PriceLevel{price, 0, {}});   // O(n) SHIFT -- naya level ka cost
    }
    it->orders.push_back(Order{id, qty});
    it->total_qty += qty;
    index_.emplace(id, Location{is_buy, price});   // <- sirf {is_buy, price}, iterator NAHI
    return true;
}
```

**Kyun iterator store nahi kar sakte (V1 ki tarah)?** `std::vector`
insert/erase pe **baaki sab elements ke iterators/pointers invalidate**
ho sakte (agar reallocation ho, ya shift ho beech mein) — 03's exercise
answer yaad karo. Isliye `index_` mein sirf `{is_buy, price}` store karte,
aur level chahiye hone pe **dobara locate() karte** (binary search).

---

## `reduce`/`remove` — ek EXTRA step jo V1 mein nahi tha

```cpp
bool reduce(OrderId id, Qty qty) {
    auto idx_it = index_.find(id);                     // O(1) avg -- price milta
    const Location loc = idx_it->second;
    Side& side = loc.is_buy ? bids_ : asks_;
    auto lvl_it = locate(side, loc.price, loc.is_buy);  // O(log n) -- level milta
    auto& orders = lvl_it->orders;
    auto ord_it = std::find_if(orders.begin(), orders.end(),
                                [id](const Order& o) { return o.id == id; });  // <- O(level size) SCAN!
    ...
}
```

**Yeh naya `std::find_if` scan V1 mein NAHI tha** (V1 ka index seedha
`list::iterator` deta tha, O(1)). V2 mein, index sirf price deta hai —
level ke ANDAR SPECIFIC order dhoondhna abhi bhi ek linear scan hai.

> **Iska measured impact 06 mein dikhega — aur yeh is folder ka sabse
> important Rule-2 moment hai.**

---

## Correctness verified — V1 se IDENTICAL

```
=== BookV2 (sorted vector) -- correctness check ===
ops applied: 20000 / 20000  (0 failed)
live orders in book: 9118

best bid = 9999 x30136
best ask = 10001 x25547
best_bid < best_ask? haan (sahi)
```

**SAME workload (same seed), EXACT SAME output** as V1 (03) — `order_count`,
`best_bid`, `best_bid_qty` sab match karte. Yeh confirm karta V2 **correct**
hai (behavior-equivalent V1 ke), performance abhi measure nahi hui.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — vector iterator ko cross-call store karna
V1's pattern (list iterator store karna) V2 mein directly copy karna ek
**dangling-pointer bug** hoga — vector insert/erase se woh iterator
invalid ho sakta agli call tak. Yeh trap khud isi lesson mein avoid kiya
gaya (index sirf price store karta), par kisi aur codebase mein yeh
common real bug hai.

### Trap 2 — "contiguous = automatically fast" assume karna, poora
algorithm re-check kiye bina
Level-storage contiguous banane se **level lookup** fast hota — par agar
level ke ANDAR ka lookup (order-within-level) linear scan ban jaata hai
(jaisa yahan hua), net effect predict karna mushkil hai bina measure kiye.

### Trap 3 — `std::deque` ko "vector jaisa hi" samajhna
`std::deque` chunk-based hai (kayi chhote contiguous blocks, indirection
array se linked) — `std::vector` se thoda kam cache-friendly, par
`std::list` se bahut behtar (koi per-element heap node nahi).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Vector iterators V1's list iterators jaise stable hote | Vector insert/erase invalidate kar sakta — price store karo, iterator nahi |
| Contiguous storage automatically end-to-end fast hai | Sirf level-lookup improve hui; order-within-level ab linear scan hai |
| V2 "obviously" V1 se fast hoga | Abhi sirf DESIGN hai — 06 measure karega |
| `std::deque` == `std::vector` cache-wise | Deque chunk-indirected, thoda kam cache-friendly than vector |

---

## Hands-on

```bash
./build.ps1 fast 39-ORDER-BOOK/examples/03_orderbook_v2_vector.cpp
```

---

## Exercises

1. `locate()` do baar call hota ek `reduce()` mein possible ho sakta —
   kahan (dhyaan se code padho)?
   <details><summary>Answer</summary>
   Sirf EK baar directly (`locate(side, loc.price, loc.is_buy)` level
   dhoondhne ke liye). Par agar level KHAALI ho jaaye reduce ke baad
   (order fully filled/cancelled), `side.erase(lvl_it)` bhi ek O(n)
   operation hai (baaki levels shift). Do alag O(n)/O(log n) operations
   hain (locate + possible-erase), V1 ke single-tree-erase se zyada.
   </details>

2. Ek price level mein agar sirf 2-3 orders average hon (bahut thin
   book), kya `find_if` ka linear-scan cost significant hoga?
   <details><summary>Answer</summary>
   Shayad nahi — 2-3 element scan bahut sasta hai (ek do cache-line reads).
   06 ka measured result specifically un workloads pe hai jahan levels
   mein zyada orders hote (yahan average ~40+ orders/level) — yeh dikhata
   ki is trap ka IMPACT workload-dependent hai, universal nahi. Kam
   orders-per-level wale books mein V2 ka yeh weakness kam matter karegi.
   </details>

---

## Interview questions

1. V2 ke do main changes V1 se batao.
2. Kyun `index_` mein iterator store nahi kar sakte (V1 ki tarah)?
3. `reduce()` mein woh EXTRA cost kya hai jo V1 mein nahi tha?
4. `std::deque` aur `std::vector` ka cache-behavior fark batao.

---

## Next
→ [`06-measuring-v2.md`](06-measuring-v2.md)
