// 04_concurrent_queue.cpp  --  49-PROJECTS advanced P4
// ============================================================
// Three queues, one harness:
//   (a) MutexQueue<T>  -- bounded, mutex + 2 condition variables, close()
//   (b) SpscRing<T,N>  -- lock-free, one producer / one consumer, no CAS
//   (c) MpscQueue<T>   -- lock-free-ish, many producers / one consumer (Vyukov)
// Stress tests prove in-order, no-loss delivery for each. Run at -O2.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -pthread -O2 04_concurrent_queue.cpp -o t && ./t
// ============================================================

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

// ---- (a) bounded blocking queue ---------------------------------
template <class T>
class MutexQueue {
    std::mutex m_;
    std::condition_variable not_full_, not_empty_;
    std::queue<T> q_;
    std::size_t cap_;
    bool closed_ = false;
public:
    explicit MutexQueue(std::size_t cap) : cap_(cap ? cap : 1) {}
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
        if (q_.empty()) return std::nullopt;
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
};

// ---- (b) lock-free SPSC ring ----------------------------------
template <class T, std::size_t N>
class SpscRing {
    static_assert((N & (N - 1)) == 0, "N must be a power of two");
    alignas(64) std::atomic<std::size_t> head_{0};   // consumer writes
    std::size_t head_cache_ = 0;
    alignas(64) std::atomic<std::size_t> tail_{0};   // producer writes
    std::size_t tail_cache_ = 0;
    alignas(64) T buf_[N]{};
public:
    bool try_push(const T& v) {
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        if (t - head_cache_ == N) {
            head_cache_ = head_.load(std::memory_order_acquire);
            if (t - head_cache_ == N) return false;
        }
        buf_[t & (N - 1)] = v;
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }
    bool try_pop(T& out) {
        const std::size_t h = head_.load(std::memory_order_relaxed);
        if (h == tail_cache_) {
            tail_cache_ = tail_.load(std::memory_order_acquire);
            if (h == tail_cache_) return false;
        }
        out = buf_[h & (N - 1)];
        head_.store(h + 1, std::memory_order_release);
        return true;
    }
};

// ---- (c) lock-free MPSC (Vyukov intrusive) -------------------
template <class T>
class MpscQueue {
    struct Node {
        std::atomic<Node*> next{nullptr};
        T                  value{};
    };
    alignas(64) std::atomic<Node*> head_;            // consumer end
    alignas(64) std::atomic<Node*> tail_;            // producer end
    Node stub_;
public:
    MpscQueue() : head_(&stub_), tail_(&stub_) {}
    MpscQueue(const MpscQueue&) = delete;
    MpscQueue& operator=(const MpscQueue&) = delete;

    void push(T v) {
        Node* n = new Node;
        n->value = std::move(v);
        n->next.store(nullptr, std::memory_order_relaxed);
        Node* prev = tail_.exchange(n, std::memory_order_acq_rel);   // wait-free for producers
        prev->next.store(n, std::memory_order_release);              // link (brief gap here is OK)
    }
    std::optional<T> pop() {
        Node* h = head_.load(std::memory_order_relaxed);
        Node* nxt = h->next.load(std::memory_order_acquire);
        if (!nxt) return std::nullopt;                               // empty (or producer mid-link)
        T v = std::move(nxt->value);
        head_.store(nxt, std::memory_order_relaxed);
        if (h != &stub_) delete h;                                   // recycle old head
        return v;
    }
    ~MpscQueue() {
        while (pop()) {}
        Node* h = head_.load();
        if (h != &stub_) delete h;
    }
};

