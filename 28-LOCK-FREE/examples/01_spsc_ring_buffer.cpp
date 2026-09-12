// 01_spsc_ring_buffer.cpp
// ============================================================
// SPSC (single-producer, single-consumer) bounded ring buffer.
// The workhorse lock-free structure. No CAS, no mutex — just a
// release-store / acquire-load hand-off of two indices.
//
//   Producer owns  head_  (writes it, release).
//   Consumer owns  tail_  (writes it, release).
//   Each side acquire-loads the OTHER index to check space / data.
//
// Build simple -> verify -> benchmark. (Optimizations in example 02.)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 01_spsc_ring_buffer.cpp -o spsc && ./spsc
//   (-O2 for the benchmark; folder check runs -O0 and still compiles)
// ============================================================

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>
#include <chrono>

using Clock = std::chrono::steady_clock;

// ------------------------------------------------------------
//  SpscRing<T, N> — N must be a power of two.
//  Capacity is N-1 usable slots (one slot kept empty to tell
//  "full" from "empty" without a separate count).
// ------------------------------------------------------------
template <class T, std::size_t N>
class SpscRing {
    static_assert((N & (N - 1)) == 0, "N must be a power of two");
public:
    // Called ONLY by the producer thread.
    bool try_push(const T& v) {
        const std::size_t h = head_.load(std::memory_order_relaxed);   // we own head_
        const std::size_t next = (h + 1) & (N - 1);
        if (next == tail_.load(std::memory_order_acquire))             // would collide with consumer
            return false;                                             // full
        buf_[h] = v;                                                  // write the slot ...
        head_.store(next, std::memory_order_release);                 // ... then publish it
        return true;
    }

    // Called ONLY by the consumer thread.
    bool try_pop(T& out) {
        const std::size_t t = tail_.load(std::memory_order_relaxed);   // we own tail_
        if (t == head_.load(std::memory_order_acquire))                // nothing published
            return false;                                             // empty
        out = buf_[t];                                                // read the slot ...
        tail_.store((t + 1) & (N - 1), std::memory_order_release);    // ... then free it
        return true;
    }

private:
    T buf_[N];
    std::atomic<std::size_t> head_{0};   // next slot the producer will write
    std::atomic<std::size_t> tail_{0};   // next slot the consumer will read
};

// ------------------------------------------------------------
//  A small trivially-copyable message (32 bytes).
// ------------------------------------------------------------
struct Msg {
    std::uint64_t seq;
    std::uint64_t payload;
    std::uint64_t a;
    std::uint64_t b;
};

int main() {
    constexpr std::size_t kCap   = 1u << 12;   // 4096
    constexpr std::uint64_t kMsgs = 20'000'000;

    static SpscRing<Msg, kCap> ring;   // static: keep it off the stack (big-ish)

    std::atomic<bool> start{false};
    std::int64_t consumed_sum = 0;     // XOR/sum checksum the consumer computes
    std::uint64_t consumed_cnt = 0;

    std::thread consumer([&] {
        while (!start.load(std::memory_order_acquire)) { /* spin */ }
        Msg m;
        std::uint64_t expect = 0;
        while (consumed_cnt < kMsgs) {
            if (ring.try_pop(m)) {
                if (m.seq != expect) {                       // ordering / loss check
                    std::printf("  ORDER BUG: got seq %llu expected %llu\n",
                                (unsigned long long)m.seq, (unsigned long long)expect);
                    return;
                }
                ++expect;
                consumed_sum += static_cast<std::int64_t>(m.payload);
                ++consumed_cnt;
            }
            // else: empty -> busy-poll (SPSC: no blocking)
        }
    });

    auto t0 = Clock::now();
    start.store(true, std::memory_order_release);

    // producer runs on the main thread
    std::int64_t produced_sum = 0;
    for (std::uint64_t i = 0; i < kMsgs; ++i) {
        Msg m{i, i * 2654435761u % 1'000'003u, i, ~i};
        produced_sum += static_cast<std::int64_t>(m.payload);
        while (!ring.try_push(m)) { /* full -> spin (consumer will catch up) */ }
    }
    consumer.join();
    const double sec = std::chrono::duration<double>(Clock::now() - t0).count();

    std::printf("SPSC ring: capacity %zu, %llu messages of %zu bytes\n\n",
                kCap, (unsigned long long)kMsgs, sizeof(Msg));
    std::printf("  consumed count : %llu  %s\n", (unsigned long long)consumed_cnt,
                consumed_cnt == kMsgs ? "OK" : "WRONG");
    std::printf("  checksum       : produced %lld / consumed %lld  %s\n",
                (long long)produced_sum, (long long)consumed_sum,
                produced_sum == consumed_sum ? "OK" : "WRONG");
    const double dmsgs = static_cast<double>(kMsgs);
    std::printf("  time           : %.3f s\n", sec);
    std::printf("  throughput     : %.1f M msg/s\n", dmsgs / sec / 1e6);
    std::printf("  per message    : %.2f ns\n", sec * 1e9 / dmsgs);

    std::puts("\nKya hua:");
    std::puts(" - Producer sirf head_ likhta (release). Consumer sirf tail_ likhta");
    std::puts("   (release). Har side doosre ka index acquire-load karta.");
    std::puts(" - Koi CAS nahi, koi lock nahi. Producer aur consumer ka apna-apna");
    std::puts("   index -> normal case mein cache-line contention bhi nahi (jab tak");
    std::puts("   head_/tail_ alag lines pe hon -> example 07).");
    std::puts(" - release store on head_ ka matlab: slot ki write consumer ke");
    std::puts("   acquire-load ke baad GUARANTEED visible (folder 27 file 08).");
    std::puts(" - N-1 usable slots: ek slot khaali rakhte hain taaki full != empty.");
    return 0;
}
