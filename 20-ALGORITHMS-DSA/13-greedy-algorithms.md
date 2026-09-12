# 13 — Greedy algorithms

## Prerequisites
- [`03-sorting-algorithms.md`](03-sorting-algorithms.md), [`10-heaps.md`](10-heaps.md), [`12-graphs.md`](12-graphs.md)

## Yeh topic abhi kyun
Greedy = har step pe **jo abhi best lagta hai woh chuno**, aur kabhi peeche mat
dekho. Jab yeh kaam karta hai, code chhota aur tez hota (aksar `O(n log n)`, sort
+ ek pass). Par yeh **aksar galat** hota — mushkil part yeh saabit karna ki
greedy is problem pe optimal hai. Yeh lesson: kab bharosa karein, kaise check
karein.

---

## The pattern

1. Sort the items by some key (or use a heap to always pull the current best).
2. Iterate once, making an **irreversible** local choice each step.
3. `O(n log n)` total (the sort dominates), `O(1)`–`O(n)` space.

Contrast with DP (file 14), which **considers all choices** and is used exactly
when the greedy choice isn't safe.

---

## Classic greedy problems that work

### Activity selection / interval scheduling (max non-overlapping)
Sort intervals by **end time**; take each interval whose start is `≥` the last
taken end. Greedy on earliest-finish is optimal — finishing early leaves the most
room.

```cpp
std::sort(iv.begin(), iv.end(), [](auto a, auto b){ return a.end < b.end; });
int lastEnd = INT_MIN, count = 0;
for (auto [s, e] : iv) if (s >= lastEnd) { ++count; lastEnd = e; }
```

### Fractional knapsack
Sort by **value/weight ratio** descending; take as much of each as fits (the last
one fractionally). Optimal because you can split items. (The **0/1** version —
no splitting — is **not** greedy-solvable; it needs DP, file 14.)

### Huffman coding
Repeatedly merge the **two lowest-frequency** nodes (a min-heap) into a subtree.
Produces an optimal prefix-free code. `O(n log n)`.

### Dijkstra & Prim / Kruskal (MST)
- **Dijkstra** (file 12): greedily settle the nearest unsettled node.
- **Prim**: grow the MST by repeatedly adding the cheapest edge leaving the tree
  (a min-heap of crossing edges).
- **Kruskal**: sort all edges by weight, add each if it doesn't form a cycle
  (union-find). `O(E log E)`.

### Coin change — **only for canonical systems**
Greedy (take the largest coin ≤ remainder) is optimal for `{1, 5, 10, 25}` and
most real currencies, **but not in general** — for `{1, 3, 4}` making 6, greedy
gives `4 + 1 + 1` (3 coins), optimal is `3 + 3` (2). General coin change needs DP.

### Jump game, gas station, task assignment, meeting rooms
Many interview problems have a clean greedy once you find the right sort key or
invariant.

---

## When greedy fails

| Problem | Greedy gives | Why it's wrong | Correct approach |
|---|---|---|---|
| 0/1 knapsack | best ratio first | can't split; a heavy high-ratio item blocks two better ones | DP `O(n·W)` |
| Coin change `{1,3,4}`, target 6 | `4+1+1` | largest coin isn't always in the optimal set | DP |
| Longest path in a DAG by "biggest next edge" | local max | a small edge now can unlock a huge tail | DP over topo order |
| Travelling salesman "nearest unvisited" | short-sighted route | early cheap hops force expensive later ones | DP (bitmask) / heuristics |
| Matrix-chain multiplication | split at cheapest single op | ignores downstream cost | DP `O(n³)` |

**Rule:** greedy is a *guess*. It's optimal only when the problem has:
- **Greedy-choice property** — a globally optimal solution can be reached by a
  locally optimal (greedy) choice at each step.
- **Optimal substructure** — an optimal solution contains optimal solutions to
  subproblems (DP has this too; greedy additionally needs the first property).

---

## How to check a greedy is correct

1. **Exchange argument** — assume an optimal solution `OPT` differs from the
   greedy one; show you can swap `OPT`'s first differing choice for the greedy
   choice **without making it worse**. By induction, greedy is optimal. (This is
   the standard proof for activity selection, Huffman, Kruskal.)
2. **Matroid theory** — if the problem's feasible sets form a *matroid*, the
   greedy algorithm on weights is provably optimal (MST is the classic example).
3. **Brute-force check** — write a tiny exponential brute force and a random test
   harness; run both on thousands of small random inputs and diff. Catches a
   wrong greedy fast. **Do this before trusting a greedy in an interview or in
   production.**

---

## Andar kya hota hai

- Greedy's speed comes from **not branching**: one sort (`O(n log n)`,
  cache-friendly introsort — file 04) plus one linear pass. No recursion tree, no
  memo table, `O(1)` working set. When it's valid, it's usually the fastest
  possible.
- The heap-driven greedies (Huffman, Prim, Dijkstra) are `O(n log n)` /
  `O(E log V)` — the `log` is the heap. A `std::priority_queue` over a
  `std::vector` keeps it contiguous.
- The failure modes are **not** performance — a wrong greedy runs fast and
  returns a wrong answer. That's the danger: it looks done.

