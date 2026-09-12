// spsc_queue.hpp
// ============================================================
// "Production-grade" single-producer/single-consumer ring buffer --
// the SAME padding + cached-opposite-index design 28-LOCK-FREE built
// and measured (examples 02, 06), packaged as a reusable template so
// this folder's pipeline (04, 07, 08) can share ONE implementation
// instead of copy-pasting a queue into every example.
//
// SINGLE-WRITER PRINCIPLE (02-single-writer-principle.md): exactly ONE
// thread may ever call try_push(), exactly ONE (different) thread may
// ever call try_pop(). Two threads calling try_push() concurrently is a
// DATA RACE (UB) -- this class does NOT defend against that (no CAS, no
// lock) because defending against it is precisely the cost a true SPSC
// queue exists to avoid. If you need multiple producers or consumers,
// you need a different structure (MPMC queue, 28/03; or the Disruptor's
// gated-fan-out shape, 05).
// ============================================================
#pragma once

#include <atomic>
#include <cstddef>

template <class T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two, >= 2 (mask trick needs it)");
    static constexpr std::size_t kMask = Capacity - 1;

public:
    // Producer-thread-only. false = queue full (backpressure -- caller
    // decides: spin, drop, or apply some other policy).
    bool try_push(const T& v) {
        const std::size_t h = head_.load(std::memory_order_relaxed);
        const std::size_t n = (h + 1) & kMask;
        if (n == cached_tail_) {                                  // maybe full?
            cached_tail_ = tail_.load(std::memory_order_acquire);  // refresh once
            if (n == cached_tail_) return false;                  // really full
        }
        buf_[h] = v;
        head_.store(n, std::memory_order_release);
        return true;
    }

    // Consumer-thread-only. false = queue empty.
    bool try_pop(T& out) {
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        if (t == cached_head_) {                                  // maybe empty?
            cached_head_ = head_.load(std::memory_order_acquire);  // refresh once
            if (t == cached_head_) return false;                  // really empty
        }
        out = buf_[t];
        tail_.store((t + 1) & kMask, std::memory_order_release);
        return true;
    }

    static constexpr std::size_t capacity() { return Capacity; }

private:
    T buf_[Capacity]{};
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    alignas(64) std::size_t cached_tail_{0};   // producer-private copy of tail_
    std::size_t              cached_head_{0};  // consumer-private copy of head_
    char pad_[64];
};
