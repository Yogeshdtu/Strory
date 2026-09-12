// 07_market_data_simulator.cpp
// ============================================================
// Poora synthetic feed generate karo, aur uski shape samjho -- message
// mix, seq numbers, exchange timestamps (inter-message gaps).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 07_market_data_simulator.cpp -o sim && ./sim
// ============================================================

#include "wire_protocol.hpp"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <vector>

int main() {
    // ============================================================
    //  1. FEED GENERATE KARO
    // ============================================================
    constexpr std::size_t N = 50000;
    const auto feed = generate_feed(N, /*start_seq=*/1, /*seed=*/2024);

    std::cout << "=== Generated feed ===\n";
    std::cout << N << " messages, " << feed.size() << " bytes total ("
              << (static_cast<double>(feed.size()) / static_cast<double>(N)) << " bytes/msg avg)\n";

    // ============================================================
    //  2. WALK KARO, STATS NIKAALO
    // ============================================================
    std::size_t counts[5] = {};
    std::vector<double> gaps_ns;
    gaps_ns.reserve(N);
    std::uint64_t prev_ts = 0;
    std::uint32_t prev_seq = 0;
    bool first = true;
    bool seq_ok = true;

    std::size_t offset = 0;
    while (offset < feed.size()) {
        MsgHeader hdr{};
        if (!peek_header(feed.data() + offset, feed.size() - offset, hdr)) break;
        if (feed.size() - offset < hdr.length) break;

        switch (hdr.msg_type) {
            case MSG_ADD_ORDER: ++counts[0]; break;
            case MSG_EXECUTE:   ++counts[1]; break;
            case MSG_CANCEL:    ++counts[2]; break;
            case MSG_DELETE:    ++counts[3]; break;
            case MSG_REPLACE:   ++counts[4]; break;
            default: break;
        }

        if (!first) {
            if (hdr.seq_num != prev_seq + 1) seq_ok = false;
            gaps_ns.push_back(static_cast<double>(hdr.exch_ts_ns - prev_ts));
        }
        prev_ts  = hdr.exch_ts_ns;
        prev_seq = hdr.seq_num;
        first    = false;

        offset += hdr.length;
    }

    // ============================================================
    //  3. MESSAGE MIX
    // ============================================================
    std::cout << "\n=== Message mix ===\n";
    const char* names[5] = {"Add", "Execute", "Cancel", "Delete", "Replace"};
    for (int i = 0; i < 5; ++i) {
        const double pct = 100.0 * static_cast<double>(counts[static_cast<std::size_t>(i)]) / static_cast<double>(N);
        std::cout << std::setw(8) << names[i] << ": " << std::setw(6) << counts[static_cast<std::size_t>(i)]
                  << "  (" << std::fixed << std::setprecision(1) << pct << "%)\n";
    }

    // ============================================================
    //  4. SEQUENCE + TIMESTAMP HEALTH (04-sequence-numbers, 14-timestamping)
    // ============================================================
    std::cout << "\n=== Sequence + timestamp health ===\n";
    std::cout << "seq_num strictly +1 monotonic? " << (seq_ok ? "haan" : "NAHI (gap ya duplicate)") << '\n';

    std::sort(gaps_ns.begin(), gaps_ns.end());
    if (!gaps_ns.empty()) {
        const double min_gap = gaps_ns.front();
        const double max_gap = gaps_ns.back();
        const double median  = gaps_ns[gaps_ns.size() / 2];
        double sum = 0;
        for (double g : gaps_ns) sum += g;
        const double mean = sum / static_cast<double>(gaps_ns.size());

        std::cout << "inter-message gap (exch_ts_ns delta): min=" << min_gap
                  << " median=" << median << " mean=" << std::fixed << std::setprecision(1) << mean
                  << " max=" << max_gap << " ns\n";
        std::cout << "=> is synthetic feed ki throughput ~ " << std::setprecision(0)
                  << (1e9 / mean) << " msgs/sec (exchange-clock-relative)\n";
    }

    // ============================================================
    //  5. DETERMINISM PROOF -- same (count, seed) = byte-identical
    // ============================================================
    const auto feed2 = generate_feed(N, 1, 2024);
    const bool identical = (feed.size() == feed2.size()) &&
                            std::memcmp(feed.data(), feed2.data(), feed.size()) == 0;
    std::cout << "\nsame (count, start_seq, seed) dubara call karne se byte-identical feed? "
              << (identical ? "haan" : "NAHI") << " (deterministic testing ke liye zaroori)\n";

    return 0;
}
