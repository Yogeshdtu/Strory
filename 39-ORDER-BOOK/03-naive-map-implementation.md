# 03 — Version 1: `std::map` implementation (simple, correct)

## Prerequisites
- [`02-design-space.md`](02-design-space.md)
- `examples/orderbook_v1_map.hpp`, `examples/01_orderbook_v1_map.cpp`

## Yeh topic abhi kyun
Spec ka rule: **"pehle correct, phir fast."** Yeh V1 — sabse obvious STL
choice — hamara baseline banega. Bina isse pehle likhe, V2/V3 ka koi
"improvement" measure hi nahi ho sakta.

---

## Design

```cpp
std::map<Price, PriceLevel, std::greater<Price>> bids_;   // best = begin()
std::map<Price, PriceLevel> asks_;                         // best = begin()
std::unordered_map<OrderId, Location> index_;

struct PriceLevel { std::list<Order> orders; Qty total_qty = 0; };
struct Location { bool is_buy; Price price; std::list<Order>::iterator it; };
```

- Bids `std::greater<Price>` se sorted (highest = best = `begin()`) —
  37/06 ka `SimpleBook` model, ab full operations ke saath.
- Har `PriceLevel` ek `std::list<Order>` rakhta (FIFO — arrival order,
  price-time priority ke liye zaroori, 37/07).
- `index_` — `OrderId` se seedha `std::list<Order>::iterator` tak —
  **yeh important hai**: `std::list` ke iterators erase/insert ke baad
  bhi VALID rehte (sirf erased element ka iterator invalid hota, baaki
  sab safe) — isliye ek order ka iterator STORE karke rakh sakte hain,
  aur cancel/execute O(1) mein us exact node tak pahunch sakta, list ko
  scan kiye bina.

---

## `add` — ek naya level milega ya nahi, dono cases

```cpp
bool add(OrderId id, bool is_buy, Price price, Qty qty) {
    if (index_.count(id)) return false;         // duplicate reject (09)
    return is_buy ? add_impl(bids_, id, true, price, qty)
                   : add_impl(asks_, id, false, price, qty);
}

template <class Map>
bool add_impl(Map& side, OrderId id, bool is_buy, Price price, Qty qty) {
    auto& level = side[price];    // EXISTING level -> tree lookup, O(log n)
                                    // NAYA level -> tree INSERT + node alloc, O(log n)
    level.orders.push_back(Order{id, qty});   // list push -- ek naya heap node
    level.total_qty += qty;
    index_.emplace(id, Location{is_buy, price, std::prev(level.orders.end())});
    return true;
}
```

