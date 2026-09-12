// 01_spsc_hft.cpp
// ============================================================
// SpscQueue<T,N> -- API + correctness demo. One producer thread, one
// consumer thread, a fixed-size POD message, order + content verified.
// (04-spsc-queue-for-pipeline.md)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 01_spsc_hft.cpp -o spschft && ./spschft
// ============================================================

#include "spsc_queue.hpp"

#include <atomic>
#include <cstdio>
#include <thread>

struct Tick {
    std::uint64_t seq;
    double        price;
};

constexpr std::size_t   kCap = 1u << 10;   // 1024, power of two
constexpr std::uint64_t kN   = 2'000'000;

int main() {
    static SpscQueue<Tick, kCap> q;
    std::atomic<bool> go{false};

    std::uint64_t received = 0, mismatches = 0, full_retries = 0;

    // ============================================================
    //  Consumer thread -- ONLY thread that calls try_pop() (single-
    //  writer/single-reader principle, 02)
    // ============================================================
    std::thread consumer([&] {
        while (!go.load(std::memory_order_acquire)) {}
        Tick t;
        std::uint64_t expect = 0;
        while (received < kN) {
            if (q.try_pop(t)) {
                if (t.seq != expect) ++mismatches;   // order MUST be preserved
                if (t.price != static_cast<double>(t.seq) * 0.5) ++mismatches;
                ++expect;
                ++received;
            }
        }
    });

    // ============================================================
    //  Producer thread (this one) -- ONLY thread that calls try_push()
    // ============================================================
    go.store(true, std::memory_order_release);
    for (std::uint64_t i = 0; i < kN; ++i) {
        const Tick t{i, static_cast<double>(i) * 0.5};
        while (!q.try_push(t)) ++full_retries;   // backpressure: spin until room
    }
    consumer.join();

    std::printf("Sent %llu ticks, received %llu, mismatches=%llu\n",
                static_cast<unsigned long long>(kN),
                static_cast<unsigned long long>(received),
                static_cast<unsigned long long>(mismatches));
    std::printf("Producer saw a full queue (backpressure) %llu times "
                "(capacity=%zu)\n",
                static_cast<unsigned long long>(full_retries), q.capacity());
    std::printf("%s\n", mismatches == 0 ? "OK -- order + content preserved exactly"
                                          : "FAIL -- see mismatches above");

    return mismatches == 0 ? 0 : 1;
}
