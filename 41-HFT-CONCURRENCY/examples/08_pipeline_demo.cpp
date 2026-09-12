// 08_pipeline_demo.cpp
// ============================================================
// CAPSTONE: a real thread-per-stage, shared-nothing HFT pipeline --
// this folder's SPSC queue (04) carrying 40-MATCHING-ENGINE's actual
// MatchingEngine (not a toy stand-in) between two more stages, with
// end-to-end latency measured from "order enters stage 1" to "trade
// reaches stage 3." (01-hft-threading-model.md, 13-timing-and-
// sequencing.md)
//
//   Stage 1 (feed)       Stage 2 (match)         Stage 3 (sink)
//   generates orders  -> SpscQueue<Order>     -> SpscQueue<Trade>   -> accumulate
//   (own thread)         MatchingEngine::submit()  (own thread)        end-to-end
//                        (own thread, single-        latency stats
//                        writer of the engine)
//
// Each arrow is a DIFFERENT thread; no stage shares mutable state with
// another except through its SPSC queue -- 02/03's shared-nothing,
// thread-per-stage principle, actually built, not just described.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 08_pipeline_demo.cpp -o pipeline && ./pipeline
// ============================================================

#include "spsc_queue.hpp"
#include "../../40-MATCHING-ENGINE/examples/matching_engine.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <random>
#include <thread>
#include <vector>
#include <x86intrin.h>

namespace ch = std::chrono;
static inline std::uint64_t tsc() { _mm_lfence(); auto t = __rdtsc(); _mm_lfence(); return t; }
static double g_tpns = 1.0;

struct PipelineOrder { std::uint64_t t_tsc; Order order; };
struct PipelineTrade { std::uint64_t t_tsc; Trade trade; };

constexpr std::size_t   kCapA = 1u << 12;
constexpr std::size_t   kCapB = 1u << 14;
constexpr std::uint64_t kN    = 500'000;   // orders fed into stage 1
constexpr std::uint64_t kPaceTicks = 20000; // ~10us @ ~2GHz between orders --
// paced BELOW Stage 2's sustained throughput so the pipeline runs in
// steady state, not backlogged (a narrower price range here makes almost
// every order cross and match -- much heavier per-order matching work
// than 40's own Add/Cancel-heavy benchmark mix, so it needs a slower feed
// to stay caught up; see 13-timing-and-sequencing.md for what happens
// when this ISN'T true).

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

    static SpscQueue<PipelineOrder, kCapA> queue_a;   // Stage1 -> Stage2
    static SpscQueue<PipelineTrade, kCapB> queue_b;   // Stage2 -> Stage3

    std::atomic<bool> go{false};
    std::atomic<bool> feed_done{false}, match_done{false};
    std::atomic<std::uint64_t> orders_matched{0}, trades_emitted{0};

    // ============================================================
    //  Stage 2: MATCH -- the ONLY thread that ever touches `engine`
    //  (single-writer principle, 02). Pulls from queue_a, feeds
    //  40-MATCHING-ENGINE's real submit(), forwards resulting trades.
    // ============================================================
    std::thread stage2([&] {
        MatchingEngine engine;
        while (!go.load(std::memory_order_acquire)) {}
        PipelineOrder po;
        std::uint64_t matched = 0;
        for (;;) {
            if (queue_a.try_pop(po)) {
                auto r = engine.submit(po.order);
                for (const auto& t : r.trades) {
                    while (!queue_b.try_push(PipelineTrade{po.t_tsc, t})) {}
                }
                ++matched;
                continue;
            }
            if (feed_done.load(std::memory_order_acquire)) {
                if (queue_a.try_pop(po)) {   // final check -- close the race window
                    auto r = engine.submit(po.order);
                    for (const auto& t : r.trades) {
                        while (!queue_b.try_push(PipelineTrade{po.t_tsc, t})) {}
                    }
                    ++matched;
                    continue;
                }
                break;
            }
        }
        orders_matched.store(matched, std::memory_order_relaxed);
        match_done.store(true, std::memory_order_release);
    });

    // ============================================================
    //  Stage 3: SINK -- accumulates end-to-end latency (order-arrival
    //  timestamp, stamped in stage 1, to trade-received-here).
    // ============================================================
    std::vector<double> e2e_ns;
    e2e_ns.reserve(kN);   // generous upper bound
    std::thread stage3([&] {
        while (!go.load(std::memory_order_acquire)) {}
        PipelineTrade pt;
        std::uint64_t got = 0;
        for (;;) {
            if (queue_b.try_pop(pt)) {
                e2e_ns.push_back(static_cast<double>(tsc() - pt.t_tsc) / g_tpns);
                ++got;
                continue;
            }
            if (match_done.load(std::memory_order_acquire)) {
                if (queue_b.try_pop(pt)) {
                    e2e_ns.push_back(static_cast<double>(tsc() - pt.t_tsc) / g_tpns);
                    ++got;
                    continue;
                }
                break;
            }
        }
        trades_emitted.store(got, std::memory_order_relaxed);
    });

    // ============================================================
    //  Stage 1: FEED -- generates a realistic-ish crossing order flow.
    // ============================================================
    std::mt19937_64 rng(42);   // fixed seed -- reproducible synthetic flow
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> offset_dist(-15, 15);
    std::uniform_int_distribution<int> qty_dist(10, 200);
    OrderId next_id = 1;
    constexpr Price kCenter = 10000;

    go.store(true, std::memory_order_release);
    std::uint64_t next = tsc();
    for (std::uint64_t i = 0; i < kN; ++i) {
        next += kPaceTicks;
        while (tsc() < next) {}
        const bool is_buy = side_dist(rng) != 0;
        const Price price = kCenter + offset_dist(rng);
        const Qty qty = static_cast<Qty>(qty_dist(rng));
        const Order o = make_limit(next_id++, /*participant=*/1, is_buy, price, qty);
        const PipelineOrder po{tsc(), o};
        while (!queue_a.try_push(po)) {}
    }
    feed_done.store(true, std::memory_order_release);

    stage2.join();
    stage3.join();

    std::printf("Fed %llu orders through the pipeline.\n", static_cast<unsigned long long>(kN));
    std::printf("Stage 2 processed %llu orders, emitted %llu trades.\n",
                static_cast<unsigned long long>(orders_matched.load()),
                static_cast<unsigned long long>(trades_emitted.load()));

    std::sort(e2e_ns.begin(), e2e_ns.end());
    if (!e2e_ns.empty()) {
        auto pc = [&](double p) {
            std::size_t idx = static_cast<std::size_t>(p / 100.0 * static_cast<double>(e2e_ns.size()));
            return e2e_ns[std::min(idx, e2e_ns.size() - 1)];
        };
        std::printf("\nEnd-to-end latency (order-arrival in Stage 1 -> trade-received in\n"
                    "Stage 3, crossing 2 SPSC hand-offs + a real MatchingEngine::submit()):\n");
        std::printf("  p50 %6.1f   p99 %8.1f   p99.9 %9.1f   max %10.1f  ns  (n=%zu)\n",
                    pc(50), pc(99), pc(99.9), e2e_ns.back(), e2e_ns.size());
    } else {
        std::printf("\n(no trades were generated -- nothing to report)\n");
    }

    return 0;
}
