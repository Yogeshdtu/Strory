// mh_types.hpp
// ============================================================
// Mini HFT engine ke SHARED types. Folder 40 ki matching_engine.hpp
// ko reuse karte hain (Price/Qty/OrderId/ParticipantId + MatchingEngine
// khud) -- capstone ka poora point yehi hai: "jodkar" banao, dobara
// mat likho. Yahan sirf woh types add karte hain jo pipeline ke baaki
// stages (market data, strategy, risk, OMS) ke liye chahiye.
//
// Price = integer ticks (scale 100 = 0.01 tick) -- 37/08, 43/09. Koi float nahi.
// ============================================================
#pragma once

#include "../../40-MATCHING-ENGINE/examples/matching_engine.hpp"   // Price, Qty, OrderId, ...

#include <cstdint>
#include <cstdio>

namespace mhft {

using Ts            = std::uint64_t;   // nanoseconds since sim epoch
using Seq           = std::uint64_t;   // exchange sequence number
using ClientOrderId = std::uint64_t;   // our own order id (OMS)

constexpr std::int64_t kPxScale = 100;              // 0.01 tick -> scale 100
constexpr ParticipantId kMarketParticipant = 0;    // "the market" (resting liquidity)
constexpr ParticipantId kUsParticipant     = 1;    // us

enum class Side : std::uint8_t { Buy, Sell };
inline bool  is_buy(Side s)  { return s == Side::Buy; }
inline Side  opposite(Side s){ return s == Side::Buy ? Side::Sell : Side::Buy; }
inline const char* to_str(Side s) { return s == Side::Buy ? "BUY " : "SELL"; }

// ---- market-data event (L3 incremental): add / cancel / trade ----
enum class MdType : std::uint8_t { Add, Cancel, Trade };

struct MdMessage {
    Seq      seq  = 0;              // gap detection (38/04)
    Ts       ts   = 0;             // exchange send timestamp
    OrderId  order_id = 0;         // Add: new id; Cancel: id to remove; Trade: resting id hit
    Price    px   = 0;             // integer ticks
    MdType   type = MdType::Add;
    Side     side = Side::Buy;     // Add/Trade: side of the resting order; Cancel: same
    Qty      qty  = 0;
};
static_assert(sizeof(MdMessage) <= 48, "keep the MD message small (38/05)");

// ---- our outbound order request (strategy -> risk -> OMS) ----
struct OrderRequest {
    ClientOrderId cl_id = 0;
    Side          side  = Side::Buy;
    OrderType     type  = OrderType::IOC;   // strategy uses Limit / IOC
    Price         px    = 0;
    Qty           qty   = 0;
    Ts            ts    = 0;                // decision timestamp
};

// ---- a fill (execution) coming back from the venue ----
struct Fill {
    ClientOrderId cl_id = 0;
    Side          side  = Side::Buy;
    Price         px    = 0;
    Qty           qty   = 0;
    Ts            ts    = 0;
};

// ---- fixed-point price <-> ASCII (no float -- 43/09) ----
inline Price px_parse(const char* s) {
    bool neg = false;
    if (*s == '-') { neg = true; ++s; }
    Price whole = 0;
    for (; *s && *s != '.'; ++s) whole = whole * 10 + (*s - '0');
    Price frac = 0;
    if (*s == '.' && s[1] && s[2]) frac = (s[1] - '0') * 10 + (s[2] - '0');
    const Price v = whole * kPxScale + frac;
    return neg ? -v : v;
}
inline int px_format(Price p, char* buf, std::size_t n) {
    const char* sign = (p < 0) ? "-" : "";
    const Price a = (p < 0) ? -p : p;
    return std::snprintf(buf, n, "%s%lld.%02lld", sign,
                         static_cast<long long>(a / kPxScale),
                         static_cast<long long>(a % kPxScale));
}

// convert our Side + an OrderRequest into folder-40's Order
inline Order to_engine_order(const OrderRequest& r, OrderId engine_id) {
    Order o;
    o.id          = engine_id;
    o.participant = kUsParticipant;
    o.is_buy      = is_buy(r.side);
    o.type        = r.type;
    o.price       = r.px;
    o.qty         = r.qty;
    return o;
}

}  // namespace mhft
