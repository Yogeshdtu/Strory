// 07_spsc_queue.cpp
// ============================================================
// PROJECT 7 -- SPSC ring buffer, in the capstone's shoes. Folder 41 ne
// isko banaya + benchmark kiya; yahan hum ise mini-engine ka pehla stage
// hand-off model banate: ek "wire reader" thread MdMessage bytes ko
// dusre "engine" thread ko deta.
//
//   1. throughput  : blast pass -- M msg/s
//   2. hand-off latency : paced pass -- queue shallow -> pure hand-off
//   3. correctness : consumer ne PRODUCED sequence exactly (no loss/dup/reorder)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 07_spsc_queue.cpp -o q && ./q
// ============================================================

#include "mh_engine_common.hpp"
#include "../../41-HFT-CONCURRENCY/examples/spsc_queue.hpp"

#include <atomic>
#include <cstdio>
#include <thread>

using namespace mhft;

struct Slot { MdMessage m; std::uint64_t t_tsc; };

constexpr std::size_t   kCap  = 1u << 12;
constexpr std::uint64_t kBlast = 8'000'000;
constexpr std::uint64_t kPaced = 400'000;
constexpr std::uint64_t kPaceTicks = 4000;   // ~2us

int main() {
    std::printf("ticks_per_ns = %.4f\n\n", g_tpns());

    static SpscQueue<Slot, kCap> q;
    std::atomic<bool> done{false};
    std::vector<double> lat; lat.reserve(kPaced);
    std::uint64_t consumed = 0;
    Seq expect = 1;
    bool order_ok = true;

    std::thread consumer([&] {
        Slot s;
        while (!done.load(std::memory_order_relaxed)) {
            if (q.try_pop(s)) {
                ++consumed;
                if (s.m.seq != expect) order_ok = false;
                expect = s.m.seq + 1;
                lat.push_back(static_cast<double>(tsc() - s.t_tsc) / g_tpns());
            }
        }
        while (q.try_pop(s)) {
            ++consumed;
            if (s.m.seq != expect) order_ok = false;
            expect = s.m.seq + 1;
        }
    });

    MarketDataSimulator sim(44);
    sim.set_limit(kBlast + kPaced);
    MdMessage m;

    // ---- blast pass ----
    const auto t0 = std::chrono::steady_clock::now();
    for (std::uint64_t i = 0; i < kBlast; ++i) {
        if (!sim.next(m)) break;
        Slot s{m, 0};
        while (!q.try_push(s)) {}
    }
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    lat.clear();

    // ---- paced pass ----
    std::uint64_t next = tsc();
    for (std::uint64_t i = 0; i < kPaced; ++i) {
        if (!sim.next(m)) break;
        next += kPaceTicks;
        while (tsc() < next) {}
        Slot s{m, tsc()};
        while (!q.try_push(s)) {}
    }
    while (lat.size() < kPaced - 1) {}
    done.store(true, std::memory_order_relaxed);
    consumer.join();

    std::sort(lat.begin(), lat.end());
    std::printf("throughput (blast, %llu msgs) : %.1f M msg/s\n",
                (unsigned long long)kBlast, static_cast<double>(kBlast) / sec / 1e6);
    std::printf("hand-off latency (paced)      : p50 %.1f   p99 %.1f   p99.9 %.1f   max %.1f  ns\n",
                pct(lat, 50), pct(lat, 99), pct(lat, 99.9), lat.empty() ? 0.0 : lat.back());
    std::printf("consumer saw %llu msgs, in-order: %s\n",
                (unsigned long long)consumed, order_ok ? "yes" : "NO");

    return order_ok ? 0 : 1;
}
