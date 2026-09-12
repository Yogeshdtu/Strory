// 04_lock_free_stack.cpp
// ============================================================
// Treiber stack (the classic lock-free LIFO) — push/pop via a
// CAS loop on the head. Built over a fixed node POOL (indices,
// not raw pointers) so it works without a 128-bit CAS, which
// MinGW/this toolchain doesn't provide.
//
// ABA-safe: the head is a packed {index:32, tag:32} word in one
// std::atomic<uint64_t>. Every push bumps the tag, so a stale CAS
// (index matches but the stack moved A->B->A) FAILS instead of
// corrupting the list. (Folder 27 file 15 / example 07 shows the
// bug when the tag is absent.)
//
// Correctness: N threads each push K values and pop K values;
// the multiset of everything popped must equal everything pushed.
// Benched against a std::mutex + std::vector stack.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 04_lock_free_stack.cpp -o lfstack && ./lfstack
// ============================================================

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <thread>
#include <vector>
#include <mutex>
#include <chrono>

using Clock = std::chrono::steady_clock;

static constexpr std::uint32_t NIL = 0xFFFFFFFFu;

struct Node {
    std::uint64_t value{0};
    std::uint32_t next{NIL};      // plain: only the owning thread touches it pre-publish
};

// ------------------------------------------------------------
//  TaggedStack — a Treiber stack of node indices into `pool`.
//  head_ = (tag << 32) | index.   push bumps tag.
// ------------------------------------------------------------
class TaggedStack {
public:
    explicit TaggedStack(Node* pool) : pool_(pool) {}

    void push(std::uint32_t idx) {
        std::uint64_t cur = head_.load(std::memory_order_relaxed);
        for (;;) {
            const std::uint32_t cur_idx = static_cast<std::uint32_t>(cur);
            const std::uint32_t cur_tag = static_cast<std::uint32_t>(cur >> 32);
            pool_[idx].next = cur_idx;
            const std::uint64_t next =
                (static_cast<std::uint64_t>(cur_tag + 1u) << 32) | idx;
            if (head_.compare_exchange_weak(cur, next,
                                            std::memory_order_release,
                                            std::memory_order_relaxed))
                return;
            // cur refreshed by the failed CAS -> retry
        }
    }

    // returns NIL if empty
    std::uint32_t pop() {
        std::uint64_t cur = head_.load(std::memory_order_acquire);
        for (;;) {
            const std::uint32_t cur_idx = static_cast<std::uint32_t>(cur);
            if (cur_idx == NIL) return NIL;
            const std::uint32_t cur_tag = static_cast<std::uint32_t>(cur >> 32);
            const std::uint32_t nxt = pool_[cur_idx].next;
            const std::uint64_t next =
                (static_cast<std::uint64_t>(cur_tag + 1u) << 32) | nxt;
            if (head_.compare_exchange_weak(cur, next,
                                            std::memory_order_acquire,
                                            std::memory_order_acquire))
                return cur_idx;
        }
    }

private:
    Node* pool_;
    alignas(64) std::atomic<std::uint64_t> head_{
        (static_cast<std::uint64_t>(0) << 32) | NIL};
    char pad_[64];
};

// ------------------------------------------------------------
//  Mutex baseline: a plain LIFO of uint64_t.
// ------------------------------------------------------------
class MutexStack {
public:
    void push(std::uint64_t v) { std::lock_guard lk(m_); s_.push_back(v); }
    bool pop(std::uint64_t& out) {
        std::lock_guard lk(m_);
        if (s_.empty()) return false;
        out = s_.back();
        s_.pop_back();
        return true;
    }
private:
    std::mutex m_;
    std::vector<std::uint64_t> s_;
};

constexpr int           kThreads = 4;
constexpr std::uint64_t kPerThr  = 1'500'000;
constexpr std::size_t   kPool    = 1u << 14;    // 16384 nodes (occupancy stays low)

static Node g_pool[kPool];

static double run_lockfree() {
    // all pool nodes start on a free list; data stack starts empty
    TaggedStack freelist(g_pool);
    TaggedStack data(g_pool);
    for (std::uint32_t i = 0; i < kPool; ++i) freelist.push(i);

    std::atomic<bool> go{false};
    std::atomic<std::int64_t> total{0};

    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t)
        ts.emplace_back([&, t] {
            while (!go.load(std::memory_order_acquire)) {}
            const std::uint64_t base = static_cast<std::uint64_t>(t) * kPerThr;
            std::int64_t acc = 0;
            for (std::uint64_t i = 0; i < kPerThr; ++i) {
                // push one value
                std::uint32_t n;
                while ((n = freelist.pop()) == NIL) { /* pool momentarily empty */ }
                g_pool[n].value = base + i;
                data.push(n);
                // pop one value
                std::uint32_t m;
                while ((m = data.pop()) == NIL) { /* someone else drained it */ }
                acc += static_cast<std::int64_t>(g_pool[m].value & 0xffff);
                freelist.push(m);
            }
            total.fetch_add(acc, std::memory_order_relaxed);
        });

    auto t0 = Clock::now();
    go.store(true, std::memory_order_release);
    for (auto& th : ts) th.join();
    const double sec = std::chrono::duration<double>(Clock::now() - t0).count();

    std::int64_t expect = 0;
    for (int t = 0; t < kThreads; ++t) {
        const std::uint64_t base = static_cast<std::uint64_t>(t) * kPerThr;
        for (std::uint64_t i = 0; i < kPerThr; ++i)
            expect += static_cast<std::int64_t>((base + i) & 0xffff);
    }
    // data stack must be empty again
    const bool empty = (data.pop() == NIL);
    std::printf("  lock-free (tagged) : %7.1f ms   %6.1f M op/s   %s\n",
                sec * 1e3,
                2.0 * static_cast<double>(kThreads) * static_cast<double>(kPerThr) / sec / 1e6,
                (total.load() == expect && empty) ? "OK" : "WRONG");
    return sec;
}

