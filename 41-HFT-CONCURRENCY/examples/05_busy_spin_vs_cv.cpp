// 05_busy_spin_vs_cv.cpp
// ============================================================
// Busy-spin vs blocking(cv) vs hybrid(spin-then-block) -- measured on
// BOTH axes of the trade-off (07-busy-spin-vs-blocking.md):
//   1) hand-off LATENCY (p50/p99/p99.9, paced producer)
//   2) CPU TIME actually consumed by the consumer thread (Windows
//      GetThreadTimes -- kernel+user time, vs wall-clock elapsed)
//
// 28-LOCK-FREE/examples/06 already measured latency (spin ~0.4us vs
// cv ~6us p50). What it did NOT measure is the OTHER half of the
// trade-off: spin wins latency by burning a full core continuously.
// This file measures BOTH sides side by side, plus a HYBRID strategy
// (spin briefly, then sleep) that's the common real-world compromise.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 05_busy_spin_vs_cv.cpp -o spinvscv && ./spinvscv
//   (Windows-only: GetThreadTimes. Linux equivalent: getrusage(RUSAGE_THREAD,...))
// ============================================================

#define NOMINMAX
#include <windows.h>

#include "spsc_queue.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>
#include <x86intrin.h>

namespace ch = std::chrono;
static inline std::uint64_t tsc() { _mm_lfence(); auto t = __rdtsc(); _mm_lfence(); return t; }
static double g_tpns = 1.0;

struct Msg { std::uint64_t seq; std::uint64_t t_tsc; };

constexpr std::size_t   kCap    = 1u << 10;
constexpr std::uint64_t kN      = 100'000;
constexpr std::uint64_t kPaceTicks = 10'000;   // ~5us @ ~2GHz -- realistic-ish tick pace

// A lightweight "doorbell" -- separate from the data path. The queue
// itself stays lock-free; this is JUST a wake signal for strategies
// that choose to sleep.
struct Doorbell {
    std::mutex              m;
    std::condition_variable cv;
    void ring() { std::lock_guard<std::mutex> lk(m); cv.notify_one(); }
};

static std::uint64_t filetime_to_ns(const FILETIME& ft) {
    const ULARGE_INTEGER u{ .LowPart = ft.dwLowDateTime, .HighPart = ft.dwHighDateTime };
    return u.QuadPart * 100u;   // FILETIME units are 100ns
}

// Returns {cpu_ns, wall_ns, latencies}.
struct Result { double cpu_ns; double wall_ns; std::vector<double> lat_ns; };

enum class Strategy { Spin, Block, Hybrid };

static Result run_strategy(Strategy strat) {
    static SpscQueue<Msg, kCap> q;
    Doorbell bell;
    std::atomic<bool> go{false};
    std::vector<double> lat; lat.reserve(kN);

    // MinGW's std::thread (posix threading model / winpthreads) does NOT
    // hand back a real Win32 HANDLE from native_handle() -- it's a
    // pthread_t, and reinterpret_cast-ing it into GetThreadTimes() fails
    // (ERROR_INVALID_HANDLE). The fix: from INSIDE the thread, ask for a
    // real handle via DuplicateHandle(GetCurrentThread()) -- that pseudo-
    // handle is only valid from within the calling thread, but a
    // DUPLICATE of it is a real, freely-shareable handle any thread can
    // later pass to GetThreadTimes().
    std::atomic<HANDLE> real_handle{nullptr};

    std::thread consumer([&] {
        HANDLE h = nullptr;
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
                         &h, 0, FALSE, DUPLICATE_SAME_ACCESS);
        real_handle.store(h, std::memory_order_release);
        while (!go.load(std::memory_order_acquire)) {}
        Msg m;
        std::uint64_t received = 0;
        std::uint64_t spin_count = 0;
        while (received < kN) {
            if (q.try_pop(m)) {
                lat.push_back(static_cast<double>(tsc() - m.t_tsc) / g_tpns);
                ++received;
                spin_count = 0;
                continue;
            }
            switch (strat) {
                case Strategy::Spin:
                    break;   // just loop again immediately
                case Strategy::Block: {
                    std::unique_lock<std::mutex> lk(bell.m);
                    bell.cv.wait_for(lk, ch::microseconds(50));   // no predicate --
                    // wakes on notify() OR timeout, whichever first; the timeout is
                    // a safety net against a lost wakeup (notify fired before we
                    // reached wait_for), NOT the normal wake path.
                    break;
                }
                case Strategy::Hybrid: {
                    if (++spin_count < 500) break;   // spin briefly first
                    std::unique_lock<std::mutex> lk(bell.m);
                    bell.cv.wait_for(lk, ch::microseconds(50));   // no predicate --
                    // wakes on notify() OR timeout, whichever first; the timeout is
                    // a safety net against a lost wakeup (notify fired before we
                    // reached wait_for), NOT the normal wake path.
                    spin_count = 0;
                    break;
                }
            }
        }
    });

    // Fresh queue for each strategy: give the consumer a moment to reach
    // its wait state, then pace-feed it.
    go.store(true, std::memory_order_release);
    const auto wall_t0 = ch::steady_clock::now();
    std::uint64_t next = tsc();
    for (std::uint64_t i = 0; i < kN; ++i) {
        next += kPaceTicks;
        while (tsc() < next) {}
        const Msg m{i, tsc()};
        while (!q.try_push(m)) {}
        if (strat != Strategy::Spin) bell.ring();
    }

    consumer.join();
    const auto wall_t1 = ch::steady_clock::now();

    // Measure consumer's total CPU time via the duplicated real handle
    // (valid even after the thread has exited -- it's a normal handle
    // now, not tied to native_handle()'s pthread_t).
    HANDLE h = real_handle.load(std::memory_order_acquire);
    FILETIME creation{}, exitt{}, kernel{}, user{};
    GetThreadTimes(h, &creation, &exitt, &kernel, &user);
    CloseHandle(h);

    Result r;
    r.cpu_ns  = static_cast<double>(filetime_to_ns(kernel) + filetime_to_ns(user));
    r.wall_ns = ch::duration<double, std::nano>(wall_t1 - wall_t0).count();
    r.lat_ns  = std::move(lat);
    return r;
}

static void report(const char* name, Result& r) {
    std::sort(r.lat_ns.begin(), r.lat_ns.end());
    auto pc = [&](double p) {
        std::size_t i = static_cast<std::size_t>(p / 100.0 * static_cast<double>(r.lat_ns.size()));
        return r.lat_ns[std::min(i, r.lat_ns.size() - 1)];
    };
    const double cpu_pct = 100.0 * r.cpu_ns / r.wall_ns;
    std::printf("  %-8s : p50 %6.1f  p99 %8.1f  p99.9 %9.1f ns  |  CPU %6.1f%% of wall time "
                "(%.1f ms CPU / %.1f ms wall)\n",
                name, pc(50), pc(99), pc(99.9), cpu_pct,
                r.cpu_ns / 1e6, r.wall_ns / 1e6);
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
    std::printf("ticks_per_ns = %.4f\n", g_tpns);
    std::printf("%llu paced messages, ~%.1f us apart\n\n",
                static_cast<unsigned long long>(kN),
                static_cast<double>(kPaceTicks) / g_tpns / 1000.0);

    auto spin   = run_strategy(Strategy::Spin);
    auto block  = run_strategy(Strategy::Block);
    auto hybrid = run_strategy(Strategy::Hybrid);

    report("spin",   spin);
    report("block",  block);
    report("hybrid", hybrid);

    return 0;
}
