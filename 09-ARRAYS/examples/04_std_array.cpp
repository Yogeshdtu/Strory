// 04_std_array.cpp
// ============================================================
// std::array<T, N> -- C array jo apna size YAAD rakhta hai
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 04_std_array.cpp -o sa && ./sa
// ============================================================
// std::array ek THIN wrapper hai C array ke upar. Zero overhead --
// same memory layout, same speed. Par:
//   - .size() jaanta hai (no decay)
//   - .at() bounds-check karta hai
//   - value-type hai -- copy, assign, return, compare sab kaam karte hain
//   - STL algorithms ke saath seedha chalta hai
// ============================================================

#include <algorithm>
#include <array>
#include <iostream>
#include <numeric>

// std::array pass by value -> COPY hoti hai (C array decay hota, yeh copy hota)
long long sumByValue(std::array<int, 5> a) {         // ⚠️ 20 bytes copy har call
    return std::accumulate(a.begin(), a.end(), 0LL);
}
// prefer: const reference (no copy), ya std::span (any size -- 05_span_demo.cpp)
long long sumByRef(const std::array<int, 5>& a) {
    return std::accumulate(a.begin(), a.end(), 0LL);
}

int main() {
    // ============================================================
    //  1. Declaration + init -- C array jaisa hi
    // ============================================================
    std::cout << "===== 1. std::array =====\n";
    std::array<int, 5> a = {10, 20, 30, 40, 50};
    std::array<int, 5> b{};                 // sab 0
    std::array c = {1, 2, 3};               // CTAD (C++17) -> std::array<int, 3>

    std::cout << "  a.size()  = " << a.size() << "   <- SIZE yaad hai (C array bhool jaata)\n";
    std::cout << "  a[2]      = " << a[2] << "\n";
    std::cout << "  a.front() = " << a.front() << ",  a.back() = " << a.back() << "\n";
    std::cout << "  c is std::array<int, " << c.size() << ">\n";
    (void)b;

    // ============================================================
    //  2. .at() -- bounds-checked (throws), [] -- unchecked (UB)
    // ============================================================
    std::cout << "\n===== 2. .at() vs [] =====\n";
    std::cout << "  a.at(4) = " << a.at(4) << "\n";
    try {
        std::cout << "  a.at(10) -> ";
        std::cout << a.at(10) << "\n";
    } catch (const std::out_of_range& e) {
        std::cout << "throw std::out_of_range: " << e.what() << "\n";
    }
    std::cout << "  a[10]    -> UB (no check). ASan/-D_GLIBCXX_ASSERTIONS se pakdo.\n";

    // ============================================================
    //  3. VALUE type -- copy, assign, compare, return
    // ============================================================
    std::cout << "\n===== 3. value semantics =====\n";
    std::array<int, 5> copy = a;            // poori copy (C array `=` nahi hota)
    copy[0] = 999;
    std::cout << "  a[0] = " << a[0] << ",  copy[0] = " << copy[0] << "  (independent)\n";

    std::array<int, 3> x = {1, 2, 3};
    std::array<int, 3> y = {1, 2, 3};
    std::cout << "  x == y ? " << (x == y ? "haan" : "nahi") << "   (element-wise compare)\n";

    // ============================================================
    //  4. NO DECAY -- function ko pass karo, size saath jaata hai
    // ============================================================
    std::cout << "\n===== 4. no decay =====\n";
    std::cout << "  sumByRef(a)   = " << sumByRef(a) << "\n";
    std::cout << "  sumByValue(a) = " << sumByValue(a) << "   (⚠️ 20 bytes copy hui)\n";

    // ============================================================
    //  5. STL algorithms seedha
    // ============================================================
    std::cout << "\n===== 5. STL algorithms =====\n";
    std::array<int, 6> nums = {5, 2, 8, 1, 9, 3};
    std::sort(nums.begin(), nums.end());
    std::cout << "  sorted: ";
    for (int v : nums) std::cout << v << " ";
    std::cout << "\n";
    std::cout << "  max = " << *std::max_element(nums.begin(), nums.end())
              << ",  sum = " << std::accumulate(nums.begin(), nums.end(), 0) << "\n";

    auto [p, q, r] = c;                     // structured binding
    std::cout << "  structured binding of c: " << p << " " << q << " " << r << "\n";

    // ============================================================
    //  6. Memory -- zero overhead
    // ============================================================
    std::cout << "\n===== 6. zero overhead =====\n";
    std::cout << "  sizeof(std::array<int,5>) = " << sizeof(std::array<int, 5>)
              << "   ==  sizeof(int[5]) = " << sizeof(int[5]) << "\n";
    std::cout << "  Same layout, same speed. Bas behtar API.\n";
    std::cout << "  Fixed size + stack pe -> heap allocation ZERO. HFT ke liye ideal.\n";

    return 0;
}
