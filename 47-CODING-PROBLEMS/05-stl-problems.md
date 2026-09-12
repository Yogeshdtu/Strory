# 05 — STL problems

## Prerequisites
- `19-STL/` (poora)
- `20-ALGORITHMS-DSA/04-std-sort-internals.md`
- Iterator invalidation: `19-STL/` container files

## Yeh file kya hai
35 problems — containers, algorithms, iterators, allocators. Focus: **sahi
container / algorithm choose karna**, aur invalidation / complexity ke traps.

Har problem: `<details>` mein approach + jo STL tool use hota hai + complexity.
Poora code → [`11-solutions/05-stl-solutions.md`](11-solutions/05-stl-solutions.md).

Compile: `g++ -std=c++20 -Wall -Wextra -Wshadow file.cpp -o t && ./t`

---

## Part A — Easy (~10 min)

### A1. Dedupe a vector
Order matter nahi karta. Do tareeke.
<details><summary>Approach</summary>

`std::sort` + `std::unique` + `erase` (erase-remove): `O(n log n)`, `O(1)`
extra. Ya `std::unordered_set` se pass: `O(n)` avg, `O(n)` space, order
preserve ho sakta. C++20: `std::ranges::sort` + `std::ranges::unique`.
</details>

### A2. Word frequency count
Text ke words → frequency.
<details><summary>Approach</summary>

`std::unordered_map<std::string, int> freq; while (in >> w) ++freq[w];`.
`operator[]` missing key ko `0`-init karta. `O(total chars)`. Ordered output
chahiye → `std::map`, ya baad mein sort.
</details>

### A3. Top-k frequent elements
`freq` map se `k` sabse frequent.
<details><summary>Approach</summary>

Size-`k` min-heap (`std::priority_queue` with `greater`, compare on count):
`O(n log k)`. Ya bucket sort by frequency (`O(n)`). `std::nth_element` on
`(count, value)` pairs → `O(n)` avg.
</details>

### A4. Sort by a field, stably
`struct P { std::string name; int age; };` — age se sort, equal age pe original
order preserve.
<details><summary>Approach</summary>

`std::stable_sort(v.begin(), v.end(), [](auto& a, auto& b){ return a.age <
b.age; })`. `std::sort` order guarantee nahi karta equal elements ka.
`stable_sort` = `O(n log² n)` worst (ya `O(n log n)` with extra memory).
</details>

### A5. Binary search family
Sorted `v` pe: pehla element `>= x`, pehla `> x`, `x` ka count.
<details><summary>Approach</summary>

`lower_bound` (`>= x`), `upper_bound` (`> x`), count = `upper_bound -
lower_bound` (ya `std::equal_range`). Sab `O(log n)` on random-access. `std::
binary_search` sirf bool deta. `std::ranges::` versions bhi.
</details>

### A6. Sum and product with accumulate
`std::accumulate` / `std::reduce` — dono, aur farq.
<details><summary>Approach</summary>

`std::accumulate(b, e, 0LL)` — init type se result type! `0` (int) → overflow;
`0LL` do. Product: `std::accumulate(b, e, 1LL, std::multiplies<>{})`. `std::
reduce` — order-independent, parallelizable (`std::execution::par`), par float
pe non-deterministic.
</details>

### A7. Partition even/odd
`v` ko rearrange: evens pehle. Order matter nahi.
<details><summary>Approach</summary>

`std::partition(v.begin(), v.end(), [](int x){ return x % 2 == 0; })` — `O(n)`,
returns partition point. Stable version `std::stable_partition` (order preserve,
`O(n log n)` in-place / `O(n)` with buffer).
</details>

### A8. Rotate a range
`v` ke element `k` ko naya first banao.
<details><summary>Approach</summary>

`std::rotate(v.begin(), v.begin() + k, v.end())` — `O(n)`, `O(1)`. Returns new
position of old first. Internally gcd-juggle ya 3-reverse.
</details>

### A9. Median via nth_element
Unsorted `v` ka median, bina full sort.
<details><summary>Approach</summary>

`std::nth_element(v.begin(), v.begin() + n/2, v.end())` — `O(n)` average
(introselect). `v[n/2]` = median (odd). Even → do nth_element calls ya ek + max
of left half.
</details>

### A10. Intersection of two sorted ranges
Common elements.
<details><summary>Approach</summary>

