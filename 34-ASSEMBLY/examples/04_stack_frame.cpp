// 04_stack_frame.cpp
// ============================================================
// Stack frame = ek function ke locals + saved registers + return address ka
// area. Prologue frame set karta, epilogue use undo karta.
//
//   ./build.ps1 asm 34-ASSEMBLY/examples/04_stack_frame.cpp
//
// Dekhne ki cheezein (Intel, System V se thoda alag -- Windows x64 ABI):
//   prologue :  push rbp / mov rbp, rsp / sub rsp, N     (frame pointer version)
//         ya :  sub rsp, N                                (frame-pointer omitted, -O2 default)
//   locals   :  [rsp+off]  ya  [rbp-off]
//   callee-saved regs :  push rbx/rsi/rdi/r12..r15  agar function unhe use kare
//   epilogue :  add rsp, N / pop rbp / ret   ya   leave / ret
//
// -O2 pe GCC aksar frame pointer (rbp) omit karta -> `-fno-omit-frame-pointer`
// se wapas aata (profiling ke liye zaroori -- folder 35).
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 04_stack_frame.cpp -o sf && ./sf
//   g++ -std=c++20 -O2 -fno-omit-frame-pointer -S -masm=intel 04_stack_frame.cpp -o -
// ============================================================

#include <cstdint>
#include <cstdio>

// functions non-static -> standalone asm; printf/volatile are the sinks.

// 1. leaf, no locals -> minimal/no frame (just uses arg registers)
std::int64_t add3(std::int64_t a, std::int64_t b, std::int64_t c) {
    return a + b + c;
}

// 2. a big local array -> forces `sub rsp, <big N>` in the prologue
std::int64_t sum_local_buffer(std::int64_t seed) {
    std::int64_t buf[256];               // 2 KiB local -> real frame
    for (int i = 0; i < 256; ++i) buf[i] = seed + i * i;
    std::int64_t s = 0;
    for (int i = 0; i < 256; ++i) s += buf[i];
    return s;
}

// 3. calls another function -> must preserve its own state across the call;
//    non-leaf -> `call`, and the return address is pushed by `call`
std::int64_t caller(std::int64_t x) {
    std::int64_t y = add3(x, x + 1, x + 2);   // `call add3`
    return y * y;
}

// 4. recursion -> a new frame per call; watch rsp move down each level
std::int64_t factorial(std::int64_t n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);          // NOT tail (mul after the call) -> real recursion
}

// 5. tail call -> `-O2` turns this into a `jmp` (no new frame, no stack growth)
std::int64_t sum_to(std::int64_t n, std::int64_t acc) {
    if (n == 0) return acc;
    return sum_to(n - 1, acc + n);        // tail position -> `jmp sum_to` at -O2
}

// print where locals live at runtime (frame addresses go DOWN with each call)
static void show_depth(int depth) {
    volatile int local = depth;
    std::printf("  depth %d : &local = %p\n", depth, static_cast<void*>(const_cast<int*>(&local)));
    if (depth < 4) show_depth(depth + 1);
    (void)local;
}

int main() {
    std::int64_t a = add3(10, 20, 30);
    std::int64_t b = sum_local_buffer(3);
    std::int64_t c = caller(7);
    std::int64_t d = factorial(10);
    std::int64_t e = sum_to(1000, 0);
    std::printf("add3(10,20,30)      = %lld\n", static_cast<long long>(a));
    std::printf("sum_local_buffer(3) = %lld\n", static_cast<long long>(b));
    std::printf("caller(7)           = %lld\n", static_cast<long long>(c));
    std::printf("factorial(10)       = %lld\n", static_cast<long long>(d));
    std::printf("sum_to(1000,0)      = %lld  (tail-call -> a loop at -O2)\n", static_cast<long long>(e));

    std::puts("\nframe addresses shrink with recursion depth:");
    show_depth(0);

    std::puts("\nRead the asm:");
    std::puts("  ./build.ps1 asm 34-ASSEMBLY/examples/04_stack_frame.cpp");
    std::puts("  add3            : no `sub rsp` -- pure register (lea/add), `ret`");
    std::puts("  sum_local_buffer: `sub rsp, 0x8xx` prologue, `add rsp,..`/`ret` epilogue");
    std::puts("  caller          : `call add3` ; result reused -> `imul`");
    std::puts("  factorial       : `call factorial` inside itself -- real recursion");
    std::puts("  sum_to          : NO `call` -- `-O2` made the tail call a `jmp` (a loop)");
    return 0;
}
