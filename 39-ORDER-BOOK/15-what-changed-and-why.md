# 15 — What changed and why: the full accounting

## Prerequisites
- Poora folder `39` (`01`–`14`)

## Yeh topic abhi kyun
CLAUDE.md ka explicit requirement: **"explain karo har optimization ne
kya kiya."** Sirf numbers table dikhana kaafi nahi — is lesson mein har
change ka **mechanism** aur uska measured **effect** ek jagah.

---

## Poora safar, ek table mein

| Step | Kya badla | Kyun | Measured effect |
|---|---|---|---|
| **V1 → V2** | Price levels: `std::map` → sorted `std::vector` | Contiguous storage, binary search "cache-friendlier hona chahiye" tree-traversal se | **Add p99.9: 10700→2755 ns (3.9x behtar)** ✅ |
| **V1 → V2** (side-effect) | Order-id index: `list::iterator` → sirf `{price}` | Vector iterators unsafe hain cross-mutation (invalidation) | **Execute/Cancel p50: 200→361 ns (1.8x WORSE)** ❌ |
| **V2 → V3** | Price levels: sorted vector → tick-indexed flat array | Binary search + shift eliminate karo, O(1) arithmetic | **Add p99.9: 2755→531 ns (5.2x behtar)** ✅ |
| **V2 → V3** | Order-within-level: `deque` + linear scan → intrusive list + arena | Linear scan (V2's mistake) eliminate karo | **Execute/Cancel p50: 361→130 ns (2.8x behtar)** ✅ |
| **V2 → V3** | Order-id index: `unordered_map` (chained, per-insert alloc) → `FlatIdIndex` (open-addressed, zero alloc) | Chaining ka heap-node-per-insert cost eliminate karo | (combined effect upar ke numbers mein already shaamil) |
| **V3** (new) | Top-of-book: implicit (`begin()`/`front()`) → explicit cached index | Flat array mein koi natural "extreme position" nahi hai | O(1) typical, worst-case O(NUM_LEVELS) — **honest, not oversold** (11) |

---

## Har change ka "kyun" — ek line mein

- **Map → sorted vector**: pointer-chasing tree-traversal se bachna
  (contiguous memory access, cache-friendly).
- **List/vector → intrusive arena list**: per-order heap allocation
  poori tarah khatam karna, aur index-based links se pointer-invalidation
  ka risk khatam karna.
- **unordered_map → flat hash**: chaining ka per-insert allocation aur
  pointer-chase khatam karna.
- **Sorted vector → flat array**: **search bhi khatam karna** — binary
  search O(log n) hai, array-index O(1) hai, aur V2's insert-shift cost
  bhi khatam.

**Pattern dikhta hai:** har step allocation aur/ya pointer-chasing ka
KOI EK source khatam karta. Yeh 36-LOW-LATENCY-CPP ka poora theme hai,
ab order-book context mein applied.

---

## Sabse important honest admission: V2 ek REGRESSION tha

Yeh **sabse zaroori** part hai is lesson ka. Agar hum sirf "V1 slow tha,
V3 fast hai, poora improvement" bolte, hum **V2's genuine lesson miss kar
dete**:

> **Ek optimization jo THEORY mein sahi lagti hai (contiguous storage =
> cache-friendly) OVERALL performance ko WORSE kar sakti hai, agar woh
> ek DOOSRE hissa mein ek naya cost inject kar de** (yahan: index design
> ka trade-off, jo linear-scan le aaya).

Yeh **guess se nahi**, **measurement se** pakda gaya (06). Agar hum sirf
"V2 banao, V3 banao, dono ka number report karo" karte bina Add vs
Execute/Cancel breakdown ke, hum yeh MISS kar dete ki V2 ka gain (Add)
aur loss (Execute/Cancel) do ALAG mechanisms se aa rahe the — aur V3 ko
design karte waqt **dono ko explicitly address** karna zaroori tha.

---

## Trade-offs jo V3 ne liye (yeh bhi honest accounting ka part hai)

| Trade-off | Cost |
|---|---|
| Bounded price range (`NUM_LEVELS`) | Range se bahar prices reject (silent, return-value check zaroori) |
| Bounded order capacity (arena `max_orders`) | Capacity-planning zaroori, overflow = reject |
| Zyada code complexity | Index-links, custom hash table — V1 ke 3-line map-access se bahut zyada |
| Top-of-book worst-case scan | "Usually O(1)" hai, guaranteed O(1) NAHI (11's honest claim) |

**V3 "free lunch" nahi hai** — speed explicit design choices + explicit
constraints ke saath aaya. Yeh 36/24's "measure → optimize → document the
floor you can't beat" process ka poora example hai.

---

## Agar hum yeh process follow NA karte

```
Agar seedha V3 likhte (bina V1/V2 se guzre):
  - koi baseline nahi hota "kitna behtar hua" measure karne ke liye
  - V2's specific mistake (index design) kabhi CONSCIOUSLY avoid nahi
    hoti -- ho sakta V3 mein bhi accidentally reproduce ho jaati
  - correctness-confidence kam hoti (3 independent implementations ka
    agreement > 1 implementation ka "lagta hai sahi hai")
```

Yeh hi spec ka poora point hai: **"pehle correct, phir fast" sirf ek
slogan nahi, ek process hai jo real bugs/regressions pehle se pakadta.**

---

## ⚠️ Traps / Common mistakes

### Trap 1 — is poori journey ko "V1 bekaar tha" ki kahani bana dena
V1 ne correctness-baseline diya, aur uska Execute/Cancel path (direct
iterator access) genuinely achha design tha — V2 ne accidentally ISE
regress kiya.

### Trap 2 — V2 ko poori tarah "galat approach" declare karna
V2 ki Add-improvement (contiguous storage) SAHI thi — problem implementation
detail (index design) mein tha, concept mein nahi.

### Trap 3 — "explain kya badla" ko sirf before/after numbers tak limit
karna
Numbers zaroori hain, par **mechanism** (kyun badla) hi asli samajh hai —
bina mechanism ke, tum agli baar wahi mistake dobara kar sakte (jaisa V2's
index design mistake).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| V1→V2→V3 ek seedhi, monotonic improvement thi | V2 ek genuine (measured) regression tha overall |
| V3 "free" speed deta hai | Explicit trade-offs (bounded range/capacity, complexity) |
| Process (V1→V2→V3) sirf teaching-purpose overhead hai | Real bugs/regressions pehle se pakadta (V2's mistake conscious ho gayi) |
| Sirf numbers dikhana "explain karna" hai | Mechanism (kyun) zaroori hai, sirf before/after nahi |

---

## Exercises

1. Ek naya engineer directly V3-jaisa design likhta hai (bina V1/V2 se
   guzre) aur galti se V2's index-design-mistake reproduce kar deta
   (order-id index mein sirf price store karta, direct handle nahi). Kya
   is process (V1→V2→V3) follow karne se yeh avoid hota?
   <details><summary>Answer</summary>
   Zaroori nahi guaranteed, par CHANCE bahut kam ho jaati — kyunki
   V1→V2 ka measured result specifically is mistake ko HIGHLIGHT karta
   (06's Rule-2 finding), aur V3 design karte waqt yeh explicitly "in mind"
   hota ki index ko DIRECT handle dena chahiye, sirf price nahi. Process
   follow karna guarantee nahi deta, par probability kaafi improve karta
   — aur agar mistake phir bhi ho jaaye, measurement (14 jaisa) usse
   dobara pakad legi.
   </details>

2. "Explain kya badla" requirement (spec) sirf documentation ke liye hai,
   ya iska practical engineering value bhi hai?
   <details><summary>Answer</summary>
   Practical value hai — mechanism samajhna (na sirf number) tumhe agli
   baar SIMILAR situation mein predict karne deta ("yeh optimization idea
   is naye context mein bhi kaam karegi ki nahi") bina phir se poora
   measure-cycle guzre. Yeh transferable understanding banata, na sirf
   "V3 fast hai" ka rote fact.
   </details>

---

## Interview questions

1. Poori V1→V2→V3 journey ek paragraph mein summarize karo (mechanism
   + measured effect ke saath).
2. V2's regression ka root cause aur uska V3 mein fix batao.
3. V3 ke explicit trade-offs kya hain?
4. Yeh process (build-measure-optimize-remeasure-explain) follow karne
   ka practical faayda kya hai, sirf "final fast code" milne ke alawa?

---

## Next
→ [`16-testing-order-book.md`](16-testing-order-book.md)
