// wire_protocol.hpp
// ============================================================
// Shared ITCH-style binary market-data wire format for folder 38 examples.
// Har example .cpp yeh header include karta hai -- protocol EK jagah
// define hai, sab examples usi ek "feed" ki language bolte hain.
// ============================================================
// Design choices (yeh sab lessons mein explain honge):
//  - Wire is BIG-ENDIAN (network byte order) -- jaise real ITCH-style
//    feeds -- isliye har multi-byte field read pe byteswap chahiye (10).
//  - Price INTEGER TICKS mein hai, double nahi (37/08 se seedha connect).
//  - symbol_id ek DENSE INTEGER hai, ticker string nahi (37/06, fast
//    array-index lookup ke liye, no hashing on hot path).
//  - Har message apna khud ka `length` carry karta (header mein) --
//    isliye parser ko HAR message type ka size hardcode nahi karna
//    padta; `hdr.length` se hi agle message tak jump kar sakta (11).
// ============================================================
#pragma once

#include <bit>       // std::endian
#include <cstddef>
#include <cstdint>
#include <cstring>   // std::memcpy
#include <vector>

// -----------------------------------------------------------------
// Message type tags (1 byte)
// -----------------------------------------------------------------
enum : std::uint8_t {
    MSG_ADD_ORDER = 'A',   // naya order book mein
    MSG_EXECUTE   = 'E',   // (partial/full) fill
    MSG_CANCEL    = 'X',   // partial cancel (qty reduce)
    MSG_DELETE    = 'D',   // poora cancel (order khatam)
    MSG_REPLACE   = 'U',   // cancel-replace (naya id/price/qty)
};

#pragma pack(push, 1)

// Har message ka common 16-byte header. `length` = poore message ka size
// (header included) -- 11-message-framing ka core. `seq_num` = feed-wide
// monotonic counter -- 04-sequence-numbers ka core. `exch_ts_ns` = exchange
// ne yeh message KAB generate kiya (ns, session-relative) -- 14-timestamping.
struct MsgHeader {
    std::uint16_t length;      // total bytes, header included
    std::uint8_t  msg_type;    // MSG_* tag
    std::uint8_t  _reserved;   // explicit pad byte (documents intent, not implicit gap)
    std::uint32_t seq_num;     // monotonic per-feed sequence number
    std::uint64_t exch_ts_ns;  // exchange-assigned timestamp (14-timestamping-and-clocks)
};
static_assert(sizeof(MsgHeader) == 16, "header must be exactly 16 bytes");

struct AddOrderMsg {
    MsgHeader     hdr;
    std::uint64_t order_id;
    std::uint32_t symbol_id;    // dense id, NOT a ticker string
    std::uint32_t qty;
    std::int64_t  price_ticks;  // integer ticks, not double
    std::uint8_t  side;         // 'B' or 'S'
};

struct ExecuteMsg {
    MsgHeader     hdr;
    std::uint64_t order_id;
    std::uint32_t exec_qty;
};

struct CancelMsg {   // partial cancel / reduce
    MsgHeader     hdr;
    std::uint64_t order_id;
    std::uint32_t cancel_qty;
};

struct DeleteMsg {   // full cancel
    MsgHeader     hdr;
    std::uint64_t order_id;
};

struct ReplaceMsg {  // cancel-replace: old id retired, new id/qty/price live
    MsgHeader     hdr;
    std::uint64_t old_order_id;
    std::uint64_t new_order_id;
    std::uint32_t qty;
    std::int64_t  price_ticks;
};

#pragma pack(pop)

// -----------------------------------------------------------------
// Byte-order helpers
// -----------------------------------------------------------------
// Wire = big-endian (network byte order). Host (x86/ARM, jis pe yeh sab
// chalta) = little-endian. C++23 mein std::byteswap hai; -std=c++20 pe
// hum GCC/Clang ke builtins seedha use karte (10-endianness-handling.md).
inline std::uint16_t bswap16(std::uint16_t v) { return __builtin_bswap16(v); }
inline std::uint32_t bswap32(std::uint32_t v) { return __builtin_bswap32(v); }
inline std::uint64_t bswap64(std::uint64_t v) { return __builtin_bswap64(v); }

