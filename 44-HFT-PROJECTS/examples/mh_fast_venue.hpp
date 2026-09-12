// mh_fast_venue.hpp
// ============================================================
// The OPTIMIZED execution venue -- drop-in replacement for
// ExecutionSimulator (which wraps folder 40's std::map MatchingEngine).
//
// Profile (11_mini_hft_engine.cpp) ne dikhaya: "book" stage ka bada hissa
// `ExecutionSimulator::on_market_event` hai -- har market message pe ek
// std::map insert/erase + std::list node alloc (40's V1 storage).
//
// FastVenue = flat-array aggregate book (39/43 V3) + per-level FIFO
// (price-time priority: best price first, FIFO within a price). O(1)-ish
// apply, O(orders-swept) send. Koi tree, koi per-message heap alloc.
//
// Semantics folder-40 ke MatchingEngine se MATCH karte (marketable IOC:
// best se limit price tak fill, maker's price pe, remainder void) -->
// 12_integration_tests.cpp isko byte-compare karta (43/01 correctness gate).
// ============================================================
#pragma once

#include "mh_types.hpp"

#include <array>
#include <vector>

namespace mhft {

class FastVenue {
public:
    static constexpr Price       kBase   = 9000;
    static constexpr std::size_t kLevels = 2048;
    static constexpr std::size_t kNoBid  = static_cast<std::size_t>(-1);

    FastVenue() { fifo_.resize(kLevels); }

    void on_market_event(const MdMessage& m) {
        const std::size_t idx = lvl(m.px);
        switch (m.type) {
            case MdType::Add: {
                arr(m.side)[idx] += static_cast<std::int64_t>(m.qty);
                fifo_[idx].push_back(Entry{m.order_id, static_cast<std::int32_t>(m.qty), m.side});
                ensure_id(m.order_id);
                loc_[static_cast<std::size_t>(m.order_id)] =
                    Loc{static_cast<std::int32_t>(idx), true};
                if (m.side == Side::Buy) { if (bb_ == kNoBid || idx > bb_) bb_ = idx; }
                else                     { if (idx < ba_) ba_ = idx; }
                break;
            }
            case MdType::Cancel:
            case MdType::Trade: {
                if (m.order_id >= loc_.size() || !loc_[static_cast<std::size_t>(m.order_id)].live) break;
                const std::size_t li = static_cast<std::size_t>(loc_[static_cast<std::size_t>(m.order_id)].idx);
                auto& v = fifo_[li];
                for (std::size_t k = 0; k < v.size(); ++k) {
                    if (v[k].id != m.order_id) continue;
                    std::int32_t take = v[k].qty;
                    if (m.type == MdType::Trade && static_cast<std::int32_t>(m.qty) < take)
                        take = static_cast<std::int32_t>(m.qty);
                    arr(v[k].side)[li] -= take;
                    v[k].qty -= take;
                    const Side sd = v[k].side;
                    if (v[k].qty <= 0) { v.erase(v.begin() + static_cast<std::ptrdiff_t>(k));
                                         loc_[static_cast<std::size_t>(m.order_id)].live = false; }
                    if (arr(sd)[li] <= 0) {
                        arr(sd)[li] = 0;
                        if (sd == Side::Buy  && li == bb_) rewalk_bb();
                        if (sd == Side::Sell && li == ba_) rewalk_ba();
                    }
                    break;
                }
                break;
            }
        }
        resolve_cross();
    }

