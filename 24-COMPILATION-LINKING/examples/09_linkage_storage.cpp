// 09_linkage_storage.cpp
// ============================================================
// Linkage (lesson 05) aur storage specifiers (lesson 06) ek hi TU mein
// jitna dikha sakte hain: anonymous namespace (internal linkage),
// namespace-scope `static`/`const`, `inline` variable (C++17),
// `static` local (init guard), `thread_local` (2 threads), `constinit`.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow 09_linkage_storage.cpp -o ls -pthread && ./ls
// ============================================================

#include <cstdio>
#include <thread>
#include <vector>

// ============================================================
//  1. INTERNAL LINKAGE — is TU ke bahar dikhta hi nahi
// ============================================================

// (a) anonymous namespace — modern tarika. Naam har TU mein unique.
namespace {
    int secret_counter = 0;              // internal linkage
    int bump() { return ++secret_counter; }
}

// (b) namespace-scope `static` — purana tarika, same effect (internal linkage)
static int file_local_id = 1000;

// (c) namespace-scope `const` — DEFAULT internal linkage (jab tak `extern` na ho)
const int kMaxRetries = 3;              // internal linkage
extern const int kSharedConst = 7;     // `extern const` -> EXTERNAL linkage (dusri TU dekh sakti)

// ============================================================
//  2. `inline` variable (C++17) — EXTERNAL linkage, par ODR-safe:
//     har TU mein define karo, linker ek hi rakhega (vague linkage).
//     Header-only "global" ke liye yahi tarika hai.
// ============================================================
inline int g_build_number = 42;

// ============================================================
//  3. `extern` — declaration (definition kahin aur). Yahan ek hi TU
//     hai to hum khud niche define kar rahe.
// ============================================================
extern int g_defined_below;            // declaration — "yeh exist karta hai, kahin"
int g_defined_below = 999;             // definition

// ============================================================
//  4. `constinit` (C++20) — "compile-time constant se initialize hua"
//     guarantee. Static-init-order-fiasco se bachao; runtime init guard
//     nahi lagta. (`const` NAHI karta — value baad mein change ho sakti.)
// ============================================================
constinit int g_start_mode = 1;        // pakka compile-time init

// ============================================================
//  5. `static` local — pehli baar function chalne pe init, phir wahi
//     instance. Compiler thread-safe init guard daalta hai.
// ============================================================
int next_seq() {
    static int seq = 0;                 // ek hi `seq`, saare calls ke beech
    return ++seq;
}

// ============================================================
//  6. `thread_local` — har thread ka apna instance
// ============================================================
thread_local int t_local = 0;

static void worker(int id, int iters) {
    for (int i = 0; i < iters; ++i) ++t_local;    // sirf IS thread ka t_local
    std::printf("   thread %d: t_local = %d  (apna alag)\n", id, t_local);
}

int main() {
    // --------------------------------------------------------
    //  Internal linkage
    // --------------------------------------------------------
    std::puts("1. internal linkage (is TU tak seemit):");
    int b1 = bump();
    int b2 = bump();
    int b3 = bump();
    std::printf("   bump() x3 -> %d %d %d   (anonymous-namespace counter)\n", b1, b2, b3);
    std::printf("   file_local_id = %d   (namespace-scope static)\n", file_local_id);
    std::printf("   kMaxRetries = %d      (const -> internal by default)\n", kMaxRetries);
    std::printf("   kSharedConst = %d     (extern const -> external)\n", kSharedConst);

    // --------------------------------------------------------
    //  External / inline / constinit
    // --------------------------------------------------------
    std::puts("\n2. external + inline variable + constinit:");
    std::printf("   g_build_number = %d   (inline var: har TU define kare, linker merge)\n",
                g_build_number);
    std::printf("   g_defined_below = %d  (extern decl + separate def)\n", g_defined_below);
    std::printf("   g_start_mode = %d     (constinit: compile-time init guaranteed)\n",
                g_start_mode);
    g_start_mode = 2;                    // constinit const NAHI hai
    std::printf("   g_start_mode = %d     (badla ja sakta — constinit != const)\n", g_start_mode);

    // --------------------------------------------------------
    //  static local
    // --------------------------------------------------------
    std::puts("\n3. static local (ek instance, calls ke beech bacha rehta):");
    int s1 = next_seq();
    int s2 = next_seq();
    int s3 = next_seq();
    int s4 = next_seq();
    std::printf("   next_seq() -> %d %d %d %d\n", s1, s2, s3, s4);

    // --------------------------------------------------------
    //  thread_local
    // --------------------------------------------------------
    std::puts("\n4. thread_local (har thread ka apna):");
    ++t_local; ++t_local;               // main thread ka t_local = 2
    std::vector<std::thread> ts;
    ts.emplace_back(worker, 1, 100);
    ts.emplace_back(worker, 2, 5000);
    for (auto& th : ts) th.join();
    std::printf("   main thread: t_local = %d  (workers se bilkul alag)\n", t_local);

    std::puts("\nSaar:");
    std::puts(" - anonymous namespace / `static` at file scope = internal linkage (TU-private)");
    std::puts(" - `inline` variable = ek external global, har TU mein safely define");
    std::puts(" - `static` local = program lifetime, first-use init (thread-safe guard)");
    std::puts(" - `thread_local` = per-thread lifetime + storage");
    std::puts(" - `constinit` = compile-time init guarantee (order-fiasco fix), not const");
    return 0;
}
