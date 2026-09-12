// 08_swap_via_pointers.cpp
// ============================================================
// Classic: pass-by-pointer se caller ka data modify karna (swap)
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 08_swap_via_pointers.cpp -o sw && ./sw
// ============================================================
// C mein "output parameter" ka standard tareeka pointer hai.
// C++ mein reference (folder 13) zyada saaf hai -- par pointer
// version samajhna zaroori hai (C APIs, aur "kyun references better").
// ============================================================

#include <iostream>
#include <utility>   // std::swap

// ❌ by value -- copies swap hoti hain, caller ka kuch nahi badalta
void swapByValue(int a, int b) {
    int t = a; a = b; b = t;
    // yahan a, b local copies hain -- return ke baad gone
}

// ✅ by pointer -- addresses pass karo, *p se caller ka data
void swapByPointer(int* a, int* b) {
    if (a == nullptr || b == nullptr) return;   // ⚠️ null check
    int t = *a;
    *a = *b;
    *b = t;
}

// ✅ by reference (folder 13) -- same effect, cleaner syntax, no null
void swapByReference(int& a, int& b) {
    int t = a; a = b; b = t;
}

int main() {
    // ============================================================
    //  1. by value -- FAILS to swap
    // ============================================================
    std::cout << "===== 1. swapByValue (broken) =====\n";
    int x = 1, y = 2;
    swapByValue(x, y);
    std::cout << "  after swapByValue(x, y): x=" << x << " y=" << y
              << "   (⚠️ unchanged -- copies swapped)\n";

    // ============================================================
    //  2. by pointer -- works
    // ============================================================
    std::cout << "\n===== 2. swapByPointer =====\n";
    x = 1; y = 2;
    swapByPointer(&x, &y);               // pass ADDRESSES
    std::cout << "  after swapByPointer(&x, &y): x=" << x << " y=" << y << "   ✅\n";

    // ============================================================
    //  3. by reference -- also works, cleaner
    // ============================================================
    std::cout << "\n===== 3. swapByReference =====\n";
    x = 1; y = 2;
    swapByReference(x, y);               // no & at the call site
    std::cout << "  after swapByReference(x, y): x=" << x << " y=" << y << "   ✅\n";

    // ============================================================
    //  4. std::swap -- just use this
    // ============================================================
    std::cout << "\n===== 4. std::swap =====\n";
    x = 1; y = 2;
    std::swap(x, y);
    std::cout << "  after std::swap(x, y): x=" << x << " y=" << y << "   ✅\n";
    std::cout << "  (std::swap is a reference-based template + uses move -- folder 18)\n";

    // ============================================================
    //  5. Pointer version for arrays / buffers
    // ============================================================
    std::cout << "\n===== 5. swap array elements via pointers =====\n";
    int arr[5] = {10, 20, 30, 40, 50};
    swapByPointer(&arr[0], &arr[4]);
    std::cout << "  after swap arr[0]<->arr[4]: ";
    for (int v : arr) std::cout << v << " ";
    std::cout << "\n";

    std::cout <<
        "\n"
        "  by value      -> function works on COPIES, caller unaffected\n"
        "  by pointer     -> pass &x; function does *p = ... ; C-style output param\n"
        "  by reference   -> pass x; function does x = ... ; no null, cleaner (C++ way)\n"
        "  In new code: reference > pointer for output params. Pointer for\n"
        "  'optional' (nullptr = 'not provided') or C interop.\n";

    return 0;
}
