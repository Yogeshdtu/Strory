# 05 — Layer 4: STL (containers, iterators, algorithms, complexity)

## Prerequisites
Folders `19-STL`, `20-ALGORITHMS-DSA`. Interviewers yahan **complexity**
aur **cache behaviour** dono poochte — big-O aur "kaunsa fast in practice".

---

## A — Containers: choose and justify

### A1. `std::vector` vs `std::list` — kab list?
<details><summary>Answer</summary>
Almost never `list` in practice. `vector` contiguous → cache-friendly,
prefetchable, ek scan ek stream. `list` = node per element, pointer
chase, allocation per insert, ~40× slower to traverse (`20`). `list` sirf
jab: stable references/iterators across insert/erase **zaroori** hain,
aur splice-heavy. Even "middle insert" pe vector aksar jeetta (memmove <
malloc + cache misses). (`19`, `32`.)
</details>

### A2. `std::map` vs `std::unordered_map` vs sorted `std::vector`?
<details><summary>Answer</summary>
`map` = red-black tree, O(log n), **ordered**, node per element (pointer
chase, ~cache-hostile). `unordered_map` = hash table, O(1) average, O(n)
worst, unordered, still node-per-element (buckets of nodes). **Sorted
`vector` + binary search** = O(log n) lookup, **contiguous** (fastest
for read-heavy / rare-mutation). HFT order book: neither — flat array
indexed by price tick (`39`, `43/14`). Interviewer wants: "depends on
mutation rate, ordering need, and cache."
</details>

### A3. `std::deque` internally kya hai?
<details><summary>Answer</summary>
Array of fixed-size chunks + a map (index array) of chunk pointers. O(1)
push/pop **both ends**, O(1) indexed access, but elements **not** fully
contiguous (chunk boundaries). References stable across end-insertion (not
middle). Ring-buffer-like use, but a real SPSC ring is faster + bounded
(`36/15`).
</details>

### A4. `std::array` vs C array vs `std::vector`?
<details><summary>Answer</summary>
`std::array<T,N>` = fixed-size, stack (or wherever the object lives), zero
overhead, knows its size, copyable, works with algorithms. C array
decays to pointer, no size, awkward. `std::vector` = heap, dynamic,
size + capacity. HFT: `std::array` for fixed bounded buffers (book
levels, ring storage), `std::vector` with `reserve()` for
startup-sized-once. (`09-ARRAYS`, `19`.)
</details>

### A5. `std::string` SSO kya hai?
<details><summary>Answer</summary>
**Small String Optimization** — chhoti strings (libstdc++ ~15 chars,
libc++ ~22) inline buffer mein store hoti, no heap allocation. Bade
strings heap pe. Isliye `std::string` ka `sizeof` ~32 (not just a
pointer). Move of a small string = copy (nothing to steal). Hot path pe
still watch for the heap case; `string_view` for non-owning. (`10-STRINGS`.)
</details>

### A6. `std::string_view` ke 2 danger.
<details><summary>Answer</summary>
(1) **Dangling** — view apne se chhoti-zindagi wale data ko point kare
(`sv = get_string().substr(...)` — temporary mar gaya). (2) **Not
null-terminated** — `sv.data()` ko `strlen`/C API ko pass karna galat
(view length-based hai). Use for parsing / passing substrings of a live
buffer; store the owning `std::string` if lifetime unclear. (`45/12` A9.)
</details>

---

## B — Iterators & invalidation

### B1. Iterator categories batao (C++20 concepts).
<details><summary>Answer</summary>
`input` (single-pass read), `output` (single-pass write), `forward`
(multi-pass), `bidirectional` (`--`), `random_access` (`+n`, `<`),
`contiguous` (C++20 — elements contiguous, `to_address`). `vector`/`array`
= contiguous; `list` = bidirectional; `forward_list` = forward;
`unordered_map` = forward. Algorithm ki minimum category se pata chalta
woh kis container pe efficient. (`19`, `20`.)
</details>