`std::set_intersection(a.begin(), a.end(), b.begin(), b.end(),
std::back_inserter(out))` — `O(n + m)`, dono sorted hone chahiye. Unsorted →
`unordered_set` of smaller, filter larger.
</details>

### A11. Transform + collect
`v` ke har element ka square ek naye vector mein.
<details><summary>Approach</summary>

`std::transform(v.begin(), v.end(), std::back_inserter(out), [](int x){ return
x*x; })`. Better: `out.reserve(v.size())` pehle. C++20: `v | std::views::
transform(...)` lazy.
</details>

### A12. for_each vs range-for
Kab `std::for_each` use karna, kab range-`for`?
<details><summary>Answer</summary>

Range-`for` default — clearer. `std::for_each` jab: (a) execution policy chahiye
(`std::execution::par`), (b) sub-range (`begin+k, end`), (c) algorithm-style
composition. Perf same at `-O2`.
</details>

---

## Part B — Medium (~25 min)

### B1. group_by on a sorted range
Consecutive equal-key elements ko groups mein.
<details><summary>Approach</summary>

Manual: `it` se `std::upper_bound` (ya `find_if` for next different key) tak ek
group. Ya `std::ranges::chunk_by` (C++23). `O(n)`. Unsorted pehle sort by key.
</details>

### B2. Sliding window with deque
Stream of ints, har point pe last `k` ka max (file 02 B14 ka STL version).
<details><summary>Approach</summary>

`std::deque<int>` (values ya indices) monotonic decreasing. `O(1)` amortized per
element. `std::queue` yahan kaam nahi karega (dono ends chahiye).
</details>

### B3. LRU with list + unordered_map
`get`/`put` `O(1)`, zero iterator invalidation issues.
<details><summary>Approach</summary>

`std::list<std::pair<K,V>>` + `unordered_map<K, list<...>::iterator>`.
`list::splice` node ko move karta bina iterator invalidate kiye — yehi reason
`list` chuna, `vector` nahi. `put` overflow → `list.back()` evict.
</details>

### B4. Iterator invalidation quiz
In operations ke baad kaunse iterators/refs invalid: `vector::push_back`,
`vector::insert(mid)`, `deque::push_front`, `list::insert`, `map::erase(it)`,
`unordered_map::insert` (rehash).
<details><summary>Answer</summary>

`vector::push_back` — reallocation ho to **sab** invalid, warna sirf `end()`.
`vector::insert(mid)` — insertion point se aage sab. `deque::push_front` —
iterators sab invalid, **refs valid**. `list::insert` — kuch nahi invalid.
`map::erase(it)` — sirf `it` (baaki nodes stable). `unordered_map::insert` —
rehash pe iterators invalid, **refs/pointers valid** (nodes move nahi hote,
buckets rewire hote).
</details>

### B5. reserve vs resize — measured
`push_back` 1e7 ints: (a) kuch nahi, (b) `reserve(1e7)`, (c) `resize(1e7)` +
index assign. Time compare.
<details><summary>Approach</summary>

`reserve` reallocations (~23 growth steps, `O(n)` total copies) ko ek allocation
mein badal deta — typically 2–4× faster. `resize` value-initializes (extra
`memset`) — agar tum turant overwrite kar rahe ho to waste. Measure with `-O2`.
Solutions file mein numbers.
</details>

### B6. erase-remove and std::erase_if
`v` se saare negatives hatao — pre-C++20 aur C++20 tareeka.
<details><summary>Approach</summary>

Pre-20: `v.erase(std::remove_if(v.begin(), v.end(), pred), v.end())` — `remove_if`
sirf shift karta (`O(n)`), `erase` size chhota karta. C++20: `std::erase_if(v,
pred)` — ek call. Naive `for` + `erase(it)` = `O(n²)` + invalidation bug.
</details>

### B7. string_view pitfalls
Teen buggy snippets: (a) `sv` into a temporary `std::string`, (b) `sv` ko
C-API ko as `const char*` pass karna, (c) `sv.data() + sv.size()` deref.
<details><summary>Answer</summary>

(a) dangling — temp destroyed, `sv` garbage. (b) `sv` **null-terminated nahi**
hota — `strlen`/`printf("%s")` OOB read. `std::string(sv).c_str()` banao. (c)
`data() + size()` one-past-end; deref = UB. `sv` = non-owning window; owner ko
zinda rakho.
</details>

### B8. Heterogeneous lookup
`std::set<std::string>` mein `std::string_view` / `const char*` se lookup bina
`std::string` banaye.
<details><summary>Approach</summary>

