// mh_risk_engine.hpp
// ============================================================
// PROJECT 9 -- Pre-trade risk engine (37/13).
//
// HFT ka non-negotiable gate: STRATEGY vs EXCHANGE ke BEECH. Har outbound
// order yahan se guzarta -- ek bhi check fail -> order block (aur repeated
// breaches -> KILL SWITCH: sab kuch band).
//
// Checks (sab O(1), hot path pe):
//   1. fat-finger      : qty <= max_order_qty, notional <= max_order_notional
//   2. price collar    : px reference-price ke +/- band_bps ke andar
//   3. position limit  : is order ke baad |position| <= max_position
//   4. message rate    : rolling window mein <= max_msgs
//   5. kill switch     : kill hone ke baad SAB reject
//
// on_fill() se position/notional update hota. Deterministic -- koi wall
// clock nahi, sab event-timestamp driven.
// ============================================================
#pragma once

#include "mh_types.hpp"

namespace mhft {

enum class RiskVerdict : std::uint8_t {
    Ok,
    RejFatFinger,
    RejPriceCollar,
    RejPositionLimit,
    RejRateLimit,
    RejKilled
};

inline const char* to_str(RiskVerdict v) {
    switch (v) {
        case RiskVerdict::Ok:               return "OK";
        case RiskVerdict::RejFatFinger:     return "REJECT:fat-finger";
        case RiskVerdict::RejPriceCollar:   return "REJECT:price-collar";
        case RiskVerdict::RejPositionLimit: return "REJECT:position-limit";
        case RiskVerdict::RejRateLimit:     return "REJECT:rate-limit";
        case RiskVerdict::RejKilled:        return "REJECT:killed";
    }
    return "?";
}

class RiskEngine {
public:
    struct Config {
        Qty          max_order_qty      = 100;
        std::int64_t max_order_notional = 500 * 100 * 100;   // 500 shares * $100 (scaled)
        std::int64_t max_position       = 400;               // net shares, abs
        std::int64_t band_bps           = 50;                // +/- 0.50% of reference
        std::uint32_t max_msgs          = 200;               // per window
        Ts           window_ns          = 1'000'000;         // 1 ms
    };

    RiskEngine() = default;
    explicit RiskEngine(Config c) : cfg_(c) {}

    // reference price = current mid (2*mid passed in as `mid2` to avoid /2).
    RiskVerdict check(const OrderRequest& r, std::int64_t mid2, Ts now) {
        if (killed_) return RiskVerdict::RejKilled;

        // 1. fat finger
        const std::int64_t notional = static_cast<std::int64_t>(r.qty) * r.px;
        if (r.qty == 0 || r.qty > cfg_.max_order_qty || notional > cfg_.max_order_notional)
            return breach(RiskVerdict::RejFatFinger);

        // 2. price collar: |px*2 - mid2| * 10000 <= band_bps * mid2
        const std::int64_t dev = (r.px * 2 - mid2);
        const std::int64_t adev = dev < 0 ? -dev : dev;
        if (adev * 10000 > cfg_.band_bps * mid2)
            return breach(RiskVerdict::RejPriceCollar);

        // 3. position limit (post-trade, worst case = full fill)
        const std::int64_t delta = is_buy(r.side) ? static_cast<std::int64_t>(r.qty)
                                                  : -static_cast<std::int64_t>(r.qty);
        const std::int64_t projected = position_ + delta;
        if (projected > cfg_.max_position || projected < -cfg_.max_position)
            return breach(RiskVerdict::RejPositionLimit);

        // 4. message rate (rolling window) -- NORMAL backpressure, not a risk
        //    violation: it drops the order but does NOT count toward the kill
        //    switch (a busy strategy is not a broken one).
        if (now - window_start_ >= cfg_.window_ns) { window_start_ = now; msgs_ = 0; }
        if (++msgs_ > cfg_.max_msgs) {
            ++rate_drops_;
            return RiskVerdict::RejRateLimit;
        }

        return RiskVerdict::Ok;
    }

    // real (post-fill) position + notional update
    void on_fill(const Fill& f) {
        const std::int64_t d = is_buy(f.side) ? static_cast<std::int64_t>(f.qty)
                                              : -static_cast<std::int64_t>(f.qty);
        position_ += d;
        traded_notional_ += static_cast<std::int64_t>(f.qty) * f.px;
    }

    void kill()            { killed_ = true; }
    bool is_killed() const { return killed_; }
    std::int64_t  position()   const { return position_; }
    std::uint32_t breaches()   const { return breach_count_; }
    std::uint32_t rate_drops() const { return rate_drops_; }

    // config for tests / tuning
    Config& config() { return cfg_; }

private:
    RiskVerdict breach(RiskVerdict v) {
        ++breach_count_;
        if (breach_count_ >= kKillAfter) killed_ = true;   // repeated REAL breaches -> kill
        return v;
    }

    static constexpr std::uint32_t kKillAfter = 50;

    Config       cfg_{};
    std::int64_t position_        = 0;
    std::int64_t traded_notional_ = 0;
    Ts           window_start_    = 0;
    std::uint32_t msgs_           = 0;
    std::uint32_t breach_count_   = 0;
    std::uint32_t rate_drops_     = 0;
    bool         killed_          = false;
};

}  // namespace mhft
