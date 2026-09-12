// 01_message_structs.cpp
// ============================================================
// Wire structs ka layout dekho -- sizeof, padding, static_asserts
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 01_message_structs.cpp -o structs && ./structs
// ============================================================

#include "wire_protocol.hpp"

#include <iostream>

// ============================================================
//  static_asserts -- layout ko COMPILE TIME pe lock karna
// ============================================================
// `#pragma pack(push, 1)` ke bina, compiler har member ko uske apne
// alignment pe rakhta (padding insert karke) -- ek uint64_t member se
// pehle agar odd offset ho, compiler gap chhod deta. Wire format mein
// yeh **acceptable nahi** -- exchange bytes bhejti hai jo EXACT hone
// chahiye, tumhare compiler ki padding-choice pe depend nahi kar sakte.
// `pack(1)` yeh guarantee deta: struct size = sum of member sizes,
// bilkul, har platform pe.
static_assert(sizeof(MsgHeader) == 16, "header must stay 16 bytes (with exch_ts_ns)");
static_assert(sizeof(AddOrderMsg) == 16 + 8 + 4 + 4 + 8 + 1, "AddOrderMsg layout");
static_assert(sizeof(ExecuteMsg) == 16 + 8 + 4, "ExecuteMsg layout");
static_assert(sizeof(CancelMsg) == 16 + 8 + 4, "CancelMsg layout");
static_assert(sizeof(DeleteMsg) == 16 + 8, "DeleteMsg layout");
static_assert(sizeof(ReplaceMsg) == 16 + 8 + 8 + 4 + 8, "ReplaceMsg layout");

// pack(1) se struct ka apna alignment bhi 1 ho jaata -- isse ek byte
// buffer (alignof 1) pe overlay-cast karna `-Wcast-align` trigger nahi
// karta (05_zero_copy_parser.cpp mein iska use hoga).
static_assert(alignof(AddOrderMsg) == 1, "packed struct must be alignment-1");

int main() {
    // ============================================================
    //  1. SIZES -- packed vs (hypothetically) unpacked
    // ============================================================
    std::cout << "=== Packed wire struct sizes ===\n";
    std::cout << "MsgHeader   " << sizeof(MsgHeader)   << " bytes\n";
    std::cout << "AddOrderMsg " << sizeof(AddOrderMsg) << " bytes\n";
    std::cout << "ExecuteMsg  " << sizeof(ExecuteMsg)  << " bytes\n";
    std::cout << "CancelMsg   " << sizeof(CancelMsg)   << " bytes\n";
    std::cout << "DeleteMsg   " << sizeof(DeleteMsg)   << " bytes\n";
    std::cout << "ReplaceMsg  " << sizeof(ReplaceMsg)  << " bytes\n";

    // ============================================================
    //  2. EK MESSAGE BANAO, RAW BYTES DEKHO
    // ============================================================
    // append_add_order host-order values leta, khud hi network-order
    // (big-endian) bytes likhta buffer mein -- exactly jaisa exchange
    // wire pe bhejti.
    std::vector<std::byte> buf;
    append_add_order(buf, /*seq=*/42, /*exch_ts_ns=*/123456789, /*order_id=*/1001, /*symbol_id=*/7,
                      /*qty=*/500, /*price_ticks=*/10050, /*side=*/'B');

    std::cout << "\n=== Raw bytes (AddOrderMsg, seq=42, order_id=1001) ===\n";
    for (std::size_t i = 0; i < buf.size(); ++i) {
        std::cout << std::hex << static_cast<int>(std::to_integer<unsigned char>(buf[i])) << ' ';
    }
    std::cout << std::dec << '\n';

    // ============================================================
    //  3. HEADER PEEK KARO -- yeh WITHOUT byteswap galat dikhega
    // ============================================================
    MsgHeader raw{};
    std::memcpy(&raw, buf.data(), sizeof raw);
    std::cout << "\nRAW header.seq_num (bina swap ke)  = " << raw.seq_num
              << "  <- galat, yeh 42 nahi dikhna chahiye (big-endian bytes,\n"
                 "                                         little-endian int ki tarah padhe gaye)\n";

    MsgHeader hdr{};
    const bool ok = peek_header(buf.data(), buf.size(), hdr);
    std::cout << "peek_header() ke baad (swapped)     = " << (ok ? hdr.seq_num : 0)
              << "  <- sahi (02_endian_handling.cpp mein isi trap ka poora demo hai)\n";
    std::cout << "msg_type = " << msg_type_name(hdr.msg_type)
              << "  length = " << hdr.length << " bytes"
              << "  exch_ts_ns = " << hdr.exch_ts_ns << '\n';

    return 0;
}
