// 07_async_logger.cpp
// ============================================================
// Lock-free async logger -- the hot path only ENQUEUES a fixed-size POD
// record (no allocation, no formatting, no syscall); a background
// thread dequeues and does the actual (slow) formatting + I/O. Measured
// against a naive "format + write directly on the hot path" baseline.
// (09-lock-free-logging.md)
//
// This is 36-LOW-LATENCY-CPP's "syscall/allocation avoidance" principle
// applied specifically to logging -- the ONE piece of infrastructure
// code that touches literally every hot-path function, so its cost
// multiplies everywhere if done naively.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 07_async_logger.cpp -o alogger && ./alogger
// ============================================================

#include "spsc_queue.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>
#include <x86intrin.h>

namespace ch = std::chrono;
static inline std::uint64_t tsc() { _mm_lfence(); auto t = __rdtsc(); _mm_lfence(); return t; }
static double g_tpns = 1.0;

// Fixed-size, no heap allocation -- 36's "allocation-free hot path"
// rule applied to a log line.
struct LogRecord {
    std::uint64_t t_tsc;
    std::uint32_t code;     // e.g. an enum/event id -- no string formatting on hot path
    char          tag[48];  // fixed buffer -- truncated if longer, never allocates
};

constexpr std::size_t   kCap = 1u << 14;
constexpr std::uint64_t kN   = 500'000;

// ============================================================
//  Async logger -- hot path: try_push() only.
// ============================================================
class AsyncLogger {
public:
    // HOT PATH. Non-blocking; drop-on-full is the explicit policy here
    // (a logger backing up must never be allowed to slow down or block
    // the thing it's logging about -- 36's "never let observability
    // become the bottleneck" principle).
    bool log(std::uint32_t code, const char* tag) {
        LogRecord r{};
        r.t_tsc = tsc();
        r.code  = code;
        std::strncpy(r.tag, tag, sizeof(r.tag) - 1);
        return q_.try_push(r);
    }

    // Background-thread-only.
    bool drain_one(LogRecord& out) { return q_.try_pop(out); }

private:
    SpscQueue<LogRecord, kCap> q_;
};

static void report(const char* name, std::vector<double>& ns) {
    std::sort(ns.begin(), ns.end());
    auto pc = [&](double p) {
        std::size_t i = static_cast<std::size_t>(p / 100.0 * static_cast<double>(ns.size()));
        return ns[std::min(i, ns.size() - 1)];
    };
    std::printf("  %-18s : p50 %6.1f   p99 %8.1f   p99.9 %9.1f   max %10.1f  ns\n",
                name, pc(50), pc(99), pc(99.9), ns.back());
}

int main() {
    {
        auto c0 = ch::steady_clock::now();
        std::uint64_t r0 = tsc();
        volatile std::uint64_t s = 0;
        while (ch::duration_cast<ch::milliseconds>(ch::steady_clock::now() - c0).count() < 120) s = s + 1;
        std::uint64_t r1 = tsc();
        auto c1 = ch::steady_clock::now();
        g_tpns = static_cast<double>(r1 - r0)
               / static_cast<double>(ch::duration_cast<ch::nanoseconds>(c1 - c0).count());
    }
    std::printf("ticks_per_ns = %.4f\n\n", g_tpns);

    // ---- A) naive: format + write DIRECTLY on the "hot path" ----
    // Writes to the SAME kind of sink (a discarded file) the async
    // consumer uses below -- isolates the ONE variable we're testing
    // (synchronous-on-hot-path vs asynchronous-on-background-thread),
    // not "console vs file."
    FILE* naive_sink = std::fopen("NUL", "w");
    if (!naive_sink) naive_sink = stderr;
    std::vector<double> ns_naive; ns_naive.reserve(kN);
    for (std::uint64_t i = 0; i < kN; ++i) {
        const auto t0 = tsc();
        std::fprintf(naive_sink, "[naive] event=%llu tag=order_ack\n",
                     static_cast<unsigned long long>(i));
        const auto t1 = tsc();
        ns_naive.push_back(static_cast<double>(t1 - t0) / g_tpns);
    }
    if (naive_sink != stderr) std::fclose(naive_sink);

    // ---- B) async: hot path only enqueues; consumer thread formats+writes ----
    AsyncLogger logger;
    std::atomic<bool> stop{false};
    std::uint64_t consumed = 0;
    std::thread consumer([&] {
        LogRecord r;
        FILE* sink = std::fopen("NUL", "w");   // background thread's slow I/O --
                                                 // discard target, only the HOT
                                                 // PATH cost is what we're measuring
        if (!sink) sink = stderr;
        while (!stop.load(std::memory_order_relaxed)) {
            if (logger.drain_one(r)) {
                std::fprintf(sink, "[async] t=%llu code=%u tag=%s\n",
                             static_cast<unsigned long long>(r.t_tsc), r.code, r.tag);
                ++consumed;
            }
        }
        while (logger.drain_one(r)) { ++consumed; }
        if (sink != stderr) std::fclose(sink);
    });

    std::vector<double> ns_async; ns_async.reserve(kN);
    for (std::uint64_t i = 0; i < kN; ++i) {
        const auto t0 = tsc();
        logger.log(static_cast<std::uint32_t>(i), "order_ack");
        const auto t1 = tsc();
        ns_async.push_back(static_cast<double>(t1 - t0) / g_tpns);
    }
    while (consumed < kN) { /* let consumer drain */ }
    stop.store(true, std::memory_order_relaxed);
    consumer.join();

    std::printf("A) naive (format+write ON hot path):\n");
    report("naive log()", ns_naive);
    std::printf("\nB) async (hot path only enqueues, background thread formats+writes):\n");
    report("async log()", ns_async);

    double sum_naive = 0, sum_async = 0;
    for (double v : ns_naive) sum_naive += v;
    for (double v : ns_async) sum_async += v;
    std::printf("\nmean hot-path cost: naive %.1f ns vs async %.1f ns  (%.1fx)\n",
                sum_naive / static_cast<double>(kN), sum_async / static_cast<double>(kN),
                (sum_naive / static_cast<double>(kN)) / (sum_async / static_cast<double>(kN)));

    return 0;
}
