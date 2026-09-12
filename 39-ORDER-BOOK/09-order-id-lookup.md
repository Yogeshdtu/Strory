# 09 — Order ID lookup: `std::unordered_map` vs custom flat hash

## Prerequisites
- [`08-intrusive-order-lists.md`](08-intrusive-order-lists.md)

## Yeh topic abhi kyun
08 ne kaha: order-slot tak O(1) direct pahunch chahiye. `std::unordered_
map` yeh deta hai — par 36-LOW-LATENCY-CPP ki spirit mein, hot-path pe
kaam karne wale kisi bhi container ki **allocation aur cache behavior**
dono check karne zaroori hain.

---

## `std::unordered_map` ka structural cost (chaining)

```
unordered_map<OrderId, uint32_t>:

  bucket[hash(id) % n] -> node -> node -> node   (chained list, collisions)

Har `insert()`: EK heap allocation (node ke liye).
Har `find()`: bucket tak jump, phir chain traverse (cache-unfriendly,
              jaisa 19-STL mein already measured -- `unordered_map`
              chaining pointer-chase karta).
```

Yeh V1's `std::list` jaisa hi weakness hai (per-element heap node) —
kaam karta hai, par har insert ek allocation lata, aur lookup thoda
pointer-chase.

---

## Alternative: open-addressed flat hash table (`FlatIdIndex`)

> **Open addressing = collision hone pe agle SLOT mein probe karo (array
> ke andar hi), koi separate chain/node nahi.**

```cpp
class FlatIdIndex {
    std::vector<OrderId> keys_;          // fixed-size, pre-allocated
    std::vector<std::uint32_t> vals_;    // arena slot indices
    ...
};
```

Sab kuch **DO flat arrays** mein — koi per-entry heap allocation, koi
chaining. Insert/find/erase sab in dono arrays ke andar probing se.

---

## Probing + tombstones — deletion ka trap

```cpp
bool insert(OrderId id, std::uint32_t slot) {
    std::size_t i = hash(id) & mask_;
    for (probe = 0; probe <= mask_; ++probe) {
        if (keys_[i] == id) return false;              // duplicate
        if (keys_[i] == EMPTY) { keys_[i]=id; vals_[i]=slot; return true; }
        i = (i + 1) & mask_;                             // linear probe
    }
}
```

**Deletion ka trap:** agar erase pe slot ko seedha `EMPTY` mark kar do,
**probing sequences TOOT jaati** — ek baad wala insert jo isi slot se
guzar ke aage gaya tha, `find()` ab beech mein EMPTY dekh ke ROOK jaayega
(galat se "not found" bol dega), chahe woh key aage kahin exist karti ho.

**Fix: `TOMBSTONE` sentinel** — erase pe `TOMBSTONE` likho, `EMPTY` nahi:

```cpp
bool find(OrderId id, ...) const {
    for (probe...) {
        if (keys_[i] == EMPTY) return false;     // sirf EMPTY pe rukta
        if (keys_[i] == id) return true;          // TOMBSTONE ko SKIP karta (continue)
        i = (i+1) & mask_;
    }
}

bool erase(OrderId id) {
    ... keys_[i] = TOMBSTONE; ...    // EMPTY nahi -- probe chain intact rehti
}
```

`find()` TOMBSTONE dekh ke **aage badhta rehta** (jaise woh EMPTY na ho),
sirf real `EMPTY` pe rukta. Isse probe chain kabhi galat "not found"
nahi deti.

---

## Sentinels: `EMPTY = 0`, `TOMBSTONE = ~0`

```cpp
static constexpr OrderId EMPTY = 0;                              // order ids 1 se shuru (generator)
static constexpr OrderId TOMBSTONE = ~static_cast<OrderId>(0);   // all-ones, kabhi real id nahi
```

Dono sentinels **workload ke actual id-range se bahar** hain — safe
choice (verify karna zaroori real system mein: order-id generation kabhi
0 ya UINT64_MAX na de).

---

## Trade-off: tombstones "accumulate" ho sakte

