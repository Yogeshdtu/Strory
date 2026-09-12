// 09_orderbook_fuzz.cpp
// ============================================================
// Fuzzing harness -- 16-testing-order-book.md. `generate_workload()`
// (order_workload.hpp) sirf VALID ops banaata (isliye happy-path tests
// hi karta). Yahan jaan-boojh kar EDGE CASES inject karte hain:
//   - duplicate add (already-live id)
//   - cancel/execute/delete ek RANDOM (aksar nonexistent) id pe
//   - over-execute / over-cancel (qty > order ki remaining qty)
//   - replace ek already-gone old_id pe
// Har op DONO teeno books (V1/V2/V3) PAR, aur ek independent REFERENCE
// model (simple, obviously-correct, O(n) allowed) par apply hota --
// teeno ka result (success/fail + resulting state) reference se match
// karna chahiye, HAR op ke baad.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 09_orderbook_fuzz.cpp -o fuzz && ./fuzz
// ============================================================

#include "order_workload.hpp"   // SimpleRng
#include "orderbook_v1_map.hpp"
#include "orderbook_v2_vector.hpp"
#include "orderbook_v3_flat.hpp"

#include <algorithm>
#include <cstdio>
#include <unordered_map>
#include <vector>

// ---- reference model: simple, deliberately NOT optimized, easy to
// convince yourself is correct by inspection ----
struct RefOrder { bool is_buy; Price price; Qty qty; };

struct RefBook {
    std::unordered_map<OrderId, RefOrder> live;

    bool add(OrderId id, bool is_buy, Price price, Qty qty) {
        if (live.count(id)) return false;
        live[id] = RefOrder{is_buy, price, qty};
        return true;
    }
    bool reduce(OrderId id, Qty qty) {
        auto it = live.find(id);
        if (it == live.end()) return false;
        const Qty amt = std::min(qty, it->second.qty);
        it->second.qty -= amt;
        if (it->second.qty == 0) live.erase(it);
        return true;
    }
    bool remove(OrderId id) {
        auto it = live.find(id);
        if (it == live.end()) return false;
        live.erase(it);
        return true;
    }
    bool replace(OrderId old_id, OrderId new_id, bool is_buy, Price price, Qty qty) {
        remove(old_id);
        return add(new_id, is_buy, price, qty);
    }
    std::size_t order_count() const { return live.size(); }
    bool has_bid() const { for (auto& kv : live) if (kv.second.is_buy) return true; return false; }
    bool has_ask() const { for (auto& kv : live) if (!kv.second.is_buy) return true; return false; }
    // O(n) scan -- deliberately naive, isi ki simplicity hi correctness ka proof hai
    Price best_bid() const {
        Price best = 0; bool found = false;
        for (auto& kv : live) if (kv.second.is_buy && (!found || kv.second.price > best)) { best = kv.second.price; found = true; }
        return best;
    }
    Price best_ask() const {
        Price best = 0; bool found = false;
        for (auto& kv : live) if (!kv.second.is_buy && (!found || kv.second.price < best)) { best = kv.second.price; found = true; }
        return best;
    }
};

