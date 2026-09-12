# 04 — Complexity tables

Big-O = growth rate, **not** speed. Same-O structures can differ 10–300× in
practice (miss count, dependency chains). Always pair with folder `32` / `10-latency-numbers.md`.

---

## Growth rates (n = 1e6)

| O | name | ~ops at n=1e6 | feel |
|---|---|---|---|
| `O(1)` | constant | 1 | instant |
| `O(log n)` | logarithmic | ~20 | instant |
| `O(n)` | linear | 1e6 | fast |
| `O(n log n)` | linearithmic | ~2e7 | fine |
| `O(n²)` | quadratic | 1e12 | ~minutes+ |
| `O(2ⁿ)` | exponential | 💥 | n ≤ ~25 only |
| `O(n!)` | factorial | 💥💥 | n ≤ ~11 |

Rule of thumb per second: ~1e8–1e9 simple ops. `O(n²)` dies around n ≈ 30–50k.

---

## Sorting

| Algo | Best | Avg | Worst | Space | Stable |
|---|---|---|---|---|---|
| quicksort | n log n | n log n | **n²** | log n | no |
| mergesort | n log n | n log n | n log n | **n** | yes |
| heapsort | n log n | n log n | n log n | 1 | no |
| introsort (`std::sort`) | n log n | n log n | **n log n** | log n | no |
| insertion | n | n² | n² | 1 | yes |
| counting / radix | n+k | n+k | n+k | n+k | yes |

`std::sort` = quick + heap fallback (guards the n² case) + insertion for tiny runs.

---

## Data structure ops

| Structure | Access | Search | Insert | Delete | Notes |
|---|---|---|---|---|---|
| array / `vector` | O(1) | O(n) | O(1)* end | O(n) | contiguous, cache-friendly |
| sorted array | O(1) | O(log n) | O(n) | O(n) | great for read-mostly |
| linked list | O(n) | O(n) | O(1) | O(1) | pointer-chase misses |
| stack / queue | O(n) | O(n) | O(1) | O(1) | ends only |
| hash table | — | O(1) / O(n) | O(1) / O(n) | O(1) / O(n) | worst = all-collide / rehash |
| balanced BST (`map`) | — | O(log n) | O(log n) | O(log n) | ordered |
| binary heap | O(1) top | O(n) | O(log n) | O(log n) top | no decrease-key |
| trie | — | O(L) | O(L) | O(L) | L = key length |
| B-tree | — | O(log n) | O(log n) | O(log n) | disk/large-node friendly |
| union-find | ~O(α(n)) | — | ~O(α(n)) | — | near-constant (α ≈ 4) |

\* amortized.

---

## Graph algorithms (V verts, E edges)

| Algo | Complexity | For |
|---|---|---|
| BFS / DFS | O(V + E) | reachability, components, shortest path (unweighted) |
| Dijkstra (binary heap) | O((V + E) log V) | shortest path, non-negative weights |
| Bellman–Ford | O(V·E) | negative edges, negative-cycle detection |
| Floyd–Warshall | O(V³) | all-pairs shortest path |
| topological sort (Kahn) | O(V + E) | DAG ordering / cycle detection |
| Kruskal (union-find) | O(E log E) | MST |
| Prim (heap) | O((V+E) log V) | MST |

---

## Classic problem patterns

| Pattern | Complexity | Examples |
|---|---|---|
| two pointers / sliding window | O(n) | max subarray, longest substring, trapping rain water |
| prefix sum + hash map | O(n) | subarray-sum-k |
| binary search on answer | O(n log(range)) | min feasible capacity, k-th smallest |
| monotonic stack/deque | O(n) | next greater, largest rectangle, window max |
| 1-D DP | O(n·k) | coin change, LIS (O(n log n) w/ patience) |
| bitmask DP | O(2ⁿ·n²) | TSP (n ≤ ~18) |
| divide & conquer | O(n log n) | median of two sorted arrays, closest pair |
| backtracking | O(bᵈ) | N-queens, permutations, sudoku |

---

## Amortized vs worst-case (why it matters for HFT)

`vector::push_back` is `O(1)` **amortized** but a single call can `O(n)` (realloc
+ copy) → a p99.9 spike. Hot path: `reserve` once, or a fixed-capacity buffer.
`unordered_map::insert` similarly spikes on rehash → `reserve(expected)`.
"Amortized O(1)" ≠ "bounded per-call latency". (folder 43/16, `08-linux-tuning-checklist.md`)

## Next
→ [`05-compiler-flags.md`](05-compiler-flags.md)
