// 08_market_data_struct.cpp
// ============================================================
// HFT-style wire message struct -- static_asserts se layout LOCK karo
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 08_market_data_struct.cpp -o md && ./md
// ============================================================
// Wire protocol structs mein layout EXACT hona chahiye (koi compiler,
// koi platform -- same bytes). Tools:
//   - fixed-width types (std::int32_t, no plain int/long)
//   - static_assert(sizeof / alignof / offsetof)
//   - #pragma pack ya explicit padding fields
//   - endianness handling (network = big-endian; x86 = little-endian)
// ============================================================

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <type_traits>

// ------------------------------------------------------------
//  Ek "quote" message, 32 bytes, koi chhupi padding nahi.
//  Members CHHOTE se BADE order mein hain (1,1,2,4,8,4,4,4,4) aur har member
//  pehle se hi apne natural offset pe baithta hai (0,1,2,4,8,16...) -> bina
//  pack ke bhi sizeof 32 aur saare offsets same (GCC 16.2 pe pack hata ke
//  chalaya: sirf alignof 8 ho gaya). pack(1) yahan layout ki GUARANTEE hai
//  (alignof 1 -> kisi bhi byte buffer position pe fit), size ki bachat nahi.
// ------------------------------------------------------------
enum class Side : std::uint8_t { Buy = 'B', Sell = 'S' };
enum class MsgType : std::uint8_t { Quote = 'Q', Trade = 'T', Heartbeat = 'H' };

#pragma pack(push, 1)          // padding bilkul NAHI -- har byte exact
                               // (dhyaan: GCC #pragma pack structs pe -Waddress-of-packed-member
                               //  nahi deta; [[gnu::packed]] pe deta hai -- lesson 06)
struct QuoteMsg {
    MsgType       type;        // offset 0   (1)
    Side          side;        // offset 1   (1)
    std::uint16_t venue;       // offset 2   (2)
    std::uint32_t seq;         // offset 4   (4)
    std::int64_t  timestampNs; // offset 8   (8)
    std::int32_t  bidPrice;    // offset 16  (4)  -- fixed-point (jaise * 1e4)
    std::int32_t  askPrice;    // offset 20  (4)
    std::uint32_t bidSize;     // offset 24  (4)
    std::uint32_t askSize;     // offset 28  (4)
};                            // total 32
#pragma pack(pop)

// ---- layout LOCK -- yeh compile-time pe fail hoga agar layout badla ----
static_assert(sizeof(QuoteMsg) == 32,            "QuoteMsg must be exactly 32 bytes");
static_assert(alignof(QuoteMsg) == 1,            "QuoteMsg must be byte-aligned (packed)");
static_assert(offsetof(QuoteMsg, timestampNs) == 8,  "timestampNs at offset 8");
static_assert(offsetof(QuoteMsg, bidPrice) == 16,    "bidPrice at offset 16");
static_assert(offsetof(QuoteMsg, askSize)  == 28,    "askSize at offset 28");
static_assert(std::is_trivially_copyable_v<QuoteMsg>, "must be memcpy-able");

// ---- endianness: wire big-endian hai, x86 little-endian ----
// (std::byteswap C++23 hai; yahan manual -- __builtin_bswap ya C++23 std::byteswap use karo)
template <typename T>
constexpr T byteSwap(T v) {
    static_assert(std::is_integral_v<T>);
    if constexpr (sizeof(T) == 1) return v;
    else if constexpr (sizeof(T) == 2) return static_cast<T>(__builtin_bswap16(static_cast<std::uint16_t>(v)));
    else if constexpr (sizeof(T) == 4) return static_cast<T>(__builtin_bswap32(static_cast<std::uint32_t>(v)));
    else                               return static_cast<T>(__builtin_bswap64(static_cast<std::uint64_t>(v)));
}

template <typename T>
T fromBigEndian(T v) {
    if constexpr (std::endian::native == std::endian::little)
        return byteSwap(v);
    else
        return v;
}

int main() {
    std::cout << "===== QuoteMsg layout (static_assert-verified) =====\n";
    std::cout << "  sizeof  = " << sizeof(QuoteMsg) << "\n";
    std::cout << "  alignof = " << alignof(QuoteMsg) << "\n";
    std::cout << "  offsets: type=0 side=1 venue=2 seq=4 ts=8 bid=16 ask=20 bidSz=24 askSz=28\n";

    // ============================================================
    //  Decode: raw bytes -> struct  (memcpy, phir endianness theek)
    // ============================================================
    std::cout << "\n===== decode from raw bytes =====\n";

    // "received buffer" ka natak (demo simple rakhne ke liye values little-endian hi hain)
    QuoteMsg src{};
    src.type = MsgType::Quote;  src.side = Side::Buy;
    src.venue = 7;  src.seq = 123456;  src.timestampNs = 1'700'000'000'000'000'000LL;
    src.bidPrice = 1923400;  src.askPrice = 1923500;  src.bidSize = 500;  src.askSize = 300;

    alignas(QuoteMsg) unsigned char buffer[sizeof(QuoteMsg)];
    std::memcpy(buffer, &src, sizeof(src));       // "network se aaya"

    // buffer ke bytes ek asli QuoteMsg object mein copy (trivially copyable hai isliye allowed).
    // Yeh zero-copy NAHI -- 32 bytes ki copy hai, jo -O2 pe kuch mov instructions ban jaati hai.
    // Zero-copy (reinterpret_cast) ka lalach mat karo -- neeche wajah.
    QuoteMsg msg;
    std::memcpy(&msg, buffer, sizeof(msg));       // ✅ safe: aliasing UB nahi, unaligned deref nahi

    std::cout << "  type = " << static_cast<char>(msg.type)
              << "  side = " << static_cast<char>(msg.side)
              << "  seq = " << msg.seq << "\n";
    std::cout << "  bid = " << msg.bidPrice / 10000.0
              << "  ask = " << msg.askPrice / 10000.0
              << "  (fixed-point, /1e4)\n";
    std::cout << "  spread = " << (msg.askPrice - msg.bidPrice) / 10000.0 << "\n";

    // endianness demo: agar seq wire pe big-endian aata
    const std::uint32_t wireSeq = byteSwap(msg.seq);          // "maano wire pe big-endian aaya"
    std::cout << "  seq " << msg.seq << " -> byteSwap -> 0x" << std::hex << wireSeq << std::dec
              << " -> fromBigEndian -> " << fromBigEndian(wireSeq) << "\n";

    // ============================================================
    //  packed + memcpy kyun (reinterpret_cast kyun nahi)
    // ============================================================
    std::cout <<
        "\n"
        "  * fixed-width types: int32_t, NOT int/long (platform pe size badalta hai)\n"
        "  * static_assert(sizeof/alignof/offsetof): layout galti compile pe pakdi jaati hai\n"
        "  * #pragma pack(1): compiler padding nahi daalega -> wire ke exact bytes\n"
        "  * memcpy into a struct (not reinterpret_cast<QuoteMsg*>(buffer)):\n"
        "      -> strict-aliasing UB nahi, unaligned pointer deref nahi (packed struct)\n"
        "  * byteswap for endianness (wire = big-endian, x86 = little)\n"
        "  Folder 25 (object model, aliasing), 34 (asm), 38 (market data).\n";

    return 0;
}
