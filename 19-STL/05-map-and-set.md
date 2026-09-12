# 05 — `map`, `set`, `multimap`, `multiset`

## Prerequisites
- [`04-deque-list-forward-list.md`](04-deque-list-forward-list.md)
- Folder 20 preview (trees), folder 07 (cache), operator `<` (folder 05)

## Yeh topic abhi kyun
Jab aapko elements ko **sorted order mein rakhna** ho, ya **range queries**
chahiye ("saare orders price 100 se 105 ke beech"), ya ordered iteration —
tab `std::map` / `std::set`. Ye **balanced binary search trees** (red-black
trees) hain: `O(log n)` sab kuch, hamesha sorted. Cost: har node ek alag heap
allocation → pointer chasing → cache misses.

---

## The four ordered associative containers

| Container | Holds | Duplicate keys? |
|---|---|---|
| `std::set<K>` | keys only | no |
| `std::map<K, V>` | key → value pairs | no (unique keys) |
| `std::multiset<K>` | keys only | yes |
| `std::multimap<K, V>` | key → value pairs | yes |

All four: **sorted by key** (via `std::less<K>` = `operator<` by default),
`O(log n)` insert / erase / find, **bidirectional** iterators that yield elements
**in sorted order**.

```cpp
#include <map>
#include <set>

std::set<int> s{5, 1, 3, 1};             // stored as 1 3 5  (dup 1 dropped)
s.insert(4);                             // O(log n)
s.count(3);                             // 0 or 1
s.contains(3);                          // C++20 -- bool
s.erase(5);

std::map<std::string, int> m;
m["apple"] = 3;                         // inserts {"apple", 0} then assigns 3  -- see Trap 1
m.insert({"banana", 5});
m.at("apple");                         // 3  -- throws if absent
auto it = m.find("cherry");           // == m.end() if absent

for (const auto& [key, val] : m) { ... }   // iterates in KEY-SORTED order
```

---

## What "red-black tree" buys you

A red-black tree is a **self-balancing BST**: after every insert/erase it does
O(1) amortized rotations + recolours to keep height ≈ `1.44 log₂(n)`. So the
tree never degrades to a linked list (which a naive BST does on sorted input).

Guarantees you get and `unordered_map` does **not**:

1. **Ordered iteration** — `begin()` → smallest, `rbegin()` → largest, in between
   sorted. `unordered_map` iteration order is arbitrary.
2. **Range queries** — `lower_bound(k)` (first element `>= k`), `upper_bound(k)`
   (first `> k`), `equal_range(k)` (the `[lower, upper)` sub-range). O(log n) to
   locate, then walk.
   ```cpp
   for (auto it = m.lower_bound(100); it != m.upper_bound(105); ++it)
       use(it->first, it->second);      // all keys in [100, 105]
   ```
3. **Predecessor / successor** — `--it` from any position.
4. **Stable iterators/references** — insert/erase **never** invalidates other
   elements' iterators or references (only `erase` invalidates the erased one).
   Same stability as `std::list`.
5. **No rehash spikes** — worst-case O(log n), always. `unordered_map` can stall
   on a rehash.

---

## Cost model

- `sizeof` per node ≈ `sizeof(K[,V]) + 3 pointers (left, right, parent) + 1
  colour byte` + allocator header ≈ 40+ bytes for a `map<int,int>`.
- `find` / `insert`: `O(log n)` **pointer chases** down the tree — each node at a
  random heap address → ~`log n` cache misses. For n = 200,000, log₂ ≈ 18 → up
  to ~18 misses per lookup.
- `examples/02_map_vs_unordered.cpp`, n = 200,000, measured on this box:

  | structure | ns / lookup |
  |---|---|
  | `std::map` | **1049** |
  | `std::unordered_map` (reserved) | 113 |
  | sorted `std::vector` + `std::lower_bound` | 357 |

  `map` is ~9x slower than `unordered_map` and ~3x slower than a sorted vector —
  **entirely cache effects**, not algorithmic (all are ~log n or O(1)).

---

## `map::operator[]` — the insert-if-absent trap

