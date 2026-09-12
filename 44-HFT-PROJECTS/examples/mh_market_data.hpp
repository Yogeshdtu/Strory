// mh_market_data.hpp
// ============================================================
// PROJECT 1 -- Market Data Simulator.
//
// Deterministic L3 incremental feed: Add / Cancel / Trade events, har ek
// pe monotonic sequence number + exchange timestamp. Mid ek momentum-wala
// random walk hai (43's generator jaisa). Book NON-CROSSING rehta (bids
// hamesha asks se neeche) -- taaki "market" sirf resting liquidity ho,
// aur trades SIRF hamare orders se banein (execution sim, project 10).
//
// Ek binary WIRE encoder bhi (`encode`) -- big-endian packed, 38 bytes --
// jise project 2 (feed parser) parse karega.
// ============================================================
#pragma once

#include "mh_types.hpp"

#include <array>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace mhft {

// ---- deterministic RNG: splitmix64 ----
struct Rng {
    std::uint64_t s;
    explicit Rng(std::uint64_t seed) : s(seed) {}
    std::uint64_t next() {
        std::uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    std::uint32_t below(std::uint32_t n) { return static_cast<std::uint32_t>(next() % n); }
};

// ---- wire format: big-endian packed, 38 bytes ----
constexpr std::size_t kWireSize = 38;

inline void put_u64_be(std::uint8_t* p, std::uint64_t v) {
    for (int i = 7; i >= 0; --i) { p[i] = static_cast<std::uint8_t>(v & 0xFF); v >>= 8; }
}
inline void put_u32_be(std::uint8_t* p, std::uint32_t v) {
    for (int i = 3; i >= 0; --i) { p[i] = static_cast<std::uint8_t>(v & 0xFF); v >>= 8; }
}

// encode one MdMessage -> 38 BE bytes. layout:
//   0 type u8 | 1 side u8 | 2 qty u32 | 6 seq u64 | 14 ts u64 | 22 order_id u64 | 30 px i64
inline void encode(const MdMessage& m, std::uint8_t* out) {
    out[0] = static_cast<std::uint8_t>(m.type);
    out[1] = static_cast<std::uint8_t>(m.side);
    put_u32_be(out + 2,  m.qty);
    put_u64_be(out + 6,  m.seq);
    put_u64_be(out + 14, m.ts);
    put_u64_be(out + 22, m.order_id);
    put_u64_be(out + 30, static_cast<std::uint64_t>(m.px));   // two's complement bits
}

// ============================================================
//  MarketDataSimulator
//
// Ek internal event QUEUE se chalta. Har "step" pe: mid advance karo,
// ek add ya cancel decide karo, AUR koi bhi live order jo ab touch se
// > kMaxDist ticks door hai use CANCEL karo (stale-quote pulling --
// real market makers yahi karte). Isse aggregate book HAMESHA mid ke
// paas tight rehta -> non-crossing (39 ka invariant), BBO sane.
// ============================================================
class MarketDataSimulator {
public:
    explicit MarketDataSimulator(std::uint64_t seed = 44) : rng_(seed) {
        live_.reserve(1u << 16);
        loc_.reserve(1u << 16);
        pending_.reserve(64);
    }

    bool next(MdMessage& m) {
        while (pending_.empty()) {
            if (produced_ >= limit_) return false;
            step();
        }
        m = pending_.front();
        pending_.erase(pending_.begin());
        ++produced_;
        return true;
    }

    void set_limit(std::uint64_t n) { limit_ = n; }
    std::uint64_t produced() const  { return produced_; }
    OrderId       max_order_id() const { return next_id_; }

    void reset(std::uint64_t seed = 44) {
        rng_ = Rng(seed); live_.clear(); loc_.clear(); pending_.clear();
        bcnt_.fill(0); acnt_.fill(0);
        hi_bid_ = -1; lo_ask_ = kSpan;
        mid_ticks_ = 10000; trend_ = 0; trend_left_ = 1;
        ts_ = 0; seq_ = 0; next_id_ = 0; produced_ = 0;
    }

private:
    static constexpr std::int32_t kBase    = 9000;    // 90.00
    static constexpr std::int32_t kSpan    = 2048;    // 90.00 .. 110.47
    static constexpr std::int32_t kMaxDist = 12;      // farther orders get pulled

    struct LiveOrd { Side side; std::int32_t px_i; Qty qty; };   // px_i = px_ticks - kBase

    std::int32_t clamp_lvl(std::int32_t i) const {
        if (i < 0) return 0;
        if (i >= kSpan) return kSpan - 1;
        return i;
    }
    void rescan_hi_bid() {
        std::int32_t i = (hi_bid_ >= 0 && hi_bid_ < kSpan) ? hi_bid_ : kSpan - 1;
        while (i >= 0 && bcnt_[static_cast<std::size_t>(i)] == 0) --i;
        hi_bid_ = i;
    }
    void rescan_lo_ask() {
        std::int32_t i = (lo_ask_ >= 0 && lo_ask_ < kSpan) ? lo_ask_ : 0;
        while (i < kSpan && acnt_[static_cast<std::size_t>(i)] == 0) ++i;
        lo_ask_ = i;
    }

