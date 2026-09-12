// 05_seqlock.cpp
// ============================================================
// Seqlock — a lock-free-for-readers snapshot mechanism. One
// writer publishes a multi-field struct; many readers get a
// consistent copy without ever blocking the writer.
//
//   writer:  seq++ (now ODD = "in progress")
//            write all fields
//            seq++ (now EVEN, release)
//
//   reader:  s1 = seq (acquire); if (s1 odd) retry
//            copy all fields
//            acquire fence
//            s2 = seq (relaxed); if (s1 != s2) retry   // writer touched it -> torn, redo
//
// Readers never take a lock; the writer is never blocked by a
// reader. THE pattern for HFT market-data snapshots (bbo, greeks,
// risk limits) that are too big for one atomic.
//
// Benched (reader throughput) against std::shared_mutex.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 05_seqlock.cpp -o seqlock && ./seqlock
// ============================================================

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <chrono>

using Clock = std::chrono::steady_clock;

// 40-byte quote — deliberately bigger than any lock-free atomic.
struct Quote {
    double        bid;
    double        ask;
    std::uint64_t ts;
    std::int64_t  bid_sz;
    std::int64_t  ask_sz;
};

// Writer keeps this invariant for tick k:
//   bid = k, ask = k + 100, ts = k, bid_sz = k, ask_sz = -k
static bool consistent(const Quote& q) {
    const std::int64_t k = static_cast<std::int64_t>(q.bid);
    return q.ask - q.bid == 100.0
        && q.ts == static_cast<std::uint64_t>(k)
        && q.bid_sz == k
        && q.ask_sz == -k;
}

constexpr int           kReaders  = 4;
constexpr std::uint64_t kReadsPer = 5'000'000;

// ------------------------------------------------------------
//  Seqlock
// ------------------------------------------------------------
class SeqlockQuote {
public:
    void write(const Quote& q) {
        const std::uint32_t s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_relaxed);       // -> odd
        std::atomic_thread_fence(std::memory_order_release);
        q_ = q;                                             // plain writes
        std::atomic_thread_fence(std::memory_order_release);
        seq_.store(s + 2, std::memory_order_release);       // -> even, publish
    }

    // returns number of retries taken
    std::uint32_t read(Quote& out) const {
        std::uint32_t retries = 0;
        for (;;) {
            const std::uint32_t s1 = seq_.load(std::memory_order_acquire);
            if (s1 & 1u) { ++retries; continue; }           // writer mid-update
            out = q_;                                       // plain reads
            std::atomic_thread_fence(std::memory_order_acquire);
            const std::uint32_t s2 = seq_.load(std::memory_order_relaxed);
            if (s1 == s2) return retries;                   // clean snapshot
            ++retries;                                      // writer moved -> redo
        }
    }

private:
    alignas(64) std::atomic<std::uint32_t> seq_{0};
    Quote q_{};
    char pad_[64];
};

// ------------------------------------------------------------
//  shared_mutex baseline
// ------------------------------------------------------------
class SharedMutexQuote {
public:
    void write(const Quote& q) {
        std::unique_lock lk(m_);
        q_ = q;
    }
    std::uint32_t read(Quote& out) const {
        std::shared_lock lk(m_);
        out = q_;
        return 0;
    }
private:
    mutable std::shared_mutex m_;
    Quote q_{};
};

template <class Store>
static void run(const char* name) {
    static Store store;
    store.write(Quote{0.0, 100.0, 0, 0, 0});

    std::atomic<bool> go{false};
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> torn{0};
    std::atomic<std::uint64_t> total_retries{0};

    // writer: bump the tick as fast as it can until readers are done
    std::thread writer([&] {
        while (!go.load(std::memory_order_acquire)) {}
        std::uint64_t k = 1;
        while (!stop.load(std::memory_order_relaxed)) {
            const double dk = static_cast<double>(k);
            store.write(Quote{dk, dk + 100.0, k,
                              static_cast<std::int64_t>(k),
                              -static_cast<std::int64_t>(k)});
            ++k;
        }
    });

    std::vector<std::thread> readers;
    for (int r = 0; r < kReaders; ++r)
        readers.emplace_back([&] {
            while (!go.load(std::memory_order_acquire)) {}
            Quote q;
            std::uint64_t bad = 0, rt = 0;
            for (std::uint64_t i = 0; i < kReadsPer; ++i) {
                rt += store.read(q);
                if (!consistent(q)) ++bad;                  // torn read detector
            }
            torn.fetch_add(bad, std::memory_order_relaxed);
            total_retries.fetch_add(rt, std::memory_order_relaxed);
        });

    auto t0 = Clock::now();
    go.store(true, std::memory_order_release);
    for (auto& t : readers) t.join();
    const double sec = std::chrono::duration<double>(Clock::now() - t0).count();
    stop.store(true, std::memory_order_relaxed);
    writer.join();

    const double reads = static_cast<double>(kReadsPer) * kReaders;
    std::printf("  %-14s : %6.1f M reads/s   torn=%llu   retries=%.2f%%\n",
                name, reads / sec / 1e6,
                (unsigned long long)torn.load(),
                100.0 * static_cast<double>(total_retries.load()) / reads);
}

int main() {
    std::printf("Seqlock vs shared_mutex — 1 writer (max rate) + %d readers x %llu reads\n",
                kReaders, (unsigned long long)kReadsPer);
    std::printf("Quote is %zu bytes (too big for a lock-free atomic)\n\n", sizeof(Quote));

    run<SeqlockQuote>("seqlock");
    run<SharedMutexQuote>("shared_mutex");

    std::puts("\nKya hua:");
    std::puts(" - Seqlock reader: seq padho -> agar odd, writer likh raha hai, retry.");
    std::puts("   Fields copy karo -> acquire fence -> seq dobara padho. Same & even");
    std::puts("   -> snapshot clean. Warna writer beech mein aaya -> dobara.");
    std::puts(" - torn=0 hona chahiye: reader kabhi half-old-half-new Quote accept");
    std::puts("   nahi karta. (Reads khud data race hain formally — isliye asli code");
    std::puts("   mein fields std::atomic_ref ya relaxed atomics se padho; yahan");
    std::puts("   x86 pe demo ke liye plain.)");
    std::puts(" - Writer kabhi block nahi hota — koi reader-lock nahi. Readers ek");
    std::puts("   doosre ko bhi block nahi karte. Isliye 1-writer-N-reader hot");
    std::puts("   market-data snapshots ke liye ideal (bbo, greeks, risk limits).");
    std::puts(" - Trade-off: high writer rate pe reader retry% badhta hai (writer ne");
    std::puts("   seq bump kiya jab reader beech mein tha). shared_mutex retry-free");
    std::puts("   hai par har read pe ek atomic RMW (lock) leta.");
    return 0;
}
