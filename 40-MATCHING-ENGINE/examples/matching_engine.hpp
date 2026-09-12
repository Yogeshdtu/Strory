// matching_engine.hpp
// ============================================================
// Poora matching engine -- price-time-priority matching, Limit/Market/
// IOC/FOK order types, self-trade prevention (STP), aur deterministic
// event sequencing (09/10/11 mein detail).
//
// SCOPE (01-what-is-matching.md): yeh engine market data CONSUME nahi
// karta (woh 38's kaam tha) aur na hi sirf STATE maintain karta (woh
// 39's kaam tha) -- yeh khud DECIDE karta ki kaunsa incoming order
// kis resting order se match karega, aur khud Trade events GENERATE
// karta. Resting-order storage 39's V1 (std::map + std::list +
// unordered_map index) jaisa hai -- SIMPLE, CORRECT, well-understood.
// Woh 3-version storage-optimization story 39 mein already ho chuki;
// is folder ka focus ALGORITHM/SEMANTICS/DETERMINISM hai, storage nahi.
// ============================================================
#pragma once

#include <algorithm>
#include <cstdint>
#include <list>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

using Price         = std::int64_t;   // integer ticks -- 37/08, double NAHI
using Qty           = std::uint32_t;
using OrderId       = std::uint64_t;
using ParticipantId = std::uint32_t;
using TradeId       = std::uint64_t;

// ------------------------------------------------------------
//  Order type aur time-in-force -- 03/04/06.
//
//  DESIGN DECISION: real venues mein order-type (Limit/Market) aur
//  time-in-force (Day/GTC/IOC/FOK) ORTHOGONAL hote (koi bhi combo
//  possible). Hum yahan 4 FLAT types rakhte hain kyunki teaching ke
//  liye simpler hai aur practically sabse common combos yehi hain:
//    Limit  = limit order, GTC-style (fill jitna ho sake, baaki REST)
//    Market = price-limit-less, jitna book de sake utna le, baaki VOID
//    IOC    = limit-price ke saath, jitna turant mile lo, baaki VOID
//    FOK    = limit-price ke saath, POORA turant fill ya bilkul nahi
//  "Market + IOC" ko alag case nahi banaya -- woh semantically plain
//  Market jaisa hi hai (Market kabhi rest nahi karta, IOC bhi nahi).
// ------------------------------------------------------------
enum class OrderType : std::uint8_t { Limit, Market, IOC, FOK };

// Order ka final disposition is submit() call ke baad (12-state-machine).
// NOTE: `status` order ka OUTCOME batata hai, `trades` (SubmitResult mein)
// EXACTLY kya-kitna fill hua batata -- ek "Cancelled" status ke saath bhi
// non-empty trades ho sakte (IOC/Market ka partial fill + remainder void).
enum class OrderStatus : std::uint8_t {
    New,             // resting hai, abhi tak zero fills
    PartiallyFilled, // resting hai, kuch fill ho chuka, baaki resting
    Filled,          // poora fill -- ab book mein nahi
    Cancelled,       // remainder VOID ho gaya (rest nahi hua) -- IOC/Market
                      // leftover, explicit cancel(), ya STP
    Rejected         // matching engine mein ENTER hi nahi hua -- duplicate
                      // id, ya FOK precheck fail
};

// Self-trade prevention mode -- 08-self-trade-prevention.md.
// STP sirf tab fire hota jab incoming.participant == resting.participant.
enum class StpMode : std::uint8_t {
    None,
    CancelNewest,  // incoming (hamesha "newest") cancel -- resting untouched, matching ABORT
    CancelOldest,  // resting (older) cancel/remove, incoming agle order pe CONTINUE try karta
    CancelBoth     // dono cancel -- resting remove, incoming ka remainder bhi VOID, matching ABORT
};

inline const char* to_string(OrderStatus s) {
    switch (s) {
        case OrderStatus::New:             return "New";
        case OrderStatus::PartiallyFilled: return "PartiallyFilled";
        case OrderStatus::Filled:          return "Filled";
        case OrderStatus::Cancelled:       return "Cancelled";
        case OrderStatus::Rejected:        return "Rejected";
    }
    return "?";
}

