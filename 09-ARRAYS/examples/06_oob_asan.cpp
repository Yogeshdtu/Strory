// 06_oob_asan.cpp
// ============================================================
//  ⚠️  IS FILE MEIN JAAN-BOOJH KAR OUT-OF-BOUNDS ACCESS HAI  ⚠️
// ============================================================
//  COMPILE theek hota hai. Chalane pe:
//    - RAW C-array OOB (BUG 1-3): MinGW pe SILENT (garbage / UB), Linux+ASan pe error.
//    - STL container OOB (BUG 4): GCC 16.2 pe `_GLIBCXX_ASSERTIONS` sirf `-O0` pe DEFAULT
//      on hai -> debug build (`./build.ps1 <file>`) yahan ABORT karta hai clear assertion ke
//      saath. `-O2` (`./build.ps1 fast`) pe macro OFF -> chupchaap UB, abort NAHI (chala ke dekha).
//      (Isi liye BUG 4 sabse aakhri mein hai -- baaki bugs pehle dikh jaayein.)
//
//  PAKADNE KE TAREEKE:
//
//  A) AddressSanitizer -- BEST (raw arrays + heap, exact line). Linux/macOS/Clang:
//        g++ -std=c++20 -fsanitize=address,undefined -g 06_oob_asan.cpp -o oob && ./oob
//     ⚠️ MinGW-w64 (yeh Windows toolchain) mein libasan NAHI -> link fail. WSL/Linux use karo.
//
//  B) libstdc++ hardened -- STL containers ke [] pe bounds-check (raw arrays pe NAHI).
//     GCC 16.2 pe sirf -O0 pe default on; -O2 pe khud lagao: -D_GLIBCXX_ASSERTIONS
//     (build.ps1 ka `san` target ASan na milne pe isi pe fall back karta hai.)
//
//  C) -O2 -Warray-bounds -- compile-time, sirf constant/provable OOB.
//
//  ⚠️ `make folder` / `checkall` sirf COMPILE karte hain -> "OK" dikhega.
// ============================================================

#include <cstddef>
#include <iostream>
#include <vector>

// runtime index -- taaki -O0 pe compiler warn na kare; runtime tools pakadte hain
static std::size_t externalIndex(int argc) {
    return static_cast<std::size_t>(5 + (argc - argc));   // hamesha 5
}

int main(int argc, char**) {
    const std::size_t i = externalIndex(argc);   // == 5
    int arr[5] = {10, 20, 30, 40, 50};           // valid index 0..4

    // ============================================================
    //  BUG 1:  raw arr[5] READ  (OOB by one) -- MinGW pe silent
    // ============================================================
    std::cout << "===== BUG 1: raw arr[5] READ (OOB by one) =====\n";
    std::cout << "  arr[" << i << "] = " << arr[i]        // ⚠️ OOB READ
              << "   <- MinGW: garbage; Linux+ASan: 'stack-buffer-overflow'\n";

    // ============================================================
    //  BUG 2:  loop  k <= 5  -> aakhri iteration OOB
    // ============================================================
    std::cout << "\n===== BUG 2: for (k = 0; k <= 5; ++k) =====\n";
    long long sum = 0;
    for (std::size_t k = 0; k < 5; ++k) sum += arr[k];   // fixed loop -- upar likha demo hai
    sum += arr[i];                                       // ⚠️ yeh line "k == 5" wali OOB
    std::cout << "  sum with the extra arr[5] = " << sum << "   (garbage jud gaya)\n";
    std::cout << "  (galti: `k <= 5` likhna. sahi: `k < 5` -- half-open range)\n";

    // ============================================================
    //  BUG 3:  raw array OOB WRITE -- sabse khatarnak
    // ============================================================
    std::cout << "\n===== BUG 3: arr[7] = 999 (OOB WRITE) =====\n";
    const std::size_t w = i + 2;             // 7
    arr[w] = 999;                            // ⚠️ OOB WRITE -- aas-paas ki memory corrupt
    std::cout << "  likh diya -- ho sakta hai kisi aur variable pe. Silent, door ka bug.\n";

    // ============================================================
    //  BUG 4:  std::vector[5]  -- yeh toolchain isse PAKAD-TA hai (abort)
    // ============================================================
    std::cout << "\n===== BUG 4: std::vector operator[] OOB =====\n";
    std::vector<int> v = {10, 20, 30, 40, 50};   // valid 0..4
    std::cout << "  v.size() = " << v.size() << "\n";
    std::cout << "  ab v[" << i << "] access karte hain...\n";
    std::cout.flush();
    std::cout << "  v[" << i << "] = " << v[i] << "\n";   // ⚠️ _GLIBCXX_ASSERTIONS -> ABORT yahan
    std::cout << "  (agar yeh line chhap gayi to _GLIBCXX_ASSERTIONS off hai)\n";

    // ============================================================
    //  FIX
    // ============================================================
    std::cout << "\n-----------------------------------------------\n";
    std::cout <<
        "FIX:\n"
        "  * Half-open range [0, n): for (i = 0; i < n; ++i)\n"
        "  * std::size(arr) se size lo, hardcode mat karo\n"
        "  * range-for: for (int x : arr) -- index hi nahi\n"
        "  * std::array/std::vector + .at() (throws) untrusted input pe\n"
        "  * CI: -fsanitize=address (Linux/Clang) ; sab jagah: -D_GLIBCXX_ASSERTIONS\n";

    return 0;
}

// ============================================================
//  KYA SEEKHNA HAI
// ============================================================
//  1. OOB access = UNDEFINED BEHAVIOUR. "Chal gaya" ka matlab "sahi" NAHI.
//     Compiler [] pe bounds check nahi lagata (speed) -- aapki zimmedaari.
//
//  2. OOB READ  -> galat data (kisi aur variable ki value / garbage).
//     OOB WRITE -> door ki memory corrupt -- dhoondhna sabse mushkil bug.
//
//  3. RAW C arrays sabse khatarnak: koi built-in check nahi. std::array /
//     std::vector / std::span ke paas .at() (checked) hai, aur is toolchain pe
//     [] bhi `_GLIBCXX_ASSERTIONS` se checked hai -- isi liye raw arrays avoid karo.
//
//  4. Tools: ASan (best, Linux) > -D_GLIBCXX_ASSERTIONS (STL only) >
//     -O2 -Warray-bounds (compile-time, constant only) > Valgrind (heap).
// ============================================================
