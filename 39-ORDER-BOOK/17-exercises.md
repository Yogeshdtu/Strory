# 17 — Exercises: order book (extensions + challenges)

## Prerequisites
- Poora folder `39` (`01`–`16`) + `examples/`

## Kaise use karein
- **Part A** — concept recall.
- **Part B** — design/scenario questions.
- **Part C** — hands-on: `examples/` modify karo, predict karo, verify.
- **Part D** — extension challenges (thoda harder, code likhna padega).
- Folder-wide interview questions end mein.

---

## Part A — Concept recall

### A1
Order book aur matching engine ka scope-fark ek sentence mein batao.
<details><summary>Answer</summary>
Order book market-data events (Add/Execute/Cancel/Delete/Replace) apply
karke apni STATE maintain karta; matching engine incoming aggressive
orders ko resting orders se MATCH karta aur khud events GENERATE karta
(01).
</details>

### A2
V2 (sorted vector) ka measured result V1 (map) se OVERALL better tha ya
worse? Kyun?
<details><summary>Answer</summary>
Overall WORSE (p50 150.3 vs 140.3 ns, p99.9 2745.2 vs 1502.9 ns) — Add
path behtar hua (contiguous array), par Execute/Cancel path worse hua
(order-id index sirf price store karta, level ke andar linear scan
zaroori — V1's direct-iterator-access ka fayda kho gaya) (06).
</details>

### A3
V3 ka `NUM_LEVELS` bounded-range trade-off kya hai?
<details><summary>Answer</summary>
Price ek fixed range (`center ± NUM_LEVELS`) ke bahar reject ho jaata
(`add()` false return karta) — V1/V2 jo koi bhi price handle kar sakte
the (dynamic structures), V3 explicit range-constraint leta O(1) access
ke against (07).
</details>

---

## Part B — Design / scenario questions

### B1
Ek naya engineer V3-style flat array design directly likhta hai (bina
V1/V2 se guzre), aur unki order-id index sirf `{price}` store karti hai
(V2's mistake, unknowingly reproduced). Kya symptom dikhega measure karne
pe?
<details><summary>Answer</summary>
Execute/Cancel operations mein ek linear-scan cost dikhega (level ke
andar order dhoondhne ke liye) — bilkul V2 jaisa regression, chahe
price-level storage khud O(1) ho. Yeh dikhata hai ki "price-level access
fast hai" aur "order-within-level access fast hai" DO ALAG cheezein hain
— dono independently sahi karni padti (08's fix inhe alag-alag address
karta).
</details>

### B2
Ek symbol ki price achanak `NUM_LEVELS` range se bahar chali jaati hai
(bada move). `add()` false return karta rehta naye orders ke liye. Kya
karoge (37/13's fail-closed principle se connect karo)?
<details><summary>Answer</summary>
Silent failure allow NAHI karna chahiye — return value check karke
detect karo, aur symbol ko "stale"/"needs re-center" mark karo (37/13,
38/13 ka pattern). Off-hot-path, book ko naye `center_` ke around
REBUILD karo (poore existing orders ko naye indices pe re-insert karo),
phir naye orders accept karna resume karo. Yeh operation rare honi
chahiye (bada price move), isliye iski cost (O(order_count)) acceptable
hai jab tak hot-path pe na ho.
</details>

### B3
Tumhara book multi-threaded ho gaya hai (ek thread market-data se update
karti, doosri snapshot leti — 13). Kaunsa concurrency primitive (26/28 se)
sabse fit hai, aur kyun?
<details><summary>Answer</summary>
Seqlock (28-LOCK-FREE) — "frequently write, occasionally read" pattern
ke liye ideal. Writer (market-data thread) KABHI block nahi hota (sirf
apna sequence number badhaata, hot-path unaffected rehta); reader
(snapshot thread) consistency verify karta retry-loop se. Mutex writer
ko bhi block kar sakta agar reader lock hold kar raha ho — undesirable
hot-path ke liye (13).
</details>

---

## Part C — Hands-on

### C1
`06_orderbook_v3_bench.cpp` mein `NUM_LEVELS` ko chhota kar do (jaise
`orderbook_v3_flat.hpp` mein `NUM_LEVELS = 50`, workload `price_range`
100 hai). Predict karo kya hoga, phir verify karo.
<details><summary>Answer</summary>
Bahut saari Add calls FAIL hongi (return false) kyunki workload prices
`price_range=100` tak jaati hain, jo `NUM_LEVELS=50` se bahar hai. Agar
benchmark loop return values check nahi karta (jaisa is folder ke
benchmarks karte — sirf timing, correctness demo alag hai), book V1/V2 se
SILENTLY DIVERGE ho jaayegi (kam orders honge kyunki kai Add reject hue) —
`final order_count` V1/V2 se KAM aayega. Yeh bilkul B2 jaisa scenario hai,
practically demonstrate kiya.
</details>

### C2
`08_orderbook_tests.cpp` mein ek naya test add karo: "delete ek order jo
already delete ho chuka" (double-delete). Sab teeno versions same
behavior dete hain?
<details><summary>Answer</summary>
Haan — `remove()` doosri baar `index_.find()`/`id_index_.find()` mein
"not found" milega (pehli delete ne already hata diya), `false` return
hoga. Sab teeno consistent contract follow karte (12's "consistent
error-handling semantics" principle) — double-delete crash nahi karta,
gracefully `false` deta.
</details>

### C3
`09_orderbook_fuzz.cpp` ka seed badal ke 5-10 alag runs karo. Kya
`disagree` count kabhi 0 se zyada aata?
<details><summary>Answer</summary>
Nahi hona chahiye (agar implementation sahi hai) — deterministic testing
ka poora point yehi hai: agar koi bug exist karta, WOH bug HAR seed pe
(agar us specific pattern ko trigger kare) consistently reproduce hoga,
"kabhi kabhi" flaky nahi hoga (assuming koi undefined behavior/race
condition nahi hai — single-threaded, deterministic RNG context mein).
</details>

---

## Part D — Extension challenges

### D1
`ids_at_price()` ko modify karo taaki woh **qty bhi** return kare (na
sirf id) — `std::vector<std::pair<OrderId, Qty>>`. Teeno versions mein
implement karo.

### D2
Ek `total_bid_depth(int max_levels)` function likho jo top N bid levels
ka total qty sum kare (37/06 ka "depth" concept). Konsi version mein yeh
sabse easy hai, aur kyun?
<details><summary>Answer (hint)</summary>
V3 mein sabse easy — `best_bid_idx_` se shuru karke array-index badhate
jao, non-empty levels ka `total_qty` sum karo, `max_levels` non-empty
levels milne tak ya array-end tak. V1/V2 mein bhi possible (map/vector
iterate karo `begin()`/`front()` se), par V3 ka index-based access thoda
zyada direct hai.
</details>

### D3
`FlatIdIndex` (09) mein ek `resize_and_rehash()` method add karo jo
tombstones ko clear kare (poora table naye array mein rebuild kare, sirf
live entries copy karke). Kab isse call karna chahiye (heuristic socho —
tombstone-count vs capacity ka ratio)?

### D4
V3 mein re-centering (`recenter(Price new_center)`) implement karo — sab
existing orders ko naye center ke around naye indices pe move karo. Yeh
operation kitni expensive hai (Big-O), aur kyun off-hot-path honi chahiye?

### D5 (challenging)
Ek `iterate_levels(bool is_buy, int max_levels, Callback cb)` function
design karo jo teeno versions (V1/V2/V3) pe **generic template code** se
kaam kare — bina har version ke liye alag implementation likhe. Kya
challenge aata (hint: V1/V2/V3 ke internal iteration mechanisms bahut
alag hain)?

---

## Folder-wide interview questions

1. Order book vs matching engine — scope fark.
2. V1 (map+list+unordered_map) ka poora design, aur measured p50/p99.9.
3. V2 ka DOUBLE effect — Add better, Execute/Cancel worse — mechanism
   aur measured numbers.
4. V2's regression ka root cause exactly kya tha (code-level)?
5. V3's flat-array price-to-index mapping formula.
6. Intrusive linked list `std::list` se kaise alag hai?
7. Open-addressed hash table mein tombstone kyun zaroori hai?
8. Price-time priority (FIFO) kahan se guarantee hoti (mechanism)?
9. Top-of-book fast path — V1/V2 mein implicit, V3 mein explicit kyun?
10. `best_bid()` ka precondition kya hai, aur violate karne pe kya hota
    (real bug story is folder se)?
11. Replace ko "remove+add" se implement karna kyun achha design hai?
12. Snapshot generation ka complexity kya hai, hot-path pe kyun nahi?
13. Poora V1→V2→V3 process summarize karo (mechanism + numbers).
14. Teen testing layers batao, har ek kis bug-class pakadta.
15. V3 ke explicit trade-offs kya hain (bounded range/capacity)?
16. Cross-version equivalence + reference model dono kyun zaroori hain
    (ek doosre ka replacement nahi)?

---

## Next
→ [`../40-MATCHING-ENGINE/00-README.md`](../40-MATCHING-ENGINE/00-README.md)
