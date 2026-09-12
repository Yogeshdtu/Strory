// memory_layout.cpp
// ============================================================
// LESSON 11 ka example: process memory layout ko apni aankhon se dekho
// ============================================================
//   g++ -std=c++20 -Wall -Wextra memory_layout.cpp -o memory_layout && ./memory_layout
// ============================================================

#include <iostream>
#include <iomanip>

// ---- Yeh DATA section mein jayega (initialized global) ----
int globalInitialized = 42;

// ---- Yeh BSS section mein jayega (uninitialized global, auto-zero hota hai) ----
int globalUninitialized;

// ---- String literal .rodata (read-only data) mein jaata hai ----
const char* stringLiteral = "Main read-only memory mein hoon";

// ---- Yeh TEXT/CODE section mein jayega ----
void someFunction() {
    // kuch nahi karta -- bas address dekhne ke liye hai
}

int main() {
    // ---- STACK: local variables ----
    int localA = 1;
    int localB = 2;
    double localC = 3.0;

    // ---- STATIC local: DATA section mein jaata hai, stack pe NAHI ----
    // `static` ka matlab: yeh variable poore program ke liye zinda rahega,
    // chahe function khatam ho jaye. Yeh stack pe nahi hota.
    static int staticLocal = 99;

    // ---- HEAP: new se milne wali memory ----
    int*  heapA = new int(10);
    int*  heapB = new int(20);
    int*  heapArray = new int[100];   // 400 bytes

    std::cout << "=================================================\n";
    std::cout << "  PROCESS MEMORY LAYOUT (chhote address se bade)\n";
    std::cout << "=================================================\n\n";

    std::cout << std::hex << std::showbase;

    std::cout << "TEXT (code):\n";
    std::cout << "  someFunction()   : " << reinterpret_cast<void*>(&someFunction) << "\n";
    std::cout << "  main()           : " << reinterpret_cast<void*>(&main) << "\n\n";

    std::cout << "RODATA (read-only):\n";
    std::cout << "  string literal   : " << static_cast<const void*>(stringLiteral) << "\n\n";

    std::cout << "DATA (initialized globals):\n";
    std::cout << "  globalInitialized: " << static_cast<void*>(&globalInitialized) << "\n";
    std::cout << "  staticLocal      : " << static_cast<void*>(&staticLocal) << "\n\n";

    std::cout << "BSS (uninitialized globals):\n";
    std::cout << "  globalUninit     : " << static_cast<void*>(&globalUninitialized) << "\n\n";

    std::cout << "HEAP (new/delete):\n";
    std::cout << "  heapA            : " << static_cast<void*>(heapA) << "\n";
    std::cout << "  heapB            : " << static_cast<void*>(heapB) << "\n";
    std::cout << "  heapArray        : " << static_cast<void*>(heapArray) << "\n\n";

    std::cout << "STACK (locals):\n";
    std::cout << "  localA           : " << static_cast<void*>(&localA) << "\n";
    std::cout << "  localB           : " << static_cast<void*>(&localB) << "\n";
    std::cout << "  localC           : " << static_cast<void*>(&localC) << "\n\n";

    std::cout << std::dec << std::noshowbase;

    // ---- Observations ----
    std::cout << "=================================================\n";
    std::cout << "  KYA DEKHA?\n";
    std::cout << "=================================================\n";
    std::cout << "1. Code ke addresses SABSE CHHOTE hain\n";
    std::cout << "2. Globals code ke thoda upar hain\n";
    std::cout << "3. Heap uske upar hai\n";
    std::cout << "4. Stack ke addresses SABSE BADE hain\n";
    std::cout << "5. Stack aur heap ke beech BAHUT bada gap hai\n";
    std::cout << "   (dono ek doosre ki taraf badhte hain)\n\n";

    // ---- Heap addresses ke beech ka gap ----
    // Note: yeh gap allocator ki metadata + alignment ki wajah se hota hai.
    // `new int` sirf 4 bytes maangta hai, par allocator usually 16+ bytes deta hai.
    auto gapAB = reinterpret_cast<char*>(heapB) - reinterpret_cast<char*>(heapA);
    std::cout << "heapA aur heapB ke beech gap: " << gapAB << " bytes\n";
    std::cout << "(4 bytes maange the, par allocator metadata + alignment\n";
    std::cout << " ki wajah se zyada jagah lagi. Yeh allocation ka OVERHEAD hai --\n";
    std::cout << " ek aur wajah HFT mein heap se bachne ki.)\n\n";

    // ---- Stack variables kitne paas hain ----
    auto stackGap = reinterpret_cast<char*>(&localA) - reinterpret_cast<char*>(&localB);
    std::cout << "localA aur localB ke beech gap: " << (stackGap < 0 ? -stackGap : stackGap)
              << " bytes\n";
    std::cout << "(Stack variables ek doosre ke bilkul paas hote hain --\n";
    std::cout << " isliye woh usually ek hi cache line mein aa jaate hain = FAST)\n";

    // ---- Cleanup: heap se li hui memory wapas karo ----
    // `new` ke saath `delete`, `new[]` ke saath `delete[]`. Mix mat karna -- woh UB hai.
    delete heapA;
    delete heapB;
    delete[] heapArray;

    // NOTE: Modern C++ mein hum aksar `new`/`delete` likhte hi nahi.
    //       std::vector, std::unique_ptr use karte hain (folder 17).
    //       Yahan sirf memory layout dikhane ke liye use kiya hai.

    return 0;
}