```cpp
std::map<std::string, int> m;
int x = m["missing"];        // ⚠️ INSERTS {"missing", 0}, returns ref to it. m.size() is now 1
if (m["k"] == 0) { ... }      // ⚠️ inserts "k" if absent -- often not what you want

// read without inserting:
auto it = m.find("k");
if (it != m.end()) use(it->second);

// C++20:
if (m.contains("k")) ...

// insert without overwriting:
m.try_emplace("k", 42);      // no-op if "k" exists; doesn't even construct the value if it exists
auto [it2, inserted] = m.insert_or_assign("k", 42);   // overwrites if exists
```

`operator[]` requires `V` to be default-constructible, and **mutates the map on a
read**. On a `const std::map` it doesn't compile — use `.at()` or `.find()`.

---

## Custom comparators

```cpp
std::set<int, std::greater<int>> desc;          // sorted DESCENDING
desc.insert(1); desc.insert(3); desc.insert(2); // iterates 3 2 1

struct ByLen { bool operator()(const std::string& a, const std::string& b) const {
    return a.size() < b.size() || (a.size() == b.size() && a < b);   // MUST be a strict weak ordering
}};
std::set<std::string, ByLen> byLen;

// transparent comparator -> heterogeneous lookup (no temporary key):
std::set<std::string, std::less<>> t;
t.find("literal");     // with std::less<> (note the <>), no std::string temp constructed
```

The comparator must be a **strict weak ordering**: irreflexive (`!cmp(a,a)`),
antisymmetric, transitive, and transitive on incomparability. A broken comparator
(e.g. `<=`) is **undefined behaviour** — crashes or infinite loops inside the
tree.

---

## `multimap` / `multiset`

```cpp
std::multimap<int, std::string> mm;
mm.insert({1, "a"}); mm.insert({1, "b"}); mm.insert({2, "c"});
mm.count(1);                              // 2
auto [lo, hi] = mm.equal_range(1);
for (auto it = lo; it != hi; ++it) use(it->second);   // "a", "b"  (insertion order preserved within a key, C++11+)
mm.erase(1);                             // erases ALL entries with key 1
mm.erase(mm.find(1));                    // erases just one
```

No `operator[]` / `at()` on multi- containers (ambiguous). Use `equal_range`.

---

## Andar kya hota hai

- Insert: walk from root comparing `cmp(key, node->key)` to pick left/right until
  a null child; allocate a node there; then fix-up — recolour and up to 2
  rotations walking back toward the root. O(log n) comparisons + O(1) amortized
  structural change.
- Iteration (`++it`): in-order tree successor — if there's a right child, go
  right then leftmost; else walk up via `parent` until you come from a left
  child. O(1) amortized, O(log n) worst per step. Node addresses are unordered in
  memory → each step risks a cache miss.
- `lower_bound(k)`: same downward walk, remembering the last node where you went
  left → O(log n), no allocation.
- libstdc++ `_Rb_tree` stores a header node whose `parent` is the root, `left`/
  `right` are min/max → `begin()` and `rbegin()` are O(1).

> **HFT relevance:** `std::map`/`std::set` are usually **too slow for hot paths** —
> `log n` cache misses per lookup. An order book *conceptually* wants a sorted
> map of price → level, but production uses a **sorted `std::vector<Level>` +
> `lower_bound`** (contiguous, one cache line often holds several levels) or a
> flat array indexed by price-ticks-from-reference (O(1), zero misses). `std::map`
> is fine in the **control plane** — config, symbol metadata, session tables —
> where lookups are rare and ordered iteration is convenient. The stable-iterator
> guarantee occasionally justifies it. See file 25 and folder 39.

---

## Hands-on

```bash
./build.ps1 fast 19-STL/examples/02_map_vs_unordered.cpp
```

Compare the three ns/lookup numbers. Then add a `std::map` range query
(`lower_bound`/`upper_bound`) over a price range and note it has **no**
`unordered_map` equivalent.

---

## ⚠️ Traps

### Trap 1 — `operator[]` inserts on read
```cpp
if (m["key"] > 0) ...   // ⚠️ inserts {"key", 0} if absent. Use m.find / m.contains
```

### Trap 2 — broken comparator (not strict-weak)
```cpp
std::set<int, std::less_equal<int>> s;   // ❌ <= is not irreflexive -> UB inside the tree
```

