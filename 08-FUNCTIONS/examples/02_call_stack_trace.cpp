// 02_call_stack_trace.cpp
// ============================================================
// CALL STACK ko aankhon se dekho -- stack frames, addresses, growth direction
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 02_call_stack_trace.cpp -o cst && ./cst
// ============================================================
// Har function call ek naya STACK FRAME banata hai: uske parameters,
// local variables, return address, saved registers.
//
// x86-64 pe stack NEECHE ki taraf badhta hai -> har nested call ka
// frame PICHLE se KAM address pe hota hai. Hum local variables ke
// addresses print karke yeh dekhenge.
// ============================================================

#include <cstdint>
#include <iomanip>
#include <iostream>

// address ko readable hex mein
static std::uintptr_t addr(const void* p) {
    return reinterpret_cast<std::uintptr_t>(p);
}

// Compiler ko variable "use hua" manne pe majboor karta hai -- warna woh
// local ko optimize karke frame chhota kar sakta hai.
#if defined(__GNUC__) || defined(__clang__)
static inline void touch(const void* p) { asm volatile("" : : "r"(p) : "memory"); }
#else
static volatile const void* g_sink;
static inline void touch(const void* p) { g_sink = p; }
#endif

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static std::uintptr_t smallFrameAddr() {
    int x = 0;
    touch(&x);
    return addr(&x);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static std::uintptr_t bigFrameAddr() {
    char buf[4096];
    buf[0] = 0;
    touch(buf);
    return addr(&buf[0]);
}

// ------------------------------------------------------------
//  Ek call chain: level3 <- level2 <- level1 <- main
//  Har function apne ek local ka address print karta hai.
// ------------------------------------------------------------
void level3() {
    int local3 = 0;
    std::cout << "    level3()  local ka address: 0x" << std::hex << addr(&local3)
              << std::dec << "\n";
}

void level2() {
    int local2 = 0;
    std::cout << "   level2()  local ka address:  0x" << std::hex << addr(&local2)
              << std::dec << "\n";
    level3();                        // deeper -> naya frame, aur neeche
}

void level1() {
    int local1 = 0;
    std::cout << "  level1()  local ka address:   0x" << std::hex << addr(&local1)
              << std::dec << "\n";
    level2();
}

// ------------------------------------------------------------
//  Recursion: har call apni depth aur frame address print karta hai.
//  Consecutive frames ka address-diff ~ ek frame ka size.
// ------------------------------------------------------------
void recurse(int depth, std::uintptr_t prevFrame) {
    int marker = depth;                       // is frame ka local
    const std::uintptr_t here = addr(&marker);

    long long frameSize = (prevFrame == 0)
        ? 0
        : static_cast<long long>(prevFrame) - static_cast<long long>(here);

    std::cout << "  depth " << std::setw(2) << depth
              << " | frame @ 0x" << std::hex << here << std::dec;
    if (prevFrame != 0)
        std::cout << " | is call ka frame size ~ " << frameSize << " bytes";
    std::cout << "\n";

    if (depth < 6)
        recurse(depth + 1, here);
    // yahan return hone ke baad `marker` aur yeh frame GAYAB ho jaata hai
}

int main() {
    int mainLocal = 0;
    std::cout << "main()     local ka address:     0x" << std::hex << addr(&mainLocal)
              << std::dec << "\n\n";

    std::cout << "===== 1. Nested calls -- frame addresses ghatte hain =====\n";
    level1();
    std::cout << "  (har deeper call ka address CHHOTA -- stack neeche badhta hai)\n";

    std::cout << "\n===== 2. Recursion -- har call ka apna frame =====\n";
    recurse(0, 0);
    std::cout << "  (6 baar return hone ke baad hum wapas main() mein hain --\n"
                 "   saare recurse() frames unwind ho gaye)\n";

    std::cout << "\n===== 3. Ek bada local -> bada frame =====\n";
    std::cout << "  chhota local wale frame ka address: 0x" << std::hex << smallFrameAddr() << std::dec << "\n";
    std::cout << "  4096-byte buffer wale frame ka addr: 0x" << std::hex << bigFrameAddr()  << std::dec << "\n";
    std::cout << "  (bada local = zyada stack space us frame mein -> 05_stack_overflow.cpp dekho)\n";

    std::cout << "\n  NOTE: exact numbers har run/OS/compiler pe alag. Dekhna yeh hai:\n"
                 "  deeper call -> chhota address, aur frame-diff frame size batata hai.\n";
    return 0;
}
