// 07_engine_fuzz.cpp
// ============================================================
// Fuzzing -- MatchingEngine (map+list, "real") ko ek INDEPENDENT,
// deliberately-simple O(n) reference (`RefEngine`, plain vector +
// linear scan) ke against thousands of random commands pe check karta.
// (15-fuzzing.md)
//
// RefEngine ka FOK precheck bhi jaan-boojh kar ALAG STRATEGY use karta
// (dry-run: pura state COPY karo, ek baar match "practice" chalao, dekho
// kitna fill hota, phir discard) -- MatchingEngine ke closed-form
// STP-aware-sum formula se COMPLETELY independent tareeka. Agar formula
// mein koi subtle bug hota, yeh dry-run approach usse pakad leta.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 07_engine_fuzz.cpp -o efuzz && ./efuzz
// ============================================================

#include "engine_workload.hpp"
#include "matching_engine.hpp"

#include <cstdio>
#include <vector>

// ============================================================
//  RefEngine -- simple, obviously-correct, O(n) per operation.
//  MatchingEngine jaisi HI semantics, POORI tarah ALAG implementation.
// ============================================================
struct RefEngine {
    std::vector<Order> resting;
    std::uint64_t next_seq       = 1;
    std::uint64_t next_trade_id  = 1;

    bool has_id(OrderId id) const {
        for (const auto& o : resting) if (o.id == id) return true;
        return false;
    }

    bool best_price(bool is_buy, Price& out) const {
        bool found = false; Price best = 0;
        for (const auto& o : resting) {
            if (o.is_buy != is_buy) continue;
            if (!found || (is_buy ? o.price > best : o.price < best)) { best = o.price; found = true; }
        }
        out = best;
        return found;
    }
    bool has_bid() const { Price p = 0; return best_price(true, p); }
    bool has_ask() const { Price p = 0; return best_price(false, p); }
    Price best_bid() const { Price p = 0; best_price(true, p); return p; }
    Price best_ask() const { Price p = 0; best_price(false, p); return p; }
    std::size_t resting_count() const { return resting.size(); }
    const Order* find_resting(OrderId id) const {
        for (const auto& o : resting) if (o.id == id) return &o;
        return nullptr;
    }

    // Linear scan: best-price, FIFO(seq) tie-break, opposite side of incoming.
    int best_opposite_index(bool incoming_is_buy) const {
        int best_idx = -1;
        for (std::size_t i = 0; i < resting.size(); ++i) {
            const Order& o = resting[i];
            if (o.is_buy == incoming_is_buy) continue;
            if (best_idx < 0) { best_idx = static_cast<int>(i); continue; }
            const Order& b = resting[static_cast<std::size_t>(best_idx)];
            const bool better = incoming_is_buy
                ? (o.price < b.price || (o.price == b.price && o.seq < b.seq))
                : (o.price > b.price || (o.price == b.price && o.seq < b.seq));
            if (better) best_idx = static_cast<int>(i);
        }
        return best_idx;
    }

    std::vector<Trade> match(Order& incoming, bool& stp_aborted) {
        std::vector<Trade> trades;
        while (incoming.qty > 0) {
            const int idx = best_opposite_index(incoming.is_buy);
            if (idx < 0) break;
            Order& r = resting[static_cast<std::size_t>(idx)];

            if (incoming.type != OrderType::Market) {
                const bool crosses = incoming.is_buy ? (incoming.price >= r.price) : (incoming.price <= r.price);
                if (!crosses) break;
            }

            if (incoming.stp != StpMode::None && incoming.participant == r.participant) {
                if (incoming.stp == StpMode::CancelOldest) {
                    resting.erase(resting.begin() + idx);
                    continue;
                }
                if (incoming.stp == StpMode::CancelBoth) {
                    resting.erase(resting.begin() + idx);
                    stp_aborted = true;
                    return trades;
                }
                stp_aborted = true;   // CancelNewest
                return trades;
            }

            const Qty fill = std::min(incoming.qty, r.qty);
            const Price px = r.price;
            const OrderId rid = r.id;
            const ParticipantId rp = r.participant;
            incoming.qty -= fill;
            r.qty -= fill;

            trades.push_back(Trade{ next_trade_id++, incoming.id, rid, incoming.participant, rp,
                                     incoming.is_buy, px, fill, next_seq++ });

            if (r.qty == 0) resting.erase(resting.begin() + idx);
        }
        return trades;
    }

    SubmitResult submit(Order incoming) {
        incoming.orig_qty = incoming.qty;
        incoming.seq = next_seq++;

        if (has_id(incoming.id)) return {OrderStatus::Rejected, {}};

        if (incoming.type == OrderType::FOK) {
            RefEngine dry = *this;              // POORA state copy -- discard-able "practice run"
            Order tmp = incoming;
            bool dummy = false;
            dry.match(tmp, dummy);
            const Qty filled = incoming.orig_qty - tmp.qty;
            if (filled < incoming.orig_qty) return {OrderStatus::Rejected, {}};
        }

        bool stp_aborted = false;
        auto trades = match(incoming, stp_aborted);

        if (incoming.qty == 0) return {OrderStatus::Filled, std::move(trades)};
        if (incoming.type == OrderType::Limit && !stp_aborted) {
            resting.push_back(incoming);
            return {trades.empty() ? OrderStatus::New : OrderStatus::PartiallyFilled, std::move(trades)};
        }
        return {OrderStatus::Cancelled, std::move(trades)};
    }