inline const char* to_string(OrderType t) {
    switch (t) {
        case OrderType::Limit:  return "Limit";
        case OrderType::Market: return "Market";
        case OrderType::IOC:    return "IOC";
        case OrderType::FOK:    return "FOK";
    }
    return "?";
}

// ------------------------------------------------------------
//  Order -- ek incoming request bhi, aur book mein resting entry bhi
//  (07-trade-events, 12-state-machine).
// ------------------------------------------------------------
struct Order {
    OrderId       id           = 0;
    ParticipantId participant  = 0;
    bool          is_buy       = true;
    OrderType     type         = OrderType::Limit;
    Price         price        = 0;   // Market order iska use nahi karta
    Qty           qty          = 0;   // REMAINING qty -- match hote hote ghatta hai
    Qty           orig_qty     = 0;   // original requested qty (fill-ratio ke liye)
    StpMode       stp          = StpMode::None;
    std::uint64_t seq          = 0;   // arrival sequence -- determinism ka core (09/11)
};

// Ek match ka result -- resting order (maker) ki price pe execute hota
// (07's "maker sets the price" convention).
struct Trade {
    TradeId       id;
    OrderId       aggressor_id;
    OrderId       resting_id;
    ParticipantId aggressor_participant;
    ParticipantId resting_participant;
    bool          aggressor_is_buy;
    Price         price;
    Qty           qty;
    std::uint64_t seq;   // global event-timeline position (10-event-sourcing)
};

struct SubmitResult {
    OrderStatus        status;
    std::vector<Trade> trades;
};

// ------------------------------------------------------------
//  Factory helpers -- examples/tests ko verbose Order{} construction se bachate
// ------------------------------------------------------------
inline Order make_limit(OrderId id, ParticipantId p, bool is_buy, Price price, Qty qty,
                         StpMode stp = StpMode::None) {
    Order o; o.id = id; o.participant = p; o.is_buy = is_buy;
    o.type = OrderType::Limit; o.price = price; o.qty = qty; o.stp = stp;
    return o;
}
inline Order make_market(OrderId id, ParticipantId p, bool is_buy, Qty qty,
                          StpMode stp = StpMode::None) {
    Order o; o.id = id; o.participant = p; o.is_buy = is_buy;
    o.type = OrderType::Market; o.price = 0; o.qty = qty; o.stp = stp;
    return o;
}
inline Order make_ioc(OrderId id, ParticipantId p, bool is_buy, Price price, Qty qty,
                       StpMode stp = StpMode::None) {
    Order o; o.id = id; o.participant = p; o.is_buy = is_buy;
    o.type = OrderType::IOC; o.price = price; o.qty = qty; o.stp = stp;
    return o;
}
inline Order make_fok(OrderId id, ParticipantId p, bool is_buy, Price price, Qty qty,
                       StpMode stp = StpMode::None) {
    Order o; o.id = id; o.participant = p; o.is_buy = is_buy;
    o.type = OrderType::FOK; o.price = price; o.qty = qty; o.stp = stp;
    return o;
}

// ============================================================
//  MatchingEngine
// ============================================================
class MatchingEngine {
public:
    // Ek naya order submit karo -- match karega jitna ho sake, phir
    // type ke hisaab se remainder rest/void/reject karega. 12-state-machine
    // aur 02-matching-algorithm mein poora flow.
    SubmitResult submit(Order incoming) {
        incoming.orig_qty = incoming.qty;
        incoming.seq      = next_seq_++;

        if (index_.count(incoming.id)) {
            return {OrderStatus::Rejected, {}};   // duplicate id -- kabhi enter hi nahi hua
        }

        // FOK precheck (06-ioc-and-fok): STP-AWARE -- naive "sum total_qty"
        // GALAT hota agar STP kuch resting orders ko skip/abort karwaata
        // (09/precheck bug story, 06-ioc-and-fok.md mein poora likha hai).
        if (incoming.type == OrderType::FOK) {
            const Qty avail = incoming.is_buy
                ? available_qty(asks_, true, incoming.price, incoming.qty, incoming.participant, incoming.stp)
                : available_qty(bids_, false, incoming.price, incoming.qty, incoming.participant, incoming.stp);
            if (avail < incoming.qty) {
                return {OrderStatus::Rejected, {}};
            }
        }

        std::vector<Trade> trades;
        bool stp_aborted = false;
        if (incoming.is_buy) match_against(incoming, asks_, trades, stp_aborted);
        else                 match_against(incoming, bids_, trades, stp_aborted);

        if (incoming.qty == 0) {
            return {OrderStatus::Filled, std::move(trades)};
        }
        if (incoming.type == OrderType::Limit && !stp_aborted) {
            add_resting(incoming);
            return {trades.empty() ? OrderStatus::New : OrderStatus::PartiallyFilled, std::move(trades)};
        }
        // Market / IOC / STP-aborted-Limit -- remainder REST nahi hota, VOID
        return {OrderStatus::Cancelled, std::move(trades)};
    }

