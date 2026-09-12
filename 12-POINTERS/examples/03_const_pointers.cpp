// 03_const_pointers.cpp
// ============================================================
// const aur pointers -- teen alag cheezein
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 03_const_pointers.cpp -o cp && ./cp
// ============================================================
// Padhne ka RULE: `const` ke DAAYE wali cheez const hoti hai.
// Ya: * se pehle -> pointee const ; * ke baad -> pointer const.
//
//   const int* p     -- pointer to const int    (data lock, pointer free)
//   int* const p     -- const pointer to int    (pointer lock, data free)
//   const int* const p -- dono lock
// ============================================================

#include <iostream>

int main() {
    int a = 10;
    int b = 20;

    // ============================================================
    //  1. const int*  --  "pointer to const int"  (== int const*)
    // ============================================================
    std::cout << "===== 1. const int* p  (data locked) =====\n";
    const int* p1 = &a;
    std::cout << "  *p1 = " << *p1 << "\n";
    // *p1 = 99;          // ❌ ERROR -- can't modify the pointed-to value THROUGH p1
    p1 = &b;             // ✅ OK -- pointer can re-point
    std::cout << "  p1 = &b;  *p1 = " << *p1 << "   (re-pointing allowed)\n";
    std::cout << "  (a is still modifiable directly: a = 11; -- p1 just can't do it)\n";
    a = 11;
    std::cout << "  a = 11 (directly);  a = " << a << "\n";

    // ============================================================
    //  2. int* const  --  "const pointer to int"  (pointer locked)
    // ============================================================
    std::cout << "\n===== 2. int* const p  (pointer locked) =====\n";
    int* const p2 = &a;              // MUST init here (can't re-point later)
    std::cout << "  *p2 = " << *p2 << "\n";
    *p2 = 100;                       // ✅ OK -- can modify the value
    std::cout << "  *p2 = 100;  ->  a = " << a << "\n";
    // p2 = &b;          // ❌ ERROR -- can't re-point

    // ============================================================
    //  3. const int* const  --  both locked
    // ============================================================
    std::cout << "\n===== 3. const int* const p  (both locked) =====\n";
    const int* const p3 = &a;
    std::cout << "  *p3 = " << *p3 << "   (read only, fixed target)\n";
    // *p3 = 5;   // ❌
    // p3 = &b;   // ❌

    // ============================================================
    //  4. Read complex declarations right-to-left
    // ============================================================
    std::cout << "\n===== 4. reading declarations =====\n";
    std::cout <<
        "  const int* p        -> p is a pointer to (const int)\n"
        "  int const* p        -> SAME as above\n"
        "  int* const p        -> p is a (const pointer) to int\n"
        "  const int* const p  -> const pointer to const int\n"
        "  const int** p       -> pointer to (pointer to const int)\n";

    // ============================================================
    //  5. Why it matters -- function parameters
    // ============================================================
    std::cout << "\n===== 5. const-correctness in APIs =====\n";
    std::cout <<
        "  void print(const int* data, size_t n);   // 'I won't modify your data'\n"
        "  void fill (int* data, size_t n);          // 'I WILL write to your data'\n"
        "  The const in the signature is a PROMISE the compiler enforces.\n"
        "  const T* / const T& params are the default for read-only access.\n";

    (void)p3;
    return 0;
}
