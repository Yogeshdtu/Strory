// orderbook_v3_flat.hpp
// ============================================================
// VERSION 3 -- "production shape": flat tick-indexed array of price
// levels (O(1), no search, no alloc per-op), intrusive doubly-linked
// order lists inside a pre-allocated arena (uint32_t index links, not
// pointers -- 08-intrusive-order-lists.md), aur ek open-addressed flat
// hash table (tombstone-based) order_id -> arena slot (09-order-id-
// lookup.md). Top-of-book cached, incrementally maintained (11).
//
// Design constraint: price range BOUNDED (`NUM_LEVELS` ticks each side
// of a `center` reference price) -- real systems re-center this
// periodically OFF the hot path (07 mentions this trade-off).
// ============================================================
#pragma once

#include "orderbook_types.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

// ---- open-addressed, tombstone-based flat hash map: OrderId -> slot ----
class FlatIdIndex {
public:
    explicit FlatIdIndex(std::size_t capacity_pow2)
        : mask_(capacity_pow2 - 1),
          keys_(capacity_pow2, EMPTY),
          vals_(capacity_pow2, NULL_SLOT) {}

    bool insert(OrderId id, std::uint32_t slot) {
        std::size_t i = hash(id) & mask_;
        std::size_t first_tomb = NPOS;
        for (std::size_t probe = 0; probe <= mask_; ++probe) {
            const OrderId k = keys_[i];
            if (k == id) return false;   // duplicate
            if (k == EMPTY) {
                const std::size_t dest = (first_tomb != NPOS) ? first_tomb : i;
                keys_[dest] = id;
                vals_[dest] = slot;
                ++size_;
                return true;
            }
            if (k == TOMBSTONE && first_tomb == NPOS) first_tomb = i;
            i = (i + 1) & mask_;
        }
        return false;   // table full -- shouldn't happen, sized generously
    }

    bool find(OrderId id, std::uint32_t& out_slot) const {
        std::size_t i = hash(id) & mask_;
        for (std::size_t probe = 0; probe <= mask_; ++probe) {
            const OrderId k = keys_[i];
            if (k == EMPTY) return false;
            if (k == id) { out_slot = vals_[i]; return true; }
            i = (i + 1) & mask_;
        }
        return false;
    }

    bool erase(OrderId id) {
        std::size_t i = hash(id) & mask_;
        for (std::size_t probe = 0; probe <= mask_; ++probe) {
            const OrderId k = keys_[i];
            if (k == EMPTY) return false;
            if (k == id) {
                keys_[i] = TOMBSTONE;
                vals_[i] = NULL_SLOT;
                --size_;
                return true;
            }
            i = (i + 1) & mask_;
        }
        return false;
    }

    std::size_t size() const { return size_; }

    static constexpr std::uint32_t NULL_SLOT = 0xFFFFFFFFu;

private:
    static constexpr OrderId EMPTY = 0;                              // order ids start at 1 (workload)
    static constexpr OrderId TOMBSTONE = ~static_cast<OrderId>(0);   // all-ones, never a real id
    static constexpr std::size_t NPOS = static_cast<std::size_t>(-1);

    static std::size_t hash(OrderId id) {
        std::uint64_t x = id;                          // splitmix64 finalizer -- good avalanche
        x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33;
        x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33;
        return static_cast<std::size_t>(x);
    }

    std::size_t mask_;
    std::vector<OrderId> keys_;
    std::vector<std::uint32_t> vals_;
    std::size_t size_ = 0;
};

// ---- the order book itself ----
class BookV3 {
public:
    static constexpr int NUM_LEVELS = 256;
    static constexpr std::uint32_t NULL_SLOT = FlatIdIndex::NULL_SLOT;

    BookV3(Price center, std::size_t max_orders)
        : center_(center),
          arena_(max_orders),
          free_list_(max_orders),
          id_index_(next_pow2(max_orders * 2 + 1)) {
        for (std::size_t i = 0; i < max_orders; ++i) free_list_[i] = static_cast<std::uint32_t>(i);
        free_top_ = max_orders;
    }

    bool add(OrderId id, bool is_buy, Price price, Qty qty) {
        std::uint32_t dummy;
        if (id_index_.find(id, dummy)) return false;

        int idx;
        if (is_buy) {
            const Price offset = center_ - 1 - price;
            if (offset < 0 || offset >= NUM_LEVELS) return false;
            idx = static_cast<int>(offset);
        } else {
            const Price offset = price - center_ - 1;
            if (offset < 0 || offset >= NUM_LEVELS) return false;
            idx = static_cast<int>(offset);
        }
        if (free_top_ == 0) return false;   // arena full

        LevelV3& level = is_buy ? bid_levels_[static_cast<std::size_t>(idx)]
                                 : ask_levels_[static_cast<std::size_t>(idx)];
        const bool was_empty = (level.count == 0);

        const std::uint32_t slot = free_list_[--free_top_];
        arena_[slot] = OrderSlot{id, qty, level.tail, NULL_SLOT, static_cast<std::uint32_t>(idx), is_buy};
        if (level.tail != NULL_SLOT) arena_[level.tail].next = slot;
        else level.head = slot;
        level.tail = slot;
        level.total_qty += qty;
        ++level.count;

        if (was_empty) {
            int& best = is_buy ? best_bid_idx_ : best_ask_idx_;
            if (idx < best) best = idx;
        }

        id_index_.insert(id, slot);
        return true;
    }

