// 02_pointer_arithmetic.cpp
// ============================================================
// Pointer arithmetic -- p + 1 type ke SIZE se badhta hai
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 02_pointer_arithmetic.cpp -o pa && ./pa
// ============================================================
// int* p;  p + 1  ->  address + sizeof(int) bytes  (4, na ki 1)
// Isi liye arr[i] == *(arr + i) kaam karta hai (folder 09).
// Sirf ARRAY ke andar (aur one-past-the-end tak) valid hai.
// ============================================================

#include <cstddef>
#include <iostream>

int main() {
    int arr[5] = {10, 20, 30, 40, 50};

    // ============================================================
    //  1. p + 1 = next element ka address
    // ============================================================
    std::cout << "===== 1. p + 1 jumps by sizeof(element) =====\n";
    int* p = arr;                          // arr -> &arr[0] (decay, folder 09)
    std::cout << "  p       = " << static_cast<const void*>(p)       << "   *p       = " << *p << "\n";
    std::cout << "  p + 1   = " << static_cast<const void*>(p + 1)   << "   *(p + 1) = " << *(p + 1) << "\n";
    std::cout << "  p + 2   = " << static_cast<const void*>(p + 2)   << "   *(p + 2) = " << *(p + 2) << "\n";
    std::cout << "  (har jump = " << sizeof(int) << " bytes, sizeof(int))\n";

    // ============================================================
    //  2. arr[i]  ==  *(arr + i)
    // ============================================================
    std::cout << "\n===== 2. arr[i] == *(arr + i) =====\n";
    for (int i = 0; i < 5; ++i)
        std::cout << "  arr[" << i << "] = " << arr[i]
                  << "   *(arr + " << i << ") = " << *(arr + i) << "\n";

    // ============================================================
    //  3. Chalte hue pointer se poora array ghoomo
    // ============================================================
    std::cout << "\n===== 3. pointer walk =====\n";
    std::cout << "  ";
    for (int* it = arr; it != arr + 5; ++it)   // arr + 5 = "one past the end" (COMPARE kar sakte ho, deref nahi)
        std::cout << *it << " ";
    std::cout << "\n";

    // ============================================================
    //  4. Pointer ka ghatav -> beech mein kitne elements
    // ============================================================
    std::cout << "\n===== 4. pointer subtraction =====\n";
    int* start = &arr[1];
    int* end   = &arr[4];
    std::ptrdiff_t n = end - start;        // ptrdiff_t = signed, "kitne elements"
    std::cout << "  &arr[4] - &arr[1] = " << n << "   (elements, NOT bytes)\n";

    // ============================================================
    //  5. Pointers pe ++ / -- / +=
    // ============================================================
    std::cout << "\n===== 5. ++ / += =====\n";
    int* q = arr;
    std::cout << "  *q        = " << *q << "\n";
    ++q;                                   // agla element (address + 4 bytes)
    std::cout << "  ++q; *q   = " << *q << "\n";
    q += 2;
    std::cout << "  q += 2; *q= " << *q << "\n";
    --q;
    std::cout << "  --q; *q   = " << *q << "\n";

    // ============================================================
    //  6. ⚠️ Out-of-range pointer arithmetic = UB
    // ============================================================
    std::cout << "\n===== 6. ⚠️ valid range =====\n";
    std::cout << "  arr .. arr+5  -> pointers valid (arr+5 = one-past-end, compare only)\n";
    std::cout << "  arr - 1       -> ⚠️ UB (before the array)\n";
    std::cout << "  arr + 6       -> ⚠️ UB (past one-past-end)\n";
    std::cout << "  *(arr + 5)    -> ⚠️ UB (deref of one-past-end)\n";
    std::cout << "  Pointer arithmetic sirf EK array ke andar defined hai.\n";

    // char* -- byte-level arithmetic (char* kisi bhi object ke bytes padh sakta hai -- aliasing ka exception)
    std::cout << "\n===== 7. char* = byte stepping =====\n";
    const char* bytes = reinterpret_cast<const char*>(arr);
    std::cout << "  arr ke pehle 8 bytes: ";
    for (int i = 0; i < 8; ++i) std::cout << static_cast<int>(bytes[i]) << " ";
    std::cout << "\n  (10 aur 20 ki little-endian byte representation)\n";

    return 0;
}
