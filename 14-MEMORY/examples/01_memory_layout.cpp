// 01_memory_layout.cpp
// ============================================================
// Ek process ki memory ke segments -- har ek ka address print
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_memory_layout.cpp -o ml && ./ml
// ============================================================
//   Classic (Linux) layout, low address -> high address:
//
//     [ .text   ]  <- code (functions)                 read-only, executable
//     [ .rodata ]  <- string literals, const globals   read-only
//     [ .data   ]  <- initialized globals/statics       read-write
//     [ .bss    ]  <- zero-initialized globals/statics  read-write (file mein 0 bytes)
//     [ heap    ]  --> grows UP    (new / malloc)
//         ...  big gap  ...
//     [ stack   ]  <-- grows DOWN  (locals, call frames)
//
//   ⚠️ Yeh exact order OS pe depend karta hai. Windows (PE) pe segments ka aapsi
//   order thoda alag ho sakta hai, aur heap/stack ka relative position bhi.
//   Concept jo har jagah sach hai: (1) code/const alag read-only region, (2)
//   globals alag read-write region, (3) heap ek badhne wala region, (4) stack
//   ek alag region jo call frames se ghatta-badhta hai. Numbers ASLR se badalte.
// ============================================================

#include <cstdint>
#include <iostream>

int   g_init      = 42;        // .data  (non-zero initializer)
int   g_zero      = 0;         // .bss   (zero -> file mein jagah nahi ghera)
const int g_const = 7;         // .rodata (read-only)
static int s_file = 100;       // .data  (internal linkage, same segment)

void some_function() {}        // .text

static std::uintptr_t U(const void* p) { return reinterpret_cast<std::uintptr_t>(p); }

int main() {
    int local = 1;                        // stack
    static int s_local = 5;               // .data (function-local static -- still static storage)
    int* heap = new int(9);               // heap
    const char* literal = "hello";        // 'literal' pointer: stack;  "hello" bytes: .rodata

    std::cout << std::hex << std::showbase;
    std::cout << "=== code / read-only ===\n";
    std::cout << "  &some_function (.text)   : " << U(reinterpret_cast<const void*>(&some_function)) << "\n";
    std::cout << "  \"hello\" literal (.rodata): " << U(literal) << "\n";
    std::cout << "  &g_const (.rodata)       : " << U(&g_const) << "\n";

    std::cout << "\n=== initialized / zero data ===\n";
    std::cout << "  &g_init (.data)          : " << U(&g_init) << "\n";
    std::cout << "  &s_file (.data)          : " << U(&s_file) << "\n";
    std::cout << "  &s_local (.data)         : " << U(&s_local) << "\n";
    std::cout << "  &g_zero (.bss)           : " << U(&g_zero) << "\n";

    // heap ka "direction" allocator pe depend: Windows UCRT pe #2 kabhi upar, kabhi neeche mila
    std::cout << "\n=== heap (classic Linux picture: grows up; allocator pe depend) ===\n";
    int* heap2 = new int(10);
    std::cout << "  new int #1               : " << U(heap)  << "\n";
    std::cout << "  new int #2               : " << U(heap2) << "   (> #1 ho zaroori nahi)\n";

    std::cout << "\n=== stack (grows down) ===\n";
    int local2 = 2;
    std::cout << "  &local                   : " << U(&local)  << "\n";
    std::cout << "  &local2                  : " << U(&local2) << "   (usually < &local)\n";
    std::cout << "  &heap (the pointer var)  : " << U(&heap)   << "   (also stack)\n";

    std::cout << std::dec;
    std::cout << "\n  Note: .text/.data/.rodata/.bss ek doosre ke paas (image ka hissa).\n";
    std::cout << "  heap aur stack alag-alag dur regions mein -- inke beech bada gap.\n";
    std::cout << "  Stack vs heap kaun upar: OS pe depend -- Linux pe classically stack >> heap,\n";
    std::cout << "  Windows pe order alag ho sakta hai. ASLR har run pe numbers badalta hai.\n";
    std::cout << "  Linux pe asli map: `cat /proc/$(pgrep ml)/maps`.\n";

    delete heap;
    delete heap2;
    (void)local2;
    return 0;
}