    bool cancel(OrderId id) {
        for (std::size_t i = 0; i < resting.size(); ++i) {
            if (resting[i].id == id) { resting.erase(resting.begin() + static_cast<long>(i)); return true; }
        }
        return false;
    }
    SubmitResult replace(OrderId old_id, Order new_order) {
        cancel(old_id);
        return submit(std::move(new_order));
    }
};

static bool trades_equal(const std::vector<Trade>& a, const std::vector<Trade>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].id != b[i].id || a[i].aggressor_id != b[i].aggressor_id ||
            a[i].resting_id != b[i].resting_id || a[i].price != b[i].price ||
            a[i].qty != b[i].qty || a[i].seq != b[i].seq) return false;
    }
    return true;
}

int main() {
    constexpr std::size_t N = 30000;
    const auto cmds = generate_commands(N, /*seed=*/2024);

    MatchingEngine real;
    RefEngine      ref;

    std::size_t agree = 0, disagree = 0;
    std::size_t fok_orders = 0, ioc_orders = 0, market_orders = 0;
    std::size_t self_trade_leaks = 0;   // invariant: stp!=None -> NEVER a same-participant trade
    std::size_t first_mismatch = N;

    for (std::size_t i = 0; i < cmds.size(); ++i) {
        const Cmd& c = cmds[i];
        if (c.kind == CmdKind::Submit) {
            const auto r_real = real.submit(c.order);
            const auto r_ref  = ref.submit(c.order);

            const bool ok = (r_real.status == r_ref.status) && trades_equal(r_real.trades, r_ref.trades);
            if (ok) ++agree; else { ++disagree; if (first_mismatch == N) first_mismatch = i; }

            // ---- invariant 1: FOK all-or-nothing (never a partial fill) ----
            if (c.order.type == OrderType::FOK) {
                ++fok_orders;
                Qty filled = 0;
                for (const auto& t : r_real.trades) filled += t.qty;
                const bool valid = (r_real.status == OrderStatus::Filled && filled == c.order.orig_qty) ||
                                   (r_real.status == OrderStatus::Rejected && r_real.trades.empty());
                if (!valid) {
                    std::printf("FOK INVARIANT VIOLATED at cmd %zu: status=%s filled=%u orig=%u\n",
                                i, to_string(r_real.status), filled, c.order.orig_qty);
                }
            }
            if (c.order.type == OrderType::IOC) ++ioc_orders;
            if (c.order.type == OrderType::Market) ++market_orders;

            // ---- invariant 2: STP -- no leaked same-participant trade ----
            if (c.order.stp != StpMode::None) {
                for (const auto& t : r_real.trades) {
                    if (t.aggressor_participant == t.resting_participant) ++self_trade_leaks;
                }
            }
        } else {
            const bool rr = real.cancel(c.cancel_id);
            const bool rf = ref.cancel(c.cancel_id);
            if (rr != rf) { ++disagree; if (first_mismatch == N) first_mismatch = i; } else ++agree;
        }

        // Periodic cross-check on queryable state too (best bid/ask/count).
        if (i % 500 == 0) {
            const bool state_ok = (real.resting_count() == ref.resting_count()) &&
                                   (real.has_bid() == ref.has_bid()) &&
                                   (real.has_ask() == ref.has_ask()) &&
                                   (!real.has_bid() || real.best_bid() == ref.best_bid()) &&
                                   (!real.has_ask() || real.best_ask() == ref.best_ask());
            if (!state_ok) {
                std::printf("STATE MISMATCH at cmd %zu\n", i);
                ++disagree;
            }
        }
    }

    std::printf("=== Fuzz result (N=%zu, seed=2024) ===\n", N);
    std::printf("agree: %zu   disagree: %zu\n", agree, disagree);
    if (disagree > 0) {
        std::printf("FIRST mismatch at cmd #%zu\n", first_mismatch);
    }
    std::printf("order-type mix hit: FOK=%zu IOC=%zu Market=%zu\n", fok_orders, ioc_orders, market_orders);
    std::printf("self-trade leaks despite STP requested: %zu (expect 0)\n", self_trade_leaks);
    std::printf("final resting_count: real=%zu ref=%zu\n", real.resting_count(), ref.resting_count());

    const bool pass = (disagree == 0) && (self_trade_leaks == 0);
    std::printf("\n%s\n", pass ? "ALL CHECKS PASSED" : "FUZZ FOUND A BUG");
    return pass ? 0 : 1;
}
