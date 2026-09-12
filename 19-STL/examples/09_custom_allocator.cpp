// 09_custom_allocator.cpp
// ============================================================
// Ek minimal custom allocator -- Allocator requirements samajhna
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 09_custom_allocator.cpp -o ca && ./ca
// ============================================================
//   Allocator concept (simplified, C++11+):
//     - value_type
//     - T* allocate(size_t n)
//     - void deallocate(T* p, size_t n)
//     - a rebind-able template  (Alloc<U> banane ke liye -- node-based containers use karte)
//     - operator== / operator!=  (do allocators "same" hain? tab memory swap ho sakti)
//   std::allocator_traits baaki (construct/destroy/max_size) fill karta hai.
// ============================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

// A logging allocator: forwards to ::operator new/delete but counts + prints.
template <class T>
struct LoggingAllocator {
    using value_type = T;

    LoggingAllocator() noexcept = default;
    template <class U> LoggingAllocator(const LoggingAllocator<U>&) noexcept {}   // rebind ctor

    T* allocate(std::size_t n) {
        std::size_t bytes = n * sizeof(T);
        void* p = ::operator new(bytes);
        std::printf("    allocate(%zu)  -> %zu bytes @ %p\n", n, bytes, p);
        return static_cast<T*>(p);
    }

    void deallocate(T* p, std::size_t n) noexcept {
        std::printf("    deallocate(%zu) -> %zu bytes @ %p\n", n, n * sizeof(T), static_cast<void*>(p));
        ::operator delete(p);
    }

    // stateless -> all instances are equal (memory allocated by one can be freed by another)
    template <class U> bool operator==(const LoggingAllocator<U>&) const noexcept { return true; }
    template <class U> bool operator!=(const LoggingAllocator<U>&) const noexcept { return false; }
};

// A real-ish one: a fixed arena. Bump-allocate; deallocate is a no-op; reset() to reuse.
template <class T>
class ArenaAllocator {
    // shared arena state (so rebind copies see the same buffer)
    struct Arena {
        std::byte*  base;
        std::size_t cap;
        std::size_t off = 0;
    };
    std::shared_ptr<Arena> arena_;

public:
    using value_type = T;

    explicit ArenaAllocator(std::size_t bytes)
        : arena_(std::make_shared<Arena>(Arena{ static_cast<std::byte*>(::operator new(bytes)), bytes, 0 })) {}
    ~ArenaAllocator() = default;
    template <class U> ArenaAllocator(const ArenaAllocator<U>& o) noexcept : arena_(o.arena()) {}

    std::shared_ptr<Arena> arena() const { return arena_; }

    T* allocate(std::size_t n) {
        std::size_t bytes = n * sizeof(T);
        std::size_t a = alignof(T);
        std::size_t p = (arena_->off + a - 1) & ~(a - 1);
        if (p + bytes > arena_->cap) throw std::bad_alloc{};
        arena_->off = p + bytes;
        return reinterpret_cast<T*>(arena_->base + p);
    }
    void deallocate(T*, std::size_t) noexcept { /* no-op -- freed en masse when arena dies */ }
    void reset() { arena_->off = 0; }

    template <class U> bool operator==(const ArenaAllocator<U>& o) const noexcept { return arena_ == o.arena(); }
    template <class U> bool operator!=(const ArenaAllocator<U>& o) const noexcept { return arena_ != o.arena(); }
};

int main() {
    std::printf("=== 1. LoggingAllocator with std::vector<int> ===\n");
    {
        std::vector<int, LoggingAllocator<int>> v;
        std::printf("  push 1..5 (watch reallocations):\n");
        for (int i = 1; i <= 5; ++i) v.push_back(i);
        std::printf("  reserve(100):\n");
        v.reserve(100);
        std::printf("  size=%zu capacity=%zu\n", v.size(), v.capacity());
        std::printf("  (vector destructs -> one deallocate)\n");
    }

    std::printf("\n=== 2. ArenaAllocator -- bump alloc, deallocate is no-op ===\n");
    {
        ArenaAllocator<int> arena(64 * 1024);                 // 64 KB arena
        std::vector<int, ArenaAllocator<int>> a(arena);
        std::vector<int, ArenaAllocator<int>> b(arena);       // SAME arena
        a.reserve(100);
        b.reserve(200);
        for (int i = 0; i < 100; ++i) a.push_back(i);
        for (int i = 0; i < 200; ++i) b.push_back(i);
        std::printf("  a.size=%zu b.size=%zu  -- both carved from the same 64 KB arena, zero ::operator new\n"
                    "    beyond the arena's initial block\n", a.size(), b.size());
    }   // vectors destruct -> deallocate is no-op; arena's shared_ptr drops -> the 64 KB freed once

    std::printf(
        "\n"
        "  Allocator = value_type + allocate/deallocate + rebind + operator==.\n"
        "  Use cases: instrumentation, arenas/pools (folder 14), NUMA-pinned memory,\n"
        "  shared-memory segments. Modern alternative: std::pmr (file 24 / example 10)\n"
        "  -- ek runtime `memory_resource*` pass karo, container ka TYPE badalta nahi.\n");
    return 0;
}
