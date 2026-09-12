// mh_order_book.hpp
// ============================================================
// PROJECT 3 -- Limit order book (L2 aggregate view).
//
// Design = folder 39 ki V3 + folder 43 ki pipeline V3:
//   - flat array: index = px_ticks - kBase, value = aggregate qty at that level
//   - cached best-bid / best-ask index (top-of-book O(1))
//   - id -> location: dense std::vector (sim ke ids sequential hain -> direct index)
//   - level empty hone pe hi BBO re-walk (bounded, cache-friendly sequential)
//
// Yeh strategy ka "book view" hai. Per-order FIFO yahan NAHI (woh matching
// engine ka kaam -- project 4/10). 39 ne poori 3-version story measure ki;
// yahan hum V3 seedha use karte (43/14).
// ============================================================
#pragma once

#include "mh_types.hpp"

#include <array>
#include <cstddef>
#include <vector>

namespace mhft {

class L2Book {
public:
    static constexpr Price       kBase   = 9000;    // 90.00 in ticks
    static constexpr std::size_t kLevels = 2048;    // 90.00 .. 110.47
    static constexpr std::size_t kNoBid  = static_cast<std::size_t>(-1);

    explicit L2Book(OrderId max_id = 0) { resize_ids(max_id); }

    void resize_ids(OrderId max_id) {
        loc_.assign(static_cast<std::size_t>(max_id) + 1, Loc{});
    }

    void apply(const MdMessage& m) {
        const std::size_t idx = level_of(m.px);
        switch (m.type) {
            case MdType::Add: {
                side_qty(m.side)[idx] += static_cast<std::int64_t>(m.qty);
                ensure_id(m.order_id);
                loc_[static_cast<std::size_t>(m.order_id)] =
                    Loc{static_cast<std::int32_t>(idx), static_cast<std::int32_t>(m.qty), m.side};
                if (m.side == Side::Buy) { if (best_bid_ == kNoBid || idx > best_bid_) best_bid_ = idx; }
                else                     { if (idx < best_ask_) best_ask_ = idx; }
                break;
            }
            case MdType::Cancel:
            case MdType::Trade: {
                if (m.order_id >= loc_.size()) break;
                Loc& l = loc_[static_cast<std::size_t>(m.order_id)];
                const std::size_t li = static_cast<std::size_t>(l.idx);
                std::int32_t take = l.qty;
                if (m.type == MdType::Trade && static_cast<std::int32_t>(m.qty) < take)
                    take = static_cast<std::int32_t>(m.qty);
                side_qty(l.side)[li] -= take;
                l.qty -= take;
                if (side_qty(l.side)[li] <= 0) {
                    side_qty(l.side)[li] = 0;
                    if (l.side == Side::Buy && li == best_bid_) rewalk_bid();
                    if (l.side == Side::Sell && li == best_ask_) rewalk_ask();
                }
                break;
            }
        }
        resolve_cross();
    }

    bool  has_bid() const { return best_bid_ != kNoBid; }
    bool  has_ask() const { return best_ask_ < kLevels; }
    Price best_bid() const { return static_cast<Price>(best_bid_) + kBase; }
    Price best_ask() const { return static_cast<Price>(best_ask_) + kBase; }
    Price mid2()     const { return best_bid() + best_ask(); }        // 2 * mid (43/09 -- no /2)
    std::int64_t bid_qty() const { return bid_[best_bid_]; }
    std::int64_t ask_qty() const { return ask_[best_ask_]; }

    // near-touch imbalance, scaled by 1000: >0 = more bid, <0 = more ask.
    // (bidQ - askQ) * 1000 / (bidQ + askQ + 1)   -- integer, no float.
    std::int64_t imbalance(std::size_t depth = 3) const {
        if (!has_bid() || !has_ask()) return 0;
        std::int64_t b = 0, a = 0;
        for (std::size_t d = 0; d < depth; ++d) {
            if (best_bid_ >= d) b += bid_[best_bid_ - d];
            if (best_ask_ + d < kLevels) a += ask_[best_ask_ + d];
        }
        return (b - a) * 1000 / (b + a + 1);
    }

private:
    struct Loc { std::int32_t idx = 0; std::int32_t qty = 0; Side side = Side::Buy; };

    static std::size_t level_of(Price px) {
        Price i = px - kBase;
        if (i < 0) i = 0;
        if (i >= static_cast<Price>(kLevels)) i = static_cast<Price>(kLevels) - 1;
        return static_cast<std::size_t>(i);
    }
    std::array<std::int64_t, kLevels>& side_qty(Side s) { return s == Side::Buy ? bid_ : ask_; }

    void ensure_id(OrderId id) {
        if (id >= loc_.size()) loc_.resize(static_cast<std::size_t>(id) + 1);
    }
    void rewalk_bid() {
        std::size_t i = best_bid_;
        while (i != kNoBid && bid_[i] == 0) --i;   // 0 -> SIZE_MAX == kNoBid
        best_bid_ = i;
    }
    void rewalk_ask() {
        std::size_t i = best_ask_;
        while (i < kLevels && ask_[i] == 0) ++i;
        best_ask_ = i;
    }

    // A pure L2 aggregator fed an imperfect (occasionally-crossing) feed can
    // transiently show best_bid >= best_ask -- crossed liquidity that, on a
    // real venue, would have traded away. Resolve it: repeatedly drop the
    // smaller of the two touch levels until bid < ask. Bounded (each step
    // removes a level). 39/16's "book invariants" made executable.
    void resolve_cross() {
        while (best_bid_ != kNoBid && best_ask_ < kLevels && best_bid_ >= best_ask_) {
            if (bid_[best_bid_] <= ask_[best_ask_]) { bid_[best_bid_] = 0; rewalk_bid(); }
            else                                    { ask_[best_ask_] = 0; rewalk_ask(); }
        }
    }

    std::array<std::int64_t, kLevels> bid_{};
    std::array<std::int64_t, kLevels> ask_{};
    std::vector<Loc> loc_;
    std::size_t best_bid_ = kNoBid;
    std::size_t best_ask_ = kLevels;
};

}  // namespace mhft
