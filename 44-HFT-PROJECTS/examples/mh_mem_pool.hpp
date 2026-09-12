// mh_mem_pool.hpp
// ============================================================
// PROJECT 5 -- Fixed-size memory pool (typed, capstone-integrated).
//
// Folder 14/36 ne concept + latency distribution measure ki; yeh polished
// reusable version hai jise mini-engine actually use karti.
//
//   - ek preallocated slab of N slots (sizeof(Slot), alignof(T)-aligned)
//   - intrusive free list: free slot ke andar next free slot ka index
//   - alloc()/free() = O(1), no syscall, no lock (per-thread), no branch churn
//   - full -> nullptr (caller handle kare). freed memory OS ko wapas nahi.
//   - thread-safe NAHI -> per-thread pool (36 SPSC discipline)
// ============================================================
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

namespace mhft {

template <class T, std::size_t N>
class FixedPool {
    static_assert(N >= 1, "pool needs at least one slot");
public:
    FixedPool() {
        // free list: 0 -> 1 -> 2 -> ... -> N-1 -> kNil
        for (std::size_t i = 0; i + 1 < N; ++i) slot(i).next = static_cast<std::uint32_t>(i + 1);
        slot(N - 1).next = kNil;
        head_ = 0;
        in_use_ = 0;
    }
    FixedPool(const FixedPool&) = delete;
    FixedPool& operator=(const FixedPool&) = delete;

    // raw storage -- caller placement-new / destroys (see ObjectPool for typed)
    T* alloc() {
        if (head_ == kNil) return nullptr;                 // pool exhausted
        const std::uint32_t i = head_;
        head_ = slot(i).next;
        ++in_use_;
        return reinterpret_cast<T*>(&slot(i).storage);
    }
    void free(T* p) {
        if (!p) return;
        const std::size_t i = index_of(p);
        slot(i).next = head_;
        head_ = static_cast<std::uint32_t>(i);
        --in_use_;
    }

    std::size_t in_use()   const { return in_use_; }
    std::size_t capacity() const { return N; }
    bool        owns(const T* p) const {
        const auto* b = reinterpret_cast<const std::byte*>(&buf_[0]);
        const auto* e = reinterpret_cast<const std::byte*>(&buf_[N]);
        const auto* q = reinterpret_cast<const std::byte*>(p);
        return q >= b && q < e;
    }

private:
    static constexpr std::uint32_t kNil = 0xFFFFFFFFu;

    struct Slot {
        alignas(T) std::byte storage[sizeof(T)];
        std::uint32_t next;                    // valid only while slot is free
    };

    Slot&       slot(std::size_t i)       { return buf_[i]; }
    const Slot& slot(std::size_t i) const { return buf_[i]; }

    std::size_t index_of(const T* p) const {
        const auto* q = reinterpret_cast<const std::byte*>(p);
        const auto* b = reinterpret_cast<const std::byte*>(&buf_[0]);
        return static_cast<std::size_t>((q - b) / static_cast<std::ptrdiff_t>(sizeof(Slot)));
    }

    std::array<Slot, N> buf_{};
    std::uint32_t head_   = kNil;
    std::size_t   in_use_ = 0;
};

}  // namespace mhft