`std::set<std::string, std::less<>>` — transparent comparator (`is_transparent`).
Ab `s.find("literal")` temporary `std::string` nahi banata. `unordered_map` C++20
mein bhi (custom hash + equal with `is_transparent`).
</details>

### B9. std::span over C arrays and vectors
Ek function jo `int[]`, `std::vector<int>`, `std::array<int,N>` teeno ko bina
copy accept kare.
<details><summary>Approach</summary>

`void f(std::span<const int> s)` — sabse implicitly convert. `s.size()`,
range-`for`, `s.subspan(a, n)`. C++17 `gsl::span` / C++20 `<span>`. `s` non-owning
— lifetime caller ki.
</details>

### B10. Custom hash for unordered_map<pair>
`std::unordered_map<std::pair<int,int>, V>` compile nahi hota — kyun, fix?
<details><summary>Answer</summary>

`std::hash<std::pair>` standard mein nahi. Custom: `struct PairHash { size_t
operator()(std::pair<int,int> p) const { return std::hash<long long>{}((long
long)p.first << 32 | (unsigned)p.second); } };`. Better: `boost::hash_combine`
style mix. Ya `std::map` (needs only `<`).
</details>

### B11. priority_queue with decrease-key
Dijkstra needs decrease-key; `std::priority_queue` deta nahi. Workaround?
<details><summary>Answer</summary>

**Lazy deletion**: purani entry ko rehne do, nayi (chhoti) key push karo; pop pe
check karo entry stale hai kya (`dist[u]` current se match?) — stale skip.
`O(E log E)` (thodi extra memory). Real decrease-key → `boost::heap::fibonacci_heap`
ya index into a hand-rolled binary heap.
</details>

### B12. map vs unordered_map vs sorted vector
Teen scenarios ke liye pick karo + kyun: (a) 50 config keys, read-mostly; (b)
2M id→object, point lookups, latency-critical; (c) range queries "price 100–110
ke saare orders".
<details><summary>Answer</summary>

(a) `std::map` ya even sorted `vector` — chhota, ordered debug output, doesn't
matter. (b) open-addressed flat hash (`reserve`, no rehash) ya direct-index array
agar id dense — `unordered_map` borderline (rehash spikes, 2 misses). `std::map`
= `O(log n)` pointer chase, out. (c) sorted `vector` + `lower_bound`/`upper_bound`
(contiguous range scan) ya `std::map` (ordered iteration).
</details>

### B13. emplace vs insert
Kab `emplace_back` actually faster hai `push_back` se?
<details><summary>Answer</summary>

Jab element ko args se in-place construct kiya jaa sakta ho aur alternative ek
temporary + move hoti — e.g. `v.emplace_back(1, "x")` vs `v.push_back(Widget(1,
"x"))`. Agar tum already-constructed object pass kar rahe ho (`emplace_back(w)`)
→ koi farak nahi. Trap: `emplace_back` explicit ctors ko bhi bula leta (kam
type-safety). `std::vector<int>` pe farak zero.
</details>

### B14. Not a strict weak ordering
`[](auto&a, auto&b){ return a <= b; }` ko `std::sort` ko dena — kya hota?
<details><summary>Answer</summary>

`<=` strict weak ordering **nahi** (irreflexivity fail: `comp(a,a)` true hona
chahiye false). `std::sort` UB — practically OOB access / crash / infinite loop
on some inputs (libstdc++ `_GLIBCXX_DEBUG` isse pakadta). Comparator hamesha
`<` semantics: `comp(a,a) == false`, transitive, `comp(a,b) && comp(b,c) ⇒
comp(a,c)`.
</details>

---

## Part C — Hard (~40+ min)

### C1. flat_map
`std::map`-like API, par sorted `std::vector<std::pair<K,V>>` backing. `find`,
`insert`, `erase`, `operator[]`, iteration.
<details><summary>Design</summary>

`find` = `lower_bound` (`O(log n)`). `insert` = find spot + `vector::insert`
(`O(n)` shift — batch inserts ke liye append + one sort). Iteration cache-friendly
(contiguous). C++23: `std::flat_map`. Trade: reads/iteration fast, single insert
slow — read-heavy workloads ke liye.
</details>

### C2. Zero-allocation LRU after warmup
LRU jo capacity tak pahunchne ke baad kabhi allocate na kare.
<details><summary>Design</summary>

