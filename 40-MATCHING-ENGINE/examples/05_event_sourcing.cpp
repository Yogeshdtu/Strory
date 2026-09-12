// 05_event_sourcing.cpp
// ============================================================
// Event log + REPLAY -- SAME command sequence do independent fresh
// engines mein chalao, prove karo trades aur final state BYTE-IDENTICAL
// hain. Yeh determinism (09/11) ka ACTUAL proof hai, sirf claim nahi.
// (10-event-sourcing.md)
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 05_event_sourcing.cpp -o replay && ./replay
// ============================================================

#include "engine_workload.hpp"
#include "matching_engine.hpp"

#include <cstdio>
#include <vector>

// Ek engine pe poora command log APPLY karo, saare generate hue trades
// (order se) ek flat vector mein collect karo -- yeh "replay output" hai
// jo compare karenge.
static std::vector<Trade> run_log(const std::vector<Cmd>& cmds, MatchingEngine& engine) {
    std::vector<Trade> all_trades;
    for (const auto& c : cmds) {
        if (c.kind == CmdKind::Submit) {
            auto r = engine.submit(c.order);
            for (const auto& t : r.trades) all_trades.push_back(t);
        } else {
            engine.cancel(c.cancel_id);
        }
    }
    return all_trades;
}

int main() {
    constexpr std::size_t N = 20000;
    const auto cmds = generate_commands(N, /*seed=*/777);

    std::printf("Command log: %zu commands (seed=777) -- ENGINE A pe chalate hain\n", N);
    MatchingEngine engine_a;
    const auto trades_a = run_log(cmds, engine_a);
    std::printf("  ENGINE A: %zu trades generated, resting_count=%zu\n\n",
                trades_a.size(), engine_a.resting_count());

    std::printf("SAME log, FRESH ENGINE B pe REPLAY (jaise crash ke baad log se\n"
                "state reconstruct karna, ya ek doosre node pe replicate karna):\n");
    MatchingEngine engine_b;
    const auto trades_b = run_log(cmds, engine_b);
    std::printf("  ENGINE B: %zu trades generated, resting_count=%zu\n\n",
                trades_b.size(), engine_b.resting_count());

    // ============================================================
    //  BYTE-IDENTICAL CHECK -- har trade field-by-field compare
    // ============================================================
    bool identical = (trades_a.size() == trades_b.size()) &&
                      (engine_a.resting_count() == engine_b.resting_count());
    std::size_t first_mismatch = trades_a.size();
    if (identical) {
        for (std::size_t i = 0; i < trades_a.size(); ++i) {
            const Trade& x = trades_a[i];
            const Trade& y = trades_b[i];
            if (x.id != y.id || x.aggressor_id != y.aggressor_id || x.resting_id != y.resting_id ||
                x.price != y.price || x.qty != y.qty || x.seq != y.seq) {
                identical = false;
                first_mismatch = i;
                break;
            }
        }
    }

    std::printf("A aur B replay se BYTE-IDENTICAL trades + final state? %s\n",
                identical ? "HAAN" : "NAHI");
    if (!identical) {
        std::printf("  (pehla mismatch trade #%zu pe -- yeh ek DETERMINISM BUG hota,\n"
                    "   production mein is class ka bug replay/replication tod deta)\n", first_mismatch);
        return 1;
    }

    // ============================================================
    //  Best bid/ask bhi match hone chahiye (agar dono ho to)
    // ============================================================
    if (engine_a.has_bid() && engine_b.has_bid()) {
        std::printf("best_bid A=%lld B=%lld match? %s\n",
                    static_cast<long long>(engine_a.best_bid()),
                    static_cast<long long>(engine_b.best_bid()),
                    engine_a.best_bid() == engine_b.best_bid() ? "haan" : "NAHI");
    }
    if (engine_a.has_ask() && engine_b.has_ask()) {
        std::printf("best_ask A=%lld B=%lld match? %s\n",
                    static_cast<long long>(engine_a.best_ask()),
                    static_cast<long long>(engine_b.best_ask()),
                    engine_a.best_ask() == engine_b.best_ask() ? "haan" : "NAHI");
    }

    std::printf("\nYeh proof karta hai: agar tum command log (event source) preserve\n"
                "karte ho, tum KABHI BHI poori book state ko scratch se, kisi bhi machine\n"
                "pe, deterministically reconstruct kar sakte ho -- backup/replication/\n"
                "audit/debugging sab isi property pe depend karte.\n");

    return 0;
}
