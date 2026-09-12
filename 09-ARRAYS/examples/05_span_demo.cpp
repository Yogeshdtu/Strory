// 05_span_demo.cpp
// ============================================================
// std::span<T> (C++20) -- non-owning VIEW: pointer + length
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 05_span_demo.cpp -o sp && ./sp
// ============================================================
// std::span = { T* ptr; size_t len; }  -- 16 bytes, koi ownership nahi,
// koi copy nahi. Ek span parameter C array, std::array, std::vector,
// initializer -- SAB pe kaam karta hai. Yeh modern "array parameter" hai.
// ============================================================

#include <array>
#include <cstddef>
#include <iostream>
#include <span>
#include <vector>

// EK function -- har contiguous container pe chalta hai
long long sum(std::span<const int> data) {          // const -> read-only view
    long long s = 0;
    for (int x : data) s += x;
    return s;
}

// modify bhi kar sakte ho -- span<int> (non-const)
void doubleAll(std::span<int> data) {
    for (int& x : data) x *= 2;
}

void printSpan(std::span<const int> s, const char* label) {
    std::cout << "  " << label << " (" << s.size() << "): ";
    for (int x : s) std::cout << x << " ";
    std::cout << "\n";
}

int main() {
    // ============================================================
    //  1. EK function, teen alag containers
    // ============================================================
    std::cout << "===== 1. one function, any contiguous container =====\n";
    int cArr[]            = {1, 2, 3, 4, 5};
    std::array<int, 4>  stdArr = {10, 20, 30, 40};
    std::vector<int>    vec    = {100, 200, 300};

    std::cout << "  sum(cArr)   = " << sum(cArr)   << "\n";
    std::cout << "  sum(stdArr) = " << sum(stdArr) << "\n";
    std::cout << "  sum(vec)    = " << sum(vec)    << "\n";
    std::cout << "  sum({7, 8, 9}) = " << sum(std::vector<int>{7, 8, 9}) << "\n";

    // ============================================================
    //  2. Modify through span
    // ============================================================
    std::cout << "\n===== 2. modify via span<int> =====\n";
    doubleAll(vec);
    printSpan(vec, "vec doubled");

    // ============================================================
    //  3. SUB-VIEWS -- copy ke bina slice
    // ============================================================
    std::cout << "\n===== 3. subspan / first / last =====\n";
    std::vector<int> big = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::span<const int> all = big;

    printSpan(all.first(3),      "first(3)  ");
    printSpan(all.last(3),       "last(3)   ");
    printSpan(all.subspan(2, 4), "subspan(2,4)");
    std::cout << "  yeh sab VIEWS hain -- koi data copy nahi hua, sirf ptr+len\n";

    // ============================================================
    //  4. size / empty / data / [] / bytes
    // ============================================================
    std::cout << "\n===== 4. span API =====\n";
    std::cout << "  all.size()        = " << all.size() << "\n";
    std::cout << "  all[4]            = " << all[4] << "\n";
    std::cout << "  all.size_bytes()  = " << all.size_bytes() << "\n";
    std::cout << "  sizeof(std::span<const int>) = " << sizeof(std::span<const int>)
              << "  (ptr + len)\n";

    // ============================================================
    //  5. ⚠️ DANGLING -- span apna data OWN nahi karta
    // ============================================================
    std::cout << "\n===== 5. ⚠️ dangling risk =====\n";
    std::span<const int> dangler;
    {
        std::vector<int> temp = {11, 22, 33};
        dangler = temp;                 // span temp ke andar dekhta hai
    }                                    // temp DESTROYED -- dangler ab dangling
    // for (int x : dangler) ...         // ⚠️ UB -- use karo to ASan pakdega
    std::cout << "  span = { T* ; len }. Agar underlying container marr gaya\n"
                 "  -> span dangling. span ko container se ZYADA zinda mat rakho.\n";

    // fixed-extent span -- size compile-time
    std::span<int, 4> fixed(stdArr);
    std::cout << "\n  std::span<int, 4> -- size type mein baked hai: " << fixed.size() << "\n";

    return 0;
}
