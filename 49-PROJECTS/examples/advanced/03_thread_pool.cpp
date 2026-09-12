// 03_thread_pool.cpp  --  49-PROJECTS advanced P3
// ============================================================
// Fixed-size thread pool. submit(callable) -> std::future<R>. Workers pull
// from one mutex+cv task queue. Dtor drains and joins. submit() after
// shutdown throws. Exceptions in a task propagate through its future.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -pthread -g -O0 03_thread_pool.cpp -o t && ./t
// ============================================================

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(unsigned n) {
        for (unsigned i = 0; i < n; ++i)
            workers_.emplace_back([this] { worker_loop(); });
    }
    ~ThreadPool() { shutdown(); }
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <class F, class... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<R()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<R> fut = task->get_future();
        {
            std::lock_guard<std::mutex> lk(m_);
            if (stop_) throw std::runtime_error("submit() on a stopped ThreadPool");
            q_.emplace([task] { (*task)(); });
        }
        cv_.notify_one();
        return fut;
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lk(m_);
            if (stop_) return;
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& w : workers_) if (w.joinable()) w.join();
    }
    std::size_t pending() {
        std::lock_guard<std::mutex> lk(m_);
        return q_.size();
    }

private:
    void worker_loop() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lk(m_);
                cv_.wait(lk, [this] { return stop_ || !q_.empty(); });
                if (q_.empty()) return;                 // stop_ && drained
                job = std::move(q_.front());
                q_.pop();
            }
            job();                                      // run outside the lock
        }
    }

    std::mutex                        m_;
    std::condition_variable           cv_;
    std::queue<std::function<void()>> q_;
    std::vector<std::thread>          workers_;
    bool                             stop_ = false;
};

int main() {
    // 10k increment tasks -> exact total
    {
        ThreadPool pool(4);
        std::atomic<long> sum{0};
        std::vector<std::future<void>> fs;
        fs.reserve(10000);
        for (int i = 0; i < 10000; ++i)
            fs.push_back(pool.submit([&sum, i] { sum.fetch_add(i, std::memory_order_relaxed); }));
        for (auto& f : fs) f.get();
        long expected = 0;
        for (int i = 0; i < 10000; ++i) expected += i;
        assert(sum.load() == expected);
    }

    // return values come back through the future
    {
        ThreadPool pool(2);
        auto a = pool.submit([] { return 6 * 7; });
        auto b = pool.submit([](int x, int y) { return x + y; }, 20, 22);
        assert(a.get() == 42 && b.get() == 42);
    }

    // an exception in a task rethrows on get()
    {
        ThreadPool pool(2);
        auto f = pool.submit([]() -> int { throw std::runtime_error("task boom"); });
        bool threw = false;
        try { (void)f.get(); } catch (const std::runtime_error& e) {
            threw = (std::string(e.what()) == "task boom");
        }
        assert(threw);
    }

    // submit after shutdown throws; dtor joins cleanly
    {
        ThreadPool pool(2);
        pool.submit([] {}).get();
        pool.shutdown();
        bool threw = false;
        try { (void)pool.submit([] {}); } catch (const std::runtime_error&) { threw = true; }
        assert(threw);
    }

    std::puts("03_thread_pool: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - submit wraps the callable in a shared packaged_task (std::function must be
//     copyable; packaged_task is move-only -> the shared_ptr bridges that).
//   - cv_.wait(lk, pred): the predicate form handles spurious wakeups AND the
//     shutdown race. Workers exit only when stop_ && the queue is drained.
//   - job() runs OUTSIDE the lock -> a slow task doesn't block submit() or other
//     workers.
//   - shutdown() is idempotent; the dtor calls it. No detached threads.
//   - Bottleneck at scale: the single mutex. v3 = per-worker deques + work
//     stealing (28) -- measure tiny-task throughput to see the contention.
// ============================================================
