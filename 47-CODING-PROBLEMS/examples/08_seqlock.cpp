// 08_seqlock.cpp
// ============================================================
// Folder 47 file 08 #7: seqlock — one writer, many readers, no reader ever
// blocks the writer. Readers retry on the rare collision. Perfect for a hot
// market-data snapshot ({price, qty}) read by many strategy threads.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -pthread -O2 08_seqlock.cpp -o t && ./t
// ============================================================

#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

struct Quote {
    std::int64_t  price;
    std::int64_t  qty;
    std::uint64_t stamp;      // writer keeps price+qty+stamp mutually consistent
};

template <class T>
class SeqLock {
    std::atomic<std::uint64_t> seq_{0};
    T                          data_{};
public:
    void store(const T& v) {                          // single writer only
        const std::uint64_t s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_relaxed); // odd => write in progress
        std::atomic_thread_fence(std::memory_order_release);
        data_ = v;
        std::atomic_thread_fence(std::memory_order_release);
        seq_.store(s + 2, std::memory_order_release); // even => complete
    }
    T load() const {
        T out;
        std::uint64_t s1, s2;
        do {
            s1 = seq_.load(std::memory_order_acquire);
            if (s1 & 1ULL) { s2 = s1 + 1; continue; } // writer active -> force retry
            out = data_;
            std::atomic_thread_fence(std::memory_order_acquire);
            s2 = seq_.load(std::memory_order_relaxed);
        } while (s1 != s2);
        return out;
    }
};

int main() {
    // ---- single-threaded round trip ----
    {
        SeqLock<Quote> sl;
        sl.store({100, 5, 1});
        Quote q = sl.load();
        assert(q.price == 100 && q.qty == 5 && q.stamp == 1);
    }

    // ---- 1 writer, N readers: every read must be a CONSISTENT snapshot ----
    {
        SeqLock<Quote>   sl;
        std::atomic<bool> run{true};
        sl.store({1000, 10, 0});

        std::thread writer([&] {
            for (std::uint64_t i = 1; i <= 2000000ULL; ++i) {
                // invariant the readers will check: qty == price/100, stamp == i
                std::int64_t px = 1000 + static_cast<std::int64_t>(i % 500);
                sl.store({px, px / 100, i});
            }
            run.store(false, std::memory_order_relaxed);
        });

        std::vector<std::thread> readers;
        std::atomic<std::uint64_t> reads{0};
        std::atomic<std::uint64_t> torn{0};
        for (int r = 0; r < 3; ++r)
            readers.emplace_back([&] {
                while (run.load(std::memory_order_relaxed)) {
                    Quote q = sl.load();
                    // a torn read would mix fields from different store()s
                    if (q.qty != q.price / 100) torn.fetch_add(1, std::memory_order_relaxed);
                    reads.fetch_add(1, std::memory_order_relaxed);
                }
            });

        writer.join();
        for (auto& t : readers) t.join();

        std::printf("  reads=%llu  torn=%llu\n",
                    static_cast<unsigned long long>(reads.load()),
                    static_cast<unsigned long long>(torn.load()));
        assert(torn.load() == 0);                      // seqlock guarantees consistency
        assert(reads.load() > 0);
    }

    std::puts("08_seqlock: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - Writer bumps seq to ODD (write in progress), writes, bumps to EVEN.
//   - Reader samples seq before and after copying the payload; if it changed,
//     or was odd, retry. No lock, no writer blocking.
//   - Best when reads >> writes and the payload is small + trivially copyable.
//   - Strictly there is a data race on `data_` under the C++ memory model;
//     production uses per-field atomics or a careful memcpy + fences. On x86
//     the fences are almost free. (folder 28)
//   - Benchmark at -O2: -O0 hides reordering the fences are there to prevent.
// ============================================================
