// 04_recursion.cpp
// ============================================================
// Recursion -- function jo khud ko call karta hai
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 04_recursion.cpp -o rec && ./rec
//   (fib benchmark ke liye -O2 behtar: g++ -std=c++20 -O2 04_recursion.cpp -o rec)
// ============================================================
// Har recursive function ke 2 hisse:
//   1. BASE CASE   -- jahan recursion RUKTA hai (bina iske -> stack overflow)
//   2. RECURSIVE CASE -- chhoti problem pe khud ko call, base ki taraf badhta hua
//
// Yeh dikhata hai:
//   - factorial (recursion vs iteration)
//   - fibonacci NAIVE (exponential -- measured) vs memoized vs iterative
//   - sum of digits, power, gcd
//   - recursion depth aur uski limit
// ============================================================

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

// ------------------------------------------------------------
//  FACTORIAL
// ------------------------------------------------------------
std::uint64_t factRec(int n) {
    if (n <= 1) return 1;              // BASE CASE
    return static_cast<std::uint64_t>(n) * factRec(n - 1);   // RECURSIVE CASE
}
std::uint64_t factIter(int n) {
    std::uint64_t result = 1;
    for (int i = 2; i <= n; ++i) result *= static_cast<std::uint64_t>(i);
    return result;
}

// ------------------------------------------------------------
//  FIBONACCI -- teen tareeke
// ------------------------------------------------------------
// NAIVE: har call do calls karta hai -> O(2^n) -- BAHUT slow
std::uint64_t fibNaive(int n) {
    if (n < 2) return static_cast<std::uint64_t>(n);
    return fibNaive(n - 1) + fibNaive(n - 2);   // <- yahi exponential blowup
}

// MEMOIZED: har n ek hi baar compute -> O(n)
std::uint64_t fibMemo(int n, std::vector<std::int64_t>& cache) {
    if (n < 2) return static_cast<std::uint64_t>(n);
    if (cache[static_cast<std::size_t>(n)] >= 0)
        return static_cast<std::uint64_t>(cache[static_cast<std::size_t>(n)]);
    const std::uint64_t r = fibMemo(n - 1, cache) + fibMemo(n - 2, cache);
    cache[static_cast<std::size_t>(n)] = static_cast<std::int64_t>(r);
    return r;
}

// ITERATIVE: no recursion, O(n), O(1) space
std::uint64_t fibIter(int n) {
    if (n < 2) return static_cast<std::uint64_t>(n);
    std::uint64_t a = 0, b = 1;
    for (int i = 2; i <= n; ++i) { const std::uint64_t c = a + b; a = b; b = c; }
    return b;
}

// ------------------------------------------------------------
//  Aur classic recursions
// ------------------------------------------------------------
int sumDigits(int n) {
    n = n < 0 ? -n : n;
    if (n < 10) return n;                       // BASE
    return n % 10 + sumDigits(n / 10);         // last digit + baaki
}
std::uint64_t power(std::uint64_t base, int exp) {
    if (exp == 0) return 1;                     // BASE
    if (exp % 2 == 0) { const std::uint64_t h = power(base, exp / 2); return h * h; }  // fast
    return base * power(base, exp - 1);
}
int gcd(int a, int b) {
    return b == 0 ? a : gcd(b, a % b);         // Euclid -- b == 0 = BASE
}

// depth counter
int maxDepth = 0;
void countDepth(int d) {
    if (d > maxDepth) maxDepth = d;
    if (d < 5000) countDepth(d + 1);           // capped -- 05_stack_overflow.cpp isko HATA deta hai
}

int main() {
    std::cout << "===== 1. FACTORIAL -- recursion vs iteration =====\n";
    for (int n : {0, 1, 5, 10, 20}) {
        std::cout << "  " << n << "! = " << factRec(n)
                  << "   (iter: " << factIter(n) << ")\n";
    }

    std::cout << "\n===== 2. FIBONACCI -- naive is EXPONENTIAL =====\n";
    auto ms = [](auto a, auto b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };

    for (int n : {30, 35, 40}) {
        auto t0 = std::chrono::steady_clock::now();
        const std::uint64_t r = fibNaive(n);
        auto t1 = std::chrono::steady_clock::now();
        std::cout << "  fibNaive(" << n << ") = " << r
                  << "   [" << ms(t0, t1) << " ms]\n";
    }
    std::cout << "  (n +5 -> ~11x slower. fibNaive(50) ghante lega.)\n\n";

    std::vector<std::int64_t> cache(91, -1);
    auto t0 = std::chrono::steady_clock::now();
    const std::uint64_t rm = fibMemo(90, cache);
    auto t1 = std::chrono::steady_clock::now();
    std::cout << "  fibMemo(90)  = " << rm << "   [" << ms(t0, t1) << " ms]\n";
    std::cout << "  fibIter(90)  = " << fibIter(90) << "   [instant]\n";
    std::cout << "  -> same problem, O(2^n) vs O(n). ALGORITHM matters, recursion nahi.\n";

    std::cout << "\n===== 3. sumDigits / power / gcd =====\n";
    std::cout << "  sumDigits(98765) = " << sumDigits(98765) << "\n";
    std::cout << "  power(2, 20)     = " << power(2, 20) << "\n";
    std::cout << "  gcd(1071, 462)   = " << gcd(1071, 462) << "\n";

    std::cout << "\n===== 4. Recursion DEPTH =====\n";
    countDepth(0);
    std::cout << "  countDepth() 5000 tak bina crash pahuncha (maxDepth = " << maxDepth << ")\n";
    std::cout << "  Har call ek stack frame khaata hai. Bina base case ke -> stack overflow.\n";
    std::cout << "  (05_stack_overflow.cpp: base case hata do, dekho kitni door tak jaata hai)\n";

    return 0;
}
