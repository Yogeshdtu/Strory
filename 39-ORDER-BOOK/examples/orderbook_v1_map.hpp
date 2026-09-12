// orderbook_v1_map.hpp
// ============================================================
// VERSION 1 -- std::map<Price, PriceLevel> per side, std::list<Order>
// per level (FIFO), std::unordered_map<OrderId, Location> for O(1)-ish
// cancel-by-id. SIMPLE, CORRECT. NOT optimized -- yeh baseline hai
// (03-naive-map-implementation.md).
// ============================================================
#pragma once

#include "orderbook_types.hpp"

#include <algorithm>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>

class BookV1 {
public:
    bool add(OrderId id, bool is_buy, Price price, Qty qty) {
        if (index_.count(id)) return false;   // duplicate id -- reject (09-order-id-lookup)
        // Note: bids_/asks_ ka comparator ALAG type hai (std::greater<Price>
        // vs default) -- isliye ek ternary se "side" reference nahi nikal
        // sakte (37-HFT-FUNDAMENTALS/examples/01 mein bhi yehi trap tha).
        // Templated impl + if/else dispatch is chhote papercut ko avoid karta.
        return is_buy ? add_impl(bids_, id, true, price, qty)
                       : add_impl(asks_, id, false, price, qty);
    }

    // Partial reduce -- ek hi mechanic Cancel (partial) aur Execute (fill)
    // dono ke liye (dono order ki qty ghataate, book-side se same tarah).
    bool reduce(OrderId id, Qty qty) {
        auto idx_it = index_.find(id);
        if (idx_it == index_.end()) return false;
        const Location& loc = idx_it->second;
        return loc.is_buy ? reduce_impl(bids_, idx_it, qty)
                           : reduce_impl(asks_, idx_it, qty);
    }

    bool remove(OrderId id) {
        auto idx_it = index_.find(id);
        if (idx_it == index_.end()) return false;
        const Location& loc = idx_it->second;
        return loc.is_buy ? remove_impl(bids_, idx_it)
                           : remove_impl(asks_, idx_it);
    }

    bool replace(OrderId old_id, OrderId new_id, bool is_buy, Price price, Qty qty) {
        remove(old_id);   // agar already gone (fuzzing), ignore -- add phir bhi try karo
        return add(new_id, is_buy, price, qty);
    }

    bool has_bid() const { return !bids_.empty(); }
    bool has_ask() const { return !asks_.empty(); }
    Price best_bid() const { return bids_.begin()->first; }
    Price best_ask() const { return asks_.begin()->first; }
    Qty best_bid_qty() const { return bids_.begin()->second.total_qty; }
    Qty best_ask_qty() const { return asks_.begin()->second.total_qty; }
    std::size_t order_count() const { return index_.size(); }

    // FIFO order-id list at a price level -- price-time priority ka
    // direct proof (10-price-time-priority-impl.md), aur test/fuzz ke
    // liye cross-version verification.
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
    struct Order { OrderId id; Qty qty; };
    struct PriceLevel { std::list<Order> orders; Qty total_qty = 0; };
    struct Location { bool is_buy; Price price; std::list<Order>::iterator it; };
    using IndexIt = std::unordered_map<OrderId, Location>::iterator;

    template <class Map>
    bool add_impl(Map& side, OrderId id, bool is_buy, Price price, Qty qty) {
        auto& level = side[price];             // std::map::operator[] -- creates level if new
        level.orders.push_back(Order{id, qty});
        level.total_qty += qty;
        auto it = std::prev(level.orders.end());
        index_.emplace(id, Location{is_buy, price, it});
        return true;
    }

    template <class Map>
    bool reduce_impl(Map& side, IndexIt idx_it, Qty qty) {
        Location& loc = idx_it->second;
        auto lvl_it = side.find(loc.price);
        if (lvl_it == side.end()) return false;
        PriceLevel& level = lvl_it->second;
        Order& order = *loc.it;
        const Qty amt = std::min(qty, order.qty);
        order.qty -= amt;
        level.total_qty -= amt;
        if (order.qty == 0) {
            level.orders.erase(loc.it);
            index_.erase(idx_it);
            if (level.orders.empty()) side.erase(lvl_it);
        }
        return true;
    }

    template <class Map>
    bool remove_impl(Map& side, IndexIt idx_it) {
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
    std::map<Price, PriceLevel> asks_;                         // best = begin()
    std::unordered_map<OrderId, Location> index_;
};