`side[price]` (map's `operator[]`) do cases handle karta khud: agar
`price` already exists, seedha existing `PriceLevel&` return karta;
agar nahi, **naya node allocate karke tree mein insert karta, phir uska
reference deta.** Simple, correct — par har naya-level case ek heap
allocation hai (04 mein measure hoga).

---

## `reduce` (Cancel/Execute) aur `remove` (Delete)

```cpp
bool reduce(OrderId id, Qty qty) {
    auto idx_it = index_.find(id);              // hash lookup, O(1) avg
    if (idx_it == index_.end()) return false;
    return loc.is_buy ? reduce_impl(bids_, idx_it, qty)
                        : reduce_impl(asks_, idx_it, qty);
}
// reduce_impl: side.find(loc.price) -> tree lookup, O(log n)
//              *loc.it -- DIRECT node access (list iterator), O(1)
//              qty subtract; agar 0 -> list.erase(loc.it) O(1),
//                                       agar level khaali -> side.erase() O(log n)
```

**Dhyaan do:** order ke andar tak pahunchna (`*loc.it`) O(1) hai — `index_`
ne pehle hi exact list-node ka iterator store kar rakha tha, koi scan
nahi. Yeh V1 ki ek **genuinely acchi choice hai** (09-order-id-lookup mein
V2 ke saath contrast dekhoge).

---

## Trap jo build karte waqt mila: alag-comparator map types

```cpp
// ❌ Compile error -- bids_ aur asks_ ke comparators ALAG hain
auto& side = is_buy ? bids_ : asks_;
// error: operands to '?:' have different types
```

`bids_` (`std::greater<Price>`) aur `asks_` (default `std::less<Price>`)
**do alag types** hain compiler ke liye — ek `std::map<Price, PriceLevel,
greater<Price>>` aur ek `std::map<Price, PriceLevel>`. Ternary dono ka
common type nahi nikal sakta.

**Fix (`orderbook_v1_map.hpp` mein):** templated private helper functions
(`add_impl<Map>`, `reduce_impl<Map>`, ...) + if/else dispatch — har call
site pe `is_buy ? call_with(bids_) : call_with(asks_)` explicit hai, koi
common-reference-nikalne-ki-koshish nahi.

> Yeh bilkul wahi trap hai jo 37-HFT-FUNDAMENTALS/examples/01 mein mila
> tha (`SimpleBook::add`) — do maps jo comparator ke alawa identical hain,
> phir bhi C++ mein "same type" nahi hote.

---

## Correctness verified

```
=== BookV1 (std::map) -- correctness check ===
ops applied: 20000 / 20000  (0 failed)
live orders in book: 9118

best bid = 9999 x30136
best ask = 10001 x25547
best_bid < best_ask? haan (sahi)
```

Sab 20000 (deterministic, seeded) operations sahi apply hue, best_bid
best_ask se seedha kam hai (guaranteed by workload design — 01's scope
note: yeh book crossing check nahi karta, par workload khud crossing
prices generate hi nahi karta).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `side[price]` ko "hamesha cheap" samajhna
Naya level create hone pe yeh ek **heap allocation + tree rebalance**
hai — chhota nahi. 04 mein iska measured cost dikhega.

### Trap 2 — `index_.find()` ke baad `idx_it` ko baad mein reuse karna
bina check kiye ki woh abhi bhi valid hai — agar beech mein `index_.erase()`
kahin call hua (nested calls mein), stale iterator UB hota.

### Trap 3 — `std::list` ko "hamesha O(1)" maan lena end-to-end
`push_back`/`erase(iterator)` O(1) hain (yeh sahi hai) — par **traversal**
(agar kabhi poori list scan karni pade) O(n), aur cache-unfriendly (har
node alag allocation). V1 abhi traversal avoid karta (direct iterator via
index_), isliye yeh weakness abhi nahi dikhti — V2/V3 discussion mein
context milega.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| `bids_`/`asks_` ek hi type ke maan sakte | Alag comparator = alag type, templated helpers chahiye |
| `side[price]` hamesha O(log n), cheap | Naya level = heap alloc + rebalance, existing level = cheap lookup |
| `std::list` iterators erase pe sab invalid ho jaate | Sirf ERASED element ka iterator invalid hota, baaki safe |
| V1 "bekaar" hai, seedha V3 likhna chahiye | V1 baseline hai — bina iske koi comparison meaningless |

---

## Hands-on

```bash
./build.ps1 fast 39-ORDER-BOOK/examples/01_orderbook_v1_map.cpp
```

---

## Exercises

1. `index_` mein `list::iterator` store karna kyun safe hai, jab ki
   `std::vector::iterator` store karna UNSAFE hota (05 mein dekhoge)?
   <details><summary>Answer</summary>
   `std::list` erase/insert pe apne DOOSRE elements ke iterators invalidate
   NAHI karta (sirf jo element khud erase hua, uska iterator invalid hota)
   — yeh ek node-based (linked) container hai, elements memory mein move
   nahi hote. `std::vector` insert/erase pe (agar reallocation ho, ya
   beech mein shift ho) baaki sab iterators bhi invalidate ho sakte —
   isliye vector-backed design mein direct iterator store karna galat
   approach hoga.
   </details>

2. `side[price]` (map's `operator[]`) EXISTING price ke liye kitna
   expensive hai — kya yeh bhi "insert" karta?
   <details><summary>Answer</summary>
   Nahi, agar key already exists, `operator[]` sirf ek tree LOOKUP karta
   (O(log n), koi allocation), existing `PriceLevel&` return karta. Naya
   insert sirf tab hota jab key absent ho. Behavior same signature ke
   peeche, cost bahut alag — yeh isi wajah se 04 mein "Add" ka latency
   distribution BIMODAL dikh sakta (existing-level Add cheap, new-level
   Add expensive).
   </details>

---

## Interview questions

1. V1 ka poora design (3 containers, unke roles) batao.
2. `std::list::iterator` ko index mein store karna safe kyun hai?
3. Alag-comparator map types ka trap, aur uska fix.
4. `side[price]` ka behavior existing vs new key ke liye alag kaise hai?

---

## Next
→ [`04-measuring-v1.md`](04-measuring-v1.md)
