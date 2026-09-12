# 01 — Complexity analysis (and its limits)

## Prerequisites
- Folder 07 (loops + nested-loop complexity), folder 19 file 25 (container performance)
- Basic algebra (logarithms, exponents)

## Yeh topic abhi kyun
Har algorithm ka ek **cost** hota hai — time aur space. Big-O woh cost ko
**input size ke function** ke roop mein likhne ka tareeka hai, machine-independent.
Yeh interview ki common bhaasha hai. Par is folder ka theme yeh bhi hai: **Big-O
poora sach nahi bolta** — HFT mein ek O(n log n) algorithm ek O(n) waale se tez
ho sakta hai agar cache behaviour better ho. Pehle notation, phir uski seema.

---

## Big-O: asymptotic upper bound

`f(n) = O(g(n))` ka matlab: kisi constant `c` aur `n₀` ke liye, `f(n) ≤ c·g(n)`
har `n ≥ n₀` pe. Yaani **bade n ke liye, `f` `g` se tezi se nahi badhta**.

Rules:
- **Constants drop**: `O(3n)` → `O(n)`. `O(n/2)` → `O(n)`.
- **Lower-order terms drop**: `O(n² + n + 100)` → `O(n²)`.
- **Only the dominant term survives** as `n → ∞`.

| Class | Naam | n=10 | n=1000 | n=10⁶ | Example |
|---|---|---|---|---|---|
| `O(1)` | constant | 1 | 1 | 1 | array index, hash lookup (avg) |
| `O(log n)` | logarithmic | ~3 | ~10 | ~20 | binary search, balanced-tree op |
| `O(n)` | linear | 10 | 1000 | 10⁶ | scan, `std::find`, sum |
| `O(n log n)` | linearithmic | ~33 | ~10⁴ | ~2×10⁷ | `std::sort`, merge sort |
| `O(n²)` | quadratic | 100 | 10⁶ | 10¹² | bubble sort, all-pairs |
| `O(n³)` | cubic | 1000 | 10⁹ | 10¹⁸ | naive matrix multiply, Floyd-Warshall |
| `O(2ⁿ)` | exponential | 1024 | ~10³⁰¹ | — | naive subset enumeration, naive fib |
| `O(n!)` | factorial | ~3.6M | — | — | brute-force TSP, all permutations |

**Rule of thumb** (~1 sec budget, ~10⁸–10⁹ simple ops):
- `n ≤ 10` → `O(n!)` / `O(2ⁿ)` ok
- `n ≤ 20` → `O(2ⁿ)` ok
- `n ≤ 500` → `O(n³)` ok
- `n ≤ 5000` → `O(n²)` ok
- `n ≤ 10⁶` → `O(n log n)` ok
- `n ≤ 10⁸` → `O(n)` ok

`examples/01_sorting_all.cpp` measures it: `O(n²)` bubble sort on **n=20,000**
takes ~730 ms; `std::sort` (`O(n log n)`) on the **same** input takes ~1 ms.

---

## Big-Θ and Big-Ω

- `O(g)` — upper bound ("at most").
- `Ω(g)` — lower bound ("at least").
- `Θ(g)` — both ("exactly, up to constants"). `f = Θ(g)` iff `f = O(g)` **and**
  `f = Ω(g)`.

Loose usage: people say "O(n log n)" for `std::sort` when they mean `Θ(n log n)`.
Comparison sorting has a proven lower bound of `Ω(n log n)` — no comparison sort
can beat it.

---

## Best / average / worst case

The **same** algorithm can have different complexity depending on the input:

| Algorithm | Best | Average | Worst |
|---|---|---|---|
| Quicksort | `O(n log n)` | `O(n log n)` | `O(n²)` (bad pivots) |
| Insertion sort | `O(n)` (already sorted) | `O(n²)` | `O(n²)` |
| Hash lookup | `O(1)` | `O(1)` | `O(n)` (all collide) |
| Binary search | `O(1)` (mid hit) | `O(log n)` | `O(log n)` |

Quote the one that matters for your use. Latency-critical code cares about
**worst case** (a tail-latency spike from a rehash or an `O(n²)` quicksort path
can blow a budget) — this is why `std::sort` uses introsort (`O(n log n)`
guaranteed) and HFT avoids `std::unordered_map`'s rehash.

---

## Amortized analysis

Some operations are usually cheap but occasionally expensive; **amortized** cost
averages over a sequence.

