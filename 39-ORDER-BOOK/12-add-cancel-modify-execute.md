# 12 — Add / Cancel / Modify / Execute: full picture, teeno versions

## Prerequisites
- [`01`](01-order-book-requirements.md) se [`11`](11-top-of-book-fast-path.md) tak

## Yeh topic abhi kyun
Ab tak har piece alag-alag dekha (levels, lists, hash, top-of-book). Yeh
lesson **char operations** (public API) ko ek jagah jodta hai — teeno
versions ka side-by-side, ek reference ki tarah.

---

## Public interface — sab teeno versions IDENTICAL signature rakhte

```cpp
bool add(OrderId id, bool is_buy, Price price, Qty qty);
bool reduce(OrderId id, Qty qty);       // Cancel (partial) AUR Execute (fill) dono
bool remove(OrderId id);                 // Delete (poora)
bool replace(OrderId old_id, OrderId new_id, bool is_buy, Price price, Qty qty);

bool has_bid() const; bool has_ask() const;
Price best_bid() const; Price best_ask() const;      // PRECONDITION: has_*() true
Qty best_bid_qty() const; Qty best_ask_qty() const;
std::size_t order_count() const;
std::vector<OrderId> ids_at_price(bool is_buy, Price price) const;   // FIFO order
```

Same interface hone se **07_comparison_suite.cpp** ek `template <class
Book>` function se teeno ko drive kar sakta (bina code-duplicate kiye) —
aur yeh khud ek design lesson hai: **jab teen implementations same
"contract" follow karte, tum unhe interchangeably test/benchmark/swap kar
sakte** (kal agar V4 banao, wahi harness reuse hoga).

---

## Add — side-by-side

```cpp
// V1: tree lookup/insert + list append
auto& level = side[price];                     // O(log n)
level.orders.push_back(Order{id, qty});         // O(1), heap alloc
index_.emplace(id, Location{is_buy, price, it}); // O(1) avg, heap alloc

// V2: binary search + possible shift + deque append
auto it = locate(side, price, is_buy);          // O(log n)
if (new level) side.insert(it, ...);            // O(n) shift
it->orders.push_back(Order{id, qty});           // O(1) amortized
index_.emplace(id, Location{is_buy, price});     // O(1) avg, heap alloc

// V3: array index + intrusive append + flat hash insert
int idx = ...;                                   // O(1) arithmetic
LevelV3& level = levels[idx];                    // O(1) direct
arena_[slot] = OrderSlot{...};                   // O(1), NO alloc (arena pre-allocated)
id_index_.insert(id, slot);                      // O(1) avg, NO alloc
```

---

## Reduce (Cancel/Execute) — side-by-side

```cpp
// V1: hash lookup -> DIRECT list-iterator access
auto idx_it = index_.find(id);                   // O(1) avg
Order& order = *loc.it;                          // O(1) DIRECT (V1's strength)
order.qty -= amt;
if (order.qty == 0) { level.orders.erase(loc.it); index_.erase(idx_it); }

// V2: hash lookup -> price -> LINEAR SCAN (V2's regression, 06)
auto lvl_it = locate(side, loc.price, ...);       // O(log n)
auto ord_it = std::find_if(orders.begin(), orders.end(), ...);  // O(level size)!
order.qty -= amt;

// V3: hash lookup -> DIRECT arena-slot access (fix, 09)
OrderSlot& order = arena_[slot];                  // O(1) DIRECT
order.qty -= amt;
if (order.qty == 0) unlink(order, level);         // O(1)
```

**Yeh exactly 06's finding ka code-level summary hai** — V1 aur V3 dono
DIRECT access dete (O(1), koi scan), V2 sirf price deta index se, isliye
level ke andar scan karna padta.

---

## Remove (Delete) — `reduce(id, full_qty)` ke barabar

Teeno versions mein `remove()` conceptually `reduce()` jaisa hi hai, bas
poori qty ek saath: V1/V2/V3 sab `remove()` ko apni `reduce`-jaisi
internal logic se implement karte (order ki poori `qty` le ke, unconditionally
remove karte — chahe woh 0 ho ya na ho).

---

