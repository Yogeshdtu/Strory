# 07 — Concurrency: worked solutions

Poore code — the blocking queue, the pool, the correct DCLP. Baaki
`07-concurrency-problems.md` ke `<details>` blocks mein. Sab `-pthread`;
TSan (Linux/WSL) ke neeche verify karo.

---

## B1 — Bounded blocking queue

```cpp
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

template <class T>
class BlockingQueue {
    std::mutex              m_;
    std::condition_variable not_full_, not_empty_;
    std::queue<T>           q_;
    std::size_t             cap_;
    bool                    closed_ = false;
public:
    explicit BlockingQueue(std::size_t cap) : cap_(cap ? cap : 1) {}

    bool push(T v) {
        std::unique_lock lk(m_);
        not_full_.wait(lk, [&] { return q_.size() < cap_ || closed_; });
        if (closed_) return false;
        q_.push(std::move(v));
        lk.unlock();
        not_empty_.notify_one();
        return true;
    }
    std::optional<T> pop() {
        std::unique_lock lk(m_);
        not_empty_.wait(lk, [&] { return !q_.empty() || closed_; });
        if (q_.empty()) return std::nullopt;      // closed and drained
        T v = std::move(q_.front());
        q_.pop();
        lk.unlock();
        not_full_.notify_one();
        return v;
    }
    void close() {
        { std::lock_guard lk(m_); closed_ = true; }
        not_full_.notify_all();
        not_empty_.notify_all();
    }
};
```

Predicate-form `wait` (spurious + lost wakeup dono handle). `close()` sab waiters
ko jagaata; `pop` closed ke baad bhi bache hue items deta, phir `nullopt`.
`notify` ko lock ke bahar (waiter ko turant lock mil jaaye).

---

## B2 — Thread pool

```cpp
#include <functional>
#include <future>
#include <thread>
#include <vector>
// uses BlockingQueue<std::function<void()>> from B1

class ThreadPool {
    BlockingQueue<std::function<void()>> tasks_;
    std::vector<std::thread>             workers_;
public:
    explicit ThreadPool(unsigned n) : tasks_(1024) {
        for (unsigned i = 0; i < n; ++i)
            workers_.emplace_back([this] {
                while (auto job = tasks_.pop()) (*job)();   // nullopt -> exit
            });
    }
    ~ThreadPool() {
        tasks_.close();
        for (auto& w : workers_) w.join();
    }
    template <class F>
    auto submit(F f) -> std::future<decltype(f())> {
        auto task = std::make_shared<std::packaged_task<decltype(f())()>>(std::move(f));
        std::future<decltype(f())> fut = task->get_future();
        tasks_.push([task] { (*task)(); });
        return fut;
    }
};
```

`packaged_task` result/exception ko `future` mein daalta. `shared_ptr` isliye ki
`std::function` copyable hona chahiye aur `packaged_task` move-only. `~ThreadPool`
= close + join (pending tasks drain hote — chahe to drop policy bhi).

---

## C3 — Correct double-checked locking

```cpp
#include <atomic>
#include <mutex>

class Widget { /* expensive ctor */ };

Widget* get_widget() {
    static std::atomic<Widget*> inst{nullptr};
    static std::mutex           m;

    Widget* p = inst.load(std::memory_order_acquire);
    if (p == nullptr) {
        std::lock_guard lk(m);
        p = inst.load(std::memory_order_relaxed);          // re-check under lock
        if (p == nullptr) {
            p = new Widget();
            inst.store(p, std::memory_order_release);      // publish AFTER ctor
        }
    }
    return p;
}
```

`release` store / `acquire` load pair = ctor ke saare writes doosre thread ko
pointer dikhne se **pehle** visible (happens-before). Pre-C++11 (no memory
model) yeh bug tha — reader ko non-null pointer par half-constructed object
milta. **Simplest correct version:** `static Widget inst;` — C++11 function-local
static thread-safe init deta, koi manual DCLP nahi.

---

## B9 — False sharing fix

```cpp
#include <atomic>
#include <cstdint>
#include <new>

#ifdef __cpp_lib_hardware_interference_size
constexpr std::size_t kLine = std::hardware_destructive_interference_size;
#else
constexpr std::size_t kLine = 64;
#endif

struct alignas(kLine) PaddedCounter {
    std::atomic<std::int64_t> value{0};
    char pad[kLine - sizeof(std::atomic<std::int64_t>)];
};

// PaddedCounter a, b;   // guaranteed different cache lines
// thread 1: a.value.fetch_add(1, std::memory_order_relaxed);
// thread 2: b.value.fetch_add(1, std::memory_order_relaxed);
```

Bina padding ke `a` aur `b` ek 64-B line pe → har `fetch_add` doosre core ki
line ko invalidate karta (coherence ping-pong), 5–10× slower on 4+ threads even
though the counters are logically independent. `alignas(64)` + tail padding →
alag lines. Measure: `09-optimization-problems.md` #19.
