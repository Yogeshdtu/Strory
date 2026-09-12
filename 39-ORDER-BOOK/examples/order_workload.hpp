// order_workload.hpp
// ============================================================
// Shared, deterministic order-flow generator for folder 39's order-book
// examples. Har V1/V2/V3 implementation isi SAME operation sequence pe
// test/measure hoti hai -- fair comparison ke liye zaroori.
//
// IMPORTANT SCOPE NOTE (01-order-book-requirements.md mein detail):
// Yeh order book EK "market-data-consuming" book hai -- jo folder
// 38-MARKET-DATA jaisi feed (Add/Execute/Cancel/Delete/Replace events)
// consume karke apni state maintain karta. Yeh KHUD matching/crossing
// NAHI karta (koi price-cross check nahi) -- woh 40-MATCHING-ENGINE ka
// kaam hai. Isliye workload generator bhi "already-decided" events
// generate karta, "incoming aggressive order match karo" nahi.
// ============================================================
#pragma once

#include <cstdint>
#include <vector>

enum class OpKind : std::uint8_t { Add, Execute, Cancel, Delete, Replace };

struct Op {
    OpKind        kind;
    std::uint64_t order_id;
    std::uint64_t new_order_id;  // Replace only
    bool          is_buy;
    std::int64_t  price_ticks;   // Add / Replace only
    std::uint32_t qty;           // Add: order qty. Execute/Cancel: reduce amount.
                                  // Replace: new order's qty.
};

// Deterministic PRNG (splitmix64) -- same seed = same workload, always
// (consistent with wire_protocol.hpp's SimpleRng from folder 38).
struct SimpleRng {
    std::uint64_t state;
    explicit SimpleRng(std::uint64_t seed) : state(seed ? seed : 0x2545F4914F6CDD1DULL) {}
    std::uint64_t next_u64() {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    std::uint32_t next_u32() { return static_cast<std::uint32_t>(next_u64() >> 32); }
    double next_unit() { return static_cast<double>(next_u64() >> 11) * (1.0 / 9007199254740992.0); }
};

// Ek live order ka generator-side bookkeeping (yeh order book ke andar
// NAHI hai -- yeh sirf workload GENERATE karne ke liye hai, taaki
// realistic Execute/Cancel/Replace targets chuna jaa sake).
struct LiveOrder {
    std::uint64_t id;
    bool          is_buy;
    std::int64_t  price_ticks;
    std::uint32_t remaining_qty;
};

// `price_range` = center ke around kitne ticks tak prices spread hoti
// (bids [center-range, center-1], asks [center+1, center+range]) --
// isse realistic "kai orders ek price level pe" clustering milta.
inline std::vector<Op> generate_workload(std::size_t count, std::uint64_t seed,
                                          std::int64_t center = 10000,
                                          std::int64_t price_range = 100) {
    std::vector<Op> ops;
    ops.reserve(count);
    SimpleRng rng(seed);
    std::vector<LiveOrder> live;
    live.reserve(8192);
    std::uint64_t next_id = 1;

    for (std::size_t i = 0; i < count; ++i) {
        const double r = rng.next_unit();

        if (live.empty() || r < 0.55) {
            // ADD
            const bool is_buy = (rng.next_u32() & 1u) != 0;
            const std::int64_t offset = 1 + static_cast<std::int64_t>(rng.next_u32() % static_cast<std::uint32_t>(price_range));
            const std::int64_t price = is_buy ? (center - offset) : (center + offset);
            const std::uint32_t qty = 100u + (rng.next_u32() % 900u);
            const std::uint64_t id = next_id++;
            ops.push_back({OpKind::Add, id, 0, is_buy, price, qty});
            live.push_back({id, is_buy, price, qty});
        } else {
            const std::size_t idx = static_cast<std::size_t>(rng.next_u64() % live.size());
            LiveOrder& lo = live[idx];
            const double kr = rng.next_unit();

            if (kr < 0.35) {
                // EXECUTE (partial or full fill)
                const std::uint32_t max_fill = lo.remaining_qty < 50u ? lo.remaining_qty : 50u;
                const std::uint32_t fill = 1u + (rng.next_u32() % max_fill);
                ops.push_back({OpKind::Execute, lo.id, 0, lo.is_buy, 0, fill});
                lo.remaining_qty -= fill;
                if (lo.remaining_qty == 0) { lo = live.back(); live.pop_back(); }
            } else if (kr < 0.65) {
                // CANCEL (partial reduce)
                const std::uint32_t max_cancel = lo.remaining_qty < 40u ? lo.remaining_qty : 40u;
                const std::uint32_t cancel_qty = 1u + (rng.next_u32() % max_cancel);
                ops.push_back({OpKind::Cancel, lo.id, 0, lo.is_buy, 0, cancel_qty});
                lo.remaining_qty -= cancel_qty;
                if (lo.remaining_qty == 0) { lo = live.back(); live.pop_back(); }
            } else if (kr < 0.85) {
                // DELETE (full cancel)
                ops.push_back({OpKind::Delete, lo.id, 0, lo.is_buy, 0, 0});
                lo = live.back();
                live.pop_back();
            } else {
                // REPLACE (cancel-replace: new id, maybe new price/qty)
                const std::int64_t offset = 1 + static_cast<std::int64_t>(rng.next_u32() % static_cast<std::uint32_t>(price_range));
                const std::int64_t new_price = lo.is_buy ? (center - offset) : (center + offset);
                const std::uint32_t new_qty = 100u + (rng.next_u32() % 900u);
                const std::uint64_t new_id = next_id++;
                ops.push_back({OpKind::Replace, lo.id, new_id, lo.is_buy, new_price, new_qty});
                lo.id = new_id;
                lo.price_ticks = new_price;
                lo.remaining_qty = new_qty;
            }
        }
    }
    return ops;
}