    bool reduce(OrderId id, Qty qty) {
        std::uint32_t slot;
        if (!id_index_.find(id, slot)) return false;
        OrderSlot& order = arena_[slot];
        LevelV3& level = order.is_buy ? bid_levels_[order.level_idx] : ask_levels_[order.level_idx];

        const Qty amt = std::min(qty, order.qty);
        order.qty -= amt;
        level.total_qty -= amt;

        if (order.qty == 0) {
            unlink(order, level);
            id_index_.erase(id);
            free_list_[free_top_++] = slot;
            if (level.count == 0) {
                const int idx = static_cast<int>(order.level_idx);
                int& best = order.is_buy ? best_bid_idx_ : best_ask_idx_;
                if (idx == best) {
                    const auto& levels = order.is_buy ? bid_levels_ : ask_levels_;
                    int i = idx + 1;
                    while (i < NUM_LEVELS && levels[static_cast<std::size_t>(i)].count == 0) ++i;
                    best = i;   // NUM_LEVELS = sentinel ("koi level nahi bacha")
                }
            }
        }
        return true;
    }

    bool remove(OrderId id) {
        std::uint32_t slot;
        if (!id_index_.find(id, slot)) return false;
        return reduce(id, arena_[slot].qty);
    }

    bool replace(OrderId old_id, OrderId new_id, bool is_buy, Price price, Qty qty) {
        remove(old_id);
        return add(new_id, is_buy, price, qty);
    }

    bool has_bid() const { return best_bid_idx_ != NUM_LEVELS; }
    bool has_ask() const { return best_ask_idx_ != NUM_LEVELS; }
    Price best_bid() const { return center_ - 1 - best_bid_idx_; }
    Price best_ask() const { return center_ + 1 + best_ask_idx_; }
    Qty best_bid_qty() const { return bid_levels_[static_cast<std::size_t>(best_bid_idx_)].total_qty; }
    Qty best_ask_qty() const { return ask_levels_[static_cast<std::size_t>(best_ask_idx_)].total_qty; }
    std::size_t order_count() const { return id_index_.size(); }

    // FIFO order-id list at a price level -- price-time priority ka
    // direct proof, aur test/fuzz ke liye cross-version verification.
    std::vector<OrderId> ids_at_price(bool is_buy, Price price) const {
        std::vector<OrderId> result;
        int idx;
        if (is_buy) {
            const Price offset = center_ - 1 - price;
            if (offset < 0 || offset >= NUM_LEVELS) return result;
            idx = static_cast<int>(offset);
        } else {
            const Price offset = price - center_ - 1;
            if (offset < 0 || offset >= NUM_LEVELS) return result;
            idx = static_cast<int>(offset);
        }
        const LevelV3& level = is_buy ? bid_levels_[static_cast<std::size_t>(idx)]
                                       : ask_levels_[static_cast<std::size_t>(idx)];
        std::uint32_t cur = level.head;
        while (cur != NULL_SLOT) {
            result.push_back(arena_[cur].id);
            cur = arena_[cur].next;
        }
        return result;
    }

private:
    struct OrderSlot {
        OrderId id;
        Qty qty;
        std::uint32_t prev, next;   // arena indices, NULL_SLOT = none
        std::uint32_t level_idx;
        bool is_buy;
    };
    struct LevelV3 {
        std::uint32_t head = NULL_SLOT, tail = NULL_SLOT;
        Qty total_qty = 0;
        std::uint32_t count = 0;
    };

    static std::size_t next_pow2(std::size_t n) {
        std::size_t p = 1;
        while (p < n) p <<= 1;
        return p;
    }

    void unlink(OrderSlot& order, LevelV3& level) {
        if (order.prev != NULL_SLOT) arena_[order.prev].next = order.next;
        else level.head = order.next;
        if (order.next != NULL_SLOT) arena_[order.next].prev = order.prev;
        else level.tail = order.prev;
        --level.count;
    }

    Price center_;
    std::array<LevelV3, NUM_LEVELS> bid_levels_{};
    std::array<LevelV3, NUM_LEVELS> ask_levels_{};
    int best_bid_idx_ = NUM_LEVELS;   // sentinel = "no live bid level"
    int best_ask_idx_ = NUM_LEVELS;

    std::vector<OrderSlot> arena_;
    std::vector<std::uint32_t> free_list_;
    std::size_t free_top_ = 0;
    FlatIdIndex id_index_;
};