### Trap 3 — modifying a key through an iterator
```cpp
for (auto it = s.begin(); it != s.end(); ++it) *it += 1;   // ❌ set elements are const -- would break the ordering
```

### Trap 4 — `multimap::erase(key)` erases all matches
```cpp
mm.erase(1);   // removes EVERY entry with key 1. To remove one: mm.erase(mm.find(1))
```

### Trap 5 — assuming `map` is a hash table (perf)
```cpp
// map lookup is O(log n) with ~log n cache misses -- for pure lookups an
// unordered_map (or sorted vector) is several times faster. Choose map for ORDER
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`map` is a hash table" | Red-black **tree** — sorted, O(log n), pointer-chasing |
| "`m[k]` just reads" | Inserts `{k, V{}}` if absent, mutates the map |
| "`map` and `unordered_map` perform the same" | `unordered_map` ~O(1); `map` O(log n) + cache misses — several times slower for lookup |
| "I can change a key via `*it`" | Keys are `const` — changing one would corrupt the tree |
| "`multimap::erase(k)` removes one" | Removes **all** entries with key `k` |

---

## Exercises

1. **Range query:** `std::map<int,std::string> book;` populated with prices. Print
   every entry with price in `[100, 110]` inclusive, in order.

   <details><summary>Answer</summary>

   `for (auto it = book.lower_bound(100); it != book.upper_bound(110); ++it)
   std::printf("%d %s\n", it->first, it->second.c_str());`
   </details>

2. **operator[] bug:** `std::map<std::string,int> freq; for (auto& w : words)
   freq[w]++;` — is this correct for word counting? What's the subtlety?

   <details><summary>Answer</summary>

   Correct. `freq[w]` inserts `{w, 0}` on first sight (value-initialised int → 0),
   then `++` makes it 1; subsequent sightings just `++`. The "insert on read"
   behaviour is exactly what you want here. (`freq.at(w)` would throw instead.)
   </details>

3. **Why sorted vector wins:** `examples/02` shows sorted `vector` + `lower_bound`
   at 357 ns/lookup vs `map` at 1049 ns, both O(log n). Explain.

   <details><summary>Answer</summary>

   Binary search over a contiguous array touches ~log n elements but they're in a
   handful of cache lines near each other (and the last few steps are in the same
   line) → few misses. The tree's log n nodes are at scattered heap addresses →
   ~log n independent cache misses. Same comparisons, very different memory
   behaviour.
   </details>

4. **Comparator:** define a `std::set<Point>` sorted by Euclidean distance from
   the origin, ties broken by `x` then `y`. Why do you need the tie-breakers?

   <details><summary>Answer</summary>

   `struct Cmp { bool operator()(Point a, Point b) const { double da=a.x*a.x+a.y*a.y,
   db=b.x*b.x+b.y*b.y; if (da!=db) return da<db; if (a.x!=b.x) return a.x<b.x;
   return a.y<b.y; } };`. Without tie-breakers, two distinct points at the same
   distance compare equal → the set treats them as duplicates and drops one.
   </details>

5. **Stability:** you hold `std::map<int,Order>::iterator`s to some orders and
   keep inserting/erasing *other* orders. Are your iterators safe? Would they be
   with `std::unordered_map`? With `std::vector`?

   <details><summary>Answer</summary>

   `map` — safe; insert/erase never invalidates other elements' iterators/refs.
   `unordered_map` — refs stay valid, but **iterators** are invalidated on rehash.
   `vector` — any reallocation invalidates everything.
   </details>

---

## Interview questions

1. `std::map` andar kya hai (red-black tree) — height guarantee kya?
2. `map` vs `unordered_map` — 5 cheezein jo `map` guarantee karta hai?
3. `map::operator[]` ka side effect — read pe insert kyun?
4. Comparator "strict weak ordering" kyun hona chahiye — `<=` diya to?
5. `map` lookup `unordered_map` se slow kyun jab dono ~log/O(1)? (cache)
6. `multimap::erase(key)` kya karta, ek hi kaise hataye?

---

## Next
→ [`06-unordered-containers.md`](06-unordered-containers.md)