    // our marketable IOC: sweep the opposite side, best price -> limit, FIFO.
    std::vector<Fill> send(const OrderRequest& r, ClientOrderId cl_id, Qty& filled_qty) {
        std::vector<Fill> fills;
        filled_qty = 0;
        std::int64_t rem = static_cast<std::int64_t>(r.qty);
        const std::size_t limit = lvl(r.px);

        if (is_buy(r.side)) {
            std::size_t i = ba_;
            while (rem > 0 && i < kLevels && i <= limit) {
                auto& v = fifo_[i];
                while (rem > 0 && !v.empty()) {
                    Entry& e = v.front();
                    const std::int64_t take = (e.qty < rem) ? e.qty : rem;
                    fills.push_back(Fill{cl_id, Side::Buy, static_cast<Price>(i) + kBase,
                                         static_cast<Qty>(take), r.ts});
                    e.qty -= static_cast<std::int32_t>(take);
                    ask_[i] -= take; rem -= take;
                    filled_qty = static_cast<Qty>(filled_qty + take);
                    if (e.qty <= 0) { loc_[static_cast<std::size_t>(e.id)].live = false;
                                      v.erase(v.begin()); }
                }
                if (v.empty()) ++i;
            }
            rewalk_ba();
        } else {
            std::size_t i = bb_;
            while (rem > 0 && i != kNoBid && i >= limit) {
                auto& v = fifo_[i];
                while (rem > 0 && !v.empty()) {
                    Entry& e = v.front();
                    const std::int64_t take = (e.qty < rem) ? e.qty : rem;
                    fills.push_back(Fill{cl_id, Side::Sell, static_cast<Price>(i) + kBase,
                                         static_cast<Qty>(take), r.ts});
                    e.qty -= static_cast<std::int32_t>(take);
                    bid_[i] -= take; rem -= take;
                    filled_qty = static_cast<Qty>(filled_qty + take);
                    if (e.qty <= 0) { loc_[static_cast<std::size_t>(e.id)].live = false;
                                      v.erase(v.begin()); }
                }
                if (v.empty()) { if (i == 0) break; --i; }
            }
            rewalk_bb();
        }

        last_status_ = (rem == 0) ? OrderStatus::Filled : OrderStatus::Cancelled;
        return fills;
    }

    OrderStatus last_status() const { return last_status_; }

private:
    struct Entry { OrderId id; std::int32_t qty; Side side; };
    struct Loc   { std::int32_t idx = 0; bool live = false; };

    static std::size_t lvl(Price px) {
        Price i = px - kBase;
        if (i < 0) i = 0;
        if (i >= static_cast<Price>(kLevels)) i = static_cast<Price>(kLevels) - 1;
        return static_cast<std::size_t>(i);
    }
    std::array<std::int64_t, kLevels>& arr(Side s) { return s == Side::Buy ? bid_ : ask_; }
    void ensure_id(OrderId id) {
        if (id >= loc_.size()) loc_.resize(static_cast<std::size_t>(id) + 1);
    }
    void rewalk_bb() {
        std::size_t i = (bb_ == kNoBid || bb_ >= kLevels) ? kLevels - 1 : bb_;
        while (i != kNoBid && bid_[i] == 0) --i;
        bb_ = i;
    }
    void rewalk_ba() {
        std::size_t i = (ba_ >= kLevels) ? 0 : ba_;
        while (i < kLevels && ask_[i] == 0) ++i;
        ba_ = i;
    }
    void resolve_cross() {
        while (bb_ != kNoBid && ba_ < kLevels && bb_ >= ba_) {
            if (bid_[bb_] <= ask_[ba_]) { clear_level(bb_, Side::Buy);  rewalk_bb(); }
            else                        { clear_level(ba_, Side::Sell); rewalk_ba(); }
        }
    }
    void clear_level(std::size_t i, Side s) {
        for (Entry& e : fifo_[i]) loc_[static_cast<std::size_t>(e.id)].live = false;
        fifo_[i].clear();
        arr(s)[i] = 0;
    }

    std::array<std::int64_t, kLevels> bid_{};
    std::array<std::int64_t, kLevels> ask_{};
    std::vector<std::vector<Entry>>   fifo_;      // per-level FIFO queue
    std::vector<Loc>                  loc_;       // id -> level + live flag
    std::size_t bb_ = kNoBid;
    std::size_t ba_ = kLevels;
    OrderStatus last_status_ = OrderStatus::New;
};

}  // namespace mhft