`std::vector::push_back`: usually `O(1)` (write + `++size`), but when capacity is
full it reallocates and moves everything → `O(n)`. Over `n` pushes with geometric
(2×) growth: total work `= n + (1 + 2 + 4 + … + n) ≈ 3n` → **`O(1)` amortized**.

Not the same as "average case": amortized is a *guarantee over the sequence*, no
probability involved. (A `push_back` that reallocates is still `O(n)` that one
time — bad for a single latency measurement, fine for throughput.)

---

## Space complexity

Same notation, for **extra** memory beyond the input:
- Merge sort: `O(n)` scratch buffer.
- Quicksort: `O(log n)` recursion stack (if you recurse into the smaller half).
- Heapsort / in-place quickselect: `O(1)`.
- Memoized recursion: `O(n)` (or `O(states)`) table + `O(depth)` stack.

Time–space trade-offs are everywhere: `examples/07_dp_problems.cpp` — knapsack
`O(n·cap)` time either way, but `O(n·cap)` space (2D table) vs `O(cap)` space
(rolling 1D row).

---

## Where Big-O lies (the folder's thesis)

Big-O counts **operations**, assuming every operation costs the same. On real
hardware **it doesn't** — a cache miss is ~100–300 cycles, an L1 hit is ~4.

1. **Constants matter at real sizes.** `O(n log n)` merge sort with `O(n)` heap
   allocations can lose to `O(n²)` insertion sort for `n < 64` (which is why
   `std::sort` finishes with insertion sort).
2. **The memory access pattern dominates.** `examples/03_linked_list.cpp`:
   traversing a 5M-node `list` (`O(n)`) is **~40×** slower than summing a 5M-int
   `vector` (`O(n)`) — same Big-O, all cache.
3. **`examples/08_flat_vs_pointer.cpp`**: identical `O(n)` tree traversal — flat
   array layout is **4–11×** faster than a pointer tree.
4. Worst-case vs average matters for latency, not just "is it fast on average".

Big-O is the **first** filter (rule out the `O(2ⁿ)` on `n=10⁶`), not the last
word. After it: measure with real data and a real access pattern.

---

## Andar kya hota hai

- The "operation" Big-O counts is an abstraction. A comparison, an array index,
  a pointer dereference all count as 1 — but a pointer dereference to cold memory
  is ~50× a comparison of two registers.
- `log n` in Big-O is base 2 for divide-and-conquer, but the base only changes
  the constant (`log₂ n = log₁₀ n / log₁₀ 2`), so it's dropped. `n = 10⁶` →
  `log₂ n ≈ 20`.
- `O(n log n)` sorts do ~`n·log₂ n` comparisons; for `n = 10⁶` that's ~2×10⁷ —
  cheap. The `O(n²)` version does 10¹² — ~50,000× more.
- Amortized `O(1)` `push_back` still has a worst-case `O(n)` event; profilers
  show it as a latency spike. `reserve()` removes it.

> **HFT relevance:** Big-O rules out the disasters (no `O(n²)` on the hot path,
> no `O(2ⁿ)` anywhere in steady state) but the actual latency winner is decided
> by **cache misses, branch predictability, and worst-case bounds**, not the
> exponent. A sorted `std::vector` + binary search (`O(log n)`, contiguous) beats
> a `std::map` (`O(log n)`, pointer-chasing) by ~3× (folder 19 file 25). An
> `O(n)` linear scan over 16 contiguous elements beats an `O(1)` hash lookup
> (hashing + a pointer chase). Quote worst case, because a p99.9 spike from an
> `O(n²)` fallback or a rehash is what gets you fired. Measure after you reason.

---

## Hands-on

```bash
./build.ps1 fast 20-ALGORITHMS-DSA/examples/01_sorting_all.cpp
./build.ps1 fast 20-ALGORITHMS-DSA/examples/03_linked_list.cpp
./build.ps1 fast 20-ALGORITHMS-DSA/examples/08_flat_vs_pointer.cpp
```

Note the `O(n²)` vs `O(n log n)` gap at n=20,000, and the two `O(n)`-vs-`O(n)`
comparisons where layout alone gives 4–40×.

---

## ⚠️ Traps

### Trap 1 — dropping a constant that's actually huge
```cpp
// "both O(n)" -- but one does a syscall per element and the other a register add.
// Big-O equal, wall-clock 1000x apart.
```