Simple tombstone approach ka ek honest downside: **baar-baar insert/erase
se table mein tombstones jama hote jaate**, jo effectively load-factor
badhaate (probing chains lambi ho jaati, chahe LIVE entries kam hon).
Production systems iske liye **periodic rehashing** karte (poora table
naye array mein rebuild, tombstones drop) — yeh course scope se bahar
(bounded workloads ke liye is folder ka simple design kaafi hai), par
**honestly note karna zaroori hai** — yeh ek "not without downsides"
choice hai, free lunch nahi.

---

## Measured impact — 06 ka fix, ab poora

```
V1 index (unordered_map + list::iterator): O(1) direct, per-order heap alloc
V2 index (unordered_map + price only):      O(1) hash + O(level) SCAN
V3 index (FlatIdIndex + arena slot):        O(1) hash, NO heap alloc, NO scan
```

06/14 ke measured numbers (Execute/Cancel specifically) confirm karte:
V3 ka Execute/Cancel p50 (130.2 ns) V1 (200.4 ns) aur V2 (360.7 ns) dono
se better hai — V2's regression poori tarah fix hui, aur V1's per-order
allocation bhi khatam.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `EMPTY` mark karna delete pe (`TOMBSTONE` ki jagah)
Yeh probe chains todta — silent "not found" bugs jo sirf SPECIFIC
insert/delete sequences pe reproduce hote (flaky-lagne-wale bugs, actually
deterministic root cause ke saath).

### Trap 2 — table ko bahut chhota rakhna (high load factor)
Open addressing high load factor (>~70-80%) pe bahut slow ho jaata (long
probe chains). `FlatIdIndex` yahan `max_orders * 2 + 1` (rounded to
power-of-2) capacity leta — generous headroom.

### Trap 3 — hash function ka avalanche property ignore karna
Ek weak hash (jaise `id % capacity` seedha) clustering create kar sakta
agar ids ek pattern follow karte (jaise sab even numbers). `FlatIdIndex`
ek splitmix64-finalizer-style hash use karta (achha avalanche — chhoti
si input-difference bhi poori tarah alag output deti).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| `unordered_map` "already fast enough" hai | Per-insert heap allocation + chaining cache-cost — measurable |
| Delete pe EMPTY mark karna safe hai | TOMBSTONE zaroori hai, warna probe chains toot ti hain |
| Open addressing "free lunch" hai chaining ke against | Tombstone accumulation trade-off — periodic rehash chahiye long-run mein |
| Koi bhi hash function chalega | Weak hash clustering create kar sakta — avalanche property zaroori |

---

## Exercises

1. Agar `find()` TOMBSTONE dekh ke RUK jaata (EMPTY jaisa treat karta),
   kya specific bug hota?
   <details><summary>Answer</summary>
   Ek key jo TOMBSTONE ke AAGE probe-chain mein hai (originally collision
   ki wajah se aage gayi thi), ab kabhi nahi milegi — `find()` galat se
   "not found" bolega chahe woh key table mein present ho. Yeh
   intermittent-lagne-wala bug hai (sirf specific insert/delete order
   pe reproduce hota) jo asal mein deterministic hai.
   </details>

2. `FlatIdIndex` ka capacity `max_orders * 2 + 1` (phir power-of-2 tak
   round) kyun rakha gaya, `max_orders` ke barabar kyun nahi?
   <details><summary>Answer</summary>
   Open addressing ka performance load-factor pe bahut depend karta —
   100% full table (capacity == max entries) pe probing worst-case O(n)
   ho sakta. ~50% load factor (double capacity) generous headroom deta,
   probe chains chhoti rehti hain typical case mein, aur `find()`/`insert()`
   fast rehte.
   </details>

---

## Interview questions

1. Chaining (`unordered_map`) vs open addressing ka trade-off batao.
2. Tombstone kyun zaroori hai open-addressed delete ke liye?
3. `EMPTY` aur `TOMBSTONE` sentinels kaise choose kiye, aur kyun safe hain?
4. Open addressing ka long-term downside kya hai (tombstone accumulation)?

---

## Next
→ [`10-price-time-priority-impl.md`](10-price-time-priority-impl.md)
