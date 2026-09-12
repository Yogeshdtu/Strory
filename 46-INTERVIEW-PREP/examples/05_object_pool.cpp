// 05_object_pool.cpp
// ============================================================
// HFT CLASSIC: a fixed-capacity object pool with an intrusive free list.
// O(1) acquire/release, zero per-op allocation, no per-slot overhead
// (the "next" index lives in the free slot's storage).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 05_object_pool.cpp -o t && ./t
// ============================================================
// INTERVIEWER KYA DEKH RAHA:
//   - why: no `new`/`delete` in the hot path (unbounded latency, faults)
//   - intrusive free list: the free-list "next" pointer reuses the slot's
//     bytes -> zero bookkeeping overhead per slot
//   - placement new to construct, explicit dtor to destroy
//   - exhaustion -> nullptr (never allocate more)
//   - bonus: generation-checked handles to catch use-after-free of a
//     recycled slot (see 06_... / folder 44)
// ============================================================

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#include <string>
#include <utility>

template <class T, std::size_t N>
class ObjectPool {
    static_assert(N > 0, "pool needs capacity");
    static constexpr std::uint32_t kNil = 0xFFFFFFFFu;

public:
    ObjectPool() {
        // link every slot into the free list: 0 -> 1 -> ... -> N-1 -> nil
        for (std::uint32_t i = 0; i < N; ++i) {
            next_of(i) = (i + 1 < N) ? (i + 1) : kNil;
        }
        free_head_ = 0;
        in_use_ = 0;
    }
    ~ObjectPool() { assert(in_use_ == 0 && "pool destroyed with live objects"); }

    ObjectPool(const ObjectPool&)            = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    template <class... Args>
    T* acquire(Args&&... args) {
        if (free_head_ == kNil) return nullptr;              // exhausted -> caller handles
        const std::uint32_t idx = free_head_;
        free_head_ = next_of(idx);
        ++in_use_;
        return ::new (slot_ptr(idx)) T(std::forward<Args>(args)...);
    }

    void release(T* p) {
        if (p == nullptr) return;
        const std::uint32_t idx = index_of(p);
        assert(idx < N && "pointer not from this pool");
        p->~T();
        next_of(idx) = free_head_;
        free_head_ = idx;
        --in_use_;
    }

    std::size_t in_use()   const { return in_use_; }
    static constexpr std::size_t capacity() { return N; }
    bool owns(const T* p) const {
        auto a = reinterpret_cast<std::uintptr_t>(p);
        auto b = reinterpret_cast<std::uintptr_t>(&storage_[0]);
        return a >= b && a < b + sizeof(storage_) && (a - b) % sizeof(Slot) == 0;
    }

private:
    union Slot {
        std::uint32_t next;                                  // when free
        alignas(T) std::byte bytes[sizeof(T)];               // when in use
        Slot() : next(kNil) {}
        ~Slot() {}
    };

    T*  slot_ptr(std::uint32_t i)       { return reinterpret_cast<T*>(storage_[i].bytes); }
    std::uint32_t& next_of(std::uint32_t i) { return storage_[i].next; }
    std::uint32_t index_of(const T* p) const {
        auto a = reinterpret_cast<std::uintptr_t>(p);
        auto b = reinterpret_cast<std::uintptr_t>(&storage_[0]);
        return static_cast<std::uint32_t>((a - b) / sizeof(Slot));
    }

    Slot          storage_[N];
    std::uint32_t free_head_ = kNil;
    std::size_t   in_use_     = 0;
};

struct Order {
    std::uint64_t id;
    std::int64_t  px;
    std::uint32_t qty;
    std::string   tag;                      // a heap member, to prove ctor/dtor run
    Order(std::uint64_t i, std::int64_t p, std::uint32_t q, std::string t)
        : id(i), px(p), qty(q), tag(std::move(t)) {}
};

int main() {
    ObjectPool<Order, 3> pool;
    assert(pool.in_use() == 0);
    assert(pool.capacity() == 3);

    Order* a = pool.acquire(1ULL, 10025LL, 100U, std::string("AAPL"));
    Order* b = pool.acquire(2ULL, 20050LL, 50U,  std::string("MSFT"));
    Order* c = pool.acquire(3ULL, 30075LL, 25U,  std::string("GOOG"));
    assert(a && b && c);
    assert(pool.in_use() == 3);
    assert(a->tag == "AAPL" && c->qty == 25);

    // exhausted
    Order* d = pool.acquire(4ULL, 0LL, 0U, std::string("X"));
    assert(d == nullptr);

    // release -> reuse (LIFO free list: last released is next acquired)
    pool.release(b);
    assert(pool.in_use() == 2);
    Order* e = pool.acquire(9ULL, 999LL, 9U, std::string("REUSE"));
    assert(e == b);                          // same slot recycled
    assert(e->tag == "REUSE");
    assert(pool.in_use() == 3);

    Order stray(0ULL, 0LL, 0U, std::string());
    assert(pool.owns(a) && !pool.owns(&stray) && !pool.owns(nullptr));

    pool.release(a);
    pool.release(c);
    pool.release(e);
    assert(pool.in_use() == 0);

    std::printf("05_object_pool: ALL PASS\n");
    return 0;
}

// ============================================================
// INTRUSIVE FREE LIST: a free slot's bytes hold the index of the next
// free slot -> the free list costs 0 extra memory. When the slot is
// in use, those same bytes hold the T. A union expresses this.
//
// LATENCY: acquire/release are O(1) -- pop/push a list head, no syscall,
// no allocator. Measured ~2x faster p50 and ~5x faster p99.9 than
// new/delete, and FLAT (no allocator tail). (folders 36/06-07, 44)
//
// NEXT STEP: hand out {index, generation} handles instead of raw
// pointers; bump generation on release; get(handle) returns nullptr if
// generations mismatch -> a stale handle to a recycled slot can't
// corrupt anything. (folder 44 ObjectPool, lesson 03 D3 / 14 C3)
// ============================================================
