// mh_order_manager.hpp
// ============================================================
// PROJECT 10 -- Order Manager (OMS) + Execution Simulator.
//
// OMS: har outbound order ka lifecycle track karta -- New -> Acked ->
// PartiallyFilled -> Filled | Rejected | Cancelled. Per-order record ek
// generation-checked ObjectPool<OmsOrder> se aata (stale ack/fill guard).
//
// ExecutionSimulator: "the exchange." Ek MatchingEngine (folder 40 --
// tested + fuzzed) andar rakhta jo market ki resting liquidity MIRROR
// karta (wahi L3 add/cancel stream jo book ko feed hota). Hamara order
// isi engine mein aggressive order ki tarah jaata -> trades -> fills ->
// OMS + risk ko wapas.
// ============================================================
#pragma once

#include "mh_object_pool.hpp"
#include "mh_risk_engine.hpp"
#include "mh_types.hpp"

#include <unordered_map>
#include <utility>
#include <vector>

namespace mhft {

enum class OmsState : std::uint8_t { New, Acked, PartiallyFilled, Filled, Rejected, Cancelled };

inline const char* to_str(OmsState s) {
    switch (s) {
        case OmsState::New:             return "New";
        case OmsState::Acked:           return "Acked";
        case OmsState::PartiallyFilled: return "PartiallyFilled";
        case OmsState::Filled:          return "Filled";
        case OmsState::Rejected:        return "Rejected";
        case OmsState::Cancelled:       return "Cancelled";
    }
    return "?";
}

struct OmsOrder {
    ClientOrderId cl_id     = 0;
    Side          side      = Side::Buy;
    Price         px        = 0;
    Qty           orig_qty  = 0;
    Qty           filled    = 0;
    OmsState      state     = OmsState::New;
    Ts            submit_ts = 0;
};

// ---------------------------------------------------------------
//  OrderManager
// ---------------------------------------------------------------
class OrderManager {
public:
    explicit OrderManager(std::size_t reserve_n = 4096) : pool_(reserve_n) {}

    ClientOrderId submit(const OrderRequest& r) {
        const ClientOrderId id = ++next_cl_id_;
        auto h = pool_.acquire();
        OmsOrder* o = pool_.get(h);
        o->cl_id = id; o->side = r.side; o->px = r.px;
        o->orig_qty = r.qty; o->filled = 0;
        o->state = OmsState::New; o->submit_ts = r.ts;
        index_.emplace(id, h);
        ++live_;
        return id;
    }

    void on_ack(ClientOrderId id) {
        if (OmsOrder* o = find(id); o && o->state == OmsState::New) o->state = OmsState::Acked;
    }
    void on_reject(ClientOrderId id) {
        if (OmsOrder* o = find(id)) { o->state = OmsState::Rejected; retire(id); }
    }
    // returns the OmsOrder snapshot after applying the fill (for risk/pnl)
    void on_fill(ClientOrderId id, Qty q) {
        OmsOrder* o = find(id);
        if (!o) return;
        o->filled = static_cast<Qty>(o->filled + q);
        o->state  = (o->filled >= o->orig_qty) ? OmsState::Filled : OmsState::PartiallyFilled;
        if (o->state == OmsState::Filled) retire(id);
    }
    void on_unfilled_cancel(ClientOrderId id) {   // IOC remainder voided
        OmsOrder* o = find(id);
        if (!o) return;
        o->state = (o->filled > 0) ? OmsState::PartiallyFilled : OmsState::Cancelled;
        retire(id);
    }

    const OmsOrder* get(ClientOrderId id) const {
        auto it = index_.find(id);
        return it == index_.end() ? nullptr : pool_.get(it->second);
    }
    std::size_t live() const { return live_; }

    // accounting: every submitted order eventually leaves 'live'
    bool all_settled() const { return live_ == 0; }

private:
    OmsOrder* find(ClientOrderId id) {
        auto it = index_.find(id);
        return it == index_.end() ? nullptr : pool_.get(it->second);
    }
    void retire(ClientOrderId id) {
        auto it = index_.find(id);
        if (it == index_.end()) return;
        pool_.release(it->second);
        index_.erase(it);
        --live_;
    }

    ObjectPool<OmsOrder>                             pool_;
    std::unordered_map<ClientOrderId, ObjectPool<OmsOrder>::Handle> index_;
    ClientOrderId next_cl_id_ = 0;
    std::size_t   live_       = 0;
};

// ---------------------------------------------------------------
//  ExecutionSimulator -- wraps folder 40's MatchingEngine as "the venue"
// ---------------------------------------------------------------
class ExecutionSimulator {
public:
    // mirror one market-data event into the venue's resting book
    void on_market_event(const MdMessage& m) {
        switch (m.type) {
            case MdType::Add: {
                Order o = make_limit(m.order_id, kMarketParticipant,
                                     is_buy(m.side), m.px, m.qty);
                engine_.submit(std::move(o));       // non-crossing by construction
                break;
            }
            case MdType::Cancel:
                engine_.cancel(m.order_id);
                break;
            case MdType::Trade:
                // sim doesn't emit these; a real venue feed would need
                // qty reduction here. left as a no-op for the sim.
                break;
        }
    }

    // send our order to the venue. returns fills; `filled_qty` out-param.
    std::vector<Fill> send(const OrderRequest& r, ClientOrderId cl_id, Qty& filled_qty) {
        Order o = to_engine_order(r, us_id_base_ + (++us_seq_));
        SubmitResult res = engine_.submit(std::move(o));
        std::vector<Fill> fills;
        filled_qty = 0;
        for (const Trade& t : res.trades) {
            fills.push_back(Fill{cl_id, r.side, t.price, t.qty, r.ts});
            filled_qty = static_cast<Qty>(filled_qty + t.qty);
        }
        last_status_ = res.status;
        return fills;
    }

    OrderStatus last_status() const { return last_status_; }
    const MatchingEngine& engine() const { return engine_; }

private:
    MatchingEngine engine_;
    OrderId        us_id_base_ = 1'000'000'000ULL;
    OrderId        us_seq_     = 0;
    OrderStatus    last_status_ = OrderStatus::New;
};

}  // namespace mhft
