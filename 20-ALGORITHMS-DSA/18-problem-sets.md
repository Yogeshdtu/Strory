# 18 — Problem sets

## Prerequisites
- All of folder 20 (files 01–17)

## Yeh file kya hai
Graded practice — easy se hard. Har problem ke saath: pattern hint,
`<details>` mein approach + complexity. **Pehle khud solve karo, phir kholo.**
Jahan code likhna ho: `./build.ps1 file.cpp`. Benchmarks ke liye `./build.ps1
fast file.cpp`.

Patterns cheat-sheet: two-pointer / sliding window (file 02) · binary search
(file 05) · sort + greedy (file 13) · hash map (file 08) · BFS/DFS (file 12) ·
heap / top-k (file 10) · DP (file 14) · bitmask (file 15) · monotonic stack
(file 07).

---

## Part A — Easy (warm-up, ~10–15 min each)

### A1. Two Sum
Unsorted `std::vector<int> a`, target `t`. Return indices of two elements summing
to `t`.
<details><summary>Approach</summary>

Hash map value→index in one pass: for each `a[i]`, check if `t - a[i]` is already
in the map. `O(n)` time, `O(n)` space. (Sorted input → two pointers, `O(n)`,
`O(1)`, but you lose original indices.)
</details>

### A2. Valid Parentheses
String of `()[]{}`. Is it balanced?
<details><summary>Approach</summary>

Stack: push openers, on a closer pop and check the match; reject on mismatch or
empty. Valid iff the stack is empty at the end. `O(n)`.
</details>

### A3. Best Time to Buy/Sell Stock (one transaction)
`std::vector<int> price`. Max profit from one buy then one later sell.
<details><summary>Approach</summary>

One pass: track `minSoFar`; `best = max(best, price[i] - minSoFar)`. `O(n)`,
`O(1)`. (This is a 1D DP / sliding-min.)
</details>

### A4. Reverse a Linked List
Iterative, `O(1)` space.
<details><summary>Approach</summary>

Three pointers `prev/cur/next`: save `next`, flip `cur->next = prev`, advance
both. Return `prev`. File 06.
</details>

### A5. Binary Search / First Bad Version
`n` versions, `isBad(k)` is monotone (false…false true…true). Find the first bad
one.
<details><summary>Approach</summary>

Half-open binary search on the predicate: `lo=1, hi=n+1; while(lo<hi){ mid=lo+(hi
-lo)/2; if(isBad(mid)) hi=mid; else lo=mid+1; }` → `lo`. `O(log n)`. File 05.
</details>

### A6. Majority Element (> n/2)
<details><summary>Approach</summary>

Boyer–Moore voting: keep a `candidate` and a `count`; `++`/`--` per element, reset
candidate at 0. The majority survives. `O(n)`, `O(1)`.
</details>

---

## Part B — Medium (~20–35 min each)

### B1. Longest Substring Without Repeating Characters
<details><summary>Approach</summary>

Variable sliding window (file 02): grow `r`, keep a `lastSeen[char]` map; when
`s[r]` was seen at index `≥ l`, jump `l` to `lastSeen[s[r]] + 1`. Track `r - l +
1`. `O(n)`.
</details>

### B2. Group Anagrams
Group strings that are permutations of each other.
<details><summary>Approach</summary>

Key = the sorted string (or a 26-int count signature). `unordered_map<key,
vector<string>>`. `O(Σ Lᵢ log Lᵢ)` with sorted keys, `O(Σ Lᵢ)` with count keys.
</details>

### B3. Kth Largest Element
Unsorted array, find the k-th largest.
<details><summary>Approach</summary>

`std::nth_element(v.begin(), v.begin() + (n - k), v.end())` → `O(n)` average
(file 04). Or a size-k min-heap → `O(n log k)` (file 10). Don't full-sort.
</details>

### B4. Number of Islands
Grid of `'1'`/`'0'`; count connected components of `'1'`.
<details><summary>Approach</summary>

BFS or DFS from each unvisited `'1'`, flood-fill it to `'0'` (or a visited set),
`++count`. `O(rows·cols)`. File 12.
</details>

### B5. Course Schedule (can all courses be finished?)
Directed graph of prerequisites — is it a DAG?
<details><summary>Approach</summary>

Kahn's topological sort: if the produced order has fewer than `V` nodes, there's
a cycle → `false`. `O(V + E)`. File 12.
</details>

