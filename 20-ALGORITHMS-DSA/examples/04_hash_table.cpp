// 04_hash_table.cpp
// ============================================================
// Write your own hash table -- OPEN ADDRESSING (linear probing),
// power-of-two capacity, tombstones for erase, and a resize.
// Then benchmark it against std::unordered_map.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 04_hash_table.cpp -o ht && ./ht
//   BENCH: g++ -std=c++20 -O2 04_hash_table.cpp -o ht && ./ht
// ============================================================
//   open addressing: all entries live in ONE contiguous array.
//   on collision -> probe the next slot (linear). ~1 cache line per lookup.
//   vs std::unordered_map (separate chaining) -> a heap node per element.
// ============================================================

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <unordered_map>
#include <vector>

using Clock = std::chrono::steady_clock;

class HashMapU64 {
    enum State : std::uint8_t { EMPTY, FULL, TOMB };
    struct Slot {
        std::uint64_t key   = 0;
        std::uint64_t value = 0;
        State         state = EMPTY;
    };
    std::vector<Slot> slots_;
    std::size_t       count_ = 0;      // FULL slots
    std::size_t       used_  = 0;      // FULL + TOMB (probe-length pressure)

    static std::uint64_t mix(std::uint64_t x) {          // splitmix64 finalizer -- good avalanche
        x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27; x *= 0x94d049bb133111ebULL;
        x ^= x >> 31;
        return x;
    }
    std::size_t mask() const { return slots_.size() - 1; }   // size is a power of two

    void rehash(std::size_t newCap) {
        std::vector<Slot> old = std::move(slots_);
        slots_.assign(newCap, Slot{});
        count_ = used_ = 0;
        for (const Slot& s : old)
            if (s.state == FULL) insert(s.key, s.value);
    }

public:
    explicit HashMapU64(std::size_t cap = 16) {
        std::size_t p = 1;
        while (p < cap) p <<= 1;
        slots_.assign(p, Slot{});
    }

    std::size_t size() const { return count_; }

    void insert(std::uint64_t key, std::uint64_t value) {
        if ((used_ + 1) * 4 >= slots_.size() * 3)            // load factor 0.75 (FULL+TOMB)
            rehash(slots_.size() * 2);
        std::size_t i = mix(key) & mask();
        std::ptrdiff_t firstTomb = -1;
        for (;;) {
            Slot& s = slots_[i];
            if (s.state == EMPTY) {
                if (firstTomb >= 0) { Slot& t = slots_[static_cast<std::size_t>(firstTomb)];
                                      t = {key, value, FULL}; }
                else                { s = {key, value, FULL}; ++used_; }
                ++count_;
                return;
            }
            if (s.state == TOMB && firstTomb < 0) firstTomb = static_cast<std::ptrdiff_t>(i);
            if (s.state == FULL && s.key == key) { s.value = value; return; }   // overwrite
            i = (i + 1) & mask();                            // linear probe
        }
    }

    const std::uint64_t* find(std::uint64_t key) const {
        std::size_t i = mix(key) & mask();
        for (;;) {
            const Slot& s = slots_[i];
            if (s.state == EMPTY) return nullptr;
            if (s.state == FULL && s.key == key) return &s.value;
            i = (i + 1) & mask();
        }
    }

    bool erase(std::uint64_t key) {
        std::size_t i = mix(key) & mask();
        for (;;) {
            Slot& s = slots_[i];
            if (s.state == EMPTY) return false;
            if (s.state == FULL && s.key == key) { s.state = TOMB; --count_; return true; }
            i = (i + 1) & mask();
        }
    }
};

int main() {
    std::printf("=== 1. correctness ===\n");
    {
        HashMapU64 h;
        for (std::uint64_t i = 0; i < 1000; ++i) h.insert(i, i * i);
        int ok = 0;
        for (std::uint64_t i = 0; i < 1000; ++i) {
            const std::uint64_t* v = h.find(i);
            if (v && *v == i * i) ++ok;
        }
        std::printf("  inserted 1000, found %d with correct value, size=%zu\n", ok, h.size());
        h.erase(500);
        std::printf("  after erase(500): find(500) = %s, find(501) = %s, size=%zu\n",
                    h.find(500) ? "hit" : "miss", h.find(501) ? "hit" : "miss", h.size());
        h.insert(500, 999);                       // re-insert reuses a tombstone
        std::printf("  re-insert(500,999): find -> %llu\n",
                    static_cast<unsigned long long>(*h.find(500)));
    }

    std::printf("\n=== 2. benchmark vs std::unordered_map (-O2) ===\n");
    {
        const int N = 1'000'000;
        std::mt19937_64 rng{12345};
        std::vector<std::uint64_t> keys(static_cast<std::size_t>(N));
        for (auto& k : keys) k = rng();

        // ---- build ----
        auto t0 = Clock::now();
        HashMapU64 mine(static_cast<std::size_t>(N) * 2);
        for (auto k : keys) mine.insert(k, k);
        auto t1 = Clock::now();

        std::unordered_map<std::uint64_t, std::uint64_t> std_map;
        std_map.reserve(static_cast<std::size_t>(N));
        for (auto k : keys) std_map.emplace(k, k);
        auto t2 = Clock::now();

        // ---- lookup (all present) + (half absent) ----
        std::vector<std::uint64_t> q = keys;
        for (std::size_t i = 0; i < q.size(); i += 2) q[i] ^= 0x1ULL;   // ~half now absent

        volatile std::uint64_t sink = 0;
        auto t3 = Clock::now();
        for (auto k : q) { const std::uint64_t* v = mine.find(k); if (v) sink += *v; }
        auto t4 = Clock::now();
        for (auto k : q) { auto it = std_map.find(k); if (it != std_map.end()) sink += it->second; }
        auto t5 = Clock::now();

        auto ns = [&](auto a, auto b){
            return std::chrono::duration<double, std::nano>(b - a).count() / static_cast<double>(N);
        };
        std::printf("  build  : mine %6.1f ns/op   std::unordered_map %6.1f ns/op\n", ns(t0,t1), ns(t1,t2));
        std::printf("  lookup : mine %6.1f ns/op   std::unordered_map %6.1f ns/op\n", ns(t3,t4), ns(t4,t5));
        std::printf("  (sink %llu)\n", static_cast<unsigned long long>(sink));
    }

    std::printf(
        "\n"
        "  Open addressing: entries contiguous -> ~1 cache line per lookup, no per-node alloc.\n"
        "  Costs: tombstones accumulate on churn (periodic rehash clears them); clustering if\n"
        "  the hash is weak or load factor too high. std::unordered_map's chaining is mandated\n"
        "  by its API (bucket iteration, reference stability) -> it can't be this flat.\n");
    return 0;
}
