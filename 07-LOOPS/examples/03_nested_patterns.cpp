// 03_nested_patterns.cpp
// ============================================================
// Nested loops -- pattern printing aur complexity
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 03_nested_patterns.cpp -o np && ./np
// ============================================================
// Yeh dikhata hai:
//   1. outer loop = rows, inner loop = columns
//   2. right triangle, number pyramid, multiplication table
//   3. inner loop ki limit outer variable pe depend kar sakti hai
//   4. complexity: O(rows * cols)
// ============================================================

#include <iostream>

int main() {
    // ============================================================
    //  1. RECTANGLE -- inner limit fixed
    // ============================================================
    std::cout << "===== 1. Rectangle (4 x 6) =====\n";
    for (int row = 0; row < 4; ++row) {          // outer: 4 rows
        for (int col = 0; col < 6; ++col) {      // inner: 6 cols -- HAR row ke liye
            std::cout << "* ";
        }
        std::cout << "\n";                        // row ke end pe newline
    }
    std::cout << "  -> 4 * 6 = 24 stars, O(rows*cols)\n";

    // ============================================================
    //  2. RIGHT TRIANGLE -- inner limit outer pe depend karti hai
    // ============================================================
    std::cout << "\n===== 2. Right triangle =====\n";
    for (int row = 1; row <= 5; ++row) {
        for (int col = 1; col <= row; ++col) {   // <- `col <= row`  (row badhta hai)
            std::cout << "# ";
        }
        std::cout << "\n";
    }
    // total stars = 1 + 2 + 3 + 4 + 5 = 15

    // ============================================================
    //  3. NUMBER PYRAMID -- spaces + numbers
    // ============================================================
    std::cout << "\n===== 3. Number pyramid =====\n";
    const int height = 5;
    for (int row = 1; row <= height; ++row) {
        for (int s = 0; s < height - row; ++s) std::cout << " ";   // leading spaces
        for (int num = 1; num <= row; ++num)   std::cout << num;   // 1..row
        for (int num = row - 1; num >= 1; --num) std::cout << num; // row-1..1
        std::cout << "\n";
    }

    // ============================================================
    //  4. MULTIPLICATION TABLE -- 2D grid of values
    // ============================================================
    std::cout << "\n===== 4. Multiplication table (1..9) =====\n";
    for (int a = 1; a <= 9; ++a) {
        for (int b = 1; b <= 9; ++b) {
            const int product = a * b;
            // width 4 mein right-align (manual, iomanip ke bina)
            if (product < 10)  std::cout << "   ";
            else if (product < 100) std::cout << "  ";
            else std::cout << " ";
            std::cout << product;
        }
        std::cout << "\n";
    }

    // ============================================================
    //  5. TRIPLE nested -- O(n^3), jaldi mehnga
    // ============================================================
    std::cout << "\n===== 5. Triple nested -- count karo =====\n";
    long long ops = 0;
    const int N = 40;
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            for (int k = 0; k < N; ++k)
                ++ops;
    std::cout << "  N=" << N << " -> " << ops << " iterations (N^3 = "
              << (static_cast<long long>(N) * N * N) << ")\n";
    std::cout << "  N ko 10x karo -> iterations 1000x. Nested loops ka complexity dhyaan se.\n";

    return 0;
}
