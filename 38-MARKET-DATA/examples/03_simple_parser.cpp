// 03_simple_parser.cpp
// ============================================================
// STEP 1 of the process (CLAUDE.md spec): build simple, CORRECT first.
// No optimization yet -- just parse every message right. 04/05/06 measure
// and optimize this.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 03_simple_parser.cpp -o simple && ./simple
// ============================================================

#include "wire_protocol.hpp"

#include <cstring>
#include <iostream>
#include <vector>

// ============================================================
//  ParsedEvent -- ek OWNED, host-order, swapped copy of a message.
//  Yeh "deserialize into an object" style hai -- readable, ek jagah sab
//  fields, koi wire-format detail caller ko dikhta nahi. Simple hai.
//  (04-05 dikhayenge yeh style ki HIDDEN cost kya hai.)
// ============================================================
struct ParsedEvent {
    std::uint8_t  type;
    std::uint32_t seq;
    std::uint64_t order_id;
    std::uint32_t symbol_id;
    std::uint32_t qty;
    std::int64_t  price_ticks;
    char          side;
};

// Ek message parse karo (buffer + offset se), ek OWNED ParsedEvent return
// karo. Har field memcpy se copy hota (alignment-safe), phir swap hota.
static ParsedEvent parse_one(const std::byte* buf, const MsgHeader& hdr) {
    ParsedEvent ev{};
    ev.type = hdr.msg_type;
    ev.seq  = hdr.seq_num;

    switch (hdr.msg_type) {
        case MSG_ADD_ORDER: {
            AddOrderMsg m{};
            std::memcpy(&m, buf, sizeof m);
            ev.order_id    = net_to_host64(m.order_id);
            ev.symbol_id   = net_to_host32(m.symbol_id);
            ev.qty         = net_to_host32(m.qty);
            ev.price_ticks = net_to_host_i64(m.price_ticks);
            ev.side        = static_cast<char>(m.side);
            break;
        }
        case MSG_EXECUTE: {
            ExecuteMsg m{};
            std::memcpy(&m, buf, sizeof m);
            ev.order_id = net_to_host64(m.order_id);
            ev.qty      = net_to_host32(m.exec_qty);
            break;
        }
        case MSG_CANCEL: {
            CancelMsg m{};
            std::memcpy(&m, buf, sizeof m);
            ev.order_id = net_to_host64(m.order_id);
            ev.qty      = net_to_host32(m.cancel_qty);
            break;
        }
        case MSG_DELETE: {
            DeleteMsg m{};
            std::memcpy(&m, buf, sizeof m);
            ev.order_id = net_to_host64(m.order_id);
            break;
        }
        case MSG_REPLACE: {
            ReplaceMsg m{};
            std::memcpy(&m, buf, sizeof m);
            ev.order_id    = net_to_host64(m.new_order_id);  // simple view: replace = "new" event
            ev.qty         = net_to_host32(m.qty);
            ev.price_ticks = net_to_host_i64(m.price_ticks);
            break;
        }
        default:
            break;
    }
    return ev;
}

// ============================================================
//  Poora buffer parse karo -- HAR message ek ParsedEvent ban ke ek
//  std::vector mein PUSH hota. Simple, correct, readable.
//  (`events` ko yahaan jaan-boojh kar RESERVE nahi kiya -- ek naya
//   feed-handler likhne wale ki natural first-draft yehi hoti; 04 mein
//   isi ka measured cost dikhega.)
// ============================================================
static std::vector<ParsedEvent> parse_feed(const std::vector<std::byte>& buf) {
    std::vector<ParsedEvent> events;   // <- no reserve() (naive default)

    std::size_t offset = 0;
    while (offset < buf.size()) {
        MsgHeader hdr{};
        if (!peek_header(buf.data() + offset, buf.size() - offset, hdr)) {
            break;  // partial header at buffer end -- wait for more bytes (11)
        }
        if (buf.size() - offset < hdr.length) {
            break;  // partial BODY at buffer end -- same reason
        }
        events.push_back(parse_one(buf.data() + offset, hdr));
        offset += hdr.length;
    }
    return events;
}

int main() {
    // ============================================================
    //  CORRECTNESS FIRST -- ek chhota feed generate karo, parse karo,
    //  verify karo ki sab kuch match karta.
    // ============================================================
    constexpr std::size_t N = 10000;
    const auto feed = generate_feed(N, /*start_seq=*/1, /*seed=*/12345);

    const auto events = parse_feed(feed);

    std::cout << "=== Simple parser -- correctness check ===\n";
    std::cout << "messages generated = " << N << "\n";
    std::cout << "messages parsed    = " << events.size() << "\n";

    // Type-by-type count -- generate_feed() ka distribution roughly:
    // ~55% Add, baaki Execute/Cancel/Delete/Replace mixed.
    std::size_t n_add = 0, n_exec = 0, n_cancel = 0, n_delete = 0, n_replace = 0;
    std::uint32_t last_seq = 0;
    bool seq_monotonic = true;
    for (std::size_t i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        switch (e.type) {
            case MSG_ADD_ORDER: ++n_add; break;
            case MSG_EXECUTE:   ++n_exec; break;
            case MSG_CANCEL:    ++n_cancel; break;
            case MSG_DELETE:    ++n_delete; break;
            case MSG_REPLACE:   ++n_replace; break;
            default: break;
        }
        if (i > 0 && e.seq != last_seq + 1) seq_monotonic = false;
        last_seq = e.seq;
    }

    std::cout << "\nAdd=" << n_add << " Execute=" << n_exec << " Cancel=" << n_cancel
              << " Delete=" << n_delete << " Replace=" << n_replace
              << "  (sum=" << (n_add + n_exec + n_cancel + n_delete + n_replace) << ")\n";
    std::cout << "sequence numbers strictly +1 monotonic? "
              << (seq_monotonic ? "haan (sahi -- generate_feed koi gap nahi banaata)" : "NAHI (bug!)")
              << '\n';

    // Ek specific message hand-verify karo (first Add order)
    for (const auto& e : events) {
        if (e.type == MSG_ADD_ORDER) {
            std::cout << "\npehla Add order: seq=" << e.seq << " order_id=" << e.order_id
                      << " symbol_id=" << e.symbol_id << " qty=" << e.qty
                      << " price=" << e.price_ticks << " side=" << e.side << '\n';
            break;
        }
    }

    std::cout << "\n(04_parser_benchmark.cpp isi parser ki LATENCY measure karta)\n";
    return 0;
}
