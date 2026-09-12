// 06_top_of_book.cpp
// ============================================================
// HFT CLASSIC: a tiny L2 order book. add / cancel / trade, and O(1)
// best-bid / best-ask. Flat array indexed by price tick + cached BBO.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 06_top_of_book.cpp -o t && ./t
// ============================================================
// INTERVIEWER KYA DEKH RAHA:
//   - the flat-array-by-tick design and WHY (O(1), contiguous, no alloc)
//   - cached best_bid/best_ask + a bounded re-walk only when the touch
//     level depletes
//   - a dense order_id -> {side, tick, qty} index for O(1) cancel
//   - integer prices (ticks), no float
//   - the "book must not cross" invariant
// ============================================================

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <vector>

enum class Side : std::uint8_t { Bid, Ask };

class TopOfBook {
public:
    static constexpr std::int32_t kBase   = 9000;   // lowest representable tick
    static constexpr std::int32_t kLevels = 2048;   // price range [kBase, kBase+kLevels)

    explicit TopOfBook(std::size_t max_order_id) : loc_(max_order_id + 1) {}

    void add(std::uint64_t oid, Side side, std::int32_t tick, std::int64_t qty) {
        const std::size_t li = idx(tick);
        auto& level = (side == Side::Bid) ? bid_[li] : ask_[li];
        level += qty;
        loc_[oid] = Loc{side, tick, qty, true};
        if (side == Side::Bid) { if (best_bid_ < 0 || tick > best_bid_) best_bid_ = tick; }
        else                   { if (best_ask_ < 0 || tick < best_ask_) best_ask_ = tick; }
    }

    void cancel(std::uint64_t oid) {
        Loc& l = loc_[oid];
        if (!l.live) return;                          // unknown / already gone -> ignore
        const std::size_t li = idx(l.tick);
        auto& level = (l.side == Side::Bid) ? bid_[li] : ask_[li];
        level -= l.qty;
        if (level < 0) level = 0;                     // defensive clamp
        l.live = false;
        if (l.side == Side::Bid && l.tick == best_bid_ && bid_[li] == 0) rewalk_bid();
        if (l.side == Side::Ask && l.tick == best_ask_ && ask_[li] == 0) rewalk_ask();
    }

    // an aggressive trade consumes `qty` from the given side's touch level
    void trade(Side resting_side, std::int64_t qty) {
        if (resting_side == Side::Bid) {
            if (best_bid_ < 0) return;
            auto& lvl = bid_[idx(best_bid_)];
            lvl -= qty; if (lvl < 0) lvl = 0;
            if (lvl == 0) rewalk_bid();
        } else {
            if (best_ask_ < 0) return;
            auto& lvl = ask_[idx(best_ask_)];
            lvl -= qty; if (lvl < 0) lvl = 0;
            if (lvl == 0) rewalk_ask();
        }
    }

    std::optional<std::int32_t> best_bid() const {
        return best_bid_ >= 0 ? std::optional{best_bid_} : std::nullopt;
    }
    std::optional<std::int32_t> best_ask() const {
        return best_ask_ >= 0 ? std::optional{best_ask_} : std::nullopt;
    }
    std::int64_t qty_at(Side s, std::int32_t tick) const {
        return (s == Side::Bid) ? bid_[idx(tick)] : ask_[idx(tick)];
    }
    bool crossed() const {
        return best_bid_ >= 0 && best_ask_ >= 0 && best_bid_ >= best_ask_;
    }

private:
    struct Loc { Side side; std::int32_t tick; std::int64_t qty; bool live; };

    static std::size_t idx(std::int32_t tick) {
        assert(tick >= kBase && tick < kBase + kLevels);
        return static_cast<std::size_t>(tick - kBase);
    }
    void rewalk_bid() {
        for (std::int32_t t = best_bid_; t >= kBase; --t)
            if (bid_[idx(t)] > 0) { best_bid_ = t; return; }
        best_bid_ = -1;
    }
    void rewalk_ask() {
        for (std::int32_t t = best_ask_; t < kBase + kLevels; ++t)
            if (ask_[idx(t)] > 0) { best_ask_ = t; return; }
        best_ask_ = -1;
    }

    std::array<std::int64_t, kLevels> bid_{};
    std::array<std::int64_t, kLevels> ask_{};
    std::vector<Loc> loc_;
    std::int32_t best_bid_ = -1;
    std::int32_t best_ask_ = -1;
};

int main() {
    TopOfBook b(100);

    b.add(1, Side::Bid, 9990, 100);
    b.add(2, Side::Bid, 9995, 50);
    b.add(3, Side::Ask, 10005, 80);
    b.add(4, Side::Ask, 10010, 200);

    assert(b.best_bid() == 9995);
    assert(b.best_ask() == 10005);
    assert(!b.crossed());
    assert(b.qty_at(Side::Bid, 9995) == 50);

    // add more depth at the touch
    b.add(5, Side::Bid, 9995, 30);
    assert(b.qty_at(Side::Bid, 9995) == 80);

    // cancel part of the touch -> BBO unchanged (level not empty)
    b.cancel(2);
    assert(b.qty_at(Side::Bid, 9995) == 30);
    assert(b.best_bid() == 9995);

    // cancel the rest of the touch -> BBO re-walks down to 9990
    b.cancel(5);
    assert(b.qty_at(Side::Bid, 9995) == 0);
    assert(b.best_bid() == 9990);

    // a trade eats the ask touch entirely -> re-walk up to 10010
    b.trade(Side::Ask, 80);
    assert(b.best_ask() == 10010);

    // cancel an unknown / already-cancelled order -> no-op
    b.cancel(2);
    b.cancel(99);
    assert(b.best_bid() == 9990 && b.best_ask() == 10010);

    // empty one side completely
    b.cancel(1);
    assert(!b.best_bid().has_value());

    std::printf("06_top_of_book: ALL PASS\n");
    return 0;
}

// ============================================================
// COMPLEXITY: add O(1); cancel O(1) + a bounded re-walk only if the
// touch level emptied (walk distance = how far the BBO moved, usually
// 1-2 ticks). No allocation after construction. No std::map.
//
// vs std::map<price, level>: every add/cancel is a tree node malloc/free
// + rebalance + pointer chasing -- measured ~25x slower book stage.
// (folders 39, 43/14, 44 L2Book)
//
// INVARIANT: the book must never cross (best_bid >= best_ask). If updates
// are applied fully and correctly it won't; a real book adds a
// resolve_cross() guard for safety. (folder 44)
// ============================================================
