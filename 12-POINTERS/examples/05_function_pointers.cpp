// 05_function_pointers.cpp
// ============================================================
// Function pointers -- code ka address, callbacks, dispatch tables
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 05_function_pointers.cpp -o fp && ./fp
// ============================================================
// Ek function ka bhi address hota hai. Function pointer us address
// ko rakhta hai -> aap "kaunsa function call karna hai" ko RUNTIME
// pe decide kar sakte ho. Yeh virtual dispatch ka basic mechanism hai.
// ============================================================

#include <array>
#include <iostream>

int add(int a, int b)      { return a + b; }
int sub(int a, int b)      { return a - b; }
int mul(int a, int b)      { return a * b; }

// ek function jo ek function pointer LETA hai (callback / higher-order)
int apply(int x, int y, int (*op)(int, int)) {
    return op(x, y);                    // op ke through call
}

// callback pattern -- "har element pe yeh function chalao"
void forEach(const int* arr, std::size_t n, void (*fn)(int)) {
    for (std::size_t i = 0; i < n; ++i) fn(arr[i]);
}
void printSquared(int x) { std::cout << x * x << " "; }

int main() {
    // ============================================================
    //  1. Declaring + using a function pointer
    // ============================================================
    std::cout << "===== 1. function pointer =====\n";
    int (*op)(int, int) = &add;          // & optional: `= add` bhi chalta hai
    std::cout << "  op = add;  op(3, 4)  = " << op(3, 4) << "\n";   // (*op)(3,4) bhi valid
    op = sub;
    std::cout << "  op = sub;  op(10, 3) = " << op(10, 3) << "\n";

    std::cout << "  sizeof(op) = " << sizeof(op) << " bytes  (ek address)\n";

    // `auto` se cleaner:
    auto op2 = &mul;
    std::cout << "  auto op2 = &mul;  op2(6, 7) = " << op2(6, 7) << "\n";

    // ============================================================
    //  2. Pass a function as an argument (callback)
    // ============================================================
    std::cout << "\n===== 2. callback =====\n";
    std::cout << "  apply(8, 5, add) = " << apply(8, 5, add) << "\n";
    std::cout << "  apply(8, 5, sub) = " << apply(8, 5, sub) << "\n";
    std::cout << "  apply(8, 5, mul) = " << apply(8, 5, mul) << "\n";

    int data[] = {1, 2, 3, 4, 5};
    std::cout << "  forEach(data, printSquared): ";
    forEach(data, 5, printSquared);
    std::cout << "\n";

    // ============================================================
    //  3. Dispatch table -- array of function pointers (poor man's switch)
    // ============================================================
    std::cout << "\n===== 3. dispatch table =====\n";
    using Op = int (*)(int, int);
    std::array<Op, 3> ops = { add, sub, mul };
    const char* names[] = { "add", "sub", "mul" };
    for (std::size_t i = 0; i < ops.size(); ++i)
        std::cout << "  ops[" << i << "] (" << names[i] << ")(20, 4) = "
                  << ops[i](20, 4) << "\n";
    std::cout << "  (message-type -> handler dispatch aise ban sakta hai -- folder 39)\n";

    // ============================================================
    //  4. Function pointer vs std::function vs lambda (preview)
    // ============================================================
    std::cout << "\n===== 4. modern alternatives =====\n";
    std::cout <<
        "  int (*fp)(int, int)          -- raw function pointer: fast, no state, no capture\n"
        "  auto lam = [](int a){...};    -- lambda: can capture; converts to fp only if NO capture\n"
        "  std::function<int(int,int)>   -- type-erased: any callable, but has overhead (folder 22)\n"
        "\n"
        "  Virtual functions (folder 16) andar se ek hidden function-pointer table\n"
        "  (vtable) use karti hain -- yeh us mechanism ka base hai.\n";

    // capture-less lambda -> function pointer
    int (*lamFp)(int, int) = [](int a, int b) { return a * b + 1; };
    std::cout << "  capture-less lambda as fp: lamFp(5, 5) = " << lamFp(5, 5) << "\n";

    return 0;
}
