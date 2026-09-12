// orderbook_v2_vector.hpp
// ============================================================
// VERSION 2 -- sorted std::vector<PriceLevel> per side (CONTIGUOUS,
// cache-friendly), std::deque<Order> per level (FIFO, better locality
// than V1's std::list). Order-id lookup still via unordered_map, par
// ab sirf {side, price} store karta (level ke andar order ek chhoti
// linear scan se milta -- levels chhote hote, yeh sasta hai).
// (05-sorted-vector-implementation.md)
// ============================================================
#pragma once

#include "orderbook_types.hpp"

#include <algorithm>
#include <deque>
#include <unordered_map>
#include <vector>

class BookV2 {
public:
    bool add(OrderId id, bool is_buy, Price price, Qty qty) {
        if (index_.count(id)) return false;
        Side& side = is_buy ? bids_ : asks_;
        auto it = locate(side, price, is_buy);
        if (it == side.end() || it->price != price) {
            it = side.insert(it, PriceLevel{price, 0, {}});   // O(n) shift -- the trade-off (06)
        }
        it->orders.push_back(Order{id, qty});
        it->total_qty += qty;
        index_.emplace(id, Location{is_buy, price});
        return true;
    }

    bool reduce(OrderId id, Qty qty) {
        auto idx_it = index_.find(id);
        if (idx_it == index_.end()) return false;
        const Location loc = idx_it->second;
        Side& side = loc.is_buy ? bids_ : asks_;
        auto lvl_it = locate(side, loc.price, loc.is_buy);
        if (lvl_it == side.end() || lvl_it->price != loc.price) return false;

        auto& orders = lvl_it->orders;
        auto ord_it = std::find_if(orders.begin(), orders.end(),
                                    [id](const Order& o) { return o.id == id; });
        if (ord_it == orders.end()) return false;

        const Qty amt = std::min(qty, ord_it->qty);
        ord_it->qty -= amt;
        lvl_it->total_qty -= amt;
        if (ord_it->qty == 0) {
            orders.erase(ord_it);
            index_.erase(idx_it);
            if (orders.empty()) side.erase(lvl_it);
        }
        return true;
    }

    bool remove(OrderId id) {
        auto idx_it = index_.find(id);
        if (idx_it == index_.end()) return false;
        const Location loc = idx_it->second;
        Side& side = loc.is_buy ? bids_ : asks_;
        auto lvl_it = locate(side, loc.price, loc.is_buy);
        if (lvl_it == side.end() || lvl_it->price != loc.price) return false;

        auto& orders = lvl_it->orders;
        auto ord_it = std::find_if(orders.begin(), orders.end(),
                                    [id](const Order& o) { return o.id == id; });
        if (ord_it == orders.end()) return false;

        lvl_it->total_qty -= ord_it->qty;
        orders.erase(ord_it);
        if (orders.empty()) side.erase(lvl_it);
        index_.erase(idx_it);
        return true;
    }

    bool replace(OrderId old_id, OrderId new_id, bool is_buy, Price price, Qty qty) {
        remove(old_id);
        return add(new_id, is_buy, price, qty);
    }

    bool has_bid() const { return !bids_.empty(); }
    bool has_ask() const { return !asks_.empty(); }
    Price best_bid() const { return bids_.front().price; }
    Price best_ask() const { return asks_.front().price; }
    Qty best_bid_qty() const { return bids_.front().total_qty; }
    Qty best_ask_qty() const { return asks_.front().total_qty; }
    std::size_t order_count() const { return index_.size(); }

    // FIFO order-id list at a price level -- price-time priority ka
    // direct proof, aur test/fuzz ke liye cross-version verification.
    std::vector<OrderId> ids_at_price(bool is_buy, Price price) const {
        std::vector<OrderId> result;
        const Side& side = is_buy ? bids_ : asks_;
        auto it = locate(side, price, is_buy);
        if (it != side.end() && it->price == price)
            for (const auto& o : it->orders) result.push_back(o.id);
        return result;
    }

private:
    struct Order { OrderId id; Qty qty; };
    struct PriceLevel { Price price; Qty total_qty; std::deque<Order> orders; };
    struct Location { bool is_buy; Price price; };
    using Side = std::vector<PriceLevel>;

    // Binary search -- bids descending, asks ascending. Returns iterator
    // to first level with price <= target (bids) / >= target (asks) --
    // i.e. "insert here if not found."
    static Side::iterator locate(Side& side, Price price, bool is_buy) {
        if (is_buy) {
            return std::lower_bound(side.begin(), side.end(), price,
                [](const PriceLevel& lvl, Price p) { return lvl.price > p; });
        }
        return std::lower_bound(side.begin(), side.end(), price,
            [](const PriceLevel& lvl, Price p) { return lvl.price < p; });
    }
    static Side::const_iterator locate(const Side& side, Price price, bool is_buy) {
        if (is_buy) {
            return std::lower_bound(side.begin(), side.end(), price,
                [](const PriceLevel& lvl, Price p) { return lvl.price > p; });
        }
        return std::lower_bound(side.begin(), side.end(), price,
            [](const PriceLevel& lvl, Price p) { return lvl.price < p; });
    }

    Side bids_;   // sorted descending (best = front())
    Side asks_;   // sorted ascending  (best = front())
    std::unordered_map<OrderId, Location> index_;
};
