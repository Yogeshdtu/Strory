// 02_lru_cache.cpp
// ============================================================
// CLASSIC: O(1) get/put LRU cache. hash map + doubly linked list.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 02_lru_cache.cpp -o t && ./t
// ============================================================
// INTERVIEWER KYA DEKH RAHA:
//   - the map -> list-node data structure choice, and WHY (O(1) both ops)
//   - splice-to-front on access, evict from back on overflow
//   - std::list::splice + iterators (no reallocation, no node copy)
//   - edge cases: capacity 0, update existing key, evict correct victim
//   - HFT aside: this uses per-node alloc + pointer chase -> for a hot
//     bounded cache you'd use a flat open-addressed table + an intrusive
//     LRU (indices, no std::list). Mention it.
// ============================================================

#include <cassert>
#include <cstdio>
#include <list>
#include <optional>
#include <unordered_map>
#include <utility>

class LruCache {
public:
    explicit LruCache(std::size_t capacity) : cap_(capacity) {}

    // Returns the value and marks the key most-recently-used.
    std::optional<int> get(int key) {
        auto it = index_.find(key);
        if (it == index_.end()) return std::nullopt;
        // move this node to the front (most recent)
        order_.splice(order_.begin(), order_, it->second);
        return it->second->second;
    }

    void put(int key, int value) {
        if (cap_ == 0) return;
        auto it = index_.find(key);
        if (it != index_.end()) {
            it->second->second = value;                       // update
            order_.splice(order_.begin(), order_, it->second); // touch
            return;
        }
        if (order_.size() == cap_) {                          // evict LRU (back)
            const int victim = order_.back().first;
            order_.pop_back();
            index_.erase(victim);
        }
        order_.emplace_front(key, value);
        index_[key] = order_.begin();
    }

    std::size_t size() const { return order_.size(); }

private:
    using ListT = std::list<std::pair<int, int>>;             // (key, value), front = MRU
    std::size_t cap_;
    ListT order_;
    std::unordered_map<int, ListT::iterator> index_;          // key -> node; iterators stay valid
};

int main() {
    // capacity 0 -- nothing stored
    {
        LruCache c(0);
        c.put(1, 1);
        assert(!c.get(1).has_value());
        assert(c.size() == 0);
    }

    // basic get/put + eviction order
    {
        LruCache c(2);
        c.put(1, 10);
        c.put(2, 20);
        assert(c.get(1) == 10);          // 1 is now MRU, 2 is LRU
        c.put(3, 30);                    // evicts key 2
        assert(!c.get(2).has_value());
        assert(c.get(3) == 30);
        assert(c.get(1) == 10);
        assert(c.size() == 2);
    }

    // update existing key doesn't grow, refreshes recency
    {
        LruCache c(2);
        c.put(1, 1);
        c.put(2, 2);
        c.put(1, 100);                   // update, 1 becomes MRU
        c.put(3, 3);                     // evicts key 2 (LRU), not key 1
        assert(c.get(1) == 100);
        assert(!c.get(2).has_value());
        assert(c.get(3) == 3);
    }

    std::printf("02_lru_cache: ALL PASS\n");
    return 0;
}

// ============================================================
// COMPLEXITY: get O(1), put O(1) amortized (hash) -- splice is O(1),
// no node reallocation, iterators into std::list stay valid across
// insert/erase of OTHER nodes (key property that makes this work).
//
// HFT VERSION: bounded capacity known -> a flat array of slots, an
// open-addressed key->slot table, and prev/next as int32 indices into
// the slot array (intrusive LRU). No per-op allocation, contiguous,
// cache-friendly. (folders 20, 32, 36)
// ============================================================