// net_to_host*: `if constexpr` se portable -- agar host khud big-endian
// hota (rare, kuch embedded/mainframe), koi swap hi nahi chahiye.
inline std::uint16_t net_to_host16(std::uint16_t v) {
    if constexpr (std::endian::native == std::endian::little) return bswap16(v);
    else return v;
}
inline std::uint32_t net_to_host32(std::uint32_t v) {
    if constexpr (std::endian::native == std::endian::little) return bswap32(v);
    else return v;
}
inline std::uint64_t net_to_host64(std::uint64_t v) {
    if constexpr (std::endian::native == std::endian::little) return bswap64(v);
    else return v;
}
inline std::int64_t net_to_host_i64(std::int64_t v) {
    std::uint64_t u;
    std::memcpy(&u, &v, sizeof u);          // safe reinterpret (no aliasing UB)
    u = net_to_host64(u);
    std::memcpy(&v, &u, sizeof v);
    return v;
}
inline std::uint16_t host_to_net16(std::uint16_t v) { return net_to_host16(v); }  // swap is its own inverse
inline std::uint32_t host_to_net32(std::uint32_t v) { return net_to_host32(v); }
inline std::uint64_t host_to_net64(std::uint64_t v) { return net_to_host64(v); }
inline std::int64_t  host_to_net_i64(std::int64_t v) { return net_to_host_i64(v); }

inline const char* msg_type_name(std::uint8_t t) {
    switch (t) {
        case MSG_ADD_ORDER: return "Add";
        case MSG_EXECUTE:   return "Execute";
        case MSG_CANCEL:    return "Cancel";
        case MSG_DELETE:    return "Delete";
        case MSG_REPLACE:   return "Replace";
        default:            return "Unknown";
    }
}

// -----------------------------------------------------------------
// Safe framing helper -- peek the 8-byte header (memcpy, alignment-safe),
// swap to host order, tell the caller the message's total length + type
// BEFORE deciding how to consume the body. Returns false if fewer than
// 8 bytes remain (partial header -- caller must wait for more, 11).
// -----------------------------------------------------------------
inline bool peek_header(const std::byte* buf, std::size_t remaining, MsgHeader& out) {
    if (remaining < sizeof(MsgHeader)) return false;
    MsgHeader raw;
    std::memcpy(&raw, buf, sizeof raw);
    out.length     = net_to_host16(raw.length);
    out.msg_type   = raw.msg_type;
    out._reserved  = raw._reserved;
    out.seq_num    = net_to_host32(raw.seq_num);
    out.exch_ts_ns = net_to_host64(raw.exch_ts_ns);
    return true;
}

// -----------------------------------------------------------------
// Deterministic PRNG (splitmix64) -- same seed = same feed, every run,
// every machine. Used only to GENERATE synthetic test data, never on a
// parsing hot path.
// -----------------------------------------------------------------
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

// -----------------------------------------------------------------
// Feed generation -- builds a byte buffer of wire-format messages,
// big-endian, framed, sequenced. `append_*` are also useful standalone
// (build one specific message for a targeted test).
// -----------------------------------------------------------------
inline void append_add_order(std::vector<std::byte>& out, std::uint32_t seq, std::uint64_t exch_ts_ns,
                              std::uint64_t order_id, std::uint32_t symbol_id,
                              std::uint32_t qty, std::int64_t price_ticks, char side) {
    AddOrderMsg m{};
    m.hdr.length     = host_to_net16(static_cast<std::uint16_t>(sizeof(AddOrderMsg)));
    m.hdr.msg_type   = MSG_ADD_ORDER;
    m.hdr._reserved  = 0;
    m.hdr.seq_num    = host_to_net32(seq);
    m.hdr.exch_ts_ns = host_to_net64(exch_ts_ns);
    m.order_id      = host_to_net64(order_id);
    m.symbol_id     = host_to_net32(symbol_id);
    m.qty           = host_to_net32(qty);
    m.price_ticks   = host_to_net_i64(price_ticks);
    m.side          = static_cast<std::uint8_t>(side);
    const auto* p = reinterpret_cast<const std::byte*>(&m);
    out.insert(out.end(), p, p + sizeof(m));
}

inline void append_execute(std::vector<std::byte>& out, std::uint32_t seq, std::uint64_t exch_ts_ns,
                            std::uint64_t order_id, std::uint32_t exec_qty) {
    ExecuteMsg m{};
    m.hdr.length     = host_to_net16(static_cast<std::uint16_t>(sizeof(ExecuteMsg)));
    m.hdr.msg_type   = MSG_EXECUTE;
    m.hdr._reserved  = 0;
    m.hdr.seq_num    = host_to_net32(seq);
    m.hdr.exch_ts_ns = host_to_net64(exch_ts_ns);
    m.order_id      = host_to_net64(order_id);
    m.exec_qty      = host_to_net32(exec_qty);
    const auto* p = reinterpret_cast<const std::byte*>(&m);
    out.insert(out.end(), p, p + sizeof(m));
}

inline void append_cancel(std::vector<std::byte>& out, std::uint32_t seq, std::uint64_t exch_ts_ns,
                           std::uint64_t order_id, std::uint32_t cancel_qty) {
    CancelMsg m{};
    m.hdr.length     = host_to_net16(static_cast<std::uint16_t>(sizeof(CancelMsg)));
    m.hdr.msg_type   = MSG_CANCEL;
    m.hdr._reserved  = 0;
    m.hdr.seq_num    = host_to_net32(seq);
    m.hdr.exch_ts_ns = host_to_net64(exch_ts_ns);
    m.order_id      = host_to_net64(order_id);
    m.cancel_qty    = host_to_net32(cancel_qty);
    const auto* p = reinterpret_cast<const std::byte*>(&m);
    out.insert(out.end(), p, p + sizeof(m));
}

