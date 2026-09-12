// 05_condition_variable.cpp
// ============================================================
// Producer-consumer: ek bounded queue, std::condition_variable se
// "data hai" / "jagah hai" signal. Predicate form of wait() (spurious
// wakeup safe), graceful shutdown.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -pthread 05_condition_variable.cpp -o cv && ./cv
// ============================================================

#include <cstdio>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <optional>
#include <atomic>
#include <utility>

template <class T>
class BoundedQueue {
    std::mutex              m_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::queue<T>           q_;
    std::size_t             cap_;
    bool                    closed_ = false;

public:
    explicit BoundedQueue(std::size_t cap) : cap_(cap) {}

    // producer side. false = queue closed.
    bool push(T v) {
        std::unique_lock lk(m_);
        not_full_.wait(lk, [&] { return q_.size() < cap_ || closed_; });   // predicate wait
        if (closed_) return false;
        q_.push(std::move(v));
        lk.unlock();
        not_empty_.notify_one();
        return true;
    }

    // consumer side. nullopt = queue closed AND drained.
    std::optional<T> pop() {
        std::unique_lock lk(m_);
        not_empty_.wait(lk, [&] { return !q_.empty() || closed_; });
        if (q_.empty()) return std::nullopt;      // closed + empty
        T v = std::move(q_.front());
        q_.pop();
        lk.unlock();
        not_full_.notify_one();
        return v;
    }

    void close() {
        {
            std::lock_guard lk(m_);
            closed_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }
};

int main() {
    BoundedQueue<int>  q(8);
    std::atomic<long>  produced{0}, consumed{0}, sum{0};

    constexpr int kProducers = 3, kConsumers = 2;
    constexpr long kPerProducer = 10000;

    std::vector<std::thread> prod, cons;

    for (int p = 0; p < kProducers; ++p)
        prod.emplace_back([&, p] {
            for (long i = 0; i < kPerProducer; ++i)
                if (q.push(static_cast<int>(p * kPerProducer + i)))
                    produced.fetch_add(1, std::memory_order_relaxed);
        });

    for (int c = 0; c < kConsumers; ++c)
        cons.emplace_back([&] {
            while (auto v = q.pop()) {
                consumed.fetch_add(1, std::memory_order_relaxed);
                sum.fetch_add(*v, std::memory_order_relaxed);
            }
        });

    for (auto& t : prod) t.join();     // saare producers khatam
    q.close();                          // ab consumers ko batao "aur data nahi aayega"
    for (auto& t : cons) t.join();

    const long expected_count = kProducers * kPerProducer;
    // sum of 0..(expected_count-1)
    const long expected_sum = expected_count * (expected_count - 1) / 2;

    std::printf("produced = %ld  consumed = %ld  (expected %ld)\n",
                produced.load(), consumed.load(), expected_count);
    std::printf("sum      = %ld  (expected %ld)  %s\n",
                sum.load(), expected_sum,
                sum.load() == expected_sum ? "OK" : "MISMATCH");

    std::puts("\nKey points:");
    std::puts(" - wait(lk, predicate): lock chhodta hai, wait karta, wake pe predicate");
    std::puts("   re-check karta -> SPURIOUS WAKEUP + missed-notify dono safe.");
    std::puts(" - notify mutex ke BAHAR (unlock ke baad) -> woken thread ko turant lock milta.");
    std::puts(" - close() + notify_all() = graceful shutdown; pop() nullopt deta jab drained.");
    std::puts(" - do CV (not_empty / not_full) -> producers aur consumers alag-alag wake.");
    return 0;
}
