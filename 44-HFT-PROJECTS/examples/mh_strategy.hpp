// mh_strategy.hpp
// ============================================================
// PROJECT 8 -- Strategy framework + one MECHANICAL strategy.
//
// >>> YEH ALPHA NAHI HAI. <<<  (37's SPECIALIZED/DOMAIN-SPECIFIC list:
// real trading signals IP hain, koi public nahi karta.) Yeh ek
// deterministic, mechanical rule hai jiska kaam pipeline ko exercise
// karna hai -- SMA crossover + book imbalance -> ek IOC quote. Backtest
// pe iska "P&L" ka koi predictive matlab nahi.
//
// Design (folder 43 se): mid ko ring buffer + running sum se track karo,
// SMA ke liye koi division nahi (window power-of-two, threshold ko
// cross-multiply se check karo). Har decision ek pure function of book
// state -- fully deterministic, replay-safe.
// ============================================================
#pragma once

#include "mh_order_book.hpp"

#include <array>
#include <cstdint>

namespace mhft {

struct StrategyDecision {
    bool      act  = false;
    Side      side = Side::Buy;
    OrderType type = OrderType::IOC;
    Price     px   = 0;
    Qty       qty  = 0;
};

class SpreadCrossStrategy {
public:
    struct Config {
        std::size_t   window     = 256;   // SMA window (power of two)
        std::int64_t  thr_num    = 1002;  // fire when mid > sma * (thr_num/thr_den)
        std::int64_t  thr_den    = 1000;  // ... i.e. 0.2% dislocation
        std::int64_t  imb_block  = 700;   // SOFT filter: skip a buy if the book is
                                          // heavily ask-heavy (imb < -imb_block),
                                          // and vice-versa. Not a hard gate.
        Qty           order_qty  = 20;
        std::size_t   cooldown   = 40;    // messages to wait after firing
    };

    SpreadCrossStrategy() { ring_.fill(0); }
    explicit SpreadCrossStrategy(Config c) : cfg_(c) { ring_.fill(0); }

    // called after every book update. returns whether/what to quote.
    StrategyDecision on_book(const L2Book& b) {
        StrategyDecision d;
        if (!b.has_bid() || !b.has_ask()) return d;

        const std::int64_t mid2 = b.mid2();               // == 2 * mid_ticks
        ring_sum_ += mid2 - ring_[pos_];
        ring_[pos_] = mid2;
        pos_ = (pos_ + 1) & (cfg_.window - 1);
        if (filled_ < cfg_.window) ++filled_;
        if (filled_ < cfg_.window) return d;

        if (cooldown_ > 0) { --cooldown_; return d; }

        const std::int64_t imb = b.imbalance();
        // cross condition, all integer (43/10): mid > sma * num/den
        //   mid   = mid2 / 2 ;  sma = (ring_sum_ / window) / 2
        //   fire  <=>  mid2 * window * den  >  ring_sum_ * num
        const std::int64_t lhs = mid2 * static_cast<std::int64_t>(cfg_.window) * cfg_.thr_den;
        const std::int64_t rhs = ring_sum_ * cfg_.thr_num;

        if (lhs > rhs && imb > -cfg_.imb_block) {
            // upward dislocation -> take the offer (unless book is crashing bid-side)
            d.act = true; d.side = Side::Buy; d.type = OrderType::IOC;
            d.px = b.best_ask(); d.qty = cfg_.order_qty;
            cooldown_ = cfg_.cooldown;
        } else if (lhs < rhs && imb < cfg_.imb_block) {
            // downward dislocation -> hit the bid (unless book is ripping ask-side)
            d.act = true; d.side = Side::Sell; d.type = OrderType::IOC;
            d.px = b.best_bid(); d.qty = cfg_.order_qty;
            cooldown_ = cfg_.cooldown;
        }
        return d;
    }

    std::size_t signals() const { return signals_seen_; }
    void note_fired() { ++signals_seen_; }

private:
    Config cfg_{};
    std::array<std::int64_t, 4096> ring_{};   // sized >= any sane window
    std::int64_t ring_sum_ = 0;
    std::size_t  pos_ = 0, filled_ = 0, cooldown_ = 0;
    std::size_t  signals_seen_ = 0;
};

}  // namespace mhft
