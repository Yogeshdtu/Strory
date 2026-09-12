// 01_array_basics.cpp
// ============================================================
// C-style array -- declaration, initialization, traversal
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_array_basics.cpp -o ab && ./ab
// ============================================================
// Yeh dikhata hai:
//   1. declaration -- type name[SIZE]
//   2. aggregate init { ... }, partial init, {} se zeroing
//   3. size compile-time constant hona chahiye (C++ mein VLA nahi)
//   4. traversal -- index, range-for, std::size (C++17)
//   5. contiguous memory -- elements ek doosre se sate hue
// ============================================================

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>   // std::size, std::begin, std::end

int main() {
    // ============================================================
    //  1. DECLARATION -- SIZE compile-time constant
    // ============================================================
    std::cout << "===== 1. declaration =====\n";
    int scores[5];                 // 5 ints -- UNINITIALIZED (garbage values!)
    scores[0] = 90;
    scores[1] = 82;
    scores[2] = 71;
    scores[3] = 65;
    scores[4] = 50;
    std::cout << "  scores[2] = " << scores[2] << "\n";
    // int n = 5; int bad[n];      // ❌ C++ mein VLA nahi (C mein hai). constexpr chahiye.

    // ============================================================
    //  2. INITIALIZATION forms
    // ============================================================
    std::cout << "\n===== 2. initialization =====\n";
    int a[5] = {10, 20, 30, 40, 50};        // full aggregate init
    int b[5] = {1, 2};                        // PARTIAL -- baaki {3,4,5} ZERO ho jaate hain
    int c[5] = {};                            // SAB zero
    int d[]  = {7, 8, 9};                     // size compiler count karta hai -> 3
    int e[5] = {0};                           // pehla 0, baaki bhi 0 (same as {})

    std::cout << "  b[] = ";
    for (int x : b) std::cout << x << " ";
    std::cout << "  <- {1, 2} diya, {0, 0, 0} apne aap\n";
    std::cout << "  d[] ka size (elements) = " << std::size(d) << "\n";
    (void)a; (void)c; (void)e;

    // ============================================================
    //  3. SIZE nikalna -- sizeof trick aur std::size
    // ============================================================
    std::cout << "\n===== 3. size =====\n";
    std::cout << "  sizeof(a)          = " << sizeof(a) << " bytes  (5 * 4)\n";
    std::cout << "  sizeof(a)/sizeof(a[0]) = " << sizeof(a) / sizeof(a[0]) << " elements (purana tareeka)\n";
    std::cout << "  std::size(a)       = " << std::size(a) << " elements   (C++17, saaf)\n";

    // ============================================================
    //  4. TRAVERSAL -- 3 tareeke
    // ============================================================
    std::cout << "\n===== 4. traversal =====\n";

    std::cout << "  index-based : ";
    for (std::size_t i = 0; i < std::size(a); ++i) std::cout << a[i] << " ";
    std::cout << "\n";

    std::cout << "  range-for   : ";
    for (int x : a) std::cout << x << " ";
    std::cout << "\n";

    std::cout << "  iterators   : ";
    for (auto it = std::begin(a); it != std::end(a); ++it) std::cout << *it << " ";
    std::cout << "\n";

    // modify -- range-for reference se
    for (int& x : a) x *= 2;
    std::cout << "  doubled     : ";
    for (int x : a) std::cout << x << " ";
    std::cout << "\n";

    // ============================================================
    //  5. CONTIGUOUS memory -- addresses sate hue
    // ============================================================
    std::cout << "\n===== 5. contiguous layout =====\n";
    int m[4] = {11, 22, 33, 44};
    for (std::size_t i = 0; i < std::size(m); ++i) {
        std::cout << "  &m[" << i << "] = " << static_cast<const void*>(&m[i])
                  << "   value " << m[i] << "\n";
    }
    std::cout << "  har address pichle se +" << sizeof(int)
              << " bytes (ek int). Array = ek block, no gaps.\n";
    std::cout << "  isi liye arr[i] fast hai: base + i * sizeof(element) -> ek jump\n";

    return 0;
}
