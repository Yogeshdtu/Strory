// 04_seqlock_snapshot.cpp
// ============================================================
// Seqlock<T> as a "top-of-book market data snapshot" -- ONE writer
// thread (fed by the pipeline) publishes, N strategy-reader threads
// consult it whenever they want, never blocking the writer and never
// blocking each other. Correctness (torn-read detector) + throughput,
// vs std::shared_mutex as the "obvious" baseline. (06-seqlock-for-
// snapshots.md)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 04_seqlock_snapshot.cpp -o seqsnap && ./seqsnap
// ============================================================

#include "seqlock.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

using Clock = std::chrono::steady_clock;

// A 40-byte top-of-book snapshot -- too big to update with one atomic.
struct Bbo {
    double        bid;
    double        ask;
    std::uint64_t seq_tick;
    std::int64_t  bid_qty;
    std::int64_t  ask_qty;
};

// Writer keeps this invariant for tick k -- a reader that sees anything
// else caught a TORN (half-old-half-new) read.
static bool consistent(const Bbo& q) {
    const auto k = static_cast<std::int64_t>(q.bid);
    return q.ask - q.bid == 100.0
        && q.seq_tick == static_cast<std::uint64_t>(k)
        && q.bid_qty == k
        && q.ask_qty == -k;
}

constexpr int           kReaders  = 4;
// NOTE: shared_mutex ke saath ek UNTHROTTLED writer (max-rate loop) itni
// bhaari contention banata is box pe ki throughput ~tens-of-thousands
// reads/sec tak collapse ho jaata (measured separately: isolated smoke
// test @ 40K reads/sec) -- isliye yeh count seqlock ke liye "chhota" hai
// (woh millions/sec karta), par shared_mutex ko REASONABLE waqt mein
// khatam hone ke liye zaroori hai. Yeh khud ek finding hai (06 mein).
constexpr std::uint64_t kReadsPer = 300'000;

// baseline: the "obvious" way to share a multi-field struct
class SharedMutexBbo {
public:
    void write(const Bbo& q) { std::unique_lock lk(m_); q_ = q; }
    std::uint32_t read(Bbo& out) const { std::shared_lock lk(m_); out = q_; return 0; }
private:
    mutable std::shared_mutex m_;
    Bbo q_{};
};

template <class Store>
static void run(const char* name) {
    static Store store;
    store.write(Bbo{0.0, 100.0, 0, 0, 0});

    std::atomic<bool> go{false}, stop{false};
    std::atomic<std::uint64_t> torn{0}, total_retries{0};

    std::thread writer([&] {
        while (!go.load(std::memory_order_acquire)) {}
        std::uint64_t k = 1;
        while (!stop.load(std::memory_order_relaxed)) {
            const auto dk = static_cast<double>(k);
            store.write(Bbo{dk, dk + 100.0, k, static_cast<std::int64_t>(k), -static_cast<std::int64_t>(k)});
            ++k;
        }
    });

    std::vector<std::thread> readers;
    for (int r = 0; r < kReaders; ++r)
        readers.emplace_back([&] {
            while (!go.load(std::memory_order_acquire)) {}
            Bbo q;
            std::uint64_t bad = 0, rt = 0;
            for (std::uint64_t i = 0; i < kReadsPer; ++i) {
                rt += store.read(q);
                if (!consistent(q)) ++bad;
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
    std::printf("  %-14s : %10.4f M reads/s  (%.3f sec total)   torn=%llu   retry%%=%.3f\n",
                name, reads / sec / 1e6, sec,
                static_cast<unsigned long long>(torn.load()),
                100.0 * static_cast<double>(total_retries.load()) / reads);
}

int main() {
    std::printf("Bbo snapshot (%zu bytes) -- 1 writer (max rate) + %d readers x %llu reads\n\n",
                sizeof(Bbo), kReaders, static_cast<unsigned long long>(kReadsPer));

    run<Seqlock<Bbo>>("seqlock");
    run<SharedMutexBbo>("shared_mutex");

    std::puts("\nKya hua:");
    std::puts(" - torn=0 chahiye dono ke liye -- seqlock ka retry-loop, shared_mutex ka lock,");
    std::puts("   dono readers ko HAMESHA ek consistent Bbo dete, kabhi half-old-half-new nahi.");
    std::puts(" - Seqlock writer KABHI block nahi hota -- koi reader-lock leta hi nahi. Readers");
    std::puts("   ek doosre ko bhi block nahi karte (shared_mutex mein reader-reader theek hai,");
    std::puts("   par writer-vs-readers dono taraf lock hai).");
    std::puts(" - Trade-off dikh raha hai retry%% mein: writer jitna fast likhta, utna zyada");
    std::puts("   chance ki reader beech mein pakda jaaye aur retry kare.");
    return 0;
}
