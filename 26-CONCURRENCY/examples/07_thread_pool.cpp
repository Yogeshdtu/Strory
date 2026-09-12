// 07_thread_pool.cpp
// ============================================================
// Ek poora (chhota) thread pool: task queue (mutex + cv), N worker
// threads, submit() jo std::future deta hai, clean shutdown in dtor.
// Benchmark: pool vs std::async-per-task vs serial.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra -pthread 07_thread_pool.cpp -o tp && ./tp
// ============================================================

#include <cstdio>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <future>
#include <functional>
#include <memory>
#include <type_traits>
#include <stdexcept>
#include <utility>
#include <chrono>

class ThreadPool {
    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex                        m_;
    std::condition_variable           cv_;
    bool                             stop_ = false;

public:
    explicit ThreadPool(unsigned n) {
        for (unsigned i = 0; i < n; ++i)
            workers_.emplace_back([this] { worker_loop(); });
    }

    // non-copyable, non-movable (owns threads + a mutex)
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ~ThreadPool() {
        {
            std::lock_guard lk(m_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& w : workers_) w.join();
    }

    // submit any callable -> get a future for its result
    template <class F, class... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<R()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<R> fut = task->get_future();
        {
            std::lock_guard lk(m_);
            if (stop_) throw std::runtime_error("submit on stopped pool");
            tasks_.emplace([task] { (*task)(); });
        }
        cv_.notify_one();
        return fut;
    }

    std::size_t size() const { return workers_.size(); }

private:
    void worker_loop() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock lk(m_);
                cv_.wait(lk, [this] { return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) return;
                job = std::move(tasks_.front());
                tasks_.pop();
            }
            job();
        }
    }
};

// ---- some CPU work to schedule ----
static long fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

using Clock = std::chrono::steady_clock;
static double ms_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

int main() {
    constexpr int  kTasks = 64;
    constexpr int  kN     = 30;      // fib(30) ~ a few ms each

    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 4;
    std::printf("hardware_concurrency = %u,  %d tasks of fib(%d)\n\n", hw, kTasks, kN);

    // --------------------------------------------------------
    //  1. Serial baseline
    // --------------------------------------------------------
    {
        auto t0 = Clock::now();
        long acc = 0;
        for (int i = 0; i < kTasks; ++i) acc += fib(kN);
        std::printf("serial            : %8.1f ms   [acc=%ld]\n", ms_since(t0), acc);
    }

    // --------------------------------------------------------
    //  2. std::async per task (spawns a thread per task — costly)
    // --------------------------------------------------------
    {
        auto t0 = Clock::now();
        std::vector<std::future<long>> fs;
        for (int i = 0; i < kTasks; ++i)
            fs.push_back(std::async(std::launch::async, fib, kN));
        long acc = 0;
        for (auto& f : fs) acc += f.get();
        std::printf("std::async/task   : %8.1f ms   [acc=%ld]\n", ms_since(t0), acc);
    }

    // --------------------------------------------------------
    //  3. ThreadPool (reused threads, bounded)
    // --------------------------------------------------------
    {
        ThreadPool pool(hw);
        auto t0 = Clock::now();
        std::vector<std::future<long>> fs;
        fs.reserve(kTasks);
        for (int i = 0; i < kTasks; ++i)
            fs.push_back(pool.submit(fib, kN));
        long acc = 0;
        for (auto& f : fs) acc += f.get();
        std::printf("ThreadPool(%u)     : %8.1f ms   [acc=%ld]\n", hw, ms_since(t0), acc);
    }

    std::puts("\nNateeja:");
    std::puts(" - serial: 1 core. Pool/async: ~hw cores -> ~hw x faster (CPU-bound).");
    std::puts(" - std::async-per-task ek thread PER TASK banata (create/destroy cost,");
    std::puts("   oversubscription). Pool N threads REUSE karta -> kam overhead.");
    std::puts(" - Pool = bounded parallelism + a work queue. HFT/servers ka standard.");
    return 0;
}
