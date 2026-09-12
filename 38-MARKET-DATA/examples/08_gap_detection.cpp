// 08_gap_detection.cpp
// ============================================================
// Sequence-number gap detection -- kuch messages "drop" karo (jaise UDP
// packet loss), phir receiver-side logic se pakdo: kaunse seq numbers
// miss hue, kitne, aur kahan.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 08_gap_detection.cpp -o gaps && ./gaps
// ============================================================

#include "wire_protocol.hpp"

#include <iostream>
#include <vector>

// Original feed ko walk karo, HAR message ko `keep_prob` chance se
// rakho -- baaki "drop" ho jaate (jaise real UDP loss). Yeh receiver
// tak sirf jo bacha woh pahunchta.
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

struct GapRange { std::uint32_t from, to; };  // inclusive [from, to] -- missing seq numbers

int main() {
    // ============================================================
    //  1. FEED BANAO, LOSS SIMULATE KARO
    // ============================================================
    constexpr std::size_t N = 20000;
    const auto sent = generate_feed(N, /*start_seq=*/1, /*seed=*/77);
    const auto received = simulate_loss(sent, /*keep_prob=*/0.995, /*seed=*/999);  // ~0.5% loss

    std::cout << "=== Loss simulation ===\n";
    std::cout << "sent: " << N << " messages, " << sent.size() << " bytes\n";
    std::cout << "received (post-loss): " << received.size() << " bytes ("
              << (100.0 * static_cast<double>(sent.size() - received.size()) / static_cast<double>(sent.size()))
              << "% bytes lost)\n";

    // ============================================================
    //  2. GAP DETECTION -- expected_seq track karo
    // ============================================================
    std::vector<GapRange> gaps;
    std::size_t total_missing = 0;
    std::size_t duplicates    = 0;
    std::uint32_t expected_seq = 0;
    bool first = true;

    std::size_t offset = 0;
    while (offset < received.size()) {
        MsgHeader hdr{};
        if (!peek_header(received.data() + offset, received.size() - offset, hdr)) break;
        if (received.size() - offset < hdr.length) break;

        if (first) {
            expected_seq = hdr.seq_num;
            first = false;
        }

        if (hdr.seq_num == expected_seq) {
            // normal -- no gap
        } else if (hdr.seq_num > expected_seq) {
            // GAP: [expected_seq, hdr.seq_num - 1] missing
            const std::uint32_t missing = hdr.seq_num - expected_seq;
            gaps.push_back({expected_seq, hdr.seq_num - 1});
            total_missing += missing;
        } else {
            // hdr.seq_num < expected_seq -- duplicate / out-of-order
            ++duplicates;
        }

        expected_seq = hdr.seq_num + 1;
        offset += hdr.length;
    }

    // ============================================================
    //  3. REPORT
    // ============================================================
    std::cout << "\n=== Gap detection result ===\n";
    std::cout << "gaps detected: " << gaps.size() << "\n";
    std::cout << "total missing sequence numbers: " << total_missing << "\n";
    std::cout << "duplicates/out-of-order seen: " << duplicates << "\n";

    std::cout << "\nfirst " << (gaps.size() < 10 ? gaps.size() : 10) << " gaps:\n";
    for (std::size_t i = 0; i < gaps.size() && i < 10; ++i) {
        const auto& g = gaps[i];
        const std::uint32_t width = g.to - g.from + 1;
        std::cout << "  seq " << g.from << (width > 1 ? "-" : "") << (width > 1 ? std::to_string(g.to) : "")
                  << "  (" << width << " message" << (width > 1 ? "s" : "") << " missing)\n";
    }

    // Sanity: total_missing should roughly match (N - actually-received-count)
    std::size_t received_count = 0;
    {
        std::size_t off = 0;
        while (off < received.size()) {
            MsgHeader h{};
            if (!peek_header(received.data() + off, received.size() - off, h)) break;
            if (received.size() - off < h.length) break;
            ++received_count;
            off += h.length;
        }
    }
    std::cout << "\nsanity: sent=" << N << " received=" << received_count
              << " (sent - received)=" << (N - received_count)
              << " total_missing_from_gaps=" << total_missing
              << "  match? " << ((N - received_count) == total_missing ? "haan" : "NAHI") << '\n';

    std::cout << "\n(real feed handler: gap dekhte hi -- 12-ab-feed-arbitration se dusre feed se\n"
                 " fill try karo, warna 13-recovery-and-retransmission -- snapshot/replay se)\n";

    return 0;
}
