// mh_engine.hpp
// ============================================================
// PROJECT 11 -- THE MINI HFT ENGINE. Sab kuch jodo:
//
//   MarketData --wire--> Parser --> Book --> Strategy --> Risk --> OMS
//        │                                                          │
//        └────────────>  Venue  <───────────────────────────────────┘
//                          │
//                       fills ──> OMS + Risk + PnL
//
// Single-threaded, fully deterministic (koi wall clock nahi -- sab
// event-timestamp driven). Har stage rdtsc se timed -> per-stage latency
// budget (37/14, 43/15).
//
// `Venue` template parameter = execution backend:
//   ExecutionSimulator -- correct/simple: folder 40 ka std::map MatchingEngine
//   FastVenue          -- optimized: flat-array aggregate book + IOC sweep
// Dono ka SAME fills dena chahiye (12_integration_tests isko assert karta).
// Yeh capstone ka before/after hai (43 methodology).
// ============================================================
#pragma once

#include "mh_engine_common.hpp"
#include "mh_fast_venue.hpp"

namespace mhft {

template <class Venue>
class MiniHftEngine {
public:
    struct Stats {
        std::uint64_t messages    = 0;
        std::uint64_t signals     = 0;    // strategy decisions to act
        std::uint64_t orders_ok   = 0;    // passed risk
        std::uint64_t rate_drops  = 0;    // risk rate-limited
        std::uint64_t risk_rejects = 0;   // risk hard-rejected (fat-finger/collar/pos)
        std::uint64_t fills       = 0;
        std::uint64_t filled_qty  = 0;
        std::int64_t  position    = 0;
        std::int64_t  realized_pnl = 0;   // scaled by kPxScale (cash flow)
        std::int64_t  gap_count   = 0;
        double t_parse = 0, t_book = 0, t_strat = 0, t_risk = 0, t_oms = 0;
        std::vector<double> tick_ns;      // end-to-end per-message (ns)
    };

    explicit MiniHftEngine(std::uint64_t seed = 44,
                           SpreadCrossStrategy::Config sc = SpreadCrossStrategy::Config{},
                           RiskEngine::Config rc = RiskEngine::Config{})
        : sim_(seed), strat_(sc), risk_(rc) {}

    Stats run(std::uint64_t n_messages, bool collect_dist = true) {
        sim_.reset(44);
        sim_.set_limit(n_messages);
        book_.resize_ids(3 * n_messages + 16);

        Stats st;
        if (collect_dist) st.tick_ns.reserve(n_messages);

        MdMessage md, p{};
        std::array<std::uint8_t, kWireSize> frame{};
        Seq expect = 1;

        while (sim_.next(md)) {
            const std::uint64_t t0 = tsc();

            // ---- stage: wire encode + parse (zero-copy path) ----
            encode(md, frame.data());
            parse_v3(frame.data(), p);
            const std::uint64_t t1 = tsc();

            if (p.seq != expect) st.gap_count += 1;
            expect = p.seq + 1;

            // ---- stage: book + venue mirror ----
            book_.apply(p);
            venue_.on_market_event(p);
            const std::uint64_t t2 = tsc();

            // ---- stage: strategy ----
            const StrategyDecision d = strat_.on_book(book_);
            const std::uint64_t t3 = tsc();

            std::uint64_t t4 = t3, t5 = t3;
            if (d.act) {
                st.signals += 1;
                OrderRequest req;
                req.side = d.side; req.type = d.type;
                req.px = d.px; req.qty = d.qty; req.ts = p.ts;

                // ---- stage: risk ----
                const RiskVerdict v = risk_.check(req, book_.mid2(), p.ts);
                t4 = tsc();

                if (v == RiskVerdict::Ok) {
                    st.orders_ok += 1;
                    // ---- stage: OMS + execution ----
                    const ClientOrderId cl = oms_.submit(req);
                    oms_.on_ack(cl);
                    Qty filled = 0;
                    fills_.clear();
                    fills_ = venue_.send(req, cl, filled);
                    for (const Fill& f : fills_) {
                        oms_.on_fill(cl, f.qty);
                        risk_.on_fill(f);
                        cash_ -= (is_buy(f.side) ? 1 : -1) *
                                 static_cast<std::int64_t>(f.qty) * f.px;
                        st.fills += 1;
                        st.filled_qty += f.qty;
                    }
                    if (venue_.last_status() != OrderStatus::Filled)
                        oms_.on_unfilled_cancel(cl);
                    strat_.note_fired();
                    t5 = tsc();
                } else if (v == RiskVerdict::RejRateLimit) {
                    st.rate_drops += 1; t5 = t4;
                } else {
                    st.risk_rejects += 1; t5 = t4;
                }
            }

            st.t_parse += static_cast<double>(t1 - t0);
            st.t_book  += static_cast<double>(t2 - t1);
            st.t_strat += static_cast<double>(t3 - t2);
            st.t_risk  += static_cast<double>(t4 - t3);
            st.t_oms   += static_cast<double>(t5 - t4);
            if (collect_dist)
                st.tick_ns.push_back(static_cast<double>(t5 - t0) / g_tpns());
            st.messages += 1;
        }

        st.signals      = strat_.signals();
        st.position     = risk_.position();
        st.realized_pnl = cash_;
        return st;
    }

private:
    MarketDataSimulator sim_;
    L2Book              book_;
    SpreadCrossStrategy strat_;
    RiskEngine          risk_;
    OrderManager        oms_;
    Venue               venue_;
    std::vector<Fill>   fills_;
    std::int64_t        cash_ = 0;
};

using NaiveEngine     = MiniHftEngine<ExecutionSimulator>;   // std::map MatchingEngine venue
using OptimizedEngine = MiniHftEngine<FastVenue>;            // flat-array sweep venue

}  // namespace mhft
