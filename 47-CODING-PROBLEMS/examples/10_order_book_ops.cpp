// 10_order_book_ops.cpp
// ============================================================
// Folder 47 file 10 #3/#4/#5: a price-indexed limit order book.
// add() / cancel() are O(1) (+ a bounded BBO rewalk); match() walks the
// opposite side by price-time priority and emits fills. Integer ticks only.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g -O0 10_order_book_ops.cpp -o t && ./t
// ============================================================

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <vector>

class Book {
public:
    static constexpr std::int32_t kLevels = 1024;      // price band around a reference

    struct Fill { std::uint32_t taker; std::uint32_t maker; std::int32_t tick; std::int64_t qty; };

    // Add a resting limit order. Returns its id, or UINT32_MAX if outside the band.
    std::uint32_t add(bool is_bid, std::int32_t tick, std::int64_t qty) {
        if (tick < 0 || tick >= kLevels || qty <= 0) return UINT32_MAX;
        auto& side = is_bid ? bid_ : ask_;
        side[static_cast<std::size_t>(tick)] += qty;
        if (is_bid) { if (tick > best_bid_) best_bid_ = tick; }
        else        { if (tick < best_ask_) best_ask_ = tick; }
        loc_.push_back({is_bid, tick, qty, true});
        return static_cast<std::uint32_t>(loc_.size() - 1);
    }

    void cancel(std::uint32_t id) {
        if (id >= loc_.size() || !loc_[id].live) return;
        Loc& o = loc_[id];
        o.live = false;
        auto& side = o.is_bid ? bid_ : ask_;
        side[static_cast<std::size_t>(o.tick)] -= o.qty;
        rewalk_if_needed(o.is_bid, o.tick);
    }

    // Match an incoming aggressive order. `limit` is the worst acceptable tick.
    // Unfilled remainder rests (returns its id) unless `ioc`.
    std::optional<std::uint32_t>
    match(bool taker_is_bid, std::int32_t limit, std::int64_t qty, bool ioc,
          std::vector<Fill>& fills) {
        const std::uint32_t taker_id = static_cast<std::uint32_t>(loc_.size());
        loc_.push_back({taker_is_bid, limit, 0, false});   // placeholder for the taker

        auto& opp = taker_is_bid ? ask_ : bid_;
        while (qty > 0) {
            const std::int32_t px = taker_is_bid ? best_ask_ : best_bid_;
            const bool crosses = taker_is_bid ? (px <= limit && px < kLevels)
                                              : (px >= limit && px >= 0);
            if (!crosses) break;
            std::int64_t& avail = opp[static_cast<std::size_t>(px)];
            const std::int64_t traded = qty < avail ? qty : avail;
            fills.push_back({taker_id, UINT32_MAX, px, traded});   // maker id omitted (level-aggregated)
            avail -= traded;
            qty   -= traded;
            if (avail == 0) rewalk_if_needed(!taker_is_bid, px);
        }

        if (qty > 0 && !ioc) {
            const std::uint32_t rest_id = add(taker_is_bid, limit, qty);
            return rest_id == UINT32_MAX ? std::nullopt : std::optional{rest_id};
        }
        return std::nullopt;
    }

    std::optional<std::int32_t> best_bid() const {
        return best_bid_ >= 0 ? std::optional{best_bid_} : std::nullopt;
    }
    std::optional<std::int32_t> best_ask() const {
        return best_ask_ < kLevels ? std::optional{best_ask_} : std::nullopt;
    }
    std::int64_t qty_at(bool is_bid, std::int32_t tick) const {
        return (is_bid ? bid_ : ask_)[static_cast<std::size_t>(tick)];
    }
    // Invariant: a well-formed book is never crossed.
    bool crossed() const { return best_bid_ >= 0 && best_ask_ < kLevels && best_bid_ >= best_ask_; }

private:
    struct Loc { bool is_bid; std::int32_t tick; std::int64_t qty; bool live; };

    void rewalk_if_needed(bool is_bid, std::int32_t tick) {
        if (is_bid && tick == best_bid_)
            while (best_bid_ >= 0 && bid_[static_cast<std::size_t>(best_bid_)] == 0) --best_bid_;
        if (!is_bid && tick == best_ask_)
            while (best_ask_ < kLevels && ask_[static_cast<std::size_t>(best_ask_)] == 0) ++best_ask_;
    }

    std::array<std::int64_t, kLevels> bid_{};
    std::array<std::int64_t, kLevels> ask_{};
    std::int32_t                      best_bid_ = -1;
    std::int32_t                      best_ask_ = kLevels;
    std::vector<Loc>                  loc_;
};

int main() {
    Book b;

    // ---- build a book ----
    const std::uint32_t o1 = b.add(true,  100, 10);   // bid 100 x10
    const std::uint32_t o2 = b.add(true,   99, 20);   // bid  99 x20
    (void)b.add(false, 105, 15);                       // ask 105 x15
    const std::uint32_t o4 = b.add(false, 106, 25);   // ask 106 x25
    assert(b.best_bid().value() == 100 && b.best_ask().value() == 105);
    assert(!b.crossed());

    // ---- cancel the best bid -> BBO rewalks down to 99 ----
    b.cancel(o1);
    assert(b.best_bid().value() == 99);
    assert(b.qty_at(true, 100) == 0);
    b.cancel(o1);                                       // double cancel is a no-op
    assert(b.best_bid().value() == 99);

    // ---- aggressive buy 30 @ limit 106: takes 15@105 then 15@106 ----
    std::vector<Book::Fill> fills;
    auto rest = b.match(true, 106, 30, /*ioc=*/false, fills);
    assert(!rest.has_value());                          // fully filled
    assert(fills.size() == 2);
    assert(fills[0].tick == 105 && fills[0].qty == 15);
    assert(fills[1].tick == 106 && fills[1].qty == 15);
    assert(b.qty_at(false, 105) == 0);
    assert(b.qty_at(false, 106) == 10);                 // 25 - 15 left
    assert(b.best_ask().value() == 106);
    assert(!b.crossed());
    (void)o2; (void)o4;

    // ---- IOC buy 50 @ limit 106: takes the 10 left, no resting remainder ----
    fills.clear();
    auto rest2 = b.match(true, 106, 50, /*ioc=*/true, fills);
    assert(!rest2.has_value());
    assert(fills.size() == 1 && fills[0].qty == 10);
    assert(!b.best_ask().has_value());                  // ask side empty now

    // ---- non-marketable limit buy rests and becomes the BBO ----
    fills.clear();
    auto rest3 = b.match(true, 101, 5, /*ioc=*/false, fills);
    assert(fills.empty() && rest3.has_value());
    assert(b.best_bid().value() == 101);
    assert(!b.crossed());

    std::puts("10_order_book_ops: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - Price is an integer tick index into a flat array -> add/cancel are a
//     single array bump + a cached-BBO compare. No std::map, no float.
//   - The BBO rewalk after a level empties is bounded: books are dense near
//     the top, so it moves a few ticks at most.
//   - id -> Loc gives O(1) cancel with no search. A real book also threads an
//     intrusive FIFO per level for true price-TIME priority and per-maker fills.
//   - match() stops when the book stops crossing or the order is done; IOC
//     drops the remainder, a plain limit rests it (and it may become the BBO).
//   - crossed() is the invariant to assert everywhere in tests.
// ============================================================
