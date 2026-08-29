// 01_first_variable.cpp
// ============================================================
// AAPKA PEHLA VARIABLE -- step by step
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 01_first_variable.cpp -o v && ./v
// ============================================================

#include <iostream>

int main() {
    // ============================================================
    //  int age = 20;
    //  ^^^ ^^^ ^ ^^
    //   |   |  |  |
    //   |   |  |  +-- VALUE: 20. Yeh ek "literal" hai -- code mein seedha likhi value.
    //   |   |  |
    //   |   |  +----- ASSIGNMENT operator. "20 ko age wale box mein daalo".
    //   |   |         Yeh "barabar hai" NAHI hai. Yeh "daalo" hai.
    //   |   |
    //   |   +-------- NAME: `age`. Yeh box ka label hai.
    //   |             Yeh naam SIRF COMPILE TIME pe exist karta hai --
    //   |             chalte hue program mein sirf address hota hai.
    //   |
    //   +------------ TYPE: `int` (integer).
    //                 Yeh compiler ko batata hai:
    //                   - kitni jagah chahiye (4 bytes)
    //                   - bits ko kaise padhna hai (signed integer)
    //                   - kaunse operations valid hain (+, -, *, /, %, ...)
    //                   - range kya hai (-2147483648 se 2147483647)
    // ============================================================

    int age = 20;

    // Ab memory mein yeh hua:
    //
    //        +----------+
    //        |    20    |     <- box ke andar ki value
    //        +----------+
    //           "age"          <- label (compile time pe)
    //        0x7ffd...c44      <- actual address (runtime pe)
    //
    // 4 bytes reserve hue, aur unme 20 ka binary likha gaya:
    //   00000000 00000000 00000000 00010100   (little-endian pe ulta stored)

    std::cout << "===== BASIC =====\n";
    std::cout << "age ki value: " << age << "\n";

    // `&` operator address deta hai (folder 12 mein detail)
    std::cout << "age ka address: " << &age << "\n";
    std::cout << "age ka size: " << sizeof(age) << " bytes\n";

    // ============================================================
    //  VALUE BADALNA
    // ============================================================
    std::cout << "\n===== VALUE BADALNA =====\n";
    age = 21;
    // Dhyaan do: yahan `int` dobara nahi likha.
    // Kyunki yeh nayi declaration nahi hai -- yeh ASSIGNMENT hai.
    // Box wahi hai, bas andar ki value badli.

    std::cout << "age ab: " << age << "\n";
    std::cout << "address ab: " << &age << "  <- WAHI address!\n";
    std::cout << "(box wahi hai, sirf andar ki cheez badli)\n";

    // ============================================================
    //  APNE AAP SE UPDATE
    // ============================================================
    std::cout << "\n===== SELF UPDATE =====\n";
    age = age + 1;
    // Kaise chala:
    //   Step 1: RIGHT side evaluate karo -> age + 1 -> 21 + 1 -> 22
    //   Step 2: Result LEFT side ke box mein daalo -> age = 22
    //
    // Math mein yeh impossible equation hai. Programming mein yeh normal instruction hai.
    // HAMESHA right pehle, phir left.
    std::cout << "age = age + 1  ->  " << age << "\n";

    // Shortcut form (same cheez, par left side ek hi baar evaluate hoti hai)
    age += 1;
    std::cout << "age += 1       ->  " << age << "\n";

    ++age;
    std::cout << "++age          ->  " << age << "\n";

    // ============================================================
    //  COPY vs LINK -- yeh bahut important hai
    // ============================================================
    std::cout << "\n===== COPY, LINK NAHI =====\n";
    int a = 5;
    int b = a;        // `b` ko `a` ki VALUE ki COPY mili -- koi link nahi bana

    std::cout << "int a = 5; int b = a;\n";
    std::cout << "  a = " << a << ", b = " << b << "\n";
    std::cout << "  a ka address: " << &a << "\n";
    std::cout << "  b ka address: " << &b << "  <- ALAG address = alag box\n";

    a = 100;
    std::cout << "a = 100 ke baad:\n";
    std::cout << "  a = " << a << ", b = " << b << "  <- b nahi badla!\n";
    std::cout << "(kyunki b ek COPY hai, alias nahi. References folder 13 mein.)\n";

    // ============================================================
    //  KAI VARIABLES
    // ============================================================
    std::cout << "\n===== KAI VARIABLES =====\n";
    int price = 100;
    int quantity = 5;
    int total = price * quantity;

    std::cout << "price    = " << price    << "  @ " << &price    << "\n";
    std::cout << "quantity = " << quantity << "  @ " << &quantity << "\n";
    std::cout << "total    = " << total    << "  @ " << &total    << "\n";
    std::cout << "(teen alag boxes, teen alag addresses)\n";

    // ============================================================
    //  ⚠️ UNINITIALIZED VARIABLE -- KABHI MAT KARNA
    // ============================================================
    std::cout << "\n===== UNINITIALIZED (KHATARNAK) =====\n";

    // int garbage;                       // ⚠️ local variable -- GARBAGE value
    // std::cout << garbage << "\n";      // ⚠️ UNDEFINED BEHAVIOUR
    //
    // Uncomment karke `-Wall` ke saath compile karo -- warning milegi.
    // Value kuch bhi ho sakti hai, aur har run pe alag ho sakti hai.

    int safe1 = 0;      // ✅ explicit
    int safe2{};        // ✅ value-initialization -> 0
    std::cout << "int safe1 = 0;  -> " << safe1 << "\n";
    std::cout << "int safe2{};    -> " << safe2 << "  <- braces se 0 mil jaata hai\n";
    std::cout << "\nGOLDEN RULE: har variable ko declare karte hi initialize karo.\n";

    return 0;
}