int main() {
    constexpr std::size_t N = 30000;
    constexpr Price CENTER = 10000;
    SimpleRng rng(0xF00DCAFEULL);

    RefBook ref;
    BookV1 v1;
    BookV2 v2;
    BookV3 v3(CENTER, N + 10);

    std::vector<OrderId> ever_added;   // includes orders that may now be gone -- for "target a maybe-dead id" fuzzing
    ever_added.reserve(N);
    OrderId next_id = 1;

    std::size_t agree = 0, disagree = 0;
    std::size_t invalid_injected = 0;

    for (std::size_t i = 0; i < N; ++i) {
        const double r = rng.next_unit();
        bool r1, r2, r3, rr;   // each version's + reference's return value

        if (r < 0.02 && !ever_added.empty()) {
            // INJECT: duplicate add -- reuse an id that was ADDED before (may be live or dead)
            ++invalid_injected;
            const OrderId dup_id = ever_added[static_cast<std::size_t>(rng.next_u64() % ever_added.size())];
            const bool is_buy = (rng.next_u32() & 1u) != 0;
            const Price price = is_buy ? (CENTER - 1 - static_cast<Price>(rng.next_u32() % 200))
                                        : (CENTER + 1 + static_cast<Price>(rng.next_u32() % 200));
            r1 = v1.add(dup_id, is_buy, price, 10);
            r2 = v2.add(dup_id, is_buy, price, 10);
            r3 = v3.add(dup_id, is_buy, price, 10);
            rr = ref.add(dup_id, is_buy, price, 10);
        } else if (r < 0.10) {
            // INJECT: op on a random (likely nonexistent) id
            ++invalid_injected;
            const OrderId random_id = 1 + (rng.next_u64() % (next_id + 1000));
            const double kr = rng.next_unit();
            if (kr < 0.5) {
                const Qty qty = 1u + (rng.next_u32() % 100u);
                r1 = v1.reduce(random_id, qty); r2 = v2.reduce(random_id, qty);
                r3 = v3.reduce(random_id, qty); rr = ref.reduce(random_id, qty);
            } else {
                r1 = v1.remove(random_id); r2 = v2.remove(random_id);
                r3 = v3.remove(random_id); rr = ref.remove(random_id);
            }
        } else if (r < 0.55 || ref.live.empty()) {
            // ADD (normal)
            const bool is_buy = (rng.next_u32() & 1u) != 0;
            const Price price = is_buy ? (CENTER - 1 - static_cast<Price>(rng.next_u32() % 200))
                                        : (CENTER + 1 + static_cast<Price>(rng.next_u32() % 200));
            const Qty qty = 10u + (rng.next_u32() % 200u);
            const OrderId id = next_id++;
            ever_added.push_back(id);
            r1 = v1.add(id, is_buy, price, qty); r2 = v2.add(id, is_buy, price, qty);
            r3 = v3.add(id, is_buy, price, qty); rr = ref.add(id, is_buy, price, qty);
        } else {
            // pick a LIVE order (per reference), reduce/remove/replace it --
            // occasionally with an OVER-SIZED qty (more than it has left)
            const auto idx = rng.next_u64() % ref.live.size();
            auto it = ref.live.begin();
            std::advance(it, static_cast<std::ptrdiff_t>(idx));
            const OrderId id = it->first;
            const Qty remaining = it->second.qty;
            const double kr = rng.next_unit();

            if (kr < 0.5) {
                // reduce -- sometimes deliberately OVER remaining (clamp behavior test)
                const bool over = rng.next_unit() < 0.3;
                const Qty qty = over ? (remaining + 1u + (rng.next_u32() % 100u))
                                       : (1u + (rng.next_u32() % remaining));
                r1 = v1.reduce(id, qty); r2 = v2.reduce(id, qty);
                r3 = v3.reduce(id, qty); rr = ref.reduce(id, qty);
            } else if (kr < 0.8) {
                r1 = v1.remove(id); r2 = v2.remove(id);
                r3 = v3.remove(id); rr = ref.remove(id);
            } else {
                const bool is_buy = it->second.is_buy;
                const Price price = is_buy ? (CENTER - 1 - static_cast<Price>(rng.next_u32() % 200))
                                            : (CENTER + 1 + static_cast<Price>(rng.next_u32() % 200));
                const Qty qty = 10u + (rng.next_u32() % 200u);
                const OrderId new_id = next_id++;
                ever_added.push_back(new_id);
                r1 = v1.replace(id, new_id, is_buy, price, qty);
                r2 = v2.replace(id, new_id, is_buy, price, qty);
                r3 = v3.replace(id, new_id, is_buy, price, qty);
                rr = ref.replace(id, new_id, is_buy, price, qty);
            }
        }

        const bool return_values_agree = (r1 == rr) && (r2 == rr) && (r3 == rr);
        const bool counts_agree = (v1.order_count() == ref.order_count()) &&
                                   (v2.order_count() == ref.order_count()) &&
                                   (v3.order_count() == ref.order_count());
        bool best_agrees = true;
        if (ref.has_bid()) best_agrees = best_agrees && v1.has_bid() && v2.has_bid() && v3.has_bid() &&
            v1.best_bid() == ref.best_bid() && v2.best_bid() == ref.best_bid() && v3.best_bid() == ref.best_bid();
        if (ref.has_ask()) best_agrees = best_agrees && v1.has_ask() && v2.has_ask() && v3.has_ask() &&
            v1.best_ask() == ref.best_ask() && v2.best_ask() == ref.best_ask() && v3.best_ask() == ref.best_ask();

        if (return_values_agree && counts_agree && best_agrees) {
            ++agree;
        } else {
            ++disagree;
            std::printf("DISAGREEMENT at op %zu: returns(v1=%d v2=%d v3=%d ref=%d) counts_agree=%d best_agrees=%d\n",
                        i, r1, r2, r3, rr, counts_agree, best_agrees);
            if (disagree > 5) break;   // stop early -- ek chhota diagnostic sample kaafi hai
        }
    }

    std::printf("=== Fuzz result: %zu ops (%zu edge-case injected) ===\n", N, invalid_injected);
    std::printf("agree: %zu / %zu  disagree: %zu\n", agree, agree + disagree, disagree);
    std::printf("final order_count: ref=%zu v1=%zu v2=%zu v3=%zu\n",
                ref.order_count(), v1.order_count(), v2.order_count(), v3.order_count());

    return disagree == 0 ? 0 : 1;
}