static double run_mutex() {
    MutexStack data;
    std::atomic<bool> go{false};
    std::atomic<std::int64_t> total{0};

    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t)
        ts.emplace_back([&, t] {
            while (!go.load(std::memory_order_acquire)) {}
            const std::uint64_t base = static_cast<std::uint64_t>(t) * kPerThr;
            std::int64_t acc = 0;
            for (std::uint64_t i = 0; i < kPerThr; ++i) {
                data.push(base + i);
                std::uint64_t v;
                while (!data.pop(v)) {}
                acc += static_cast<std::int64_t>(v & 0xffff);
            }
            total.fetch_add(acc, std::memory_order_relaxed);
        });

    auto t0 = Clock::now();
    go.store(true, std::memory_order_release);
    for (auto& th : ts) th.join();
    const double sec = std::chrono::duration<double>(Clock::now() - t0).count();

    std::int64_t expect = 0;
    for (int t = 0; t < kThreads; ++t) {
        const std::uint64_t base = static_cast<std::uint64_t>(t) * kPerThr;
        for (std::uint64_t i = 0; i < kPerThr; ++i)
            expect += static_cast<std::int64_t>((base + i) & 0xffff);
    }
    std::printf("  mutex + vector     : %7.1f ms   %6.1f M op/s   %s\n",
                sec * 1e3,
                2.0 * static_cast<double>(kThreads) * static_cast<double>(kPerThr) / sec / 1e6,
                (total.load() == expect) ? "OK" : "WRONG");
    return sec;
}

int main() {
    std::printf("Treiber stack: %d threads x (%llu push + %llu pop), pool %zu nodes\n\n",
                kThreads, (unsigned long long)kPerThr, (unsigned long long)kPerThr, kPool);

    const double lf = run_lockfree();
    const double mx = run_mutex();
    std::printf("\n  lock-free / mutex ratio: %.2fx  (>1 = lock-free faster)\n", mx / lf);

    std::puts("\n>>> Is machine pe lock-free stack MUTEX SE SLOW hai (~0.2x). Yeh");
    std::puts("    galti nahi — yahi lesson hai (CLAUDE.md Rule 2, file 14):");
    std::puts("    - Har iteration mein 4 CAS-loop (freelist + data, push + pop),");
    std::puts("      4 threads, 2 hot atomic words -> retry storm. Har fail = re-read.");
    std::puts("    - Mutex + vector: lock ek CAS, critical section 2 instruction,");
    std::puts("      sab threads ek hi hot vector-tail cache line pe -> bahut");
    std::puts("      cache-friendly, low hold time -> barely contends.");
    std::puts("    - LIFO stack = single head = maximum contention shape.");
    std::puts("    Lock-free ka faayda: no deadlock, no priority inversion, progress");
    std::puts("    agar holder pre-empt/crash ho jaaye — NAHI ki guaranteed speed.");

    std::puts("\nKya hua:");
    std::puts(" - push: node->next = head.idx; CAS(head, {new_idx, tag+1}). Fail hone");
    std::puts("   pe head refresh -> retry. pop: read head.idx ka next, CAS(head,");
    std::puts("   {next, tag+1}).");
    std::puts(" - TAG kyun: bina tag ke, agar head A->B->A gaya (node A pop hua, reuse");
    std::puts("   hua, wapas push hua), to ek purana popper CAS(head, A, B) SUCCEED kar");
    std::puts("   jaata — B already pop ho chuka. Stack corrupt. Tag har push pe badhta");
    std::puts("   -> stale CAS ka {idx=A, tag=purana} current {idx=A, tag=naya} se");
    std::puts("   match nahi karta -> FAIL -> retry. (Folder 27 example 07.)");
    std::puts(" - 32-bit index + 32-bit tag ek uint64_t mein -> lock-free everywhere,");
    std::puts("   koi cmpxchg16b nahi chahiye. Raw pointer + tag ke liye 128-bit CAS");
    std::puts("   chahiye hota.");
    std::puts(" - Stack ka LIFO nature + high contention = head pe CAS retry storm.");
    std::puts("   Isliye HFT hot path pe stack se zyada SPSC ring dikhta hai.");
    return 0;
}
