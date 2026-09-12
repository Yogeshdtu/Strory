// 09_ab_arbitration.cpp
// ============================================================
// A/B feed arbitration -- EK hi upstream data, DO independent multicast
// paths (A aur B) se bheja jaata (real exchanges aksar yeh karte). Har
// path apni ALAG jagah packet khoti hai. Combine karke, single-path loss
// ka zyaadatar hissa BINA kisi round-trip/retransmit ke fill ho jaata.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 09_ab_arbitration.cpp -o ab && ./ab
// ============================================================

#include "wire_protocol.hpp"

#include <iostream>
#include <vector>

static std::vector<std::byte> simulate_loss(const std::vector<std::byte>& feed,
                                             double keep_prob, std::uint64_t seed) {
    std::vector<std::byte> received;
    received.reserve(feed.size());
    SimpleRng rng(seed);
    std::size_t offset = 0;
    while (offset < feed.size()) {
        MsgHeader hdr{};
        if (!peek_header(feed.data() + offset, feed.size() - offset, hdr)) break;
        if (feed.size() - offset < hdr.length) break;
        if (rng.next_unit() < keep_prob) {
            const auto* p = feed.data() + offset;
            received.insert(received.end(), p, p + hdr.length);
        }
        offset += hdr.length;
    }
    return received;
}

// Ek received stream ko walk karke, present seq numbers ko `seen` mein
// mark karo (seq - base_seq index pe).
static void mark_present(const std::vector<std::byte>& stream, std::uint32_t base_seq,
                          std::vector<char>& seen) {
    std::size_t offset = 0;
    while (offset < stream.size()) {
        MsgHeader hdr{};
        if (!peek_header(stream.data() + offset, stream.size() - offset, hdr)) break;
        if (stream.size() - offset < hdr.length) break;
        const std::size_t idx = hdr.seq_num - base_seq;
        if (idx < seen.size()) seen[idx] = 1;
        offset += hdr.length;
    }
}

int main() {
    // ============================================================
    //  1. UPSTREAM FEED -- EK BAAR banaya, DO paths se "bheja" jaata
    // ============================================================
    constexpr std::size_t N = 20000;
    constexpr std::uint32_t BASE_SEQ = 1;
    const auto upstream = generate_feed(N, BASE_SEQ, /*seed=*/555);

    // Feed A aur Feed B -- SAME upstream, alag paths, alag (independent) loss.
    const auto feed_a = simulate_loss(upstream, /*keep_prob=*/0.99, /*seed=*/111);  // ~1% loss
    const auto feed_b = simulate_loss(upstream, /*keep_prob=*/0.99, /*seed=*/222);  // ~1% loss, INDEPENDENT

    // ============================================================
    //  2. ARBITRATE -- seq-by-seq, "kisi bhi feed mein mila?"
    // ============================================================
    std::vector<char> seen_a(N, 0), seen_b(N, 0);
    mark_present(feed_a, BASE_SEQ, seen_a);
    mark_present(feed_b, BASE_SEQ, seen_b);

    std::size_t only_a = 0, only_b = 0, both = 0, neither = 0;
    for (std::size_t i = 0; i < N; ++i) {
        const bool a = seen_a[i] != 0;
        const bool b = seen_b[i] != 0;
        if (a && b) ++both;
        else if (a && !b) ++only_a;
        else if (!a && b) ++only_b;
        else ++neither;
    }

    const std::size_t recovered_by_arbitration = only_a + only_b;  // ek feed se mila, doosre se miss hua -- COMBINE se recover
    const std::size_t total_present = only_a + only_b + both;      // arbitrated stream mein kitna aaya
    const std::size_t a_alone_missing = N - (only_a + both);       // agar SIRF A use karte, kitna miss hota
    const std::size_t b_alone_missing = N - (only_b + both);       // agar SIRF B use karte, kitna miss hota

    // ============================================================
    //  3. REPORT
    // ============================================================
    std::cout << "=== A/B feed arbitration (" << N << " messages upstream) ===\n\n";
    std::cout << "Feed A alone:  " << (N - a_alone_missing) << " / " << N
              << "  (" << a_alone_missing << " missing)\n";
    std::cout << "Feed B alone:  " << (N - b_alone_missing) << " / " << N
              << "  (" << b_alone_missing << " missing)\n\n";

    std::cout << "in BOTH A and B:      " << both << '\n';
    std::cout << "only in A (B dropped): " << only_a << '\n';
    std::cout << "only in B (A dropped): " << only_b << '\n';
    std::cout << "in NEITHER (both dropped -- true gap): " << neither << '\n';

    std::cout << "\n=== Arbitrated (A+B combined) result ===\n";
    std::cout << "recovered by combining (present in exactly one feed): " << recovered_by_arbitration << '\n';
    std::cout << "total present after arbitration: " << total_present << " / " << N
              << "  (" << neither << " STILL missing -- needs 13-recovery-and-retransmission)\n";

    const double single_feed_loss_pct = 100.0 * static_cast<double>(a_alone_missing) / static_cast<double>(N);
    const double arbitrated_loss_pct  = 100.0 * static_cast<double>(neither) / static_cast<double>(N);
    std::cout << "\nsingle-feed loss: ~" << single_feed_loss_pct << "%"
              << "   arbitrated loss: ~" << arbitrated_loss_pct << "%"
              << "  (" << (single_feed_loss_pct / (arbitrated_loss_pct > 0 ? arbitrated_loss_pct : 1))
              << "x better, ZERO round-trips)\n";

    return 0;
}
