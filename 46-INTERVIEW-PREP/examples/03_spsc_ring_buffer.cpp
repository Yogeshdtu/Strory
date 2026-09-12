// 03_spsc_ring_buffer.cpp
// ============================================================
// THE HFT FAVOURITE: single-producer single-consumer lock-free ring.
// No CAS -- each side owns one index; acquire/release on the two
// indices; power-of-two capacity with & mask; alignas(64) to avoid
// false sharing.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 03_spsc_ring_buffer.cpp -o t && ./t
// ============================================================
// INTERVIEWER KYA DEKH RAHA:
//   - why NO compare-exchange is needed (single writer per index)
//   - acquire on the load you read from the other thread, release on
//     the store the other thread reads -> the happens-before edge
//   - power-of-two + mask (no % / no divider), monotonic counters
//     (mask only on index -> no ABA, no wasted slot)
//   - alignas(64) on head_/tail_ (false sharing) + cached opposite index
//   - producer-on-full policy: drop + count, never block
// ============================================================

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#include <thread>
#include <vector>

template <class T, std::size_t Capacity>
class SpscRing {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
    static constexpr std::size_t kMask = Capacity - 1;

public:
    // Producer thread only.
    bool try_push(const T& v) {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        // read the consumer's progress with acquire so we see its pops
        if (head - tail_cached_ >= Capacity) {
            tail_cached_ = tail_.load(std::memory_order_acquire);
            if (head - tail_cached_ >= Capacity) return false;   // full -> caller drops
        }
        buf_[head & kMask] = v;
        head_.store(head + 1, std::memory_order_release);        // publish
        return true;
    }

    // Consumer thread only.
    bool try_pop(T& out) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_cached_) {
            head_cached_ = head_.load(std::memory_order_acquire);
            if (tail == head_cached_) return false;              // empty
        }
        out = buf_[tail & kMask];
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    static constexpr std::size_t capacity() { return Capacity; }

private:
    alignas(64) std::atomic<std::size_t> head_{0};   // written by producer
    std::size_t head_cached_{0};                     // consumer's cached view of head_
    alignas(64) std::atomic<std::size_t> tail_{0};   // written by consumer
    std::size_t tail_cached_{0};                     // producer's cached view of tail_
    alignas(64) T buf_[Capacity]{};
};

int main() {
    // ---- single-threaded correctness ----
    // This design uses monotonic counters; a capacity-N ring holds N items
    // (head - tail ranges 0..N).
    {
        SpscRing<int, 4> q;
        int x = -1;
        assert(!q.try_pop(x));                 // empty
        assert(q.try_push(1));
        assert(q.try_push(2));
        assert(q.try_push(3));
        assert(q.try_push(4));                 // 4 items in a capacity-4 ring
        assert(!q.try_push(5));                // full
        assert(q.try_pop(x) && x == 1);
        assert(q.try_pop(x) && x == 2);
        assert(q.try_push(5));                 // room again (FIFO wrap via & mask)
        assert(q.try_pop(x) && x == 3);
        assert(q.try_pop(x) && x == 4);
        assert(q.try_pop(x) && x == 5);
        assert(!q.try_pop(x));                 // empty again
    }

    // ---- two-thread blast: consumer must see every value, in order ----
    {
        constexpr std::size_t kCap = 1024;
        constexpr std::uint64_t kN = 2'000'000;
        SpscRing<std::uint64_t, kCap> q;

        std::uint64_t dropped = 0;
        std::thread producer([&] {
            for (std::uint64_t i = 0; i < kN; ++i) {
                while (!q.try_push(i)) { /* spin; a real one would count+drop */ ++dropped; }
            }
        });

        std::uint64_t expected = 0;
        std::uint64_t got = 0;
        std::thread consumer([&] {
            std::uint64_t v = 0;
            while (got < kN) {
                if (q.try_pop(v)) {
                    assert(v == expected);   // strict in-order, no gaps, no dups
                    ++expected;
                    ++got;
                }
            }
        });

        producer.join();
        consumer.join();
        assert(got == kN);
        std::printf("  blast: %llu msgs in order, producer spun %llu times\n",
                    static_cast<unsigned long long>(kN),
                    static_cast<unsigned long long>(dropped));
    }

    std::printf("03_spsc_ring_buffer: ALL PASS\n");
    return 0;
}

// ============================================================
// WHY NO CAS: the producer is the ONLY writer of head_; the consumer is
// the ONLY writer of tail_. Each side only READS the other's index. Two
// threads never write the same location -> plain atomic load/store with
// acquire/release is sufficient; compare-exchange is only needed when
// multiple threads write the same index (MPSC / MPMC).
//
// CACHED OPPOSITE INDEX: without it, every push does an acquire-load of
// tail_ (a cache line the consumer keeps dirtying) -> a coherence miss
// per op. Caching it and only refreshing when the cached value says
// "full/empty" removes almost all of those misses -- the single biggest
// win in the naive->tuned ladder. (folders 28, 36/15, 41)
// ============================================================
