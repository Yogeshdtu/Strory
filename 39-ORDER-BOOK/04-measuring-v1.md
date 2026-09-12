# 04 — Measuring V1: latency distribution, bottlenecks

## Prerequisites
- [`03-naive-map-implementation.md`](03-naive-map-implementation.md)
- `examples/02_orderbook_v1_bench.cpp`

## Yeh topic abhi kyun
Spec ka process: correct banao (03), **ab measure karo** — bina yeh kiye,
V2/V3 "improvement" claim karna sirf guess hai (Rule 2).

---

## Measured (`02_orderbook_v1_bench.cpp`, N=200000 ops, `-O2`)

```
=== BookV1 (std::map + std::list + unordered_map) ===
  ALL ops                p50  140.3   p99    831.6   p99.9    1502.9   max  5305437.4  ns
  Add only               p50  130.2   p99    450.9   p99.9   10700.3   max  5305437.4  ns
  Execute/Cancel only    p50  200.4   p99    731.4   p99.9     961.8   max    13585.8  ns
```

**Teen observations:**

1. **p50 chhota (140.3 ns), p99.9 bahut zyada (1502.9 ns, ~10.7x)** —
   35/05 ka classic pattern: typical case tree navigation cheap hai
   (shallow tree, warm cache lines usually), tail mein zyada mehnga
   (rebalancing, allocation, cache misses).

2. **Add ka p99.9 (10700.3 ns) sabse bada hai poori table mein** — yeh
   03's prediction confirm karta: naya price-level create hone pe
   (`side[price]` jab key absent hai) ek tree-node allocation +
   rebalance hota — **allocation ka tail-cost** (36/01 ka pattern), yahan
   order-book context mein.

3. **Execute/Cancel ka p50 (200.4 ns) Add ke p50 (130.2 ns) se zyada hai**
   — interesting, kyunki reduce() mein order tak pahunchna O(1) hai
   (stored iterator). Yeh extra cost `side.find(loc.price)` (ek doosri
   tree lookup, Location se price milne ke baad) + occasionally
   `side.erase(lvl_it)` (level khaali hone pe, ek doosra tree-rebalance)
   se aata — **do tree operations ek reduce mein** (find + maybe-erase),
   `add`'s ek (`side[price]`) ke against.

---

## Bottleneck kya hai (bina profiler ke bhi, structurally pata chalta)

```
std::map  = red-black tree = HAR node ek ALAG heap allocation, pointer-
            linked (left/right/parent child pointers). Traversal/insert/
            erase = pointer chasing, cache-unfriendly (32-CACHE se yaad
            karo -- "1 second = L1" analogy, tree traversal = far jumps).

std::list = HAR order bhi ek ALAG heap allocation.

Net: EK "add" mein potentially 2 heap allocations (map node + list node)
+ tree rebalancing + pointer chasing.
```

Yeh **structural** bottleneck hai — profiler (`perf record`/`perf
annotate`, 35/10-11, Linux pe) isi ko cache-miss stalls ke roop mein
dikhaata (agar Linux/WSL pe verify karo). Is Windows box pe hum sirf
`-O2` measured **numbers** rakhte hain (ratios/shapes port karte, tail
absolutes nahi — 35/06).

---

## V1 ka "achha" hissa bhi note karo (fair rehna zaroori)

- **Execute/Cancel ka p99.9 (961.8 ns) Add ke p99.9 (10700.3 ns) se
  bahut kam hai** — kyunki reduce direct-iterator-access use karta
  (koi list scan nahi), sirf tree operations ka cost hai.
- V1 **correct** hai, aur code **readable** hai — for a first pass, yeh
  bilkul reasonable engineering choice tha.

---

## Agla step

04 ne dikhaya: **allocation (naya price-level) sabse bada tail-source
hai**, aur tree-traversal cache-unfriendly hai. V2 (05) inhe address karne
ki koshish karega — **par 06 ka measured result surprising hoga.**

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sirf p50 dekh ke "V1 theek hai" bol dena
p50 140.3 ns "fine" lag sakta — par p99.9 (1502.9 ns, ~10x) hi HFT ka
scorecard hai (37/02, 35/05). Hamesha tail dekho.

### Trap 2 — Add aur Execute/Cancel ko same "cost profile" maan lena
Numbers alag hain, aur mechanism bhi alag (Add = potentially-new-node,
Execute/Cancel = find + maybe-erase) — breakdown na karte to yeh nuance
miss ho jaata.

### Trap 3 — is data se seedha "map bekaar hai" conclude karna
04 sirf V1's numbers hain — "V2/V3 better honge" abhi tak SIRF theory hai.
06/14 mein measure karke pata chalega kaunsa hissa sach mein improve hota.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| p50 kaafi hai V1 judge karne ke liye | p99.9 (10x zyada) hi real signal hai |
| Add aur Execute/Cancel same cost profile | Alag mechanism, alag numbers (measured) |
| Yeh data batata V2/V3 better honge | Sirf ek baseline hai — 06/14 measure karega |
| `std::map` "hamesha slow" | Structurally cache-unfriendly hai — measured tail confirm karta, par "kitna bura" context-dependent |

---

## Exercises

1. Add ka p99.9 (10700.3 ns) Execute/Cancel ke p99.9 (961.8 ns) se ~11x
   zyada hai. Ek sentence mein KYUN.
   <details><summary>Answer</summary>
   Add occasionally ek NAYA price-level create karta (tree-node allocation
   + rebalance) — Execute/Cancel kabhi naya node create nahi karta (sirf
   existing order ki qty ghataata, aur occasionally EK level erase karta
   jo allocation se sasta hai). Naya-node-creation ka tail cost hi is
   fark ka source hai.
   </details>

2. Agar tum is data ko Linux pe `perf record` + `perf annotate` (35/10-11)
   se profile karte, kaunsi specific instructions/functions expect karoge
   top pe dikhna?
   <details><summary>Answer</summary>
   `std::map` ka internal rebalancing code (`_Rb_tree_insert_and_rebalance`
   jaisa libstdc++ symbol), `operator new`/heap-allocator internals (map
   node + list node allocation ke liye), aur possibly cache-miss-heavy
   pointer dereferences tree-traversal ke andar (`cycle_activity.stalls_
   l3_miss` jaisa counter high hoga — 35/10).
   </details>

---

## Interview questions

1. V1 ke measured p50/p99.9 numbers batao, aur unka ratio kya signal
   deta.
2. Add ka tail Execute/Cancel se zyada kyun hai (mechanism-level answer)?
3. `std::map` + `std::list` ka structural bottleneck kya hai (cache
   perspective se)?
4. Kyun sirf average dekhna is measurement mein misleading hota?

---

## Next
→ [`05-sorted-vector-implementation.md`](05-sorted-vector-implementation.md)
