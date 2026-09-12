// 03_arena_and_pool.cpp
// ============================================================
// Folder 47 file 03: bump (arena) allocator + fixed-size object pool.
// Zero malloc after construction. acquire/release O(1). Exhaustion -> nullptr.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g -O0 03_arena_and_pool.cpp -o t && ./t
// ============================================================

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#include <utility>

// ---- B1: bump / arena allocator -----------------------------------
class Arena {
    std::byte* base_;
    std::byte* cur_;
    std::byte* end_;
public:
    Arena(void* buffer, std::size_t size)
        : base_(static_cast<std::byte*>(buffer)), cur_(base_), end_(base_ + size) {}

    void* allocate(std::size_t n, std::size_t align) {
        auto p = reinterpret_cast<std::uintptr_t>(cur_);
        p = (p + align - 1) & ~(align - 1);                 // align up (align = pow2)
        auto* aligned = reinterpret_cast<std::byte*>(p);
        if (aligned + n > end_) return nullptr;             // out of room
        cur_ = aligned + n;
        return aligned;
    }
    void        reset()       { cur_ = base_; }
    std::size_t used()  const { return static_cast<std::size_t>(cur_ - base_); }
};

// ---- B2: fixed-size free-list pool -------------------------------
template <class T, std::size_t N>
class Pool {
    union Slot {
        Slot* next;
        alignas(T) std::byte storage[sizeof(T)];
        Slot() : next(nullptr) {}
        ~Slot() {}
    };
    std::array<Slot, N> slots_{};
    Slot*              free_ = nullptr;
public:
    Pool() {
        for (std::size_t i = 0; i < N; ++i) { slots_[i].next = free_; free_ = &slots_[i]; }
    }
    Pool(const Pool&)            = delete;
    Pool& operator=(const Pool&) = delete;

    template <class... Args>
    T* acquire(Args&&... args) {
        if (free_ == nullptr) return nullptr;               // exhausted
        Slot* s = free_;
        free_ = free_->next;
        return ::new (static_cast<void*>(s->storage)) T(std::forward<Args>(args)...);
    }
    void release(T* p) {
        if (p == nullptr) return;
        p->~T();
        auto* s = reinterpret_cast<Slot*>(p);
        s->next = free_;
        free_ = s;
    }
};

struct Order {
    std::uint64_t id;
    std::int64_t  px;
    std::uint32_t qty;
    Order(std::uint64_t i, std::int64_t p, std::uint32_t q) : id(i), px(p), qty(q) {}
};

int main() {
    // ---- Arena ----
    alignas(std::max_align_t) std::array<std::byte, 256> buf{};
    Arena arena(buf.data(), buf.size());

    auto* a = static_cast<std::uint32_t*>(arena.allocate(sizeof(std::uint32_t), alignof(std::uint32_t)));
    auto* b = static_cast<double*>(arena.allocate(sizeof(double), alignof(double)));
    assert(a != nullptr && b != nullptr);
    *a = 42;
    *b = 3.5;
    assert(reinterpret_cast<std::uintptr_t>(b) % alignof(double) == 0);   // aligned
    assert(arena.used() >= sizeof(std::uint32_t) + sizeof(double));

    assert(arena.allocate(1024, 1) == nullptr);              // doesn't fit -> nullptr
    arena.reset();
    assert(arena.used() == 0);                               // O(1) "free everything"

    // ---- Pool ----
    Pool<Order, 3> pool;
    Order* o1 = pool.acquire(1ULL, 10025LL, 100U);
    Order* o2 = pool.acquire(2ULL, 10050LL, 200U);
    Order* o3 = pool.acquire(3ULL, 10075LL, 300U);
    assert(o1 && o2 && o3);
    assert(o2->id == 2 && o2->px == 10050 && o2->qty == 200);
    assert(pool.acquire(4ULL, 0LL, 0U) == nullptr);          // pool full

    pool.release(o2);
    Order* o4 = pool.acquire(4ULL, 999LL, 9U);
    assert(o4 == o2);                                        // LIFO reuse — same slot
    assert(o4->id == 4);

    pool.release(o1);
    pool.release(o3);
    pool.release(o4);

    std::puts("03_arena_and_pool: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - Arena: `(p + align - 1) & ~(align - 1)` rounds up to a power-of-two
//     alignment. No per-object metadata; reset() is the only "free".
//   - Pool: the free list is threaded THROUGH the unused slots (union) — zero
//     extra memory. acquire = pop + placement-new; release = dtor + push.
//   - Exhaustion returns nullptr; the caller rejects the work. NEVER fall
//     back to malloc on the hot path (that's the tail-latency spike).
//   - LIFO reuse keeps the hottest cache line in play.
// ============================================================
