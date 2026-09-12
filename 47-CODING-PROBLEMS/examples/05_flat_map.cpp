// 05_flat_map.cpp
// ============================================================
// Folder 47 file 05 C1: a std::map-like container backed by a SORTED vector.
// O(log n) find, contiguous memory (cache-friendly), O(n) insert/erase.
// Read-heavy / batch-build workloads. C++23 ships this as std::flat_map.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g -O0 05_flat_map.cpp -o t && ./t
// ============================================================

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <utility>
#include <vector>

template <class K, class V>
class FlatMap {
    std::vector<std::pair<K, V>> data_;                 // kept sorted by .first

    auto lower(const K& k) {
        return std::lower_bound(data_.begin(), data_.end(), k,
                                [](const std::pair<K, V>& e, const K& key) { return e.first < key; });
    }
public:
    V& operator[](const K& k) {
        auto it = lower(k);
        if (it != data_.end() && it->first == k) return it->second;
        it = data_.insert(it, std::pair<K, V>(k, V{}));  // O(n) shift, keeps sorted
        return it->second;
    }
    const V* find(const K& k) {
        auto it = lower(k);
        return (it != data_.end() && it->first == k) ? &it->second : nullptr;
    }
    bool erase(const K& k) {
        auto it = lower(k);
        if (it == data_.end() || it->first != k) return false;
        data_.erase(it);
        return true;
    }
    bool        contains(const K& k) { return find(k) != nullptr; }
    std::size_t size() const { return data_.size(); }
    auto        begin() const { return data_.begin(); }
    auto        end()   const { return data_.end(); }
};

int main() {
    FlatMap<int, int> m;

    // insert out of order — storage stays sorted
    m[50] = 500;
    m[10] = 100;
    m[30] = 300;
    m[20] = 200;
    m[40] = 400;
    assert(m.size() == 5);

    // iteration is in key order (that's the whole point vs unordered_map)
    int prev = -1;
    int seen = 0;
    for (const auto& [k, v] : m) {
        assert(k > prev);
        assert(v == k * 10);
        prev = k;
        ++seen;
    }
    assert(seen == 5);

    // find
    assert(m.find(30) != nullptr && *m.find(30) == 300);
    assert(m.find(35) == nullptr);
    assert(m.contains(10) && !m.contains(999));

    // operator[] on existing key updates; on missing key inserts default
    m[30] = 333;
    assert(*m.find(30) == 333);
    assert(m[999] == 0 && m.size() == 6);               // inserted V{}

    // erase
    assert(m.erase(20) && !m.erase(20));
    assert(m.size() == 5 && m.find(20) == nullptr);

    // still sorted after erase + the default insert
    prev = -1;
    for (const auto& [k, v] : m) { assert(k > prev); prev = k; (void)v; }

    std::puts("05_flat_map: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - find = lower_bound = O(log n), and the data is ONE contiguous block:
//     ~2 cache lines vs std::map's O(log n) scattered node misses
//     (folder 32: 2-10x faster lookups, way faster iteration).
//   - Cost: a single insert/erase is O(n) (vector shift). For bulk build,
//     push_back everything then one std::sort. Read-mostly maps win big.
//   - Iterator/reference stability is weaker than std::map (any insert can
//     realloc). Know your workload.
// ============================================================