> **HFT relevance:** greedy fits the **latency budget** perfectly — sort once,
> one pass, no allocation, bounded work — so where a greedy is provably optimal
> (interval scheduling of tasks, earliest-deadline-first, cheapest-venue-first
> order routing when routing cost is separable) it's the right tool. But a
> **wrong greedy in a routing or sizing decision silently loses money**, and
> unlike a crash it won't page anyone. So: prove it (exchange argument) or
> brute-force-check it against a reference on random inputs, and keep the check in
> the test suite. When the greedy-choice property genuinely fails (position
> sizing with interacting constraints, multi-leg optimization), it's DP or an
> optimizer, not greedy.

---

## Hands-on

```bash
./build.ps1 20-ALGORITHMS-DSA/examples/06_graph_algorithms.cpp   # Dijkstra is a greedy
./build.ps1 fast 20-ALGORITHMS-DSA/examples/07_dp_problems.cpp    # knapsack: greedy fails, DP needed
```

Implement: activity selection (sort by end), fractional knapsack (sort by ratio),
Huffman (min-heap). Then write a brute-force 0/1 knapsack and a greedy
"best-ratio-first" 0/1 knapsack; run both on random small inputs and find a case
where greedy loses.

---

## ⚠️ Traps

### Trap 1 — sorting by the wrong key
```cpp
// activity selection sorted by START time (or by duration) -> NOT optimal. Sort by END time.
```

### Trap 2 — assuming greedy coin change is always optimal
```cpp
// True for {1,5,10,25}. For {1,3,4} target 6, greedy = 3 coins, optimal = 2. Use DP for arbitrary denominations.
```

### Trap 3 — using greedy for 0/1 knapsack
```cpp
// "highest value/weight first" fails when items can't be split. examples/07 needs the DP table.
```

### Trap 4 — no proof and no brute-force check
```cpp
// A wrong greedy runs fast and returns a plausible wrong answer. Verify with an exchange argument or a reference test.
```

### Trap 5 — irreversible choice that a later step regrets
```cpp
// If a locally-best pick can block a strictly-better combination downstream, greedy is unsafe -> DP.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Greedy is a general strategy that usually works" | It works only when the greedy-choice property holds — prove it |
| "Greedy coin change is optimal" | Only for canonical denomination systems; general case needs DP |
| "Fractional and 0/1 knapsack both greedy" | Fractional yes; 0/1 needs DP (can't split items) |
| "A fast wrong answer is a bug you'll notice" | A wrong greedy doesn't crash — it silently returns suboptimal |
| "If greedy passes my examples it's correct" | Random brute-force diffing catches counterexamples your examples miss |

---

## Exercises

1. **Activity selection:** intervals `[(1,3),(2,5),(4,7),(1,8),(5,9),(8,10)]`.
   Max non-overlapping, greedy by end time. Trace.

   <details><summary>Answer</summary>

   Sorted by end: `(1,3),(2,5),(4,7),(5,9),(1,8),(8,10)`. Take `(1,3)`
   [end 3]. `(2,5)` starts 2 < 3, skip. `(4,7)` starts 4 ≥ 3, take [end 7].
   `(5,9)` 5 < 7 skip. `(1,8)` 1 < 7 skip. `(8,10)` 8 ≥ 7, take. → 3 activities:
   `(1,3),(4,7),(8,10)`.
   </details>

2. **Exchange argument:** sketch the proof that "earliest finish time" is optimal
   for activity selection.

   <details><summary>Answer</summary>

   Let `OPT` be an optimal set; let `a` be greedy's first pick (earliest finish).
   If `OPT`'s first activity isn't `a`, swap it for `a` — `a` finishes no later,
   so it still doesn't overlap `OPT`'s second activity → the swapped set is
   feasible and the same size. Recurse on the remaining activities that start
   after `a`. By induction, greedy matches `OPT` in size.
   </details>

3. **Counterexample:** find an input where greedy 0/1 knapsack (best ratio
   first) is worse than optimal.

   <details><summary>Answer</summary>

   Capacity 10; items `(w=6, v=7)` (ratio 1.17), `(w=5, v=5)`, `(w=5, v=5)`.
   Greedy takes the ratio-1.17 item then can't fit a 5 → value 7. Optimal takes
   the two 5s → value 10.
   </details>

4. **Coin change:** for denominations `{1, 7, 10}`, is greedy optimal for making
   14? For all amounts?

   <details><summary>Answer</summary>

   Making 14: greedy `10 + 1×4` = 5 coins; optimal `7 + 7` = 2 coins. Greedy is
   not optimal for this system → use DP `dp[a] = 1 + min(dp[a - c])`.
   </details>

5. **Kruskal validity:** why is Kruskal (sort edges, add if no cycle) guaranteed
   to produce a minimum spanning tree?

   <details><summary>Answer</summary>

   The set of forests of a graph forms a **matroid**, and the greedy algorithm on
   a matroid with edge weights yields a maximum/minimum-weight basis. Concretely:
   the cheapest edge crossing any cut is in some MST (cut property); Kruskal adds
   exactly such edges and never creates a cycle (union-find), so it builds an MST.
   </details>

---

## Interview questions

1. Greedy pattern — sort + ek pass, complexity kya?
2. Greedy-choice property + optimal substructure — dono kyun chahiye?
3. Activity selection — kaunsa sort key, kyun (exchange argument)?
4. 0/1 knapsack greedy se kyun fail, fractional se kyun kaam karta?
5. Coin change greedy kab optimal, kab nahi?
6. Ek greedy correct hai ya nahi — kaise verify (proof / brute-force diff)?

---

## Next
→ [`14-dynamic-programming.md`](14-dynamic-programming.md)
