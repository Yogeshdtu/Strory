# 14 — Dynamic programming

## Prerequisites
- [`13-greedy-algorithms.md`](13-greedy-algorithms.md), [`01-complexity-analysis.md`](01-complexity-analysis.md)
- Folder 07 (recursion), folder 19 file 02 (`vector`)

## Yeh topic abhi kyun
DP = "recursion + yaad rakhna". Jab ek problem chhote overlapping subproblems
mein toota jaata, aur naive recursion unhe baar-baar recompute karta — DP
har subproblem ek baar solve karke store kar leta. `examples/07_dp_problems.cpp`:
naive fib(40) ~244 ms, memoized ~0.02 ms — **~11,500×**.

---

## When DP applies (two properties)

1. **Overlapping subproblems** — the naive recursion solves the *same*
   subproblem many times. (If subproblems are all distinct → plain
   divide-and-conquer, not DP.)
2. **Optimal substructure** — an optimal solution is built from optimal solutions
   to subproblems. (Greedy needs this too, plus the greedy-choice property —
   file 13.)

If a greedy choice is provably safe → use greedy (faster). If not, but the two
properties above hold → DP.

---

## The recipe

1. **State** — what parameters *fully* describe a subproblem? (`dp[i]`,
   `dp[i][w]`, `dp[i][j][mask]`…). Smaller state = faster.
2. **Transition** — how does a state combine answers of smaller states?
3. **Base cases** — the smallest states, answered directly.
4. **Order** —
   - **Top-down (memoization)**: write the natural recursion, cache each result.
     Only computes reachable states. Recursion-stack `O(depth)`.
   - **Bottom-up (tabulation)**: fill a table from base cases upward in an order
     where every dependency is already computed. No recursion; often easier to
     space-optimize.
5. **Space optimization** — if `dp[i]` only depends on `dp[i-1]` (and `dp[i-2]`…),
   keep a rolling window instead of the whole table.
6. **Reconstruction** — to recover *which* choices gave the optimum, store parent
   pointers or re-derive by walking the table backward.

---

## The three classics (`examples/07_dp_problems.cpp`)

### 1. Fibonacci — the "why DP" demo
- State: `n`. Transition: `f(n) = f(n-1) + f(n-2)`. Base: `f(0)=0, f(1)=1`.
- Naive recursion: `O(2ⁿ)` — the call tree recomputes `f(n-2)` twice, `f(n-3)`
  three times, …
- Memoized: `O(n)` time, `O(n)` space.
- Tabulated + rolling: `O(n)` time, **`O(1)` space** (keep `a`, `b`).

Measured (fib(40)): naive ~244 ms, memo ~0.02 ms, tab ~0.0001 ms.

### 2. 0/1 Knapsack — classic 2D DP
- State: `dp[i][c]` = best value using the first `i` items with capacity `c`.
- Transition: `dp[i][c] = max(dp[i-1][c],  dp[i-1][c - w[i]] + v[i])` (skip vs
  take item `i`).
- `O(n·cap)` time, `O(n·cap)` space → **`O(cap)` space** with a rolling 1D row,
  iterating `c` **downward** so each item is used at most once (`examples/07`).
- This is why greedy fails here (file 13): you can't split an item, so the local
  best-ratio choice can block a better pair.

### 3. Longest Common Subsequence — 2D + reconstruction
- State: `dp[i][j]` = LCS length of `a[0..i)` and `b[0..j)`.
- Transition: `a[i-1]==b[j-1]` → `dp[i-1][j-1]+1`; else `max(dp[i-1][j],
  dp[i][j-1])`.
- `O(n·m)` time and space. Reconstruct by walking back from `dp[n][m]`.
- Relatives: edit distance (Levenshtein), longest common substring, sequence
  alignment.

---

## Common DP families (interview map)

