# Examples — Folder 20 (Algorithms & Data Structures)

| File | Lesson(s) | Kya dikhata hai |
|---|---|---|
| `01_sorting_all.cpp` | 01, 03, 04 | bubble / selection / insertion / merge / quick (median-of-3) / heap sort — each implemented, correctness-checked, then **benchmarked** vs `std::sort`; includes an already-sorted-input run (quicksort worst-case guard) |
| `02_binary_search.cpp` | 05, 02 | half-open `[lo,hi)` `contains` / `lowerBound` / `upperBound` verified against `std::lower_bound`/`upper_bound`; sorted-insert position; "binary search the answer" (integer ceil-sqrt); the 5 classic traps |
| `03_linked_list.cpp` | 06, 01, 17 | owning singly list (push_front/back, pop_front, insert_after, **iterative reverse**, Floyd cycle detect); **measured**: sum a 5M-node list vs a 5M-int vector |
| `04_hash_table.cpp` | 08, 17 | **open-addressing** hash map (linear probing, power-of-two capacity, splitmix64 finalizer, tombstones, resize) — correctness + **benchmark** vs `std::unordered_map` |
| `05_bst.cpp` | 09 | BST insert / find / **3-case delete** (via `Node**`); pre/in/post/level-order traversals; **unbalanced demo**: 1023 ascending inserts → height 1023 vs middle-out → height 10 |
| `06_graph_algorithms.cpp` | 12, 07, 10, 13 | on a flat `vector<vector<Edge>>`: BFS (edge distance), recursive + iterative DFS, **Dijkstra** (`priority_queue` + lazy stale-skip), **topological sort** (Kahn) |
| `07_dp_problems.cpp` | 14, 13 | Fibonacci (naive / memo / rolling — **measured ~11,500×**), 0/1 knapsack (2D table then O(cap) rolling), LCS (2D table + reconstruction) |
| `08_flat_vs_pointer.cpp` | 17, 01, 09 | **measured**: identical `O(n)` tree traversal over a pointer tree vs a flat array tree (`2i+1`/`2i+2`) — layout beats Big-O |

## Compile / run

```bash
./build.ps1 20-ALGORITHMS-DSA/examples/01_sorting_all.cpp        # Windows (debug -O0 + heavy warnings)
make FILE=20-ALGORITHMS-DSA/examples/01_sorting_all.cpp          # Linux/Mac/Git-Bash
```

Benchmarks at **`-O2`** (mandatory — `-O0` numbers are meaningless):

```bash
./build.ps1 fast 20-ALGORITHMS-DSA/examples/01_sorting_all.cpp
./build.ps1 fast 20-ALGORITHMS-DSA/examples/03_linked_list.cpp
./build.ps1 fast 20-ALGORITHMS-DSA/examples/04_hash_table.cpp
./build.ps1 fast 20-ALGORITHMS-DSA/examples/07_dp_problems.cpp
./build.ps1 fast 20-ALGORITHMS-DSA/examples/08_flat_vs_pointer.cpp
```

## Measured (GCC 15.1.0, `-O2`, x86-64, this box) — sample runs

### `01_sorting_all.cpp`
```
n = 20000  (O(n^2) sorts):   bubble ~730 ms   selection ~586 ms   insertion ~58 ms   std::sort ~1.1 ms
n = 2000000 (O(n log n)):     merge ~262 ms    quick ~188 ms       heap ~390 ms       std::sort ~163 ms
n = 2000000 ALREADY SORTED:   quick (median-of-3) ~50 ms           std::sort ~30 ms
```

### `03_linked_list.cpp` (n = 5,000,000)
```
list  sum : ~70 ms   (5M scattered heap nodes)
vector sum: ~1.7 ms  (one contiguous buffer)      ratio ~40x
```

### `04_hash_table.cpp` (n = 1,000,000)
```
build  : mine (open addressing) ~66 ns/op    std::unordered_map ~295 ns/op
lookup : mine ~45 ns/op                       std::unordered_map ~83 ns/op
```

### `07_dp_problems.cpp`
```
fib(40): naive ~244 ms   memo ~0.02 ms   rolling ~0.0001 ms      speedup naive->memo ~11,500x
knapsack (2D == 1D rolling) = 26 ;  LCS("AGGTABCDEF","GXTXAYBCF") = "GTABCF"
```

### `08_flat_vs_pointer.cpp` (~4.2M nodes)
```
pointer tree, recursive sum : ~30 ms
flat array,   recursive sum : ~6.6 ms   (~4.6x)
flat array,   linear scan   : ~2.7 ms   (~11x)
```

The **shape** reproduces everywhere: `O(n²)` is hopeless past a few thousand;
`std::sort` (introsort) beats every hand-rolled comparison sort; two structures
with the **same Big-O** can be 3–40× apart purely on cache behaviour; memoizing
an exponential recursion is a 10,000×-class win.

## Notes / jaan-boojh kar cheezein

- **No `broken_on_purpose` file** in this folder.
- `01_sorting_all.cpp` — the quicksort **median-of-3 was a real bug** during
  development: it picked `a[hi]` (the max of the three) instead of the median,
  giving `O(n²)` on sorted-with-duplicates input (it hung the benchmark). Fixed
  by swapping the median into the pivot slot before partitioning. The
  already-sorted-input run in the example exists to keep that regression visible.
- `01`/`03` use `n = 2,000,000` / `5,000,000` — heavy at `-O0`; `./build.ps1
  folder` only **compiles** (never runs), so the folder check is fast. Run the
  benchmarks with `./build.ps1 fast`.
- All files compile clean under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion
  -Wsign-conversion -Wcast-align -Wunused -Wnull-dereference -Wdouble-promotion`
  (`./build.ps1 folder 20-ALGORITHMS-DSA`).