### B6. Coin Change (min coins for amount)
Arbitrary denominations (greedy won't do — file 13).
<details><summary>Approach</summary>

1D DP: `dp[a] = 1 + min over c of dp[a - c]`, `dp[0] = 0`. `O(amount · |coins|)`
time, `O(amount)` space. File 14.
</details>

### B7. Product of Array Except Self (no division)
<details><summary>Approach</summary>

Prefix and suffix products: `out[i] = (prod of a[0..i-1]) · (prod of a[i+1..])`.
Two passes, `O(n)`, `O(1)` extra (write prefixes into `out`, then multiply by a
running suffix). File 02.
</details>

### B8. LRU Cache (`get`/`put` in `O(1)`)
<details><summary>Approach</summary>

Hash map `key → list iterator` + a doubly linked list ordered by recency. `get`:
splice the node to the front. `put`: insert front, evict the back on overflow.
Use `std::list` + `std::unordered_map`, or an **intrusive index-linked arena
list** (file 06) for the cache-aware version.
</details>

### B9. Daily Temperatures / Next Greater Element
For each element, distance to the next strictly greater one.
<details><summary>Approach</summary>

Monotonic (decreasing) stack of indices: for each `i`, pop while `a[stack.top()]
< a[i]` setting `ans[popped] = i - popped`, then push `i`. `O(n)` — each index
pushed/popped once. File 07.
</details>

### B10. Merge Intervals
<details><summary>Approach</summary>

Sort by start; sweep, extending the current interval while the next starts `≤`
the current end, else emit and start a new one. `O(n log n)`. File 13.
</details>

---

## Part C — Hard (~40+ min each)

### C1. Median of Two Sorted Arrays — `O(log(m + n))`
<details><summary>Approach</summary>

Binary search the partition of the *smaller* array so that `leftCount = (m + n +
1) / 2`; check `maxLeftA ≤ minRightB && maxLeftB ≤ minRightA`; adjust. The median
comes from the four boundary elements. `O(log min(m, n))`.
</details>

### C2. Trapping Rain Water
<details><summary>Approach</summary>

Two pointers from both ends with `leftMax`/`rightMax`; move the side with the
smaller max inward, adding `max - height[i]` at each step. `O(n)`, `O(1)`. (Also
solvable with a monotonic stack.)
</details>

### C3. Word Ladder (shortest transform chain)
<details><summary>Approach</summary>

BFS over the implicit graph where words are nodes and edges connect words
differing in one letter. Precompute `*`-pattern buckets (`h*t → hot, hit`) to
find neighbours fast. Bidirectional BFS to cut the frontier. `O(N · L²)`.
</details>

### C4. Largest Rectangle in Histogram
<details><summary>Approach</summary>

Monotonic increasing stack of bar indices; when a shorter bar arrives, pop and
compute the area with the popped bar as the height (width = distance between the
new left boundary and `i`). `O(n)`. File 07.
</details>

### C5. Travelling Salesman (exact, `n ≤ 18`)
<details><summary>Approach</summary>

Held–Karp bitmask DP (file 15): `dp[mask][i]` = shortest path visiting `mask`,
ending at `i`. `O(2ⁿ · n²)` time, `O(2ⁿ · n)` space. `dp` is a contiguous array
indexed by `(mask, i)`.
</details>

### C6. Sliding Window Maximum
Max of every length-`k` window, `O(n)`.
<details><summary>Approach</summary>

`std::deque<size_t>` of indices, kept decreasing by value: push each `i` (popping
smaller values off the back first), pop the front when it falls out of the
window; the front is the window max. Each index enters/leaves once → `O(n)`.
File 07.
</details>

### C7. Alien Dictionary (order of letters from sorted words)
<details><summary>Approach</summary>

Build a graph: for each adjacent pair of words, the first differing char gives an
edge. Topological sort the char graph (Kahn's). Detect the "prefix after longer
word" invalid case. `O(total chars)`. File 12.
</details>

---

## Part D — Systems / HFT-flavoured

### D1. Design a fixed-capacity SPSC ring buffer
<details><summary>Approach</summary>

`std::array<T, CAP>` (CAP a power of two) + `head`/`tail` indices, masked wrap
(`& (CAP-1)`). Single producer writes `tail`, single consumer writes `head`.
Zero allocation, `O(1)`, back-pressure when full. (Lock-free version with atomics
+ cache-line padding → folders 27–28, 41.) File 07.
</details>

### D2. Order-book price-level lookup, three ways — benchmark
<details><summary>Approach</summary>

(a) `std::map<int, Level>`; (b) sorted `std::vector<Level>` + `std::lower_bound`;
(c) `std::array<Level, RANGE>` indexed by `tick - reference`. Benchmark lookup +
update for a realistic mix. Expect (c) ≫ (b) > (a). Files 09, 17; folder 19
files 25–26; challenge in folder 19 file 27.
</details>

### D3. Top-N bids for a UI, updated per tick
<details><summary>Approach</summary>

`std::nth_element` / `std::partial_sort` to pull the N best from the level array —
`O(levels)` / `O(levels · log N)`, not a full sort (files 04, 10; folder 19 file
11). Or maintain a size-N heap incrementally.
</details>

### D4. Dedup a stream of fixed-size messages
<details><summary>Approach</summary>

Rolling/polynomial hash of each message (file 16) into an open-addressed
`unordered_set<uint64_t>` (file 08) sized to the dedup window; evict oldest
(ring of hashes). `O(1)` amortized per message, no allocation in steady state.
</details>

### D5. Free-list allocator for fixed-size objects
<details><summary>Approach</summary>

A slab (`std::vector<Slot>` or a static array) + a free list. Bitmask free list
(`std::countr_zero` to find a slot, `& (x-1)` to claim) for ≤ 64 slots (file
15); an index-linked free list for more (file 06). `allocate`/`free` are `O(1)`,
no `malloc`. Folder 19 files 23–24; folder 36.
</details>

---

## Part E — Discussion (no single answer)

1. **Big-O lies:** give three pairs of structures/algorithms with the same Big-O
   where one is ≥ 10× faster in practice, and name the cause each time.
   <details><summary>Answer sketch</summary>

   (1) `std::list` vs `std::vector` iterate — `O(n)` each, ~40× (pointer-chase
   cache misses vs prefetched contiguous scan). (2) `std::map` vs sorted-`vector`
   `lower_bound` — `O(log n)` each, ~3× (`log n` scattered node misses vs ~2
   cache lines). (3) AoS vs SoA sum of 2 fields — `O(n)` each, ~4–8× (whole
   struct per line vs packed fields + SIMD). (4) top-down memo vs bottom-up
   table — same `O(states)`, ~2–5× (recursion + hash lookups vs tight contiguous
   loop).
   </details>

2. **Choosing under a latency budget:** you need id→object lookup, ~200k live
   ids, p99.9 < 200 ns. Walk through `std::map` → `std::unordered_map` →
   open-addressed flat → direct index, and where you'd stop.
   <details><summary>Answer sketch</summary>

   `std::map`: `O(log n)` ~18 misses, ~1 µs — out. `std::unordered_map` reserved:
   ~113 ns average but rehash spikes and ~2 misses — borderline, p99.9 risky.
   Open-addressed fixed-capacity (no rehash): ~1 cache line, ~45 ns, flat tail —
   passes. Direct index (if ids are dense/bounded): one array load, ~5 ns, zero
   collisions — stop here if the id space allows it.
   </details>

3. **When to stop optimizing:** the config loader vs the tick handler — how do
   you decide which gets a hand-rolled cache-aware structure and which gets a
   `std::map`?
   <details><summary>Answer sketch</summary>

   Measure. If it's not on a profiled hot path and doesn't affect p99.9, use the
   standard container for clarity and fewer bugs (config, admin, startup). Spend
   the hand-rolled-structure budget only where a benchmark with production-shaped
   data shows it moves the tail latency.
   </details>

---

## Challenge

Open-ended — answer key nahi. Upar ke problems ka "sahi answer" hota hai; yahan ka answer
**aapka measurement** hai. Har number `-O2`, best-of-N, machine ke naam ke saath.

### Challenge 1 — apna open-addressing hash map vs `std::unordered_map`
Linear probing, power-of-2 capacity, tombstones ke saath delete (`examples/04_hash_table.cpp`
se shuru karo). Load factor 0.5, 0.7, 0.9 pe **hit** aur **miss** lookup ka p50 / p99 per op
naapo, `std::unordered_map` ke against. Table banao, aur batao: kis load factor pe aapka map
achanak slow hua (probe length ka "cliff"), aur miss lookups hit se itne alag kyun hain?

### Challenge 2 — order book price levels: kaunsa data structure?
Synthetic stream: 90% updates best price ke 5 ticks ke andar, 10% door. Char structures pe
chalao: `std::map`, sorted `std::vector`, `std::flat_map` (folder 22 file 15), aur tick-indexed
flat array (file 17, `examples/08_flat_vs_pointer.cpp` ki soch). Per-op p50 / p99.9 aur
memory. Phir access pattern badlo (updates poore range mein barabar faile hue) aur dobara
naapo. Kaunsa structure kis pattern pe jeeta? (Folder 39 isi sawaal ko poori tarah hal karta hai.)

### Challenge 3 — stream pe top-K symbols
10M trade events, symbols Zipf distribution se. Do tareeke: (a) exact — `unordered_map`
counting + end mein top-K, (b) approximate — count-min sketch + size-K min-heap. Memory vs
accuracy (kitne top-K sahi pakde) ki table banao, sketch ke 3 alag sizes ke saath. Kab
approximate answer "kaafi achha" hai, aur kab nahi?

---

## Next
→ [`../21-TEMPLATES/00-README.md`](../21-TEMPLATES/00-README.md)
