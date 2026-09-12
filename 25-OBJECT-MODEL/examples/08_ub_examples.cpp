// 08_ub_examples.cpp
// ============================================================
// UB ka chhota catalog. Zyada tar cases #if 0 ke peeche hain (chalne pe
// crash/garbage) — padhne ke liye. Kuch "harmless dikhne wale" cases
// actually run hote hain taaki -O2 pe surprising result dikhe.
//
//   compile-verify:  g++ -std=c++20 -Wall -Wextra -Wshadow 08_ub_examples.cpp -o ub && ./ub
//   Linux pe pakdo:  g++ -std=c++20 -O1 -g -fsanitize=address,undefined 08_ub_examples.cpp -o ub && ./ub
//   (MinGW pe libasan/libubsan nahi — Linux/Clang chahiye)
//
//   overflow_check divergence:
//     g++ -std=c++20 -O2 08_ub_examples.cpp        -> "1. ... = 1"  (UB: check folded away)
//     g++ -std=c++20 -O2 -fwrapv 08_ub_examples.cpp -> "1. ... = 0"  (overflow defined)
// ============================================================

#include <cstdio>
#include <cstdint>
#include <climits>
#include <cstring>

// prevent the optimizer from folding everything away
static volatile int g_sink;
static int identity(int x) { return x; }   // opaque-ish at -O0

// ============================================================
//  RUN THESE — "harmless dikhne wale" par -O2 pe surprising
// ============================================================

// 1. Signed overflow: compiler MAAN leta hai `x + 1 > x` hamesha true
//    (kyunki signed overflow UB hai). GCC ise `1` fold kar deta hai
//    HAR optimization level pe (-O0 bhi) — yeh check DEAD hai.
//    Sirf `-fwrapv` (jo signed overflow ko wrap = defined bana deta) pe
//    yeh INT_MAX ke liye `0` deta hai (asli wrap-around detect).
static int overflow_check(int x) {
    return x + 1 > x;
}

// 2. Null-check-after-deref: `*p` ne "p != null" imply kiya ->
//    -O2 pe `if (!p)` DELETE. (Yahan p hamesha valid, safe to run.)
static int deref_then_check(int* p) {
    int v = *p;                 // "p valid hai" — warna UB
    if (!p) return -1;          // -O2: dead code -> removed
    return v + identity(0);
}

// 3. Infinite loop with no side effects — compiler forward-progress
//    maan sakta hai. (Yahan condition false hai, safe.)
static int maybe_loop(bool go) {
    int i = 0;
    while (go) { ++i; if (i == 0) break; }   // `go` false -> skip
    return i;
}

int main() {
    std::puts("=== cases that RUN (see -O0 vs -O2 difference) ===\n");

    int big = identity(INT_MAX);
    std::printf("1. overflow_check(INT_MAX) = %d   "
                "(GCC folds `x+1>x` to 1 at every -O; only -fwrapv gives 0)\n",
                overflow_check(big));

    int local = 42;
    std::printf("2. deref_then_check(&local) = %d   "
                "(the `if (!p)` is removed at -O2 — never run deref_then_check(nullptr))\n",
                deref_then_check(&local));

    std::printf("3. maybe_loop(false) = %d\n", maybe_loop(false));

    g_sink = overflow_check(big) + deref_then_check(&local) + maybe_loop(false);
    std::printf("   [sink = %d]\n", g_sink);

    // ========================================================
    //  READ-ONLY catalog (chalao mat — #if 0)
    // ========================================================
    std::puts("\n=== UB catalog (code padho — #if 0, chalao mat) ===");

#if 0
    // --- MEMORY ---
    int a[4] = {};
    int x = a[4];                       // (M1) out-of-bounds read
    a[4] = 1;                           // (M2) out-of-bounds write
    int* p = new int(5);
    delete p; delete p;                // (M3) double free
    int y = *p;                        // (M4) use-after-free
    int* q;                            // (M5) uninitialized -> read is UB for most types
    int z = *q;
    int arr[10]; free(arr);            // (M6) free() on non-malloc pointer
    char* c = (char*)malloc(4); delete c;   // (M7) malloc + delete mismatch

    // --- INTEGERS / ARITH ---
    int o = INT_MAX + 1;              // (I1) signed overflow
    int d = 1 / 0;                    // (I2) divide by zero
    int m = INT_MIN % -1;            // (I3) INT_MIN % -1
    int s1 = 1 << 32;               // (I4) shift >= width
    int s2 = 1 << -1;              // (I5) negative shift count
    int fi = (int)1e20;          // (I6) float->int out of range

    // --- LIFETIME / OBJECTS ---
    struct T { int v; };
    const T ct{5};
    const_cast<T&>(ct).v = 9;        // (L1) modifying a truly-const object
    int& refToTemp = *new int(3);
    delete &refToTemp; int rr = refToTemp;   // (L2) use after delete via ref
    // returning &local from a function -> (L3) dangling
    // calling a pure virtual from ctor/dtor -> (L4)

    // --- SEQUENCING ---
    int i = 0;
    i = i++ + ++i;                   // (S1) multiple unsequenced modifications
    int aa[3]; int k = 0; aa[k] = k++;   // (S2) unsequenced

    // --- TYPE / ALIASING ---
    float f = 1.5f;
    int fb = *(int*)&f;             // (T1) strict aliasing (see 06_strict_aliasing.cpp)
    // reading inactive union member (implementation-defined in C, UB-ish in C++)

    // --- LIBRARY PRECONDITIONS ---
    std::vector<int> v;
    int fr = v.front();             // (P1) front() on empty
    std::string str; char cc = str[str.size()] = 'x';   // (P2) writing at size()
    std::memcpy(buf+1, buf, n);    // (P3) overlapping memcpy (use memmove)
    std::optional<int> opt;
    int ov = *opt;                 // (P4) deref empty optional

    // --- CONCURRENCY ---
    // two threads, one non-atomic object, >=1 write, no sync -> (C1) data race
    // (folder 26/27)

    // --- __builtin_unreachable / [[assume]] reached ---
    if (big > 0) {} else __builtin_unreachable();   // (U1) if false -> UB
#endif

    std::puts("\nSaar:");
    std::puts(" - UB ka matlab: standard KOI requirement nahi lagata. Compiler yeh");
    std::puts("   maan ke chalta hai ki UB kabhi hoti hi nahi, aur US par optimize");
    std::puts("   karta hai (dead-code elim, range narrowing, load reuse).");
    std::puts(" - '-O0 pe kaam kar gaya' UB ke safe hone ka proof NAHI hai.");
    std::puts(" - CI mein -fsanitize=address,undefined (+ thread) chalao — har hit fix.");
    std::puts(" - defensively likho: fixed-width ints + overflow checks, .at()/bounds off");
    std::puts("   hot path, RAII (no UAF/double-free), std::bit_cast (no aliasing), init everything.");
    return 0;
}
