// 10_realtime_thread.linux.cpp
// ============================================================
// SCHED_OTHER (CFS, default) vs SCHED_FIFO (real-time) thread ka jitter.
// Ek periodic "wake every 200 us" loop, dono policies, wake-up error naapo.
// ============================================================
//  LINUX-ONLY (sched_setscheduler, SCHED_FIFO, pthread rt). SCHED_FIFO ke liye
//  CAP_SYS_NICE / root chahiye (ya `ulimit -r`, /etc/security/limits.conf).
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra -pthread 10_realtime_thread.linux.cpp -o rt
//      sudo ./rt        # SCHED_FIFO ke liye privilege chahiye
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <sched.h>
#include <pthread.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <vector>

static std::uint64_t now_ns() {
    timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ull + static_cast<std::uint64_t>(ts.tv_nsec);
}

static constexpr std::uint64_t PERIOD_NS = 200'000;      // 200 us
static constexpr int           CYCLES    = 20'000;

// clock_nanosleep(TIMER_ABSTIME) -> drift-free periodic wakeups.
// Return: wake-up error samples (actual - target), sorted.
static std::vector<long> run_periodic() {
    std::vector<long> err;
    err.reserve(CYCLES);
    timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    for (int i = 0; i < CYCLES; ++i) {
        next.tv_nsec += static_cast<long>(PERIOD_NS);
        while (next.tv_nsec >= 1'000'000'000L) { next.tv_nsec -= 1'000'000'000L; ++next.tv_sec; }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, nullptr);
        std::uint64_t woke = now_ns();
        std::uint64_t target = static_cast<std::uint64_t>(next.tv_sec) * 1'000'000'000ull
                             + static_cast<std::uint64_t>(next.tv_nsec);
        err.push_back(static_cast<long>(woke - target));   // >=0: kitni der se uthe
        // thoda kaam taaki loop trivial na ho
        volatile std::uint64_t x = woke; for (int k = 0; k < 200; ++k) x *= 3;
    }
    std::sort(err.begin(), err.end());
    return err;
}

static void report(const char* tag, const std::vector<long>& e) {
    auto q = [&](double p) { return e[static_cast<size_t>(p * (e.size() - 1))]; };
    std::printf("  [%-11s] wake error  p50=%ld  p99=%ld  p99.9=%ld  max=%ld ns\n",
                tag, q(0.50), q(0.99), q(0.999), e.back());
}

int main() {
    std::printf("periodic wake every %llu ns, %d cycles\n\n",
                (unsigned long long)PERIOD_NS, CYCLES);

    // ---------- 1. Default CFS ----------
    report("SCHED_OTHER", run_periodic());

    // ---------- 2. SCHED_FIFO, high prio ----------
    sched_param sp{};
    sp.sched_priority = 80;                       // 1..99; 80 typical for a trading thread
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) {
        std::perror("  sched_setscheduler(SCHED_FIFO) fail -- root/CAP_SYS_NICE chahiye");
        std::puts("  (privilege ke bina bas SCHED_OTHER wala number upar dekho)");
    } else {
        // RT thread ke saath: memory lock karo (page fault RT ko bhi rok sakta)
        // aur is thread ko ek core pe pin karo.
        cpu_set_t set; CPU_ZERO(&set); CPU_SET(1, &set);
        pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
        report("SCHED_FIFO", run_periodic());
    }

    std::puts(
        "\nKya seekha:\n"
        "  - SCHED_OTHER (CFS): scheduler fairness ke liye tumhe preempt karega\n"
        "    jab dusre runnable threads hon -> p99.9 wake error ms-scale spike.\n"
        "  - SCHED_FIFO: koi timeslice nahi, sirf higher-prio RT thread ya\n"
        "    interrupt tumhe rok sakta. p99/p99.9 kaafi tight.\n"
        "  - SCHED_FIFO khatarnak bhi hai: agar tum yield/sleep na karo to lower\n"
        "    prio kuch bhi chalne hi nahi milega (system hang). Isi liye isolated\n"
        "    core + `RLIMIT_RTTIME` guard.\n"
        "  - Best jitter: SCHED_FIFO + isolated core (isolcpus/nohz_full) +\n"
        "    mlockall + IRQ affinity dusre core pe. Tab busy-spin loop bhi\n"
        "    SCHED_OTHER se kam jitter deta -- kai HFT shops SCHED_OTHER + isolated\n"
        "    core + busy-poll hi use karte, SCHED_FIFO ka risk lene ke bajaye.");

    std::printf("\n[NOTE] Numbers TYPICAL hain. Bina core isolation ke SCHED_FIFO ka\n"
                "faayda ~2-5x; isolation ke saath tail ~10-50x tight ho sakta.\n");
    return 0;
}

/* ============================================================
 * EXPECTED OUTPUT (typical Linux x86-64, run as root, NON-isolated) -- NAHI napa gaya
 * ------------------------------------------------------------
 * periodic wake every 200000 ns, 20000 cycles
 *
 *   [SCHED_OTHER ] wake error  p50=3500  p99=28000  p99.9=140000  max=1900000 ns
 *   [SCHED_FIFO  ] wake error  p50=1800  p99=6200   p99.9=14000   max=52000 ns
 *
 * isolcpus=1 nohz_full=1 pe SCHED_FIFO: max ~8000-12000 ns tak gir sakta.
 * WSL2 pe timer behaviour host pe depend karta -- numbers vahan bharosemand nahi.
 * ============================================================ */
