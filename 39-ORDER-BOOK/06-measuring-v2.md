# 06 — Measuring V2: kya improve hua, kya NAHI (the honest surprise)

## Prerequisites
- [`05-sorted-vector-implementation.md`](05-sorted-vector-implementation.md)
- `examples/04_orderbook_v2_bench.cpp`

## Yeh topic abhi kyun
**Yeh is poore folder ka sabse important lesson hai.** CLAUDE.md Rule 2:
"agar measurement expectation se ulta aaye — usse chhupao mat, wahi teach
karo." V2 ka result exactly aisa hai.

---

## Measured (`04_orderbook_v2_bench.cpp`, SAME workload as V1, N=200000, `-O2`)

```
=== BookV2 (sorted vector + deque + unordered_map) ===
  ALL ops                p50  150.3   p99   1312.5   p99.9    2745.2   max  5102127.4  ns
  Add only               p50  110.2   p99    511.0   p99.9    2755.2   max  5102127.4  ns
  Execute/Cancel only    p50  360.7   p99   1172.2   p99.9    2063.9   max   158170.6  ns
```

**V1 se side-by-side:**

| | V1 (map) | V2 (sorted vector) | Better? |
|---|---|---|---|
| **ALL ops p50** | 140.3 ns | 150.3 ns | ❌ V2 **worse** |
| **ALL ops p99.9** | 1502.9 ns | 2745.2 ns | ❌ V2 **worse** (~1.8x) |
| **Add p99.9** | 10700.3 ns | 2755.2 ns | ✅ V2 **better** (~3.9x) |
| **Execute/Cancel p50** | 200.4 ns | 360.7 ns | ❌ V2 **worse** (~1.8x) |
| **Execute/Cancel p99.9** | 961.8 ns | 2063.9 ns | ❌ V2 **worse** (~2.1x) |

**V2 sorted vector — "cache-friendly" — OVERALL WORSE hai V1 (tree-based)
se.** Yeh 02's warning ("contiguous ≠ automatically faster") ka concrete
proof hai.

---

## Kya sach mein hua — do effects, do directions

### Effect 1 (jo expect kiya tha): Add behtar hui

Add p99.9: **10700.3 → 2755.2 ns (~3.9x behtar).** Yeh theek expectation
ke mutabik hai — naya price-level create karna ab ek contiguous-array
insert hai (binary search + shift), tree-node-allocation-aur-rebalance
se sasta.

### Effect 2 (jo expect NAHI kiya tha): Execute/Cancel bahut worse ho gaya

Execute/Cancel p50: **200.4 → 360.7 ns (~1.8x WORSE).** Yeh 05 mein
already flag kiya gaya extra step hai — `index_` ab sirf `{is_buy, price}`
store karta (kyunki vector iterators unsafe hain), isliye har `reduce()`/
`remove()` ko ek **`std::find_if` linear scan** karna padta level ke
andar specific order dhoondhne ke liye. V1 ka `index_` seedha `list::
iterator` deta tha — O(1), koi scan nahi.

```
V1 reduce: index_ -> DIRECT list-node access (O(1))
V2 reduce: index_ -> price -> level -> LINEAR SCAN orders (O(level size))
```

Is workload mein average ~40+ orders/level hain — is scan ka cost
Execute/Cancel ke poore latency-budget ka bada hissa ban jaata.

### Net effect: Execute/Cancel workload ka bada hissa hai (~29%), isliye
overall V2 worse

Add ~55% ops hai, Execute+Cancel ~29% hai (Delete/Replace baaki ~16%).
Add ka gain (3.9x behtar) itna bada hissa cover nahi karta jitna
Execute/Cancel ka loss (1.8-2.1x worse) le leta — net **ALL ops** number
V1 se worse aata hai.

---

## Yeh Rule 2 ka textbook example kyun hai

```
Hypothesis: "sorted vector cache-friendly hai, isliye order book fast
             hoga" -- INTUITIVE, REASONABLE-SOUNDING.

Measurement: V2 OVERALL worse nikla.

Kya kiya: hypothesis ko discard NAHI kiya (woh partially SAHI thi -- Add
          genuinely behtar hui). Root-cause dhoondha: EK specific design
          choice (index sirf price store karna, iterator nahi) ne EK
          specific operation (reduce/remove) mein ek NAYA cost (linear
          scan) inject kar diya jo V1 mein exist hi nahi karta tha.
```