| Family | State shape | Examples |
|---|---|---|
| Linear / 1D | `dp[i]` | house robber, climbing stairs, max sub-array (Kadane), decode ways |
| Knapsack-style | `dp[i][capacity]` | 0/1 & unbounded knapsack, subset sum, coin change, partition |
| Grid | `dp[r][c]` | unique paths, min path sum, dungeon game |
| Two-sequence | `dp[i][j]` | LCS, edit distance, regex/wildcard match, interleaving |
| Interval | `dp[l][r]` | matrix-chain multiply, burst balloons, palindrome partitioning |
| Tree DP | `dp[node][state]` | max independent set on a tree, tree diameter |
| Bitmask DP | `dp[mask]` / `dp[mask][i]` | TSP, assignment, "visit all" (file 15) |
| Digit DP | `dp[pos][tight][…]` | count numbers with a property in `[L, R]` |

Recognizing the family from the problem statement is 80% of solving it.

---

## Top-down vs bottom-up — which

| | Top-down (memo) | Bottom-up (tabulation) |
|---|---|---|
| write it | natural recursion + a cache line | explicit table + fill order |
| computes | only **reachable** states (can be far fewer) | **all** states in range |
| space | table + `O(depth)` stack | table only |
| space-opt | hard (recursion order) | easy (rolling rows) |
| overhead | recursion + hash/array lookup per call | tight loops, cache-friendly |
| deep states | stack-overflow risk | none |

Start top-down to get it correct (mirror the recursion), convert to bottom-up if
you need the space optimization or the recursion is too deep.

---

## Andar kya hota hai

- Memoization turns an `O(bᵈ)` call tree into `O(states × transition_cost)` by
  short-circuiting repeated calls. fib: `2⁴⁰ ≈ 10¹²` calls → `40` — hence the
  ~11,500× (`examples/07`).
- The memo table: a `std::vector<long long>` (dense integer state) is one
  contiguous array — cache-friendly, `O(1)` lookup. An `unordered_map` (sparse or
  tuple state) adds hashing + a possible cache miss per lookup; use it only when
  the state space is sparse.
- Bottom-up DP over a 1D/2D `std::vector` is a **tight nested loop over
  contiguous memory** → prefetch-friendly, often vectorizable, no recursion
  overhead. This is why converting a correct top-down solution to bottom-up
  frequently gives a 2–5× constant-factor speedup on top of the same Big-O.
- Rolling-array optimization also shrinks the **working set** — a `O(cap)` 1D
  knapsack row fits in L1/L2 where the `O(n·cap)` table might not → another
  constant-factor win, not just memory.

> **HFT relevance:** DP is **research / calibration / offline** work in trading —
> optimal execution schedules (how to slice a large order over time to minimize
> impact — a DP over `(time, remaining quantity)`), position sizing under
> interacting constraints, backtesting optimizers, sequence alignment on event
> logs. It's **not** a tick-path tool: `O(n·W)` with a table doesn't fit a
> microsecond budget, and the answer usually doesn't change per tick. When a DP
> result *is* consulted at runtime (a precomputed execution curve), it's baked
> into a flat lookup table at startup. The lesson that transfers to hot code:
> **dense integer state → contiguous array**, and **shrink the working set** so
> it stays in cache.

---

## Hands-on

```bash
./build.ps1 fast 20-ALGORITHMS-DSA/examples/07_dp_problems.cpp
```