int main() {
    // (a) mutex queue: N producers, M consumers, checksum balances
    {
        constexpr int P = 3, C = 3, PER = 20000;
        MutexQueue<std::uint64_t> q(64);
        std::atomic<std::uint64_t> psum{0}, csum{0};
        std::atomic<int> ccount{0};
        std::vector<std::thread> ths;
        for (int p = 0; p < P; ++p)
            ths.emplace_back([&, p] {
                for (int i = 0; i < PER; ++i) {
                    const std::uint64_t x = static_cast<std::uint64_t>(p) * 1000000ULL
                                          + static_cast<std::uint64_t>(i) + 1ULL;
                    psum.fetch_add(x, std::memory_order_relaxed);
                    q.push(x);
                }
            });
        for (int c = 0; c < C; ++c)
            ths.emplace_back([&] {
                while (auto v = q.pop()) {
                    csum.fetch_add(*v, std::memory_order_relaxed);
                    ccount.fetch_add(1, std::memory_order_relaxed);
                }
            });
        for (int i = 0; i < P; ++i) ths[static_cast<std::size_t>(i)].join();
        q.close();
        for (int i = P; i < P + C; ++i) ths[static_cast<std::size_t>(i)].join();
        assert(ccount.load() == P * PER);
        assert(csum.load() == psum.load());
    }

    // (b) SPSC ring: 2 threads, 2e6 items, strict in-order
    {
        constexpr std::uint64_t COUNT = 2'000'000;
        SpscRing<std::uint64_t, 1024> ring;
        std::thread prod([&] {
            for (std::uint64_t i = 0; i < COUNT; ++i)
                while (!ring.try_push(i)) { /* spin */ }
        });
        std::uint64_t expected = 0;
        std::uint64_t got = 0;
        while (got < COUNT) {
            std::uint64_t v;
            if (ring.try_pop(v)) { assert(v == expected); ++expected; ++got; }
        }
        prod.join();
        assert(expected == COUNT);
    }

    // (c) MPSC: 4 producers x 250k, consumer sees every value exactly once,
    //     and each producer's items arrive in that producer's order
    {
        constexpr int PN = 4;
        constexpr std::uint64_t PER = 250000;
        MpscQueue<std::uint64_t> q;
        std::vector<std::thread> prods;
        for (int p = 0; p < PN; ++p)
            prods.emplace_back([&q, p] {
                const std::uint64_t base = static_cast<std::uint64_t>(p) << 40;
                for (std::uint64_t i = 0; i < PER; ++i) q.push(base | i);
            });
        std::vector<std::uint64_t> last(static_cast<std::size_t>(PN), 0);
        std::vector<std::uint64_t> seen(static_cast<std::size_t>(PN), 0);
        std::uint64_t total = 0;
        const std::uint64_t want = static_cast<std::uint64_t>(PN) * PER;
        while (total < want) {
            if (auto v = q.pop()) {
                const auto p   = static_cast<std::size_t>(*v >> 40);
                const auto idx = *v & ((std::uint64_t{1} << 40) - 1);
                if (seen[p] > 0) assert(idx == last[p] + 1);   // per-producer FIFO order
                last[p] = idx;
                ++seen[p];
                ++total;
            }
        }
        for (auto& t : prods) t.join();
        for (std::size_t p = 0; p < static_cast<std::size_t>(PN); ++p) assert(seen[p] == PER);
    }

    std::puts("04_concurrent_queue: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - MutexQueue: predicate waits, two CVs (producers/consumers wait on
//     different conditions), close() to end the consumers.
//   - SpscRing: NO CAS -- each index has exactly one writer. release store on
//     the index pairs with the acquire load on the other side (happens-before
//     covers the buffer write). alignas(64) + cached opposite index kills the
//     coherence ping-pong. (27, 28, 32)
//   - MpscQueue (Vyukov): producers do a single atomic exchange on tail_ ->
//     wait-free, no retry loop. A stub node means head/tail are never null.
//     There is a transient window where prev->next isn't linked yet -> pop()
//     reports "empty" for that instant; that's correct, not a bug.
//   - Run at -O2: -O0 hides the reordering these fences exist to prevent.
// ============================================================
