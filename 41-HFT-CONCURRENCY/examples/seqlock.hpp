// seqlock.hpp
// ============================================================
// Seqlock -- 1-writer/N-reader consistent snapshot, packaged as a
// reusable template. SAME mechanism 28-LOCK-FREE built + measured
// (example 05); here it's the "market data snapshot" building block
// this folder's pipeline (08) and lesson 06 share.
//
//   writer: seq++ (now ODD = "in progress")
//           write all fields (plain stores)
//           seq++ (now EVEN, release = "published")
//
//   reader: s1 = seq (acquire); if (s1 odd) retry (writer mid-update)
//           copy all fields (plain reads)
//           acquire fence
//           s2 = seq (relaxed); if (s1 != s2) retry (writer moved -> torn)
//
// Readers NEVER block the writer; the writer NEVER blocks on readers.
// Payload reads/writes are formally a data race (relaxed by the fences,
// not sequenced) -- fine on x86 for POD payloads in a teaching context;
// production code would use std::atomic_ref per field or relaxed atomics
// (05_seqlock.cpp's original comment, carried forward here).
// ============================================================
#pragma once

#include <atomic>
#include <cstdint>

template <class T>
class Seqlock {
public:
    // Writer-thread-only (single writer -- 02's principle again).
    void write(const T& v) {
        const std::uint32_t s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_relaxed);           // -> odd
        std::atomic_thread_fence(std::memory_order_release);
        value_ = v;                                             // plain write
        std::atomic_thread_fence(std::memory_order_release);
        seq_.store(s + 2, std::memory_order_release);           // -> even, publish
    }

    // Any number of reader threads may call this concurrently.
    // Returns the number of retries taken (0 = clean first try).
    std::uint32_t read(T& out) const {
        std::uint32_t retries = 0;
        for (;;) {
            const std::uint32_t s1 = seq_.load(std::memory_order_acquire);
            if (s1 & 1u) { ++retries; continue; }                // writer mid-update
            out = value_;                                        // plain read
            std::atomic_thread_fence(std::memory_order_acquire);
            const std::uint32_t s2 = seq_.load(std::memory_order_relaxed);
            if (s1 == s2) return retries;                        // clean snapshot
            ++retries;                                           // writer moved -> redo
        }
    }

private:
    alignas(64) std::atomic<std::uint32_t> seq_{0};
    T value_{};
    char pad_[64];
};