### Trap 2 — using average case for a latency budget
```cpp
// std::unordered_map lookup is O(1) average -- but a rehash is O(n), and it lands
// on some unlucky insert. For p99.9, that's the number that matters.
```

### Trap 3 — `O(n²)` hidden in a library call inside a loop
```cpp
for (int x : v) if (std::find(w.begin(), w.end(), x) != w.end()) ...;   // O(n*m), not O(n)
```

### Trap 4 — assuming `log` base matters
```cpp
// O(log2 n) and O(log10 n) are the SAME class -- the base is a constant factor.
```

### Trap 5 — counting recursion stack as free
```cpp
// A recursive O(n) tree walk uses O(height) stack -- O(n) for a degenerate tree
// (examples/05_bst.cpp: 1023 ascending inserts -> height 1023 -> stack overflow risk).
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "O(n) always beats O(n log n)" | Only for large n; constants + cache can flip it at real sizes |
| "Big-O tells you the running time" | It tells you how it **scales**; the constant (and hardware) set the actual time |
| "Amortized O(1) means every call is O(1)" | The average over a sequence is O(1); individual calls can be O(n) |
| "O(2n) is worse than O(n)" | Same class — constants are dropped |
| "Best/worst/average are the same thing" | Different inputs; quote the one your use case cares about (worst, for latency) |

---

## Exercises

1. **Classify:** give the tightest Big-O for: (a) two nested loops each `0..n`,
   (b) a loop that halves `n` each step, (c) `std::sort` then one linear scan,
   (d) a recursive function calling itself twice with `n-1`.

   <details><summary>Answer</summary>

   (a) `O(n²)`. (b) `O(log n)`. (c) `O(n log n) + O(n) = O(n log n)`. (d) `O(2ⁿ)`
   (each level doubles the calls) — unless memoized, then `O(n)`.
   </details>

2. **Feasibility:** you have `n = 100,000` and a 1-second budget. Which are
   feasible: `O(n²)`, `O(n log n)`, `O(n√n)`, `O(n)`?

   <details><summary>Answer</summary>

   `O(n²)` = 10¹⁰ — too slow (~10–100 s). `O(n√n)` ≈ 3×10⁷ — fine. `O(n log n)` ≈
   1.7×10⁶ — fine. `O(n)` — trivial.
   </details>

3. **Amortized:** `std::vector<int> v;` then 1,000,000 `push_back`s with no
   `reserve` (2× growth). Total element-move work? Amortized cost per push?

   <details><summary>Answer</summary>

   Reallocations at capacity 1,2,4,…,~10⁶ → total moves ≈ 1+2+…+2¹⁹ ≈ 10⁶ ≈ n.
   Total work ≈ `n (pushes) + n (moves)` = `O(n)` → `O(1)` amortized per push.
   </details>

4. **Big-O lies:** `examples/03` shows a 5M-node linked-list sum at ~70 ms and a
   5M-int vector sum at ~1.7 ms — both `O(n)`. Explain the 40×.

   <details><summary>Answer</summary>

   `vector`: contiguous → prefetcher streams it, loop auto-vectorizes, ~1 cache
   miss per 16 ints. `list`: each `->next` is an unpredictable heap address → a
   cache miss (~200 cycles) almost every node, no prefetch, no SIMD, and the
   dependent load serializes iterations.
   </details>

5. **Worst vs average:** why does `std::sort` use introsort instead of plain
   quicksort, and why does that matter for a latency-sensitive system?

   <details><summary>Answer</summary>

   Plain quicksort is `O(n²)` worst case (adversarial or already-sorted input
   with a bad pivot). Introsort switches to heapsort once recursion goes too
   deep → **guaranteed `O(n log n)`**. A latency system can't tolerate an
   occasional `O(n²)` blowup on the hot path — the p99.9 would be catastrophic.
   </details>

---

## Interview questions

1. Big-O, Big-Θ, Big-Ω — teenon ka fark?
2. Amortized `O(1)` vs average-case `O(1)` — kaise alag?
3. `std::vector::push_back` amortized `O(1)` kaise (geometric growth)?
4. Do algorithms dono `O(n)` — ek 40× slow kyun ho sakta (cache)?
5. Latency-critical code worst case kyun quote karta, average nahi?
6. Comparison sort ka lower bound `Ω(n log n)` kyun?

---

## Next
→ [`02-arrays-and-two-pointers.md`](02-arrays-and-two-pointers.md)
