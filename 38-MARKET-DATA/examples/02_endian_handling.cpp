// 02_endian_handling.cpp
// ============================================================
// Safe byte-order conversion -- garbled-without-swap demo, alignment-safe
// read (memcpy), aur byteswap ki ACTUAL measured cost
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 02_endian_handling.cpp -o endian && ./endian
// ============================================================

#include "wire_protocol.hpp"

#include <bit>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <vector>

int main() {
    // ============================================================
    //  1. YEH BOX LITTLE-ENDIAN HAI (x86/ARM -- is course ke sab targets)
    // ============================================================
    std::cout << "std::endian::native == little? "
              << (std::endian::native == std::endian::little ? "haan" : "nahi") << '\n';

    // ============================================================
    //  2. GARBLED-WITHOUT-SWAP -- ek known value se dikhao
    // ============================================================
    // Wire pe 0x12345678 big-endian likha jaata: bytes [12 34 56 78].
    // Little-endian host agar unhi bytes ko "seedha" ek uint32_t maan le,
    // woh unhe [78 56 34 12] ke roop mein padhega -- 0x78563412.
    const std::uint32_t host_value = 0x12345678u;
    const std::uint32_t wire_value = host_to_net32(host_value);  // ab big-endian bytes hain isi variable mein

    std::cout << "\n=== Ek known value: 0x12345678 ===\n";
    std::cout << std::hex << std::setfill('0');
    std::cout << "host value (jo bhejna tha)        = 0x" << std::setw(8) << host_value << '\n';
    std::cout << "wire bytes (as if raw-read, NO swap) = 0x" << std::setw(8) << wire_value
              << "  <- GARBLED (yeh 0x78563412 hai, 0x12345678 nahi)\n";
    std::cout << "wire bytes ko net_to_host32 karne ke baad = 0x" << std::setw(8)
              << net_to_host32(wire_value) << "  <- SAHI, wapas 0x12345678\n";
    std::cout << std::dec;

    // ============================================================
    //  3. ALIGNMENT-SAFE READ -- memcpy vs direct pointer cast
    // ============================================================
    // Ek buffer jahan ek uint64_t field 3-byte offset pe shuru hoti
    // (jaise back-to-back variable-length messages ke baad ho sakta) --
    // yeh 8-byte-aligned NAHI hai.
    alignas(64) unsigned char misaligned_buf[16] = {};
    std::uint64_t test_val = 0xAABBCCDDEEFF0011ULL;
    std::memcpy(misaligned_buf + 3, &test_val, sizeof test_val);  // offset 3 = misaligned

    // ❌ UNSAFE (kaam kar sakta x86 pe, par UB hai -- strict-alignment
    // platforms jaise kuch ARM configs pe fault kar sakta, aur compiler
    // ko "yeh aligned hai" maan ke wrong codegen ka bhi risk deta):
    //   auto v = *reinterpret_cast<const std::uint64_t*>(misaligned_buf + 3);
    // (yahan intentionally comment-out hai -- yeh file crash NAHI karni chahiye)

    // ✅ SAFE -- memcpy alignment ki parwaah nahi karta, aur -O2 pe compiler
    // ise ek single (possibly unaligned) load instruction mein compile
    // karta -- koi runtime "loop" ya function-call overhead nahi.
    std::uint64_t safe_read;
    std::memcpy(&safe_read, misaligned_buf + 3, sizeof safe_read);

    std::cout << "\n=== Misaligned read (offset 3, memcpy se) ===\n";
    std::cout << std::hex << "expected = 0x" << test_val
              << "  memcpy se mila = 0x" << safe_read
              << "  match? " << (safe_read == test_val ? "haan" : "NAHI") << std::dec << '\n';

    // ============================================================
    //  4. PARSE KARO EK POORA MESSAGE, SAB FIELDS SWAP KARO
    // ============================================================
    std::vector<std::byte> buf;
    append_add_order(buf, 100, 5'000'000'000ULL, 555, 3, 250, 10075, 'B');

    AddOrderMsg raw{};
    std::memcpy(&raw, buf.data(), sizeof raw);  // alignment-safe copy out

    std::cout << "\n=== Poora AddOrderMsg parse (swap har multi-byte field) ===\n";
    std::cout << "seq_num     = " << net_to_host32(raw.hdr.seq_num) << '\n';
    std::cout << "order_id    = " << net_to_host64(raw.order_id) << '\n';
    std::cout << "symbol_id   = " << net_to_host32(raw.symbol_id) << '\n';
    std::cout << "qty         = " << net_to_host32(raw.qty) << '\n';
    std::cout << "price_ticks = " << net_to_host_i64(raw.price_ticks) << '\n';
    std::cout << "side        = " << static_cast<char>(raw.side)
              << "  (single byte -- KOI swap nahi chahiye)\n";

    // ============================================================
    //  5. BYTESWAP KI ACTUAL COST -- measure karo, guess mat karo (Rule 2)
    // ============================================================
    constexpr int N = 20'000'000;
    std::vector<std::uint64_t> vals(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) vals[static_cast<std::size_t>(i)] = static_cast<std::uint64_t>(i) * 0x9E3779B97F4A7C15ULL;

    volatile std::uint64_t sink = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) sink = bswap64(vals[static_cast<std::size_t>(i)]);
    const auto t1 = std::chrono::steady_clock::now();
    (void)sink;

    const double ns_per_swap =
        std::chrono::duration<double, std::nano>(t1 - t0).count() / N;

    std::cout << "\n=== bswap64() measured cost (-O2 recommended; -O0 yahan chal raha) ===\n";
    std::cout << std::fixed << std::setprecision(3) << ns_per_swap << " ns/swap over " << N << " calls\n";
    std::cout << "(is file ko `./build.ps1 fast` se -O2 pe chalao real number ke liye --\n"
                 " 05-binary-protocols.md mein woh number hai)\n";

    return 0;
}