Nodes ek `std::array<Node, CAP>` mein (intrusive prev/next indices, not
pointers). Free list of unused slots. `unordered_map<K, uint32_t slotIdx>` ko
`reserve(CAP)` (still may rehash — ya open-addressed flat map with fixed
capacity). Move-to-front = index relink, `O(1)`, no alloc.
</details>

### C3. Fixed-capacity ring queue with STL-style iterators
`RingQueue<T, N>` — `begin()`/`end()`, `push`/`pop`, range-`for` works, wraps.
<details><summary>Design</summary>

`std::array<T, N>` storage; custom iterator holding `(ring*, logicalIndex)`;
`operator++` → `++idx`; `operator*` → `ring->buf[(head + idx) % N]`. `end()` =
index `size`. Random-access iterator tag → `std::sort` on the queue bhi chalega.
Careful: `operator-` for distance.
</details>

### C4. Custom allocator / pmr
`std::pmr::monotonic_buffer_resource` se ek `pmr::vector` jo stack buffer se
allocate kare. Phir apna `Mallocator` likho (`allocate`/`deallocate`/`==`).
<details><summary>Design</summary>

`std::array<std::byte, 4096> buf; std::pmr::monotonic_buffer_resource
res{buf.data(), buf.size()}; std::pmr::vector<int> v{&res};` — pehle
`push_back`s zero heap. Custom: `template<class T> struct Mallocator { T*
allocate(size_t n); void deallocate(T*, size_t); bool operator==(...) const; };`
— stateless → `==` always true. Stateful (arena) → compare the arena.
</details>

### C5. stable_partition two ways
`O(n)` time `O(n)` space, aur `O(n log n)` in-place. Dono.
<details><summary>Design</summary>

`O(n)`/`O(n)`: ek pass mein true elements ko buffer A, false ko buffer B, phir
copy back. In-place: divide-and-conquer — half karo, dono halves stable_partition
karo (recursion), phir `std::rotate` se middle ko join karo. `T(n) = 2T(n/2) +
O(n)` = `O(n log n)`. libstdc++ yehi karta jab memory na mile.
</details>

### C6. Interval map
`IntervalMap<K, V>` — key ranges → value (`[0,10) → A`, `[10,20) → B`),
`operator[](K)` point query, overlapping insert splits.
<details><summary>Design</summary>

`std::map<K, V>` jahan key = interval **start**, aur "canonical form": consecutive
equal values coalesced. Insert `[a,b) → v`: `a` aur `b` pe boundaries daalo,
beech ke entries erase, neighbours se merge agar same value. Point query =
`upper_bound(k)` se ek peeche. Marius Bancila / the classic "interval_map"
interview question.
</details>

### C7. Sort algorithm cost table
`std::sort`, `stable_sort`, `partial_sort`, `nth_element`, `is_sorted`, `sort` on
`std::list` — complexity + kab kaunsa.
<details><summary>Answer</summary>

`sort`: `O(n log n)` introsort (quick + heap fallback + insertion for small),
not stable, random-access only. `stable_sort`: `O(n log n)` with buffer / `O(n
log² n)` without. `partial_sort`: top-`k` sorted, `O(n log k)`. `nth_element`:
`O(n)` avg, just the pivot in place. `std::list::sort`: member fn, `O(n log n)`
merge sort, no random access needed. `is_sorted`: `O(n)`.
</details>

### C8. Merge k sorted lists
`k` sorted vectors ko ek sorted vector mein.
<details><summary>Approach</summary>

Min-heap of `k` "current heads" (`{value, listIdx, elemIdx}`): pop min, push
next from that list. `O(N log k)` where `N` = total elements. `std::priority_queue`
with `greater` comparator. Pairwise merge = `O(N log k)` bhi par more passes.
</details>

### C9. Why is std::list::size() O(1)
Pre-C++11 kuch implementations `O(n)` the. Kya badla, aur `splice` ka trade-off?
<details><summary>Answer</summary>

C++11 ne `size()` ko `O(1)` mandate kiya → `list` ek `size_` counter rakhta hai.
Trade: `splice(pos, other, first, last)` (range splice) ko ab spliced elements
**count** karne padte hain (`O(distance)`) taaki dono lists ke `size_` update
ho — pehle yeh `O(1)` tha. Full-list splice abhi bhi `O(1)`.
</details>

---

## Next
→ [`06-templates-problems.md`](06-templates-problems.md)
