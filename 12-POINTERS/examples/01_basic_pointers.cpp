// 01_basic_pointers.cpp
// ============================================================
// Pointer basics -- & (address-of), * (dereference), step by step
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_basic_pointers.cpp -o bp && ./bp
// ============================================================
// Pointer = ek variable jo kisi DOOSRE variable ka ADDRESS rakhta hai.
//   &x   -> "x ka address"        (address-of operator)
//   int* p = &x;  -> p mein x ka address
//   *p   -> "us address pe jo value hai"   (dereference)
// ============================================================

#include <cstdint>
#include <iomanip>
#include <iostream>

int main() {
    // ============================================================
    //  1. Ek normal variable
    // ============================================================
    std::cout << "===== 1. normal variable =====\n";
    int age = 25;
    std::cout << "  age ki value   = " << age << "\n";
    std::cout << "  age ka address = " << &age << "   (&age)\n";

    // ============================================================
    //  2. Ek pointer -- age ka address rakhta hai
    // ============================================================
    std::cout << "\n===== 2. pointer =====\n";
    int* p = &age;                     // p = "pointer to int", value = age ka address
    std::cout << "  p ki value (ek address) = " << p << "\n";
    std::cout << "  *p (us address pe value) = " << *p << "   <- 'dereference'\n";
    std::cout << "  &p (khud p ka address)   = " << &p
              << "   <- p bhi ek variable hai, uska bhi address hai\n";

    // ============================================================
    //  3. Pointer ke through value badlo
    // ============================================================
    std::cout << "\n===== 3. modify through pointer =====\n";
    *p = 30;                           // "jis pe p point karta hai, usko 30 kar do"
    std::cout << "  *p = 30;  ->  age ab = " << age << "   (age BADAL gaya!)\n";
    age = 40;
    std::cout << "  age = 40; ->  *p ab = " << *p << "   (dono ek hi memory dekh rahe)\n";

    // ============================================================
    //  4. Address ko number ki tarah dekho
    // ============================================================
    std::cout << "\n===== 4. address as a number =====\n";
    std::cout << "  address hex mein : " << p << "\n";
    std::cout << "  address decimal  : " << reinterpret_cast<std::uintptr_t>(p) << "\n";
    std::cout << "  sizeof(int*)     : " << sizeof(int*) << " bytes  (64-bit -> 8)\n";
    std::cout << "  sizeof(char*)    : " << sizeof(char*) << " bytes  (har pointer 8 bytes)\n";

    // ============================================================
    //  5. Alag types ke pointers
    // ============================================================
    std::cout << "\n===== 5. pointers of different types =====\n";
    double pi = 3.14159;
    double* pd = &pi;
    char c = 'A';
    char* pc = &c;
    std::cout << "  *pd = " << *pd << "\n";
    std::cout << "  *pc = " << *pc << "\n";
    // int* wrong = &pi;   // ❌ ERROR -- int* double ko point nahi kar sakta (type matters)

    // ============================================================
    //  6. Ek pointer, alag-alag objects
    // ============================================================
    std::cout << "\n===== 6. re-pointing =====\n";
    int a = 1, b = 2;
    int* q = &a;
    std::cout << "  q -> a,  *q = " << *q << "\n";
    q = &b;                            // ab q b ko point karta hai (references yeh nahi kar sakti)
    std::cout << "  q -> b,  *q = " << *q << "\n";

    // ============================================================
    //  SUMMARY
    // ============================================================
    std::cout <<
        "\n"
        "  int x = 5;      -- ek int, ek address pe\n"
        "  int* p = &x;    -- p mein x ka address\n"
        "  *p              -- x ki value (read/write)\n"
        "  p = &y;         -- p ab y ko point karta hai (re-bindable)\n"
        "  Har pointer 8 bytes (64-bit), chahe kis type ka ho.\n"
        "  Type (int* vs double*) batata hai deref pe kitne bytes padhne/likhne hain.\n";

    return 0;
}