Implement fib (naive / memo / rolling), 0/1 knapsack (2D then 1D), LCS (with
reconstruction). Then: coin change (min coins for an amount, arbitrary
denominations — the case greedy can't do), and Kadane's max-subarray as a 1D DP.
Time your memo vs your tabulated version for the same input.

---

## ⚠️ Traps

### Trap 1 — no memo → exponential
```cpp
long long f(int n){ return n<2 ? n : f(n-1)+f(n-2); }   // ⚠️ O(2^n). Add a cache -> O(n).
```

### Trap 2 — 1D knapsack iterating capacity upward
```cpp
for (int c = w[i]; c <= cap; ++c) dp[c] = max(dp[c], dp[c-w[i]] + v[i]);
// ⚠️ upward -> dp[c-w[i]] may already include item i -> item used multiple times (that's UNBOUNDED knapsack).
// For 0/1, iterate c DOWNWARD.
```

### Trap 3 — wrong / incomplete state
```cpp
// If two subproblems that need different answers map to the same state, the memo returns a wrong cached value.
// The state must capture EVERYTHING the answer depends on.
```

### Trap 4 — fill order that reads an uncomputed cell
```cpp
// Bottom-up: dp[i][j] must be filled AFTER every cell it reads (dp[i-1][*], dp[i][j-1], ...). Wrong loop order -> garbage.
```

### Trap 5 — deep memo recursion → stack overflow
```cpp
// Top-down over a chain of length 1e6 -> 1e6 stack frames. Convert to bottom-up (iterative).
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "DP = a 2D table" | DP = memoized recursion; the table can be 1D, 3D, a hash map, a tree |
| "Memoization and tabulation give different answers" | Same answers; they differ in fill order, space, and overhead |
| "1D knapsack loop direction doesn't matter" | Downward = 0/1 (each item once); upward = unbounded (reuse) |
| "DP is always slower to write than greedy" | Often, but greedy that isn't provably correct is a wrong answer, not a fast one |
| "Bottom-up is always better" | Top-down skips unreachable states and is easier to get right first |

---

## Exercises

1. **State design:** "min coins to make amount `A` from denominations `d[]`".
   What's the state, transition, base case, complexity?

   <details><summary>Answer</summary>

   State `dp[a]` = min coins for amount `a`. Transition `dp[a] = 1 + min over c in
   d of dp[a - c]` (for `c ≤ a`). Base `dp[0] = 0`, others `∞`. `O(A · |d|)` time,
   `O(A)` space.
   </details>

2. **Rolling array:** unique paths in an `m×n` grid (only right/down moves).
   Full DP is `O(m·n)` space. Reduce to `O(n)`.

   <details><summary>Answer</summary>

   `dp[j] += dp[j-1]` scanning left-to-right, one row at a time, `dp` initialized
   to all 1s (first row). After `m` rows, `dp[n-1]` is the answer. `O(m·n)` time,
   `O(n)` space.
   </details>

3. **0/1 loop direction:** for `dp` size `cap+1`, why does iterating `c` from
   `cap` down to `w[i]` make item `i` used at most once?

   <details><summary>Answer</summary>

   `dp[c]` reads `dp[c - w[i]]`, which is a *smaller* index. Going downward,
   `dp[c - w[i]]` hasn't been updated for item `i` yet this pass → it still
   reflects "items before `i`" → item `i` contributes once. Going upward,
   `dp[c - w[i]]` may already include item `i` → it gets counted again.
   </details>

4. **Reconstruction:** you have the LCS length table `dp[i][j]`. How do you
   recover the actual subsequence?

   <details><summary>Answer</summary>

   Start at `(n, m)`. If `a[i-1] == b[j-1]`, that char is in the LCS; go to
   `(i-1, j-1)`. Else move to whichever of `(i-1, j)` / `(i, j-1)` has the larger
   `dp` value. Collect chars, reverse at the end.
   </details>

5. **DP vs greedy:** why does 0/1 knapsack need DP while fractional knapsack is
   greedy-solvable?

   <details><summary>Answer</summary>

   Fractional: you can split items, so taking the highest value/weight ratio
   until full is provably optimal (exchange argument). 0/1: items are indivisible,
   so a locally-best (high-ratio) item can occupy space that two lower-ratio items
   would have filled for more total value → the greedy choice isn't safe → you
   must consider take/skip for every item → DP.
   </details>

---

## Interview questions

1. DP kab lagta (overlapping subproblems + optimal substructure)?
2. Top-down vs bottom-up — kya compute, space, overhead?
3. Naive fib `O(2ⁿ)` se memoized `O(n)` — call tree mein kya badla?
4. 0/1 knapsack 1D rolling — loop direction kyun matter karta?
5. DP state "complete" hona kyun zaroori — galat state se kya hota?
6. DP vs greedy — kaunsa kab, 0/1 vs fractional knapsack ka example?

---

## Next
→ [`15-bit-manipulation.md`](15-bit-manipulation.md)
