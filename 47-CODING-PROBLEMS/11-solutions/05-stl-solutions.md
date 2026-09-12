# 05 — STL: worked solutions

Poore code — flat containers, custom hashes, allocators, the invalidation-safe
patterns. Baaki `05-stl-problems.md` ke `<details>` blocks mein.

---

## B10 — Custom hash for `unordered_map<pair<int,int>, V>`

```cpp
#include <cstdint>
#include <unordered_map>
#include <utility>

struct PairHash {
    std::size_t operator()(const std::pair<int, int>& p) const noexcept {
        std::uint64_t a = static_cast<std::uint32_t>(p.first);
        std::uint64_t b = static_cast<std::uint32_t>(p.second);
        std::uint64_t x = (a << 32) | b;
        // splitmix64 finalizer — cheap, good avalanche
        x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27; x *= 0x94d049bb133111ebULL;
        x ^= x >> 31;
        return static_cast<std::size_t>(x);
    }
};
// std::unordered_map<std::pair<int,int>, V, PairHash> m;
```

`std::hash<std::pair>` standard mein nahi. Naive `h(a) ^ h(b)` — `(x,y)` aur
`(y,x)` collide, aur `std::hash<int>` often identity → poor spread. Bit-pack +
a real finalizer. Alternatives: `boost::hash_combine`, ya just use `std::map`
(needs only `<`).

---

## B14 — Not a strict weak ordering (why `<=` breaks `std::sort`)

```cpp
#include <algorithm>
#include <vector>
// std::vector<int> v = ...;
// std::sort(v.begin(), v.end(), [](int a, int b){ return a <= b; });   // UB
```

`std::sort` ka comparator **strict weak ordering** hona chahiye:
`comp(a, a) == false` (irreflexive), transitive, aur "equivalence" transitive.
`a <= b` pe `comp(a, a) == true` → invariant toota. libstdc++ implementation is
par array bounds ke bahar padh sakta (partition pointer overshoot) → crash /
corruption. `-D_GLIBCXX_DEBUG` isse assert karta. Hamesha `<` semantics likho:
`return a < b;` (ascending), `return a > b;` (descending).

---

## C1 — `flat_map` (sorted vector backing)

```cpp
#include <algorithm>
#include <vector>

template <class K, class V>
class FlatMap {
    std::vector<std::pair<K, V>> data_;      // sorted by key
    auto lb(const K& k) {
        return std::lower_bound(data_.begin(), data_.end(), k,
                                [](const auto& e, const K& key) { return e.first < key; });
    }
public:
    V& operator[](const K& k) {
        auto it = lb(k);
        if (it != data_.end() && it->first == k) return it->second;
        return data_.insert(it, {k, V{}})->second;      // O(n) shift
    }
    const V* find(const K& k) {
        auto it = lb(k);
        return (it != data_.end() && it->first == k) ? &it->second : nullptr;
    }
    bool erase(const K& k) {
        auto it = lb(k);
        if (it == data_.end() || it->first != k) return false;
        data_.erase(it);
        return true;
    }
    auto begin() { return data_.begin(); }
    auto end()   { return data_.end(); }
    std::size_t size() const { return data_.size(); }
};
```

`find` = `O(log n)` + **contiguous** memory (2 cache lines vs `std::map` ki
`log n` scattered node misses — folder 32, 2–10× faster lookups). `insert`/
`erase` = `O(n)` shift. Read-heavy / batch-build workloads ke liye. C++23:
`std::flat_map`. Bulk build → `push_back` all + one `std::sort`.

---

## C4 — Stateless custom allocator + pmr

```cpp
#include <cstddef>
#include <cstdlib>
#include <new>

template <class T>
struct Mallocator {
    using value_type = T;
    Mallocator() = default;
    template <class U> Mallocator(const Mallocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        if (n > std::size_t(-1) / sizeof(T)) throw std::bad_alloc{};
        if (void* p = std::malloc(n * sizeof(T))) return static_cast<T*>(p);
        throw std::bad_alloc{};
    }
    void deallocate(T* p, std::size_t) noexcept { std::free(p); }
};
template <class A, class B>
bool operator==(const Mallocator<A>&, const Mallocator<B>&) noexcept { return true; }
template <class A, class B>
bool operator!=(const Mallocator<A>&, const Mallocator<B>&) noexcept { return false; }

// pmr version — stack buffer, zero heap for the first pushes:
//   #include <memory_resource>
//   #include <vector>
//   std::array<std::byte, 4096> buf;
//   std::pmr::monotonic_buffer_resource res{buf.data(), buf.size()};
//   std::pmr::vector<int> v{&res};
```

Stateless allocator → `operator==` always `true` (any instance can free any
other's memory). Stateful (arena-backed) → `==` compares the arena pointer, and
containers won't move-optimize across unequal allocators.

---

## C8 — Merge k sorted lists (min-heap)

```cpp
#include <queue>
#include <vector>

std::vector<int> merge_k(const std::vector<std::vector<int>>& lists) {
    struct Node { int val; int list; std::size_t idx; };
    auto cmp = [](const Node& a, const Node& b) { return a.val > b.val; };  // min-heap
    std::priority_queue<Node, std::vector<Node>, decltype(cmp)> pq(cmp);

    for (int i = 0; i < static_cast<int>(lists.size()); ++i)
        if (!lists[i].empty()) pq.push({lists[i][0], i, 0});

    std::vector<int> out;
    while (!pq.empty()) {
        Node n = pq.top(); pq.pop();
        out.push_back(n.val);
        if (n.idx + 1 < lists[n.list].size())
            pq.push({lists[n.list][n.idx + 1], n.list, n.idx + 1});
    }
    return out;
}
```

Heap mein har list ka current head (max `k` entries). Pop min, push us list ka
next. `O(N log k)` where `N` = total elements. `std::priority_queue` default
max-heap — `greater` comparator se min-heap.
