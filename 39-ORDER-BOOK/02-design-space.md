# 02 — Design space: map vs sorted vector vs flat array

## Prerequisites
- [`01-order-book-requirements.md`](01-order-book-requirements.md)
- `32-CACHE-MEMORY-PERFORMANCE` (latency ladder, pointer chasing ka cost)

## Yeh topic abhi kyun
Code likhne se pehle, trade-off space samajhna — taaki 03/05/07 ke
decisions **guess na ho, reasoned choices hon.**

---

## Teen design axes

Order book ke liye do alag sub-problems hain, HAR ek ka apna storage
choice:

1. **Price levels kaise store karein** (sorted by price, best = extreme)
2. **Ek level ke andar orders kaise store karein** (FIFO, arrival order)
3. **Order ID → order kaise map karein** (O(1) cancel ke liye)

Har sub-problem ke liye options:

### 1. Price levels

| Option | Insert/erase level | Find level | Iterate sorted | Cache behavior |
|---|---|---|---|---|
| `std::map` (red-black tree) | O(log n), node alloc | O(log n) | O(n) but pointer-chase | **Poor** — har node alag heap allocation, tree traversal = random memory jumps |
| Sorted `std::vector` | O(n) shift | O(log n) binary search | O(n), contiguous | **Good** for the SCAN, par shift bhi O(n) hai |
| Flat tick-indexed array | O(1) (direct index) | O(1) (direct index) | O(range), contiguous | **Best** — no search, no shift, sirf ek array-index computation |

### 2. Orders within a level (FIFO)

| Option | Insert (append) | Remove (any position) | Cache |
|---|---|---|---|
| `std::list` | O(1) | O(1) (with iterator) | **Poor** — har node heap-allocated, alag jagah |
| `std::deque` | O(1) amortized | O(n) within level (shift) | **Better** — chunks contiguous |
| Intrusive linked list (arena-backed) | O(1) | O(1) (with a stored slot index) | **Best** — arena contiguous-ish, no per-node alloc |

### 3. Order ID → location

| Option | Lookup | Insert/erase | Cache |
|---|---|---|---|
| `std::unordered_map` | O(1) avg | O(1) avg, node alloc | **Poor-medium** — chaining = pointer chase, node alloc per insert |
| Open-addressed flat hash (custom) | O(1) avg | O(1) avg, no alloc (pre-sized array) | **Good** — contiguous probe sequence |
| Dense array (if ids are small/sequential) | O(1) exact | O(1) | **Best** — direct index, no hashing at all |

---

## Yeh folder ke teen versions, in terms of in choices

```
V1 (03): std::map + std::list + std::unordered_map
         -- "sabse obvious STL choice." Correct, simple, SLOW-ish.

V2 (05): sorted std::vector + std::deque + std::unordered_map
         -- "cache-friendly price-levels" -- theory: array scan/lookup
         behtar hona chahiye tree se. MEASURED RESULT (06): only HALF
         true -- Add behtar, Execute/Cancel WORSE (kyun -- ek specific,
         findable design mistake, 06/15 mein poora).

V3 (07-12): flat tick-indexed array + intrusive list (arena) + custom
            flat hash -- "production shape." O(1) HAR operation ke liye,
            koi search, koi per-op allocation.
```

---

## Trade-off yeh sab jo V3 leta hai

V3 "fastest" hai, par **free nahi** — 01 ka scope-note yaad karo, ab
concrete trade-offs:

| Trade-off | Kya cost hai |
|---|---|
| **Bounded price range** | `NUM_LEVELS` fixed array size — price range se bahar ka order reject ho jaata (add() false return karta). Re-centering off-hot-path chahiye agar price bahut door drift kare. |
| **Bounded order capacity** | Arena fixed size — `max_orders` se zyada live orders ho to add() fail. |
| **Zyada code complexity** | Intrusive list index-links, custom hash table — V1 ke 3-line `side[price]` se bahut zyada code. |
| **Debugging harder** | Raw indices (`uint32_t prev, next`) debugger mein utne readable nahi jitna `std::list` ke pointers/iterators. |

