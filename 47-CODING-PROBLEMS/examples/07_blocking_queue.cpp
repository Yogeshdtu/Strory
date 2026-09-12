// 07_blocking_queue.cpp
// ============================================================
// Folder 47 file 07 B1: bounded blocking queue (mutex + 2 condition vars),
// with close(). Tested with real producer/consumer threads.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -pthread -g -O0 07_blocking_queue.cpp -o t && ./t
// ============================================================

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

template <class T>
class BlockingQueue {
    mutable std::mutex      m_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    std::queue<T>           q_;
    std::size_t             cap_;
    bool                    closed_ = false;
public:
    explicit BlockingQueue(std::size_t cap) : cap_(cap ? cap : 1) {}

    bool push(T v) {
        std::unique_lock<std::mutex> lk(m_);
        not_full_.wait(lk, [&] { return q_.size() < cap_ || closed_; });
        if (closed_) return false;
        q_.push(std::move(v));
        lk.unlock();
        not_empty_.notify_one();
        return true;
    }
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lk(m_);
        not_empty_.wait(lk, [&] { return !q_.empty() || closed_; });
        if (q_.empty()) return std::nullopt;         // closed and drained
        T v = std::move(q_.front());
        q_.pop();
        lk.unlock();
        not_full_.notify_one();
        return v;
    }
    void close() {
        { std::lock_guard<std::mutex> lk(m_); closed_ = true; }
        not_full_.notify_all();
        not_empty_.notify_all();
    }
    std::size_t size() const {
        std::lock_guard<std::mutex> lk(m_);
        return q_.size();
    }
};

int main() {
    // ---- single-threaded basics ----
    {
        BlockingQueue<int> q(2);
        assert(q.push(1) && q.push(2));
        assert(q.pop().value() == 1);
        assert(q.pop().value() == 2);
        q.close();
        assert(!q.push(3));                            // push after close -> false
        assert(q.pop() == std::nullopt);              // empty + closed -> nullopt
    }

    // ---- N producers, M consumers, checksum must balance ----
    {
        constexpr int kProducers = 3;
        constexpr int kConsumers = 3;
        constexpr int kPerProducer = 20000;

        BlockingQueue<std::uint64_t> q(64);
        std::atomic<std::uint64_t> produced_sum{0};
        std::atomic<std::uint64_t> consumed_sum{0};
        std::atomic<int>           consumed_count{0};

        std::vector<std::thread> prod;
        for (int p = 0; p < kProducers; ++p)
            prod.emplace_back([&, p] {
                for (int i = 0; i < kPerProducer; ++i) {
                    std::uint64_t val = static_cast<std::uint64_t>(p) * 1000000ULL
                                      + static_cast<std::uint64_t>(i) + 1ULL;
                    produced_sum.fetch_add(val, std::memory_order_relaxed);
                    q.push(val);
                }
            });

        std::vector<std::thread> cons;
        for (int c = 0; c < kConsumers; ++c)
            cons.emplace_back([&] {
                while (auto v = q.pop()) {
                    consumed_sum.fetch_add(*v, std::memory_order_relaxed);
                    consumed_count.fetch_add(1, std::memory_order_relaxed);
                }
            });

        for (auto& t : prod) t.join();
        q.close();                                     // wakes consumers to drain + exit
        for (auto& t : cons) t.join();

        assert(consumed_count.load() == kProducers * kPerProducer);
        assert(consumed_sum.load() == produced_sum.load());   // nothing lost / duplicated
    }

    std::puts("07_blocking_queue: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - Predicate-form wait(lk, pred) handles BOTH spurious wakeup and lost
//     wakeup — never `if (!ready) cv.wait(lk);`.
//   - Two CVs: producers wait on not_full_, consumers on not_empty_. One CV
//     would work but wakes the wrong side half the time.
//   - close() sets the flag and notify_all()s both — every waiter re-checks
//     its predicate and either proceeds or leaves. pop() keeps draining
//     buffered items after close, THEN returns nullopt.
//   - notify outside the lock: the woken thread can grab the mutex immediately.
// ============================================================