## Replace — SABSE simple operation, sabse teeno mein

```cpp
bool replace(OrderId old_id, OrderId new_id, bool is_buy, Price price, Qty qty) {
    remove(old_id);   // agar already gone, ignore (fuzzing-tested, 16)
    return add(new_id, is_buy, price, qty);
}
```

**Teeno versions mein IDENTICAL** — Replace ko "remove + add" ke roop mein
implement karna simplest, aur automatically correct hai (dono operations
already tested/correct hain, koi naya logic nahi). Yeh ek achha design
principle hai: **complex operations ko simple, already-correct operations
se compose karo**, jab tak performance-critical na ho ki custom fast-path
chahiye.

---

## Duplicate-add aur nonexistent-id handling — teeno consistent

```cpp
// add(): agar id already index mein hai -> false (duplicate reject)
if (index_.count(id)) return false;    // V1/V2
if (id_index_.find(id, dummy)) return false;  // V3

// reduce()/remove(): agar id NAHI mila -> false
auto it = index_.find(id);
if (it == index_.end()) return false;
```

Yeh consistent contract **09_orderbook_fuzz.cpp** ki bunyaad hai — sab
teeno (aur reference model) **same return-value semantics** follow karte,
isliye unhe directly compare kar sakte har fuzzed operation ke baad.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `replace()` ko custom-optimize karne ki koshish (premature)
"Remove+add" thoda wasteful lag sakta (do full operations ek "modify" ke
liye) — par 37/24's Rule 0 (measure first) yaad rakho: agar Replace
tumhara workload ka chhota fraction hai (is folder mein ~9%), custom
fast-path likhna complexity add karta bina proven benefit ke.

### Trap 2 — public interface ko versions ke beech thoda alag rakhna
"Chhota sa" signature difference (jaise ek version mein `Qty` by value,
doosre mein by const-ref) generic template code (`07`) ko todta. Interface
consistency **deliberately** maintain karni padti.

### Trap 3 — teeno mein error-handling semantics alag rakhna
Agar V1 duplicate-add pe `false` return kare par V3 exception throw kare,
fuzz-testing (16) automatically kaam nahi karega — consistent contract
zaroori hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Replace ek complex, custom operation honi chahiye | Remove+Add composition — simple, correct, teeno mein identical |
| Har version ka apna interface ho sakta hai | Consistent interface generic testing/benchmarking enable karta |
| Execute aur Cancel alag implementation chahiye | Same `reduce()` — sirf semantic naam alag |
| Duplicate/nonexistent handling "detail" hai | Consistent contract fuzz/cross-version testing ki bunyaad hai |

---

## Exercises

1. `replace()` ko "remove+add" ki jagah ek custom, single-pass optimized
   version banane ka kya risk hai (agar galat likha jaaye)?
   <details><summary>Answer</summary>
   Do already-tested, already-correct operations (remove, add) ko reuse
   karne ke bajaye naya code likhna matlab naye bugs ka risk — jaise
   partial state (purana order half-removed, naya order half-added) agar
   koi edge case miss ho jaaye. "Compose from correct primitives" safer
   hai jab tak measured proof na ho ki custom path genuinely zaroori hai.
   </details>

2. Kyun `07_comparison_suite.cpp` EK `template <class Book> run(...)`
   function se teeno versions test kar sakta, teen alag functions likhe
   bina?
   <details><summary>Answer</summary>
   Kyunki V1/V2/V3 sab **identical public interface** (same method names,
   signatures, return-value semantics) rakhte — template code kisi bhi
   `Book` type ke saath instantiate ho sakta jab tak woh interface match
   kare, C++'s duck-typed templates (21-TEMPLATES) ka natural use-case.
   </details>

---

## Interview questions

1. Add/Reduce/Remove/Replace — char operations ka poora contract batao.
2. Reduce mein V1/V2/V3 ka fark ek sentence mein (mechanism-level).
3. Replace ko "remove+add" ki tarah implement karna kyun ek achha design
   choice hai?
4. Consistent interface ka kya practical faayda hota testing/benchmarking
   mein?

---

## Next
→ [`13-book-snapshots.md`](13-book-snapshots.md)
