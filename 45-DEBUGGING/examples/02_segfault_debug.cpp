// 02_segfault_debug.cpp
// ============================================================
// ⚠️  YEH PROGRAM CRASH KARTA HAI (SIGSEGV).  Yeh galti jaan-boojh
//     kar hai -- iska kaam hai tumhe crash-debugging sikhana.
//     Ismein compile ERROR nahi hai (build/checkall isse "OK" ginte
//     hain) -- runtime pe segfault hota hai.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 02_segfault_debug.cpp -o sf
//   ./sf                 # -> "Segmentation fault"
//   gdb -q ./sf
//     (gdb) run
//     (gdb) backtrace
//     (gdb) print p
//     (gdb) frame 1
//     (gdb) info locals
// ============================================================
// Linux pe core dump se post-mortem:
//   ulimit -c unlimited
//   ./sf                 # -> "Segmentation fault (core dumped)"
//   gdb -q ./sf core     # ya  coredumpctl debug
//     (gdb) bt full
// ============================================================

#include <cstdio>
#include <cstdlib>
#include <vector>

struct Node {
    int          value;
    Node*        next;
};

// Ek linked list ka sum. BUG neeche.
static long sum_list(const Node* head) {
    long total = 0;
    // BUG: loop ki koi terminating condition nahi. Aakhri node ke baad
    // `p` == nullptr hota hai, phir `p->value` = null dereference = SIGSEGV.
    for (const Node* p = head; ; p = p->next) {
        total += p->value;
    }
    return total;
}

// argc ka use isliye ki compiler `head` ko compile-time pe null prove na
// kar sake (-Wnull-dereference chup rahe). Normal run mein argc == 1.
int main(int argc, char** /*argv*/) {
    Node c{30, nullptr};
    Node b{20, &c};
    Node a{10, &b};

    const Node* head = (argc > 999) ? nullptr : &a;

    std::printf("summing list...\n");
    long s = sum_list(head);            // <- yahan se crash tak pahunchega
    std::printf("sum = %ld\n", s);      // yeh line kabhi nahi chhapegi
    return 0;
}

// ============================================================
//                    C R A S H   A N A L Y S I S
// ============================================================
//
// GDB session (is box pe real output):
//
//   (gdb) run
//   Thread 1 received signal SIGSEGV, Segmentation fault.
//   0x... in sum_list (head=0x...) at 02_segfault_debug.cpp:39
//   39              total += p->value;
//
//   (gdb) print p
//   $1 = (const Node *) 0x0            <-- p null hai
//
//   (gdb) print total
//   $2 = 60                            <-- 10+20+30: loop teeno node ghoom
//                                          chuka, ab null pe hai
//
//   (gdb) backtrace
//   #0  sum_list (head=0x...) at 02_segfault_debug.cpp:39
//   #1  main (argc=1) at 02_segfault_debug.cpp:54
//
// DIAGNOSIS:
//   `total == 60` batata hai loop ne saare valid nodes (10,20,30) add kar
//   liye -- crash *aakhri* iteration ke baad hua jab `p = c.next = nullptr`.
//   Symptom line 39 pe hai; asli bug line 38 pe -- loop condition khaali
//   hai (`for (...; ; ...)`).
//
// FIX:
//   for (const Node* p = head; p != nullptr; p = p->next) {
//       total += p->value;
//   }
//
// SEEKH:
//   - SIGSEGV = "aisi memory chhui jo tumhari nahi / accessible nahi".
//     Address 0x0 (null) sabse common; chhota address (0x8, 0x20) = null
//     struct pe member access (`p->field` jab p null, field ka offset).
//   - Crash ki LINE aur bug ki LINE alag ho sakti. `total`/locals se
//     reconstruct karo kaise pahunche.
//   - `-fsanitize=address` (Linux/Clang) yahi turant, ek clear message
//     mein bataata: "SEGV on unknown address 0x000000000000".
// ============================================================
