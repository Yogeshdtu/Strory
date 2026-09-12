// mh_feed_parser.hpp
// ============================================================
// PROJECT 2 -- Binary feed parser: v1 (simple) -> v3 (fast).
//
// Wire = big-endian packed, 38-byte frames (mh_market_data.hpp `encode`).
//
//   parse_v1 : portable, field-by-field shift-and-or BE reads, har frame
//              pe length check. Kaam har host pe, easy to read.
//   parse_v3 : per-field `memcpy` (8/4 bytes) + `__builtin_bswap` -- no
//              shift loop, compiler ise aksar ek `movbe`/`bswap` mein
//              fold kar deta. Length ek baar validate hota (frame level),
//              hot loop mein nahi.
//
// Dono BILKUL same MdMessage dete -- 04_feed_parser.cpp isko assert karta
// (agreement gate) benchmark se PEHLE (43/01 methodology).
// ============================================================
#pragma once

#include "mh_market_data.hpp"

#include <cstring>

namespace mhft {

// ---------- v1: portable, safe ----------
inline std::uint32_t rd_u32_be_v1(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) <<  8) |
           (static_cast<std::uint32_t>(p[3]));
}
inline std::uint64_t rd_u64_be_v1(const std::uint8_t* p) {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | p[i];
    return v;
}

// returns bytes consumed (kWireSize) or 0 if `avail` too small.
inline std::size_t parse_v1(const std::uint8_t* buf, std::size_t avail, MdMessage& m) {
    if (avail < kWireSize) return 0;
    m.type     = static_cast<MdType>(buf[0]);
    m.side     = static_cast<Side>(buf[1]);
    m.qty      = rd_u32_be_v1(buf + 2);
    m.seq      = rd_u64_be_v1(buf + 6);
    m.ts       = rd_u64_be_v1(buf + 14);
    m.order_id = rd_u64_be_v1(buf + 22);
    m.px       = static_cast<Price>(rd_u64_be_v1(buf + 30));
    return kWireSize;
}

// ---------- v3: memcpy + bswap, frame-validated once ----------
inline std::uint32_t rd_u32_be_v3(const std::uint8_t* p) {
    std::uint32_t v;
    std::memcpy(&v, p, 4);
    return __builtin_bswap32(v);
}
inline std::uint64_t rd_u64_be_v3(const std::uint8_t* p) {
    std::uint64_t v;
    std::memcpy(&v, p, 8);
    return __builtin_bswap64(v);
}

// precondition: caller ne pehle hi check kiya ki poora frame available hai
inline void parse_v3(const std::uint8_t* buf, MdMessage& m) {
    m.type     = static_cast<MdType>(buf[0]);
    m.side     = static_cast<Side>(buf[1]);
    m.qty      = rd_u32_be_v3(buf + 2);
    m.seq      = rd_u64_be_v3(buf + 6);
    m.ts       = rd_u64_be_v3(buf + 14);
    m.order_id = rd_u64_be_v3(buf + 22);
    m.px       = static_cast<Price>(rd_u64_be_v3(buf + 30));
}

// Build a packed wire buffer from a MarketDataSimulator run -- both the
// parser project and the mini-engine's "wire" hop use this.
inline std::vector<std::uint8_t> record_wire(MarketDataSimulator& sim, std::uint64_t n) {
    std::vector<std::uint8_t> wire;
    wire.resize(n * kWireSize);
    MdMessage m;
    std::uint64_t i = 0;
    while (i < n && sim.next(m)) {
        encode(m, wire.data() + i * kWireSize);
        ++i;
    }
    wire.resize(i * kWireSize);
    return wire;
}

}  // namespace mhft