**Yeh guess nahi tha — measure karke, breakdown (Add vs Execute/Cancel)
dekh ke, mechanism trace kiya gaya.** Isi tarah har surprising result ko
handle karna chahiye (35's poora ethos): number chhupao mat, uski WAJAH
dhoondo.

---

## Fix kya hoga (preview — V3, 07-12)

V2 ki mistake fix karne ke DO tareeke:
1. **Order-within-level ko bhi index-able banao** — jaise ek `unordered_
   map<OrderId, size_t>` per level (position track kare) — par yeh apna
   khud ka maintenance-overhead lata (position shift hone pe update
   chahiye).
2. **Poori tarah alag approach** — arena-backed intrusive list, jahan
   index_ SEEDHA arena-slot deta (stable, kabhi invalidate nahi hota,
   kyunki arena mein elements MOVE nahi hote, sirf free-list se
   recycle hote). Yeh **V3 ka approach hai** (08-09).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sirf "Add" number dekh ke V2 ko "success" declare karna
Agar tum sirf Add ka p99.9 dekhte (3.9x behtar!), tum V2 ko ship kar dete
— aur production mein Execute/Cancel-heavy workload pe **worse**
performance dete. **Poora operation mix measure karna zaroori hai.**

### Trap 2 — is result ko "sorted vector bekaar hai" generalize karna
Galat generalization. Sorted vector ka Add-path genuinely behtar hai —
problem ek SPECIFIC implementation choice (index design) mein thi, sorted
vector ke concept mein nahi. V3 sorted-array-jaisa benefit rakhega bina
is mistake ke.

### Trap 3 — measurement ko "noise" maan ke ignore karna
Yeh ek consistent, repeatable, mechanism-explainable result hai — koi
one-off anomaly nahi. Ignore karna galat hoga.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| V2 "obviously" V1 se fast hoga | Measured: overall WORSE (Execute/Cancel regression) |
| Sorted vector ek galat idea thi | Idea sahi thi (Add behtar), implementation ki EK choice galat thi |
| Sirf headline number (Add) dekhna kaafi hai | Poora operation-mix breakdown zaroori |
| Surprising result = measurement error | Yeh consistent, mechanism-traceable result hai |

---

## Exercises

1. Agar workload mein Execute/Cancel sirf 5% hota (95% Add), kya overall
   result badalta?
   <details><summary>Answer</summary>
   Haan, likely — Add ka bada gain (3.9x behtar) tab poore workload pe
   dominate karega, aur Execute/Cancel ka loss (chhota fraction hone se)
   overall number ko utna neeche nahi khinchega. Yeh dikhata ki "kaunsa
   design behtar hai" **workload-dependent** hai — yeh khud ek important
   lesson: benchmark tumhare REAL workload jaisa hona chahiye, generic
   nahi.
   </details>

2. V2 ka Execute/Cancel p99.9 (2063.9 ns) V1 ke Execute/Cancel p99.9
   (961.8 ns) se zyada hai — par V1 ke Add p99.9 (10700.3) se KAM hai.
   Iska matlab kya hai practically?
   <details><summary>Answer</summary>
   V2 ka Execute/Cancel worst-case, V1 ke Add worst-case se abhi bhi behtar
   hai — matlab agar tumhari system ka bottleneck specifically V1's Add
   tail thi, V2 (chahe apni khud ki naya regression ke saath) us specific
   problem ko already improve kar deta. Yeh optimization "trade-offs"
   ki nature dikhata — koi version universally best nahi, context/metric
   pe depend karta.
   </details>

---

## Interview questions

1. V2 ka overall result kya tha, aur V1 se kaise compare karta (numbers
   ke saath)?
2. Do effects (Add behtar, Execute/Cancel worse) ka mechanism explain
   karo.
3. Kyun yeh ek "Rule 2" moment hai — measurement expectation se ulta
   kyun aaya?
4. Is result se galat generalization kya hogi, aur sahi lesson kya hai?

---

## Next
→ [`07-flat-array-book.md`](07-flat-array-book.md)
