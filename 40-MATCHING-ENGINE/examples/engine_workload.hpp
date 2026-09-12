// engine_workload.hpp
// ============================================================
// Shared, deterministic command generator -- 05/06/07/08 examples isi se
// apna input banate. Har run SAME seed = SAME commands (10-event-sourcing,
// 09-determinism ka core requirement).
//
// Command = "Submit" (naya order) ya "Cancel" (existing order hatao) --
// yeh ek EVENT LOG hai jo MatchingEngine pe REPLAY kiya jaa sakta.
// ============================================================
#pragma once

#include "matching_engine.hpp"

#include <cstdint>
#include <vector>

enum class CmdKind : std::uint8_t { Submit, Cancel };

struct Cmd {
    CmdKind kind;
    Order   order;       // Submit only
    OrderId cancel_id;   // Cancel only
};

// Deterministic PRNG (splitmix64) -- 38/39 ka SAME idiom.
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

// `num_participants` chhota rakha (default 6) -- taaki STP collisions
// (same participant dono taraf) REALISTICALLY often hon, sirf theoretically
// possible na rahein (large pool mein STP kabhi practically fire hi na hota).
inline std::vector<Cmd> generate_commands(std::size_t count, std::uint64_t seed,
                                           std::size_t num_participants = 6,
                                           Price center = 10000, Price price_range = 40) {
    std::vector<Cmd> cmds;
    cmds.reserve(count);
    SimpleRng rng(seed);
    std::vector<OrderId> recent_ids;   // Cancel target-pool -- gone ids bhi rakhte
                                        // (cancel-on-gone-id EK valid/common case hai)
    recent_ids.reserve(4096);
    OrderId next_id = 1;

    for (std::size_t i = 0; i < count; ++i) {
        const double r = rng.next_unit();

        if (recent_ids.empty() || r < 0.82) {
            // SUBMIT
            const bool is_buy = (rng.next_u32() & 1u) != 0;
            const auto participant = static_cast<ParticipantId>(rng.next_u32() % static_cast<std::uint32_t>(num_participants));
            const std::int64_t offset = 1 + static_cast<std::int64_t>(rng.next_u32() % static_cast<std::uint32_t>(price_range));
            const Price price = is_buy ? (center - offset) : (center + offset);
            const Qty qty = 10u + (rng.next_u32() % 200u);
            const OrderId id = next_id++;

            const double tr = rng.next_unit();
            const double sr = rng.next_unit();
            const StpMode stp = sr < 0.55 ? StpMode::None
                               : sr < 0.70 ? StpMode::CancelNewest
                               : sr < 0.85 ? StpMode::CancelOldest
                                           : StpMode::CancelBoth;

            Order o;
            if (tr < 0.55) {
                o = make_limit(id, participant, is_buy, price, qty, stp);
            } else if (tr < 0.70) {
                o = make_market(id, participant, is_buy, qty, stp);
            } else if (tr < 0.85) {
                o = make_ioc(id, participant, is_buy, price, qty, stp);
            } else {
                o = make_fok(id, participant, is_buy, price, qty, stp);
            }
            cmds.push_back(Cmd{CmdKind::Submit, o, 0});
            recent_ids.push_back(id);
            if (recent_ids.size() > 2000) recent_ids.erase(recent_ids.begin());   // bounded pool
        } else {
            // CANCEL -- recent id pool se (resting ho ya already gone, dono valid)
            const std::size_t idx = static_cast<std::size_t>(rng.next_u64() % recent_ids.size());
            cmds.push_back(Cmd{CmdKind::Cancel, Order{}, recent_ids[idx]});
        }
    }
    return cmds;
}