### B2. `std::vector` mein iterator/reference invalidation — kab?
<details><summary>Answer</summary>
**`push_back`/`insert`/`resize`** jab capacity exceed ho → reallocation →
**sab** iterators/pointers/references invalid. Within capacity: insert/
erase se erase-point ke **baad** wale invalid. `reserve()` upfront isse
control karta. `erase` return karta agla valid iterator. (`19`, `45/12`
A10.)
</details>

### B3. `std::map` / `std::unordered_map` invalidation?
<details><summary>Answer</summary>
`map` (node-based): insert kabhi invalidate nahi karta; erase sirf erased
element ke iterators. `unordered_map`: erase → only erased; insert →
**rehash** ho sakta → **iterators** invalid, but **references/pointers to
elements stay valid** (nodes move nahi hote, sirf bucket pointers). Yeh
distinction interviewers like. (`19`.)
</details>

### B4. Erase-remove idiom — kya aur kyun.
<details><summary>Answer</summary>
`v.erase(std::remove_if(v.begin(), v.end(), pred), v.end());` —
`remove_if` matching elements ko end ki taraf shift karke ek "new logical
end" iterator return karta (elements physically delete nahi karta),
`erase` phir tail ko truly hata deta. Single pass, no per-element erase
(jo O(n) memmove each). C++20: `std::erase_if(v, pred)` — ek call.
(`19`, `45/12` A10.)
</details>

### B5. Loop mein `v.erase(it)` — bug kyun, fix?
<details><summary>Answer</summary>
`erase(it)` `it` (aur baad wale) invalidate karta; phir `++it` UB. Fix:
`it = v.erase(it);` (return value = next valid) aur `else ++it;`. Ya
`std::erase_if`. `-D_GLIBCXX_DEBUG` yeh runtime pe pakadta. (`45/12` A10.)
</details>

---

## C — Algorithms & complexity

### C1. `std::sort` ka algorithm aur complexity?
<details><summary>Answer</summary>
Introsort = quicksort + heapsort fallback (recursion depth limit se O(n²)
avoid) + insertion sort for small ranges. **O(n log n)** worst case
guaranteed (C++11+). Not stable — `std::stable_sort` (O(n log n) with
extra memory, ya O(n log² n) in-place). (`20-DSA`.)
</details>

### C2. `std::lower_bound` / `upper_bound` / `equal_range` — kya deta?
<details><summary>Answer</summary>
Sorted range pe binary search. `lower_bound(v, x)` = first element **not
less than** x (i.e. `>= x`). `upper_bound(v, x)` = first element
**greater than** x. `equal_range` = `[lower, upper)` = all elements equal
to x. O(log n) comparisons; on a `vector` also cache-friendly.
`std::binary_search` = just bool. (`20`.)
</details>

### C3. `std::vector::push_back` amortized O(1) — kaise?
<details><summary>Answer</summary>
Capacity full pe geometric growth (usually ×1.5 or ×2) → n pushes mein
total copies ≈ 2n → **amortized O(1)** per push (though individual pushes
that reallocate are O(n)). `reserve(n)` upfront se reallocation hi nahi —
important for latency (no mid-hot-path O(n) spike). (`19`, `43`.)
</details>

### C4. `std::unordered_map` O(1) — kab O(n)?
<details><summary>Answer</summary>
Hash collisions — worst case sab keys ek bucket mein (adversarial input
ya bad hash) → O(n) per operation. Also rehash (all elements re-inserted)
O(n) occasionally. HFT: bounded, known key ranges → flat array / perfect
hash / open addressing with a good hash; `std::unordered_map`'s
node-per-element + chaining is cache-hostile anyway. (`19`, `20`.)
</details>

