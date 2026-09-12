// 01_basics_kata.cpp
// ============================================================
// Folder 47 file 01 ke "trap" problems, ek jagah, assertions ke saath.
// reverse-int (overflow-safe) · nCr (multiply-then-divide) · isqrt · powmod
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g -O0 01_basics_kata.cpp -o t && ./t
// ============================================================

#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <optional>

// ---- A3: reverse an int, return 0 on overflow ----------------------
static int reverse_int(int n) {
    int rev = 0;
    while (n != 0) {
        const int digit = n % 10;                       // C++: sign follows dividend
        if (rev > INT_MAX / 10 || (rev == INT_MAX / 10 && digit > 7))  return 0;
        if (rev < INT_MIN / 10 || (rev == INT_MIN / 10 && digit < -8)) return 0;
        rev = rev * 10 + digit;
        n /= 10;
    }
    return rev;
}

// ---- B8: nCr without intermediate overflow -------------------------
static std::uint64_t nCr(int n, int r) {
    if (r < 0 || r > n) return 0;
    if (r > n - r) r = n - r;                            // symmetry
    std::uint64_t result = 1;
    for (int i = 1; i <= r; ++i)
        result = result * static_cast<std::uint64_t>(n - r + i)
                        / static_cast<std::uint64_t>(i); // multiply THEN divide
    return result;
}

// ---- B7: integer square root (binary search) ----------------------
static std::uint64_t isqrt(std::uint64_t n) {
    std::uint64_t lo = 0, hi = 4294967296ULL;           // 2^32 covers all u64
    while (lo < hi) {
        const std::uint64_t mid = lo + (hi - lo + 1) / 2;
        if (mid <= n / mid) lo = mid;                    // mid*mid <= n, overflow-safe
        else                hi = mid - 1;
    }
    return lo;
}

// ---- B12: modular exponentiation --------------------------------
static std::uint64_t powmod(std::uint64_t a, std::uint64_t e, std::uint64_t m) {
    std::uint64_t r = 1 % m;
    a %= m;
    while (e != 0) {
        if (e & 1ULL) r = static_cast<std::uint64_t>((static_cast<__uint128_t>(r) * a) % m);
        a = static_cast<std::uint64_t>((static_cast<__uint128_t>(a) * a) % m);
        e >>= 1;
    }
    return r;
}

int main() {
    // reverse_int
    assert(reverse_int(123) == 321);
    assert(reverse_int(-123) == -321);
    assert(reverse_int(120) == 21);
    assert(reverse_int(1534236469) == 0);               // reversed overflows int
    assert(reverse_int(0) == 0);

    // nCr
    assert(nCr(5, 2) == 10);
    assert(nCr(52, 5) == 2598960ULL);
    assert(nCr(10, 0) == 1);
    assert(nCr(10, 10) == 1);
    assert(nCr(3, 5) == 0);

    // isqrt — exact where std::sqrt(double) would round
    assert(isqrt(0) == 0);
    assert(isqrt(15) == 3);
    assert(isqrt(16) == 4);
    assert(isqrt(1000000000000ULL) == 1000000);
    assert(isqrt(UINT64_MAX) == 4294967295ULL);

    // powmod
    assert(powmod(2, 10, 1000) == 24);                  // 1024 % 1000
    assert(powmod(3, 0, 7) == 1);
    assert(powmod(7, 100, 13) == 9);                    // Fermat-checked
    assert(powmod(123456789, 987654321, 1000000007ULL) == 652541198ULL);

    std::puts("01_basics_kata: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - reverse_int: overflow ko multiply se PEHLE check karo. Post-check =
//     signed overflow UB -> compiler check delete kar sakta.
//   - nCr: `result * x / i` (is order mein) — har step pe result ek valid
//     binomial coefficient hai, aur i consecutive integers ka product i! se
//     divisible. `/ i` pehle karo to precision loss.
//   - isqrt: `mid <= n / mid` overflow-safe hai `mid*mid <= n` ke comparison
//     ke liye. std::sqrt(double) 2^53 ke baad exact nahi.
//   - powmod: __uint128_t se a*a overflow avoid. m < 2^63 ke liye kaafi.
// ============================================================
