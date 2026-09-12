// 04_acquire_release.cpp
// ============================================================
// The PUBLISH / SUBSCRIBE pattern, done right.
//   Producer: fill data ; store(flag, RELEASE)
//   Consumer: load(flag, ACQUIRE) ; read data
// release store "synchronizes-with" the acquire load that reads its
// value -> everything the producer did BEFORE the release is visible
// to the consumer AFTER the acquire. Guaranteed on every architecture.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 04_acquire_release.cpp -o ar -pthread && ./ar
// ============================================================

#include <cstdio>
#include <atomic>
#include <thread>
#include <cstdint>

struct Message {
    std::int64_t seq;
    std::int64_t px;
    std::int64_t qty;
    char         tag[8];
};

int main() {
    // --------------------------------------------------------
    //  Correct publish/subscribe — release/acquire on the flag
    // --------------------------------------------------------
    {
        Message           msg{};
        std::atomic<int>  ready{0};

        long mismatches = 0;
        constexpr int kRounds = 5000;          // thread create/join per round -> keep modest

        for (int r = 0; r < kRounds; ++r) {
            msg = Message{};
            ready.store(0, std::memory_order_relaxed);

            std::thread producer([&] {
                // fill the message (plain, non-atomic writes)
                msg.seq = r;
                msg.px  = r * 100 + 1;
                msg.qty = r * 10 + 2;
                msg.tag[0] = 'A'; msg.tag[1] = char('0' + (r % 10)); msg.tag[2] = '\0';
                // PUBLISH: release store. All the writes above are ordered
                // before this store and become visible to any acquire-load
                // that observes this value.
                ready.store(1, std::memory_order_release);
            });

            std::thread consumer([&] {
                // SUBSCRIBE: acquire load. Once we see 1, everything the
                // producer wrote before its release store is visible.
                while (ready.load(std::memory_order_acquire) == 0) { /* spin */ }

                bool ok = msg.seq == r
                       && msg.px  == r * 100 + 1
                       && msg.qty == r * 10 + 2
                       && msg.tag[0] == 'A'
                       && msg.tag[1] == char('0' + (r % 10));
                if (!ok) ++mismatches;
            });

            producer.join();
            consumer.join();
        }

        std::printf("release/acquire publish: %ld mismatches in %d rounds  %s\n",
                    mismatches, kRounds, mismatches == 0 ? "OK (as guaranteed)" : "BUG");
    }

    // --------------------------------------------------------
    //  The one-writer / one-reader hand-off, N messages
    //  (a mini SPSC: producer bumps a "write index" with release,
    //   consumer reads it with acquire — folder 28 builds this out)
    // --------------------------------------------------------
    {
        constexpr int N = 1 << 12;                 // ring capacity (power of two)
        static Message buf[N];                     // static: keep it off the stack
        std::atomic<std::uint64_t> widx{0};        // producer writes (release)
        std::atomic<std::uint64_t> ridx{0};        // consumer writes (release), for backpressure

        constexpr std::uint64_t kMsgs = 2'000'000;
        std::int64_t checksum = 0;

        std::thread producer([&] {
            for (std::uint64_t i = 0; i < kMsgs; ++i) {
                while (i - ridx.load(std::memory_order_acquire) >= N) { /* wait for space */ }
                Message& slot = buf[i & (N - 1)];
                slot.seq = static_cast<std::int64_t>(i);        // plain writes into the slot
                slot.px  = static_cast<std::int64_t>(i) * 2 + 1;
                slot.qty = 1;
                widx.store(i + 1, std::memory_order_release);   // PUBLISH this slot
            }
        });

        std::thread consumer([&] {
            std::uint64_t got = 0;
            while (got < kMsgs) {
                std::uint64_t avail = widx.load(std::memory_order_acquire);   // SUBSCRIBE
                while (got < avail) {
                    checksum += buf[got & (N - 1)].px;          // read published data
                    ++got;
                }
                ridx.store(got, std::memory_order_release);     // publish consumed count
            }
        });

        producer.join();
        consumer.join();

        // expected checksum = sum of (2*i + 1) for i in [0, kMsgs)
        std::int64_t exp = static_cast<std::int64_t>(kMsgs) * (kMsgs - 1)
                         + static_cast<std::int64_t>(kMsgs);
        std::printf("SPSC hand-off (%llu msgs): checksum=%lld  %s\n",
                    static_cast<unsigned long long>(kMsgs), static_cast<long long>(checksum),
                    checksum == exp ? "OK" : "MISMATCH");
    }

    std::puts("\nSaar:");
    std::puts(" - release store + acquire load on the SAME atomic = 'synchronizes-with'.");
    std::puts(" - Producer's writes BEFORE the release -> visible to consumer AFTER the");
    std::puts("   acquire that reads the released value. Guaranteed on x86, ARM, everywhere.");
    std::puts(" - Cheaper than seq_cst: release = no full fence on x86 (plain store);");
    std::puts("   acquire = plain load. seq_cst store needs an `mfence` / `xchg` (file 09).");
    std::puts(" - This is THE pattern for SPSC queues, publishing a snapshot, a 'done' flag");
    std::puts("   that gates data (folder 28).");
    return 0;
}
