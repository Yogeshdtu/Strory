# 02 — STL containers cheatsheet

Deep dives: folder `19-STL`. Cache angle: folder `32`, `43`.

---

## Sequence containers

| Container | Memory | Random access | Insert/erase | Iterator/ref stability | Use when |
|---|---|---|---|---|---|
| `vector<T>` | one contiguous block | `O(1)` | back `O(1)` amort; middle `O(n)` | realloc → **all** invalid; else refs stable, `end()` moves | **default choice** |
| `array<T,N>` | inline, fixed | `O(1)` | — | always stable | fixed size, no heap |
| `deque<T>` | chunks of blocks | `O(1)` | both ends `O(1)`; middle `O(n)` | push_front/back → iters invalid, **refs stay valid**; middle → all | queue-ish, grows both ends |
| `list<T>` | node per element | `O(n)` | anywhere `O(1)` (with iterator) | only the erased node | huge elements + splice; rarely worth it |
| `forward_list<T>` | singly-linked node | `O(n)` | `O(1)` after a position | only the erased node | memory-tight linked list |
| `string` | contiguous + SSO | `O(1)` | like vector | like vector; `c_str()` invalidated by mutation | text |

> `list`/`forward_list` iterate ~10–40× slower than `vector` for the same `O(n)`
> — pointer-chase cache misses vs a prefetched contiguous scan. (folder 32/04)

---

## Associative — ordered (red-black tree)

| Container | Lookup | Ordered iter | Notes |
|---|---|---|---|
| `map<K,V>` | `O(log n)` | yes | node per entry, `log n` scattered misses |
| `multimap<K,V>` | `O(log n)` | yes | duplicate keys |
| `set<K>` / `multiset<K>` | `O(log n)` | yes | keys only |

`erase(it)` invalidates only `it`. Needs only `operator<` (or a comparator).
Use `std::less<>` (transparent) for heterogeneous lookup without building a key.

---

## Associative — unordered (hash table)

| Container | Lookup avg / worst | Notes |
|---|---|---|
| `unordered_map<K,V>` | `O(1)` / `O(n)` | needs `std::hash<K>` + `operator==`; **`reserve(n)` upfront** to avoid rehash spikes |
| `unordered_set<K>` | `O(1)` / `O(n)` | |
| `unordered_multimap/set` | `O(1)` / `O(n)` | |

Rehash invalidates **iterators**, keeps **refs/pointers** valid (nodes don't move).
Default `std::hash<int>` is often identity → combine your own for `pair`/struct keys.
For a fixed dense key range → a plain `array` indexed by `key - base` beats any hash.

---

## Container adaptors

| Adaptor | Backed by | Ops |
|---|---|---|
| `stack<T>` | `deque` (default) | `push` / `pop` / `top` |
| `queue<T>` | `deque` | `push` / `pop` / `front` / `back` |
| `priority_queue<T>` | `vector` + heap | `push` / `pop` / `top` (max-heap; `greater` → min-heap). **No decrease-key** — use lazy deletion |

---

## Views & spans (non-owning — watch lifetime)

| Type | Is | Gotcha |
|---|---|---|
| `string_view` | `{const char*, size_t}` | **not** null-terminated; dangles if the owner dies |
| `span<T>` (C++20) | `{T*, size_t}` over any contiguous range | non-owning; caller owns the data |

---

## Picking one (interview answer shape)

1. **Contiguous by default** — `vector` / `array`. Cache locality wins most real workloads.
2. Need **ordered iteration** or **range queries** → `map` or a **sorted `vector` + `lower_bound`**.
3. Need **point lookup, big N, latency-critical** → `unordered_map` *reserved*, or an
   open-addressed flat hash, or a direct-index `array` if keys are dense.
4. `list` only for `O(1)` splice of large nodes across lists — measure first.
5. HFT hot path: fixed-capacity, preallocated, no rehash/realloc mid-session.

---

## Complexity quick table

| Op | vector | deque | list | map | unordered_map |
|---|---|---|---|---|---|
| `[]` / `at` | O(1) | O(1) | — | O(log n) | O(1) avg |
| push_back | O(1)* | O(1) | O(1) | — | — |
| push_front | O(n) | O(1) | O(1) | — | — |
| insert middle | O(n) | O(n) | O(1)† | O(log n) | O(1) avg |
| find | O(n) | O(n) | O(n) | O(log n) | O(1) avg |
| erase | O(n) | O(n) | O(1)† | O(log n) | O(1) avg |

\* amortized (realloc doubles).  † given an iterator to the position.

## Next
→ [`03-stl-algorithms.md`](03-stl-algorithms.md)