**Nichod:** V3 ka speed **explicit trade-offs ke through aata hai**, magic
se nahi. Yeh 24-tradeoffs-and-when-not-to (36) ka principle order-book
context mein.

---

## Hybrid designs (real world mein common)

Production systems aksar **mix** karte:
- Price levels: flat array (V3 style) jab price range predictable ho
  (equities, options with known strikes); sorted structure jab range
  bahut wide/unpredictable ho.
- Order-within-level: hamesha kuch FIFO-preserving structure (list/
  intrusive), kabhi deque bhi (agar cancel-by-position rare ho).
- Order-id lookup: flat hash ya dense array almost hamesha (fastest,
  aur order ids often dense/sequential enough for a direct-index scheme).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "cache-friendly container" ka matlab automatically "faster"
06 ka measured result yehi dikhayega — ek container ko "cache-friendly"
banana ek AXIS ko improve karta; agar tumhare algorithm mein doosri jagah
(jaise order-id lookup ka linear scan) ek naya bottleneck ban jaaye, net
result WORSE ho sakta.

### Trap 2 — sabse complex design (V3) seedha shuru mein likhna
Spec ka rule: "pehle correct, phir fast." V1 (simple, correct) baseline
hai jispe har optimization measure hoti — bina iske, tumhe pata nahi
chalega V3 ne kitna faayda diya.

### Trap 3 — design choice ko benchmark ke bina "obviously better" maan lena
V2 ka poora point hai yeh dikhana: "obviously better" (contiguous array)
measure karne pe pura sach nahi nikla.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Ek "best" data structure hoti hai order books ke liye | Trade-off space hai; choice constraints pe depend karta (price range, order volume) |
| Cache-friendly = automatically fast | Sirf EK axis; doosre axes (algorithm complexity, scan cost) bhi matter karte |
| Fastest design (V3) "free" hai | Bounded range/capacity, zyada code complexity — explicit trade-offs |
| V1 se seedha V3 jump karna chahiye | Measure-first process se real understanding milta, sirf fast code nahi |

---

## Exercises

1. Ek order book jahan symbol ki price range totally unpredictable hai
   (jaise crypto, jo 10x move kar sakta ek din mein) — kya V3 ka flat
   tick-indexed array approach directly use kar sakte ho?
   <details><summary>Answer</summary>
   Directly nahi — `NUM_LEVELS` fixed hai, aur agar price 10x move kare,
   woh range se bahar chali jaayegi. Ek "re-centering" mechanism chahiye
   hota (jab price drift ho, array ko naye center ke around REBUILD karo
   — yeh operation off-hot-path honi chahiye, jaisa 02 mein note kiya).
   Ya, ek hybrid: sorted-vector-style (V2) jo bounded range assume nahi
   karta, agar re-centering complexity avoid karni ho.
   </details>

2. Order-id lookup ke liye "dense array" (direct index) kab possible
   hota, aur hash table se kyun behtar hota jab possible ho?
   <details><summary>Answer</summary>
   Jab order ids DENSE aur SEQUENTIAL hon (jaise 1, 2, 3, ... koi gap
   nahi) — tab `array[id]` seedha O(1), no-hash, no-collision access deta.
   Hash table iske against ek hash COMPUTE karta (kuch ns) aur collision
   handle karta (probing/chaining) — dense array in dono costs ko poori
   tarah eliminate kar deta. Trade-off: agar ids sparse/large-range hon
   (jaise 64-bit random UUIDs), dense array impractical (bahut bada array
   chahiye), hash table zaroori ban jaata.
   </details>

---

## Interview questions

1. Price-level storage ke 3 options batao, har ek ka Big-O aur cache
   trade-off.
2. Order-within-level FIFO ke liye `std::list` vs `std::deque` vs
   intrusive list ka trade-off.
3. Order-id lookup ke liye hash table vs dense array kab-kab appropriate?
4. V3 ka speed kis explicit trade-off (bounded range/capacity) ke saath
   aata hai?

---

## Next
→ [`03-naive-map-implementation.md`](03-naive-map-implementation.md)