    bool cancel(OrderId id) {
        auto idx_it = index_.find(id);
        if (idx_it == index_.end()) return false;
        const bool is_buy = idx_it->second.is_buy;
        // ternary se side reference NAHI (bids_/asks_ alag map types --
        // 39's recurring trap). Templated impl + explicit dispatch.
        return is_buy ? cancel_impl(bids_, idx_it) : cancel_impl(asks_, idx_it);
    }

    // Replace = purana cancel (gone ho to ignore, fuzzing/late-cancel case)
    // + naya submit, same-id-chain semantics jaisa 38/39.
    SubmitResult replace(OrderId old_id, Order new_order) {
        cancel(old_id);
        return submit(std::move(new_order));
    }

    bool has_bid() const { return !bids_.empty(); }
    bool has_ask() const { return !asks_.empty(); }
    Price best_bid() const { return bids_.begin()->first; }        // precondition: has_bid()
    Price best_ask() const { return asks_.begin()->first; }        // precondition: has_ask()
    Qty best_bid_qty() const { return bids_.begin()->second.total_qty; }
    Qty best_ask_qty() const { return asks_.begin()->second.total_qty; }
    std::size_t resting_count() const { return index_.size(); }

    // Test/debug query -- resting order ki current (remaining-qty) state.
    // nullptr agar id resting nahi hai (kabhi tha hi nahi, ya fill/cancel
    // ho chuka).
    const Order* find_resting(OrderId id) const {
        auto it = index_.find(id);
        if (it == index_.end()) return nullptr;
        return &(*it->second.it);
    }

    std::vector<OrderId> ids_at_price(bool is_buy, Price price) const {
        std::vector<OrderId> result;
        if (is_buy) {
            auto it = bids_.find(price);
            if (it != bids_.end())
                for (const auto& o : it->second.orders) result.push_back(o.id);
        } else {
            auto it = asks_.find(price);
            if (it != asks_.end())
                for (const auto& o : it->second.orders) result.push_back(o.id);
        }
        return result;
    }

private:
    struct PriceLevel { std::list<Order> orders; Qty total_qty = 0; };
    struct Location { bool is_buy; Price price; std::list<Order>::iterator it; };
    using IndexIt = std::unordered_map<OrderId, Location>::iterator;

