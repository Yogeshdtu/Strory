// 06_strict_aliasing.cpp
// ============================================================
// Strict aliasing rule: ek object ko uske "asli" type (ya char/byte,
// ya compatible) ke alawa kisi aur type ke glvalue se access karna UB.
// -O2 pe compiler is rule pe optimize karta hai -> stale values,
// reordered loads. Fix: std::memcpy ya std::bit_cast.
// ============================================================
//   compile-verify:  g++ -std=c++20 -Wall -Wextra -Wshadow 06_strict_aliasing.cpp -o sa && ./sa
//   DIVERGENCE dekho: g++ -std=c++20 -O2 -fstrict-aliasing 06_strict_aliasing.cpp -o sa && ./sa
//                     g++ -std=c++20 -O2 -fno-strict-aliasing 06_strict_aliasing.cpp -o sa2 && ./sa2
// ============================================================

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <bit>
#include <type_traits>

// ============================================================
//  1. THE UB WAY — read a float's bits through a uint32_t glvalue
// ============================================================
// float aur uint32_t "similar types" nahi hain -> yeh access UB hai.
// (Is simple function mein GCC aksar "sahi" answer de deta — par woh luck
//  hai, guarantee nahi. aliasing_trap() neeche woh case hai jahan compiler
//  ki no-alias assumption VISIBLY galat answer deti hai.)
static std::uint32_t bits_via_cast(float f) {
    std::uint32_t* pi = reinterpret_cast<std::uint32_t*>(&f);   // ⚠️ aliasing violation
    return *pi;
}

// ============================================================
//  2. THE LEGAL WAYS
// ============================================================
static std::uint32_t bits_via_memcpy(float f) {
    std::uint32_t out;
    std::memcpy(&out, &f, sizeof out);          // ✅ memcpy sees bytes — always legal
    return out;
}

static std::uint32_t bits_via_bitcast(float f) {
    return std::bit_cast<std::uint32_t>(f);     // ✅ C++20 — the intended tool
}

// ============================================================
//  3. A function where the optimizer's aliasing assumption BITES
// ============================================================
// Agar `long*` aur `int*` alias kar sakte to compiler ko `*lp` write ke
// baad `*ip` dobara load karna padta. Kyunki woh alias nahi karte
// (strict aliasing), compiler pehla load reuse kar sakta hai.
static int aliasing_trap(int* ip, long* lp) {
    int a = *ip;            // load ip
    *lp = 0x11223344L;      // write lp
    int b = *ip;            // -O2 + strict-aliasing: compiler MAAN sakta hai b == a
    return b - a;           // "hamesha 0" — bhale ip aur lp same address ho
}

int main() {
    const float pi = 3.14159265f;

    std::printf("float %.8f  ->  bits:\n", static_cast<double>(pi));
    std::printf("  via reinterpret_cast (UB): 0x%08x\n", bits_via_cast(pi));
    std::printf("  via memcpy       (legal): 0x%08x\n", bits_via_memcpy(pi));
    std::printf("  via std::bit_cast(legal): 0x%08x\n", bits_via_bitcast(pi));
    std::puts("  (teenon aksar match karte — par pehla UB hai; asli divergence"
              " aliasing_trap mein neeche)\n");

    // round-trip the other way — legal
    std::uint32_t raw = bits_via_bitcast(pi);
    float back = std::bit_cast<float>(raw);
    std::printf("  bit_cast round-trip: %.8f (== original: %d)\n",
                static_cast<double>(back), back == pi);

    // --------------------------------------------------------
    //  aliasing_trap — same memory as int* and long*
    // --------------------------------------------------------
    std::puts("\naliasing_trap: same address as int* and long*");
    static_assert(sizeof(long) >= sizeof(int));
    // ek buffer jise dono tarah dekhte hain (khud UB — demo ke liye):
    alignas(long) unsigned char slot[sizeof(long)] = {};
    int*  ip = static_cast<int*>(static_cast<void*>(slot));
    long* lp = static_cast<long*>(static_cast<void*>(slot));
    *ip = 111;
    int delta = aliasing_trap(ip, lp);
    std::printf("  *ip after = %d,  delta reported = %d\n", *ip, delta);
    std::puts("  -O0: delta reflects the real change. -O2 -fstrict-aliasing: delta may be 0");
    std::puts("       (compiler reused the first load — it 'knew' int* and long* don't alias)");

    std::puts("\nRULE:");
    std::puts(" - object ko uske asli type se access karo (ya char/unsigned char/std::byte)");
    std::puts(" - bits chahiye -> std::bit_cast<T>(x)  (C++20)  ya  std::memcpy");
    std::puts(" - reinterpret_cast<OtherType*>(&x) then deref  =  UB  (union bhi C++ mein UB-ish)");
    std::puts(" - -fno-strict-aliasing 'kaam kara deta' par woh ek crutch hai, fix nahi");
    return 0;
}
