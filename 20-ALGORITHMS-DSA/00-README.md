# 20 — ALGORITHMS & DATA STRUCTURES (PHASE 10)

## Prerequisites
`19-STL`, `12-POINTERS`

## Yeh folder kyun
Interview ka core. Par hum ise **cache-aware** tareeke se padhenge — kyunki HFT mein
Big-O se zyada memory access pattern matter karta hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-complexity-analysis.md` | Big-O, big-Theta, amortized analysis, **aur uski limitations** |
| 02 | `02-arrays-and-two-pointers.md` | Two-pointer, sliding window, prefix sums |
| 03 | `03-sorting-algorithms.md` | Bubble/insertion/merge/quick/heap — implement karo, compare karo |
| 04 | `04-std-sort-internals.md` | **Introsort** — quicksort + heapsort + insertion sort ka mix |
| 05 | `05-binary-search.md` | Binary search, `lower_bound`/`upper_bound`, off-by-one traps |
| 06 | `06-linked-lists.md` | Singly, doubly, circular — implement karo, **cache problem samjho** |
| 07 | `07-stacks-and-queues.md` | Implementation, use cases, circular buffer |
| 08 | `08-hash-tables.md` | **Apni hash table likho** — open addressing vs chaining |
| 09 | `09-trees-and-bst.md` | Binary trees, BST, traversals, balancing intro |
| 10 | `10-heaps.md` | Binary heap, `priority_queue`, heapify |
| 11 | `11-tries.md` | Trie, prefix matching, symbol lookup mein use |
| 12 | `12-graphs.md` | Representation, BFS, DFS, Dijkstra, topological sort |
| 13 | `13-greedy-algorithms.md` | Greedy, proof techniques, classic problems |
| 14 | `14-dynamic-programming.md` | DP, memoization, tabulation, classic problems |
| 15 | `15-bit-manipulation.md` | Bit tricks for algorithms, subsets, DP with bitmasks |
| 16 | `16-string-algorithms.md` | KMP, rolling hash, Z-algorithm |
| 17 | `17-cache-aware-dsa.md` | **Cache-friendly data structures** — flat vs pointer-based, HFT flavour |
| 18 | `18-problem-sets.md` | Graded problems, easy → hard |

## Examples

| File | Kya |
|---|---|
| `examples/01_sorting_all.cpp` | Saare sorting algorithms + benchmark |
| `examples/02_binary_search.cpp` | Variants aur traps |
| `examples/03_linked_list.cpp` | Implementation |
| `examples/04_hash_table.cpp` | Apni hash table |
| `examples/05_bst.cpp` | BST implementation |
| `examples/06_graph_algorithms.cpp` | BFS/DFS/Dijkstra |
| `examples/07_dp_problems.cpp` | Classic DP |
| `examples/08_flat_vs_pointer.cpp` | Array-based vs pointer-based tree — cache ka asar |

## Time
4–6 hafte

## Status
✅ **COMPLETE (Batch 7 — PHASE 10).** 17 lessons (`01`–`17`) + `18-problem-sets.md`
+ 8 verified examples. Sab `.cpp` `-Wall -Wextra -Wpedantic -Wshadow -Wconversion
-Wsign-conversion` pe clean (`./build.ps1 folder 20-ALGORITHMS-DSA`). Benchmarks
`-O2` pe chalaye, **real numbers** lessons mein:

- `01_sorting_all`: O(n²) sorts (n=20k) bubble ~730 / selection ~586 / insertion
  ~58 ms vs `std::sort` ~1.1 ms; O(n log n) (n=2M) merge ~262 / quick ~188 / heap
  ~390 vs `std::sort` ~163 ms. (Quicksort median-of-3 bug found + fixed during
  dev — picked the max, not the median → O(n²) on sorted input.)
- `03_linked_list`: sum 5M-node list ~70 ms vs 5M-int vector ~1.7 ms → **~40×**
  (same O(n)).
- `04_hash_table`: open-addressing map build ~66 / lookup ~45 ns/op vs
  `std::unordered_map` ~295 / ~83 ns/op.
- `07_dp_problems`: fib(40) naive ~244 ms vs memoized ~0.02 ms → **~11,500×**.
- `08_flat_vs_pointer`: identical O(n) tree traversal — flat array **4–11×**
  faster than a pointer tree (cache).

**Theme:** Big-O rules out disasters; cache behaviour + branch predictability +
bounded worst case decide the winner. `17-cache-aware-dsa.md` is the synthesis.

## Next
→ [`../21-TEMPLATES/00-README.md`](../21-TEMPLATES/00-README.md)