inline void append_delete(std::vector<std::byte>& out, std::uint32_t seq, std::uint64_t exch_ts_ns,
                           std::uint64_t order_id) {
    DeleteMsg m{};
    m.hdr.length     = host_to_net16(static_cast<std::uint16_t>(sizeof(DeleteMsg)));
    m.hdr.msg_type   = MSG_DELETE;
    m.hdr._reserved  = 0;
    m.hdr.seq_num    = host_to_net32(seq);
    m.hdr.exch_ts_ns = host_to_net64(exch_ts_ns);
    m.order_id      = host_to_net64(order_id);
    const auto* p = reinterpret_cast<const std::byte*>(&m);
    out.insert(out.end(), p, p + sizeof(m));
}

inline void append_replace(std::vector<std::byte>& out, std::uint32_t seq, std::uint64_t exch_ts_ns,
                            std::uint64_t old_id, std::uint64_t new_id,
                            std::uint32_t qty, std::int64_t price_ticks) {
    ReplaceMsg m{};
    m.hdr.length     = host_to_net16(static_cast<std::uint16_t>(sizeof(ReplaceMsg)));
    m.hdr.msg_type   = MSG_REPLACE;
    m.hdr._reserved  = 0;
    m.hdr.seq_num    = host_to_net32(seq);
    m.hdr.exch_ts_ns = host_to_net64(exch_ts_ns);
    m.old_order_id   = host_to_net64(old_id);
    m.new_order_id   = host_to_net64(new_id);
    m.qty            = host_to_net32(qty);
    m.price_ticks    = host_to_net_i64(price_ticks);
    const auto* p = reinterpret_cast<const std::byte*>(&m);
    out.insert(out.end(), p, p + sizeof(m));
}

// A realistic mixed-lifecycle feed: ~55% Add, rest Execute/Cancel/Delete/
// Replace against previously-added (still "live") order ids -- so a
// consumer that tracks orders will never see a reference to an id it
// hasn't seen an Add for. Deterministic: same (count, start_seq, seed)
// always produces byte-identical output.
// `base_ts_ns` = pehle message ka exchange timestamp; har agla message
// +500ns se +5000ns tak (random, busy-feed-jaisa) aage badhta -- 07 mein
// yeh inter-message gaps hi "kitni tez feed hai" dikhaate.
inline std::vector<std::byte> generate_feed(std::size_t count, std::uint32_t start_seq,
                                             std::uint64_t seed, std::uint32_t num_symbols = 32,
                                             std::uint64_t base_ts_ns = 1'000'000'000ULL) {
    std::vector<std::byte> buf;
    buf.reserve(count * 40);
    SimpleRng rng(seed);
    std::vector<std::uint64_t> live;
    live.reserve(4096);
    std::uint64_t next_id = 1;
    std::uint64_t ts = base_ts_ns;

    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t seq    = start_seq + static_cast<std::uint32_t>(i);
        const std::uint32_t symbol = rng.next_u32() % num_symbols;
        const double        r      = rng.next_unit();
        ts += 500u + (rng.next_u32() % 4500u);   // +500..4999 ns gap to next message

        if (live.empty() || r < 0.55) {
            const std::uint64_t id  = next_id++;
            const std::uint32_t qty = 100u + (rng.next_u32() % 900u);
            const std::int64_t  px  = 10000 + static_cast<std::int64_t>(rng.next_u32() % 500u);
            const char side = (rng.next_u32() & 1u) ? 'B' : 'S';
            append_add_order(buf, seq, ts, id, symbol, qty, px, side);
            live.push_back(id);
        } else {
            const std::size_t idx = static_cast<std::size_t>(rng.next_u64() % live.size());
            const std::uint64_t id = live[idx];
            const double kr = rng.next_unit();
            if (kr < 0.40) {
                append_execute(buf, seq, ts, id, 50u + (rng.next_u32() % 50u));
            } else if (kr < 0.65) {
                append_cancel(buf, seq, ts, id, 10u + (rng.next_u32() % 40u));
            } else if (kr < 0.85) {
                append_delete(buf, seq, ts, id);
                live[idx] = live.back();
                live.pop_back();
            } else {
                const std::uint64_t new_id = next_id++;
                const std::uint32_t qty    = 100u + (rng.next_u32() % 900u);
                const std::int64_t  px     = 10000 + static_cast<std::int64_t>(rng.next_u32() % 500u);
                append_replace(buf, seq, ts, id, new_id, qty, px);
                live[idx] = new_id;
            }
        }
    }
    return buf;
}
