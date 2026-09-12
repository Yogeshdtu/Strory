// 03_2d_arrays.cpp
// ============================================================
// Multidimensional arrays -- ROW-MAJOR memory layout
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 03_2d_arrays.cpp -o d2 && ./d2
// ============================================================
// int m[3][4]  =  "3 rows, har row 4 ints"  =  12 ints ONE contiguous block.
// C++ mein layout ROW-MAJOR hai: pehle row 0 poori, phir row 1, phir row 2.
//   m[i][j]  ka address  =  &m[0][0] + (i * COLS + j)
// Isi liye traversal ka order cache ke liye matter karta hai (folder 07 file 09).
// ============================================================

#include <cstddef>
#include <iostream>

int main() {
    // ============================================================
    //  1. Declaration + init
    // ============================================================
    std::cout << "===== 1. int m[3][4] =====\n";
    int m[3][4] = {
        {10, 11, 12, 13},     // row 0
        {20, 21, 22, 23},     // row 1
        {30, 31, 32, 33},     // row 2
    };
    // {} zeroing, partial init sab yahan bhi chalta hai:
    // int z[3][4] = {};                 // sab 0
    // int p[3][4] = {{1,2}};            // row 0 = {1,2,0,0}, baaki rows sab 0

    std::cout << "  m[1][2] = " << m[1][2] << "\n";
    std::cout << "  sizeof(m) = " << sizeof(m) << " bytes (12 * 4)\n";

    // ============================================================
    //  2. Grid ki tarah print
    // ============================================================
    std::cout << "\n===== 2. grid =====\n";
    for (std::size_t i = 0; i < 3; ++i) {
        std::cout << "  ";
        for (std::size_t j = 0; j < 4; ++j) std::cout << m[i][j] << " ";
        std::cout << "\n";
    }

    // ============================================================
    //  3. MEMORY LAYOUT -- sab ek line mein
    // ============================================================
    std::cout << "\n===== 3. row-major layout (addresses) =====\n";
    const int* base = &m[0][0];
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 4; ++j) {
            const std::ptrdiff_t offset = &m[i][j] - base;
            std::cout << "  m[" << i << "][" << j << "]  value " << m[i][j]
                      << "   offset " << offset
                      << "   ( i*4 + j = " << (i * 4 + j) << " )\n";
        }
    }
    std::cout << "  -> ek hi contiguous block. row 0, phir row 1, phir row 2.\n";

    // Proof: &m[0][0] + 5  ==  &m[1][1]
    std::cout << "  &m[0][0] + 5 == &m[1][1] ? "
              << ((base + 5 == &m[1][1]) ? "haan" : "nahi") << "\n";

    // ============================================================
    //  4. Ek row ek 1D array hai -- pointer-to-row
    // ============================================================
    std::cout << "\n===== 4. row = 1D array =====\n";
    int (*rowPtr)[4] = &m[1];        // "pointer to array-of-4-int" -- row 1
    std::cout << "  rowPtr = &m[1];  (*rowPtr)[2] = " << (*rowPtr)[2]
              << "   ( == m[1][2] )\n";
    // m[i] khud ek `int[4]` hai jo `int*` mein decay hota hai:
    int* firstRow = m[0];
    std::cout << "  int* firstRow = m[0];  firstRow[3] = " << firstRow[3] << "\n";

    // ============================================================
    //  5. Flat 1D indexing -- jo bade code mein prefer hota hai
    // ============================================================
    std::cout << "\n===== 5. flat 1D array + manual indexing =====\n";
    constexpr std::size_t ROWS = 3, COLS = 4;
    int flat[ROWS * COLS];
    for (std::size_t i = 0; i < ROWS; ++i)
        for (std::size_t j = 0; j < COLS; ++j)
            flat[i * COLS + j] = static_cast<int>(100 + i * COLS + j);

    std::cout << "  flat[1*4 + 2] = " << flat[1 * COLS + 2] << "   ( 'row 1, col 2' )\n";
    std::cout << "  Faayda: ek malloc/vector, size runtime pe, clean pointer math,\n"
                 "  aur std::span / std::mdspan (C++23) ke saath kaam karta hai.\n";
    std::cout << "  ⚠️ Traversal: inner loop = j (last index) -> row-major -> cache-friendly.\n";

    return 0;
}
