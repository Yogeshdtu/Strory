// 02_array_decay.cpp
// ============================================================
// ARRAY -> POINTER DECAY -- aur sizeof ka classic trap
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 02_array_decay.cpp -o dec && ./dec
// ============================================================
// Jab array ko function ko pass karo (ya pointer context mein use karo),
// woh apne pehle element ke POINTER mein "decay" ho jaata hai.
// Result: function ke andar array ka SIZE gayab -- sirf ek address bacha.
// ============================================================

#include <cstddef>
#include <iostream>
#include <iterator>

// ------------------------------------------------------------
//  1. `int arr[]` parameter -- yeh JHOOTH hai. Compiler isse `int* arr` bana deta hai.
// ------------------------------------------------------------
void takesArray(int arr[]) {
    std::cout << "  [inside takesArray]  sizeof(arr) = " << sizeof(arr)
              << "  <- POINTER ka size (8), array ka nahi!\n";
    // std::size(arr);   // ❌ compile error -- arr ek pointer hai, array nahi
}

void takesPointer(int* arr) {   // upar wale se BILKUL same signature
    std::cout << "  [inside takesPointer] sizeof(arr) = " << sizeof(arr) << "\n";
    (void)arr;
}

// yeh dono declarations SAME function hain (int arr[10] mein bhi 10 ignore hota hai):
void sameFn(int arr[]);
void sameFn(int* arr);          // ⚠️ NOT an overload -- redeclaration
void sameFn(int arr[]) { (void)arr; }

// ------------------------------------------------------------
//  2. Size ko SAATH mein pass karo -- honest tareeka
// ------------------------------------------------------------
long long sumWithSize(const int* arr, std::size_t n) {
    long long s = 0;
    for (std::size_t i = 0; i < n; ++i) s += arr[i];
    return s;
}

// ------------------------------------------------------------
//  3. REFERENCE-to-array -- decay NAHI hota, size preserve
// ------------------------------------------------------------
template <std::size_t N>
long long sumArrayRef(const int (&arr)[N]) {   // N compiler DEDUCE karta hai
    long long s = 0;
    for (std::size_t i = 0; i < N; ++i) s += arr[i];
    return s;
}

int main() {
    int data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // ============================================================
    //  1. main() mein array poora hai
    // ============================================================
    std::cout << "===== 1. main() mein array =====\n";
    std::cout << "  sizeof(data)       = " << sizeof(data) << " bytes  (10 * 4)\n";
    std::cout << "  std::size(data)    = " << std::size(data) << " elements\n";

    // ============================================================
    //  2. Function ko pass karte hi -> DECAY
    // ============================================================
    std::cout << "\n===== 2. function ko pass -> decay =====\n";
    takesArray(data);
    takesPointer(data);
    std::cout << "  dono ke andar sizeof = 8. Array ka '10' kho gaya.\n";

    // decay explicitly bhi hota hai:
    int* p = data;               // array naam -> pointer to data[0]  (koi & nahi chahiye)
    std::cout << "  int* p = data;  ->  *p = " << *p << ", p[3] = " << p[3] << "\n";
    std::cout << "  data == &data[0] ? " << (p == &data[0] ? "haan" : "nahi") << "\n";

    // ============================================================
    //  3. Size ko saath pass karke sahi sum
    // ============================================================
    std::cout << "\n===== 3. size saath pass karo =====\n";
    std::cout << "  sumWithSize(data, std::size(data)) = "
              << sumWithSize(data, std::size(data)) << "\n";

    // ============================================================
    //  4. Reference-to-array -- decay nahi, size auto
    // ============================================================
    std::cout << "\n===== 4. reference-to-array (no decay) =====\n";
    std::cout << "  sumArrayRef(data) = " << sumArrayRef(data)
              << "   <- N=10 compiler ne khud nikala, size pass karne ki zaroorat nahi\n";
    // sumArrayRef(p);   // ❌ compile error -- p ek pointer hai, array nahi

    // ============================================================
    //  5. Decay se OFF-BY-ONE bug
    // ============================================================
    std::cout << "\n===== 5. classic decay bug =====\n";
    std::cout << "  Galti: function ke andar `for (i = 0; i < sizeof(arr); ++i)` likhna\n";
    std::cout << "  -> sizeof(arr) = 8 (pointer), 10 nahi -> sirf 8/4 = 2 elements process\n";
    std::cout << "  Fix: size parameter, std::span (05_span_demo.cpp), ya std::array\n";

    sameFn(data);
    return 0;
}