    void emit_cancel(OrderId id) {
        const LiveOrd o = loc_[id];
        MdMessage c;
        c.seq = ++seq_; c.ts = (ts_ += 20 + rng_.below(60));
        c.type = MdType::Cancel; c.side = o.side;
        c.order_id = id; c.px = static_cast<Price>(o.px_i + kBase); c.qty = o.qty;
        pending_.push_back(c);
        const std::size_t li = static_cast<std::size_t>(o.px_i);
        if (o.side == Side::Buy) {
            if (bcnt_[li] > 0) --bcnt_[li];
            if (bcnt_[li] == 0 && o.px_i == hi_bid_) rescan_hi_bid();
        } else {
            if (acnt_[li] > 0) --acnt_[li];
            if (acnt_[li] == 0 && o.px_i == lo_ask_) rescan_lo_ask();
        }
        for (std::size_t i = 0; i < live_.size(); ++i)
            if (live_[i] == id) { live_[i] = live_.back(); live_.pop_back(); break; }
        loc_.erase(id);
    }

    void step() {
        // --- mid process: random walk + periodically-resampled momentum ---
        if (--trend_left_ == 0) {
            trend_      = static_cast<std::int32_t>(rng_.below(7)) - 3;
            trend_left_ = 120 + rng_.below(280);
        }
        const std::int32_t stp  = static_cast<std::int32_t>(rng_.below(5)) - 2;
        const std::int32_t pull = static_cast<std::int32_t>((mid_ticks_ - 10000) / 200);
        mid_ticks_ += stp + trend_ - pull;
        if (mid_ticks_ < 9400)  mid_ticks_ = 9400;
        if (mid_ticks_ > 10600) mid_ticks_ = 10600;
        const std::int32_t mid_i = mid_ticks_ - kBase;

        // --- pull ONE stale far order (stale-quote pulling) ---
        if (live_.size() > 8) {
            const std::uint32_t k  = rng_.below(static_cast<std::uint32_t>(live_.size()));
            const OrderId       id = live_[k];
            const std::int32_t  d  = loc_[id].px_i - mid_i;
            if (d > kMaxDist || d < -kMaxDist) { emit_cancel(id); return; }
        }

        // --- random cancel ---
        if (live_.size() > 4 && rng_.below(100) < 35u) {
            emit_cancel(live_[rng_.below(static_cast<std::uint32_t>(live_.size()))]);
            return;
        }

        // --- non-crossing add: clamp against the sim's OWN best on the far side ---
        const Side side = (rng_.below(2) == 0) ? Side::Buy : Side::Sell;
        const std::int32_t skew = static_cast<std::int32_t>(rng_.below(6));   // 0..5 ticks
        std::int32_t t;
        if (is_buy(side)) {
            t = mid_i - 1 - skew;
            if (lo_ask_ < kSpan) t = (t < lo_ask_ - 1) ? t : lo_ask_ - 1;   // strictly below best ask
        } else {
            t = mid_i + 1 + skew;
            if (hi_bid_ >= 0) t = (t > hi_bid_ + 1) ? t : hi_bid_ + 1;       // strictly above best bid
        }
        t = clamp_lvl(t);
        const Qty qty = 1 + rng_.below(499);
        const OrderId id = ++next_id_;
        MdMessage a;
        a.seq = ++seq_; a.ts = (ts_ += 40 + rng_.below(120));
        a.type = MdType::Add; a.side = side;
        a.order_id = id; a.px = static_cast<Price>(t + kBase); a.qty = qty;
        pending_.push_back(a);
        const std::size_t li = static_cast<std::size_t>(t);
        if (is_buy(side)) { ++bcnt_[li]; if (t > hi_bid_) hi_bid_ = t; }
        else              { ++acnt_[li]; if (t < lo_ask_) lo_ask_ = t; }
        live_.push_back(id);
        loc_[id] = LiveOrd{side, t, qty};
    }

    Rng rng_;
    std::vector<OrderId>                 live_;
    std::unordered_map<OrderId, LiveOrd> loc_;
    std::vector<MdMessage>               pending_;
    std::array<std::uint16_t, kSpan>     bcnt_{};   // live bid orders per level
    std::array<std::uint16_t, kSpan>     acnt_{};
    std::int32_t  hi_bid_     = -1;
    std::int32_t  lo_ask_     = kSpan;
    std::int32_t  mid_ticks_  = 10000;
    std::int32_t  trend_      = 0;
    std::uint32_t trend_left_ = 1;
    Ts            ts_         = 0;
    Seq           seq_        = 0;
    OrderId       next_id_    = 0;
    std::uint64_t produced_   = 0;
    std::uint64_t limit_      = 200000;
};

}  // namespace mhft
