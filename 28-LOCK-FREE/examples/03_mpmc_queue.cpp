// 03_mpmc_queue.cpp
// ============================================================
// Bounded MPMC (multi-producer, multi-consumer) queue — Dmitry
// Vyukov's design. Fixed-size ring; each cell carries its own
// atomic sequence number that acts as a tiny per-slot turnstile,
// so producers and consumers never step on each other.
//
//   - enqueue/dequeue advance a shared position with CAS.
//   - the cell's `seq` tells a thread whether the slot is "mine
//     to write", "mine to read", or "not ready" (full / empty).
//   - lock-free: a stalled thread never blocks the others.
//
// Benched against a std::mutex + std::deque queue doing the same.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 03_mpmc_queue.cpp -o mpmc && ./mpmc
// ============================================================

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>
#include <deque>
#include <mutex>
#include <chrono>

using Clock = std::chrono::steady_clock;

// ------------------------------------------------------------
//  Vyukov bounded MPMC queue.
// ------------------------------------------------------------
template <class T, std::size_t N>
class MpmcQueue {
    static_assert((N & (N - 1)) == 0, "N power of two");
    struct Cell {
        std::atomic<std::size_t> seq;
        T data;
    };
public:
    MpmcQueue() {
        for (std::size_t i = 0; i < N; ++i)
            buf_[i].seq.store(i, std::memory_order_relaxed);
    }

    bool enqueue(const T& v) {
        Cell* cell;
        std::size_t pos = enq_.load(std::memory_order_relaxed);
        for (;;) {
            cell = &buf_[pos & kMask];
            const std::size_t seq = cell->seq.load(std::memory_order_acquire);
            const std::ptrdiff_t dif =
                static_cast<std::ptrdiff_t>(seq) - static_cast<std::ptrdiff_t>(pos);
            if (dif == 0) {                                   // slot is free & ours to claim
                if (enq_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            } else if (dif < 0) {
                return false;                                 // full
            } else {
                pos = enq_.load(std::memory_order_relaxed);   // someone beat us; re-read
            }
        }
        cell->data = v;
        cell->seq.store(pos + 1, std::memory_order_release);  // publish -> readable
        return true;
    }

    bool dequeue(T& out) {
        Cell* cell;
        std::size_t pos = deq_.load(std::memory_order_relaxed);
        for (;;) {
            cell = &buf_[pos & kMask];
            const std::size_t seq = cell->seq.load(std::memory_order_acquire);
            const std::ptrdiff_t dif =
                static_cast<std::ptrdiff_t>(seq) - static_cast<std::ptrdiff_t>(pos + 1);
            if (dif == 0) {
                if (deq_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            } else if (dif < 0) {
                return false;                                 // empty
            } else {
                pos = deq_.load(std::memory_order_relaxed);
            }
        }
        out = cell->data;
        cell->seq.store(pos + kMask + 1, std::memory_order_release);  // free for next lap
        return true;
    }

private:
    static constexpr std::size_t kMask = N - 1;
    Cell buf_[N];
    alignas(64) std::atomic<std::size_t> enq_{0};
    alignas(64) std::atomic<std::size_t> deq_{0};
    char pad_[64];
};

// ------------------------------------------------------------
//  Mutex + std::deque, bounded to the same capacity.
// ------------------------------------------------------------
template <class T, std::size_t N>
class MutexQueue {
public:
    bool enqueue(const T& v) {
        std::lock_guard lk(m_);
        if (q_.size() >= N) return false;
        q_.push_back(v);
        return true;
    }
    bool dequeue(T& out) {
        std::lock_guard lk(m_);
        if (q_.empty()) return false;
        out = q_.front();
        q_.pop_front();
        return true;
    }
private:
    std::mutex m_;
    std::deque<T> q_;
};

constexpr std::size_t   kCap      = 1u << 12;   // 4096
constexpr int           kProd     = 3;
constexpr int           kCons     = 3;
constexpr std::uint64_t kPerProd  = 2'000'000;
constexpr std::uint64_t kTotal    = kPerProd * static_cast<std::uint64_t>(kProd);

template <class Q>
static double run(const char* name) {
    static Q q;
    std::atomic<bool> go{false};
    std::atomic<std::uint64_t> consumed{0};
    std::atomic<std::int64_t>  csum{0};

    std::vector<std::thread> threads;
    for (int c = 0; c < kCons; ++c)
        threads.emplace_back([&] {
            while (!go.load(std::memory_order_acquire)) {}
            std::uint64_t v;
            std::int64_t local = 0;
            while (consumed.load(std::memory_order_relaxed) < kTotal) {
                if (q.dequeue(v)) {
                    local += static_cast<std::int64_t>(v & 0xffff);
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            }
            csum.fetch_add(local, std::memory_order_relaxed);
        });

    for (int p = 0; p < kProd; ++p)
        threads.emplace_back([&, p] {
            while (!go.load(std::memory_order_acquire)) {}
            const std::uint64_t base = static_cast<std::uint64_t>(p) * kPerProd;
            for (std::uint64_t i = 0; i < kPerProd; ++i) {
                const std::uint64_t v = base + i;
                while (!q.enqueue(v)) { /* full -> retry */ }
            }
        });

    auto t0 = Clock::now();
    go.store(true, std::memory_order_release);
    for (auto& t : threads) t.join();
    const double sec = std::chrono::duration<double>(Clock::now() - t0).count();

    // expected checksum: sum over p,i of ((p*kPerProd + i) & 0xffff)
    std::int64_t expect = 0;
    for (int p = 0; p < kProd; ++p) {
        const std::uint64_t base = static_cast<std::uint64_t>(p) * kPerProd;
        for (std::uint64_t i = 0; i < kPerProd; ++i)
            expect += static_cast<std::int64_t>((base + i) & 0xffff);
    }
    const bool ok = (consumed.load() == kTotal) && (csum.load() == expect);
    std::printf("  %-14s : %7.1f ms   %6.1f M op/s   %s\n",
                name, sec * 1e3,
                static_cast<double>(kTotal) / sec / 1e6,
                ok ? "OK" : "WRONG");
    return sec;
}

int main() {
    std::printf("MPMC: %d producers x %llu + %d consumers, cap %zu, %llu items total\n\n",
                kProd, (unsigned long long)kPerProd, kCons, kCap,
                (unsigned long long)kTotal);

    const double lf = run<MpmcQueue<std::uint64_t, kCap>>("lock-free");
    const double mx = run<MutexQueue<std::uint64_t, kCap>>("mutex+deque");

    std::printf("\n  lock-free vs mutex: %.2fx\n", mx / lf);

    std::puts("\nKya hua:");
    std::puts(" - Vyukov queue: har cell ka apna atomic `seq`. enqueue tab hi likhta");
    std::puts("   jab seq == pos (slot free). Likhne ke baad seq = pos+1 (release) ->");
    std::puts("   ab woh readable. dequeue seq == pos+1 dekhta, padhta, phir");
    std::puts("   seq = pos + N (agla lap) set karta.");
    std::puts(" - Shared enq_/deq_ position CAS se badhti. CAS fail = koi aur aage");
    std::puts("   badha -> re-read -> retry. Lock-free: koi thread ruk jaaye to baaki");
    std::puts("   chalte rehte.");
    std::puts(" - Fixed capacity -> no allocation, no ABA (positions monotonically");
    std::puts("   badhti hain, wrap 2^64 pe). Yahi HFT ko chahiye.");
    std::puts(" - Under real contention lock-free ka faayda latency ke TAIL mein hai");
    std::puts("   (no priority inversion / convoy), throughput mein hamesha nahi.");
    return 0;
}