### C5. `emplace_back` vs `push_back` — real difference?
<details><summary>Answer</summary>
`emplace_back(args...)` element ko **in-place** construct karta (perfect-
forwards args to the ctor); `push_back(x)` ek object leta phir move/copy.
Fayda jab ek temporary avoid ho: `v.emplace_back(1, "a")` vs
`v.push_back(T(1, "a"))`. Existing object ke liye fark negligible.
Caveat: `emplace_back` explicit ctors bhi call kar leta (kabhi surprising).
(`19`.)
</details>

### C6. `reserve` vs `resize` — fark?
<details><summary>Answer</summary>
`reserve(n)` — capacity ≥ n karta, **size unchanged**, koi element
construct nahi. `resize(n)` — size ko exactly n karta, naye elements
value-initialize (ya given value se), chhota kare to destruct. Latency:
`reserve` once at startup; `resize` in hot path constructs — avoid.
(`19`.)
</details>

---

## D — HFT-flavoured STL

### D1. Order book ke liye `std::map<price, level>` kyun galat?
<details><summary>Answer</summary>
Har market message pe ek tree node insert/erase = ek `malloc`/`free` +
pointer-chasing rebalance, O(log n) with terrible constants (cache
misses). Prices ek bounded tick range mein hote → **flat `std::array`
indexed by (price − base)** + cached best-bid/best-ask → O(1), no
allocation, contiguous. Measured ~25× faster book stage (`43/14`, `39`,
`44` `L2Book`).
</details>

### D2. `std::function` hot path mein kyun avoid?
<details><summary>Answer</summary>
Type-erased — ek indirect call (like virtual), aur agar captured state
bada ho to **heap allocation** (small-buffer optimization varies). Cost
+ non-determinism. Alternatives: template callback (`template<class F>`),
function pointer, `std::variant` of concrete handlers, tag dispatch,
compile-time `if constexpr`. (`21`, `36`, `43/12`.)
</details>

### D3. `std::pmr` (polymorphic memory resources) kya deta?
<details><summary>Answer</summary>
Runtime-swappable allocator via a base `memory_resource*` — `pmr::vector`,
`pmr::string` etc. ek `monotonic_buffer_resource` (bump allocator on a
stack buffer) use kar sakte → zero heap in a scope, O(1) alloc, bulk
free. Measured `pmr::vector` p50 ~40 ns vs `std::vector` ~270
(`36/05`). Downside: type carries the resource pointer; not zero-cost
like a template allocator. (`19`, `36`.)
</details>

### D4. `std::span` kaise use karte HFT parsing mein?
<details><summary>Answer</summary>
`std::span<const std::byte>` = pointer + length, non-owning, zero-cost
view over a buffer. Wire frame ko `span` se parse karo (bounds-checked
sub-spans), copy nahi. Ownership buffer ke paas rehta. C++20; pre-C++20
`gsl::span` ya `{ptr, len}` by hand. (`38`, `42`, `12-POINTERS`.)
</details>

### D5. `std::vector<bool>` ka trap.
<details><summary>Answer</summary>
Ye **specialization** hai — bits pack karta, `operator[]` ek real `bool&`
nahi, ek **proxy object** return karta. `auto x = v[i];` → proxy, not
`bool`. `&v[i]` doesn't give `bool*`. Algorithms jo `bool*` expect karte
break. Use `std::vector<char>`, `std::bitset<N>` (fixed), ya
`boost::dynamic_bitset`. (`19`, `45/17`.)
</details>

---

## Interview tips for Layer 4

- Container choice ka jawab hamesha teen axes pe: **complexity**,
  **mutation pattern**, **cache/allocation behaviour**. "vector by
  default; list almost never; map when ordered + node-stability needed;
  for HFT often a flat array."
- Invalidation rules crisp rakho — `vector` realloc = all; `map` = only
  erased; `unordered_map` insert = iterators (not refs).
- Har STL question ka HFT follow-up: "aur hot path pe hum yeh
  container/algorithm nahi use karte kyunki..." — allocation, node
  chasing, indirect calls.

## Next
→ [`06-templates-questions.md`](06-templates-questions.md)