    // --------------------------------------------------------
    //  Core matching loop -- best opposite level se shuru, price cross
    //  hone tak (Market ke liye hamesha) FIFO order se match karta.
    //  02-matching-algorithm.md mein poora walkthrough.
    // --------------------------------------------------------
    template <class OppMap>
    void match_against(Order& incoming, OppMap& opp_side, std::vector<Trade>& trades, bool& stp_aborted) {
        while (incoming.qty > 0 && !opp_side.empty()) {
            auto lvl_it = opp_side.begin();
            const Price lvl_price = lvl_it->first;

            if (incoming.type != OrderType::Market) {
                const bool crosses = incoming.is_buy ? (incoming.price >= lvl_price)
                                                       : (incoming.price <= lvl_price);
                if (!crosses) break;   // best opposite price ab incoming se cross nahi karti
            }

            PriceLevel& level = lvl_it->second;
            auto ord_it = level.orders.begin();
            while (incoming.qty > 0 && ord_it != level.orders.end()) {
                Order& resting = *ord_it;

                if (incoming.stp != StpMode::None && incoming.participant == resting.participant) {
                    if (incoming.stp == StpMode::CancelOldest) {
                        // Resting (older) hatao, incoming AGLE order pe try karta rahega.
                        const OrderId dead_id  = resting.id;
                        const Qty     dead_qty = resting.qty;
                        ord_it = level.orders.erase(ord_it);
                        level.total_qty -= dead_qty;
                        index_.erase(dead_id);
                        continue;
                    }
                    if (incoming.stp == StpMode::CancelBoth) {
                        const OrderId dead_id  = resting.id;
                        const Qty     dead_qty = resting.qty;
                        level.orders.erase(ord_it);
                        level.total_qty -= dead_qty;
                        index_.erase(dead_id);
                        if (level.orders.empty()) opp_side.erase(lvl_it);
                        stp_aborted = true;
                        return;
                    }
                    // CancelNewest -- incoming (hamesha "newest") cancel; resting UNTOUCHED.
                    stp_aborted = true;
                    return;
                }

                const Qty fill_qty = std::min(incoming.qty, resting.qty);
                incoming.qty -= fill_qty;
                resting.qty  -= fill_qty;
                level.total_qty -= fill_qty;

                trades.push_back(Trade{
                    next_trade_id_++,
                    incoming.id, resting.id,
                    incoming.participant, resting.participant,
                    incoming.is_buy,
                    lvl_price,          // maker (resting) ki price -- 07 convention
                    fill_qty,
                    next_seq_++
                });

                if (resting.qty == 0) {
                    const OrderId dead_id = resting.id;
                    ord_it = level.orders.erase(ord_it);
                    index_.erase(dead_id);
                } else {
                    ++ord_it;   // partial fill -- resting apni jagah (front) pe rehta
                }
            }

            if (level.orders.empty()) opp_side.erase(lvl_it);
        }
    }

    // FOK precheck: kitna qty ACTUALLY match ho sakta agar abhi match_against
    // chalta -- STP-mode-AWARE (naive "sum total_qty" precheck STP ke saath
    // combine hone pe galat guarantee de sakta -- 06-ioc-and-fok.md).
    template <class OppMap>
    static Qty available_qty(const OppMap& opp_side, bool is_buy, Price price, Qty target,
                              ParticipantId self, StpMode stp) {
        Qty sum = 0;
        for (const auto& kv : opp_side) {
            const Price lvl_price = kv.first;
            const bool crosses = is_buy ? (price >= lvl_price) : (price <= lvl_price);
            if (!crosses) break;
            const PriceLevel& level = kv.second;
            if (stp == StpMode::None) {
                sum += level.total_qty;
            } else {
                for (const auto& o : level.orders) {
                    if (o.participant == self) {
                        if (stp == StpMode::CancelOldest) continue;   // skip, aage badhta
                        return sum;   // CancelNewest/CancelBoth -- yahin match_against ABORT karega
                    }
                    sum += o.qty;
                }
            }
            if (sum >= target) break;
        }
        return sum;
    }

    template <class Map>
    void add_resting_impl(Map& side, const Order& o) {
        auto& level = side[o.price];
        level.orders.push_back(o);
        level.total_qty += o.qty;
        auto it = std::prev(level.orders.end());
        index_.emplace(o.id, Location{o.is_buy, o.price, it});
    }
    void add_resting(const Order& o) {
        if (o.is_buy) add_resting_impl(bids_, o); else add_resting_impl(asks_, o);
    }

    template <class Map>
    bool cancel_impl(Map& side, IndexIt idx_it) {
        Location& loc = idx_it->second;
        auto lvl_it = side.find(loc.price);
        if (lvl_it == side.end()) return false;
        PriceLevel& level = lvl_it->second;
        level.total_qty -= loc.it->qty;
        level.orders.erase(loc.it);
        if (level.orders.empty()) side.erase(lvl_it);
        index_.erase(idx_it);
        return true;
    }

    std::map<Price, PriceLevel, std::greater<Price>> bids_;   // best = begin()
    std::map<Price, PriceLevel>                      asks_;   // best = begin()
    std::unordered_map<OrderId, Location>            index_;

    std::uint64_t next_seq_      = 1;   // shared event timeline -- orders AUR trades dono (10)
    std::uint64_t next_trade_id_ = 1;
};
