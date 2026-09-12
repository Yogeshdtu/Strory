// 03_disruptor.cpp
// ============================================================
// A simplified LMAX-Disruptor-style ring: ONE producer, MULTIPLE
// INDEPENDENT consumers -- every consumer sees EVERY event (fan-out),
// unlike an SPSC/MPMC queue where each item goes to exactly ONE
// consumer (work-distribution). The producer only overwrites a slot
// once the SLOWEST consumer has confirmed it's done with it (gating).
// Consumers naturally BATCH: wait_for() returns however much has been
// published since they last looked, so a consumer that's momentarily
// behind processes a whole run of events in one pass, not one at a time.
//
// Simplifications vs the real LMAX Disruptor (documented, not hidden):
// no pluggable WaitStrategy (always busy-spin here), no consumer
// dependency graphs (SequenceBarrier chains -- all consumers here are
// independent, gated only against the producer's own gating check), no
// multi-producer support. (05-lmax-disruptor.md)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 03_disruptor.cpp -o disruptor && ./disruptor
// ============================================================

#include <atomic>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <thread>
#include <vector>

// ============================================================
//  DisruptorRing<T, Capacity> -- single producer, N independent
//  consumers, gated ring buffer.
// ============================================================
template <class T, std::size_t Capacity>
class DisruptorRing {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two, >= 2");
    static constexpr std::size_t kMask = Capacity - 1;

public:
    explicit DisruptorRing(std::size_t num_consumers) : consumer_seqs_(num_consumers) {
        for (auto& c : consumer_seqs_) c.store(-1, std::memory_order_relaxed);
    }

    // ---- Producer (single thread only, 02's single-writer principle) ----
    std::int64_t claim() {
        const std::int64_t s = next_to_claim_++;
        if (s >= static_cast<std::int64_t>(Capacity)) {
            const std::int64_t floor = s - static_cast<std::int64_t>(Capacity);
            // Only re-scan the (potentially contended) consumer sequences
            // when our CACHED view says we might be about to lap someone --
            // same cached-index trick as spsc_queue.hpp, applied to N gates
            // instead of 1.
            while (cached_min_consumed_ < floor) {
                cached_min_consumed_ = min_consumer_seq();
            }
        }
        return s;
    }
    T& slot(std::int64_t seq) { return buf_[static_cast<std::size_t>(seq) & kMask]; }
    void publish(std::int64_t seq) { cursor_.store(seq, std::memory_order_release); }

    // ---- Consumers (one thread per index; each thread owns its index) ----
    std::int64_t wait_for(std::int64_t want_seq) const {
        std::int64_t c;
        while ((c = cursor_.load(std::memory_order_acquire)) < want_seq) { /* spin */ }
        return c;   // caller may batch-process [want_seq .. c] in one pass
    }
    const T& slot(std::int64_t seq) const { return buf_[static_cast<std::size_t>(seq) & kMask]; }
    void consumed_up_to(std::size_t consumer_idx, std::int64_t seq) {
        consumer_seqs_[consumer_idx].store(seq, std::memory_order_release);
    }

private:
    std::int64_t min_consumer_seq() const {
        std::int64_t m = std::numeric_limits<std::int64_t>::max();
        for (const auto& c : consumer_seqs_) {
            const std::int64_t v = c.load(std::memory_order_acquire);
            if (v < m) m = v;
        }
        return m;
    }

    T buf_[Capacity]{};
    alignas(64) std::atomic<std::int64_t> cursor_{-1};
    std::int64_t next_to_claim_ = 0;          // producer-private
    std::int64_t cached_min_consumed_ = -1;   // producer-private cache of the gate
    std::vector<std::atomic<std::int64_t>> consumer_seqs_;
};

struct MarketEvent { std::int64_t seq; std::uint64_t payload; };

constexpr std::size_t   kCap = 1u << 14;   // 16384
constexpr std::int64_t  kN   = 4'000'000;

int main() {
    DisruptorRing<MarketEvent, kCap> ring(/*num_consumers=*/2);
    std::atomic<bool> go{false};
    std::atomic<std::uint64_t> checksum_a{0}, checksum_b{0};
    std::atomic<std::int64_t>  count_a{0}, count_b{0};
    std::atomic<std::uint64_t> batches_a{0}, batches_b{0};   // how many wait_for calls
                                                               // returned >1 new event

    auto make_consumer = [&](std::size_t idx, std::atomic<std::uint64_t>& sum,
                              std::atomic<std::int64_t>& cnt, std::atomic<std::uint64_t>& batches) {
        return [&, idx] {
            while (!go.load(std::memory_order_acquire)) {}
            std::int64_t next = 0;
            std::uint64_t local_sum = 0;
            std::int64_t local_cnt = 0;
            std::uint64_t local_batches = 0;
            while (next <= kN - 1) {
                const std::int64_t avail = ring.wait_for(next);
                if (avail > next) ++local_batches;   // more than 1 new event since last look
                for (std::int64_t s = next; s <= avail; ++s) {
                    local_sum += ring.slot(s).payload;
                    ++local_cnt;
                }
                next = avail + 1;
                ring.consumed_up_to(idx, avail);
            }
            sum.store(local_sum, std::memory_order_relaxed);
            cnt.store(local_cnt, std::memory_order_relaxed);
            batches.store(local_batches, std::memory_order_relaxed);
        };
    };

    std::thread consumer_a(make_consumer(0, checksum_a, count_a, batches_a));
    std::thread consumer_b(make_consumer(1, checksum_b, count_b, batches_b));

    // ---- Producer (this thread) ----
    std::uint64_t expected_sum = 0;
    go.store(true, std::memory_order_release);
    for (std::int64_t i = 0; i < kN; ++i) {
        const std::int64_t s = ring.claim();
        const std::uint64_t payload = static_cast<std::uint64_t>(i) * 3u + 7u;
        ring.slot(s) = MarketEvent{s, payload};
        ring.publish(s);
        expected_sum += payload;
    }

    consumer_a.join();
    consumer_b.join();

    std::printf("Published %lld events. Each consumer independently sees ALL of them\n"
                "(fan-out -- NOT split like a work queue):\n\n", static_cast<long long>(kN));
    std::printf("  consumer A: count=%lld  checksum %s  batched-reads=%llu\n",
                static_cast<long long>(count_a.load()),
                checksum_a.load() == expected_sum ? "OK" : "MISMATCH",
                static_cast<unsigned long long>(batches_a.load()));
    std::printf("  consumer B: count=%lld  checksum %s  batched-reads=%llu\n",
                static_cast<long long>(count_b.load()),
                checksum_b.load() == expected_sum ? "OK" : "MISMATCH",
                static_cast<unsigned long long>(batches_b.load()));

    const bool ok = count_a.load() == kN && count_b.load() == kN &&
                     checksum_a.load() == expected_sum && checksum_b.load() == expected_sum;
    std::printf("\n%s\n", ok ? "OK -- both consumers processed every event exactly once, in order"
                              : "FAIL");
    return ok ? 0 : 1;
}
