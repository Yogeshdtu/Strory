// 03_memory_bugs.cpp
// ============================================================
// ⚠️  MENU-DRIVEN MEMORY BUGS. Har mode ek classic memory bug
//     trigger karta hai. Compile CLEAN hoti hai (bug runtime pe;
//     index/pointer runtime-computed hai aur do chhote noinline
//     "launder" helpers se static analysis bhi bypass hota, taaki
//     -Wall/-Wextra/-Wreturn-local-addr/-Wuse-after-free chup rahe).
//     Bug pakadne ke liye SANITIZER chahiye -- yeh lesson 06 ka lab.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 03_memory_bugs.cpp -o mb
//
//   # Linux / Clang -- yahan bugs asli mein pakde jaate hain:
//   g++ -std=c++20 -g -O1 -fsanitize=address,undefined 03_memory_bugs.cpp -o mb
//   clang++ -std=c++20 -g -O1 -fsanitize=memory 03_memory_bugs.cpp -o mb_msan
//
//   ./mb leak        # definite leak -- ASan LeakSanitizer / valgrind
//   ./mb uaf         # heap use-after-free -- ASan
//   ./mb oob         # heap buffer overflow (read + write) -- ASan
//   ./mb double      # double free -- ASan
//   ./mb uninit      # uninitialized read -- MSan (Clang) / valgrind
//   ./mb stackuaf    # use of pointer to a returned local -- ASan (stack-use-after-return)
// ============================================================
// Windows/MinGW box pe libasan/libubsan nahi -- yeh file wahan
// compile hoti hai (checkall "OK"), par bug tab dikhega jab tum
// Linux/WSL/Clang pe sanitizer ke saath chalao. Har mode ka
// EXPECTED sanitizer output + FIX file ke neeche comment mein.
// ============================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// -- "launder" helpers: optimizer/static-analyzer ke liye opaque --------
// Pointer ko function ke aar-paar bhejo taaki -Wuse-after-free /
// -Wreturn-local-addr uska data-flow trace na kar paaye. UAF/double-free
// wale modes pointer ke BITS free se PEHLE capture karte (integer mein),
// warna GCC textually "p used after free" pakad leta.
[[gnu::noinline]] static int*           launder_ptr(int* p)       { return p; }
[[gnu::noinline]] static std::uintptr_t launder_bits(std::uintptr_t x) { return x; }

// runtime se aaya chhoti value -- compiler constant-fold nahi kar sakta
static int noise(const char* s) {
    int a = 0;
    for (const char* p = s; *p; ++p) a += static_cast<unsigned char>(*p);
    return a % 7;                               // 0..6
}

// ---- MODE: leak ---------------------------------------------
static void mode_leak(int n) {
    // 64-byte block allocate, kabhi free nahi. Function return -> pointer
    // gaya -> block reachable nahi -> "definitely lost".
    char* buf = static_cast<char*>(std::malloc(64 + static_cast<std::size_t>(n)));
    std::snprintf(buf, 64, "leaked block, n=%d", n);
    std::printf("%s\n", buf);
    // BUG: free(buf) missing
}

// ---- MODE: uaf (heap use-after-free) -----------------------
static void mode_uaf(int n) {
    std::vector<int>* v = new std::vector<int>(8, 0);
    (*v)[static_cast<std::size_t>(n)] = 42;
    const std::uintptr_t bits = launder_bits(reinterpret_cast<std::uintptr_t>(v));
    delete v;                                   // freed
    auto* alias = reinterpret_cast<std::vector<int>*>(bits);
    int x = (*alias)[static_cast<std::size_t>(n)];   // BUG: freed memory ko padhna
    std::printf("uaf read = %d\n", x);
}

// ---- MODE: oob (heap buffer overflow) ---------------------
static void mode_oob(int n) {
    int* a = new int[4];
    for (int i = 0; i < 4; ++i) a[i] = i;
    int idx = 4 + n;                            // 4..10 -- always out of bounds
    int* cell = launder_ptr(a) + idx;
    *cell = 99;                                 // BUG: write past end
    std::printf("oob wrote a+%d, a[0]=%d\n", idx, a[0]);
    int r = *cell;                              // BUG: read past end
    std::printf("oob read = %d\n", r);
    delete[] a;
}

// ---- MODE: double (double free) --------------------------
static void mode_double(int n) {
    void* p = std::malloc(16);
    std::memset(p, 0, 16);
    const std::uintptr_t bits = launder_bits(reinterpret_cast<std::uintptr_t>(p));
    std::free(p);
    void* q = reinterpret_cast<void*>(bits);
    if ((n & 0) == 0) {                          // hamesha true; opaque
        std::free(q);                            // BUG: dobara free (same block)
    }
    std::printf("double free done\n");
}

// ---- MODE: uninit (uninitialized read) ------------------
static int mode_uninit(int n) {
    int vals[4];                                // NOT initialized
    if (n > 100) {                              // n = 0..6 -> branch never taken
        for (int i = 0; i < 4; ++i) vals[i] = i;
    }
    int* base = launder_ptr(vals);
    int sum = 0;
    for (int i = 0; i < 4; ++i) sum += base[i];  // BUG: mostly-uninitialized read
    std::printf("uninit sum = %d\n", sum);
    return sum;
}

// ---- MODE: stackuaf (dangling pointer to a returned local) --------
static int* leak_local(int n) {
    int local = 7 + n;
    return launder_ptr(&local);                 // BUG: address of a local -- dangles on return
}
static void mode_stackuaf(int n) {
    int* p = leak_local(n);
    std::printf("stack-uaf read = %d\n", *p);   // BUG: dangling deref
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: %s <leak|uaf|oob|double|uninit|stackuaf>\n", argv[0]);
        return 2;
    }
    const std::string mode = argv[1];
    const int n = noise(argv[1]);               // 0..6, opaque to the optimizer

    if      (mode == "leak")     mode_leak(n);
    else if (mode == "uaf")      mode_uaf(n);
    else if (mode == "oob")      mode_oob(n);
    else if (mode == "double")   mode_double(n);
    else if (mode == "uninit")   (void)mode_uninit(n);
    else if (mode == "stackuaf") mode_stackuaf(n);
    else { std::printf("unknown mode: %s\n", mode.c_str()); return 2; }

    return 0;
}

// ============================================================
//          E X P E C T E D   S A N I T I Z E R   O U T P U T
//     (Linux x86-64, g++/clang++ -fsanitize=...  -- shapes;
//      addresses/line numbers thode alag ho sakte)
// ============================================================
//
// ./mb leak     (ASan + LeakSanitizer)
//   ==NNN==ERROR: LeakSanitizer: detected memory leaks
//   Direct leak of 64 byte(s) in 1 object(s) allocated from:
//       #0 ... in malloc
//       #1 ... in mode_leak(int)   03_memory_bugs.cpp:55
//       #2 ... in main             03_memory_bugs.cpp:...
//   FIX: std::free(buf) return se pehle. Behtar: std::string / std::vector
//        -- RAII, leak structurally impossible (17-RAII, 14-MEMORY/05).
//
// ./mb uaf      (ASan)
//   ==NNN==ERROR: AddressSanitizer: heap-use-after-free
//   READ of size 4 at 0x... thread T0
//       #0 ... in mode_uaf(int)    03_memory_bugs.cpp:67
//   freed by thread T0 here:
//       #1 ... in mode_uaf(int)    03_memory_bugs.cpp:66   (delete v)
//   previously allocated by thread T0 here:
//       #1 ... in mode_uaf(int)    03_memory_bugs.cpp:63   (new)
//   FIX: `delete` ke baad pointer ko chhuo mat; `v = nullptr`. unique_ptr
//        se ownership + lifetime clear (14-MEMORY/06, 17-RAII).
//
// ./mb oob      (ASan)
//   ==NNN==ERROR: AddressSanitizer: heap-buffer-overflow
//   WRITE of size 4 at 0x... thread T0
//       #0 ... in mode_oob(int)    03_memory_bugs.cpp:78
//   0x... is located 0-24 bytes to the right of 16-byte region
//   allocated by thread T0 here:  03_memory_bugs.cpp:74  (new int[4])
//   FIX: index [0,size) mein. std::vector + .at() (throws), ya
//        -D_GLIBCXX_ASSERTIONS (09-ARRAYS/11, 12-POINTERS/13).
//
// ./mb double   (ASan)
//   ==NNN==ERROR: AddressSanitizer: attempting double-free on 0x...
//       #0 ... in free
//       #1 ... in mode_double(int) 03_memory_bugs.cpp:93  (2nd free)
//   freed by thread T0 here:      03_memory_bugs.cpp:90  (1st free)
//   FIX: ek block, ek free. free ke baad `p = nullptr` (free(nullptr) safe).
//        Ownership ek jagah -- do owners = do frees (14-MEMORY, 18-COPY-MOVE).
//
// ./mb uninit   (Clang MSan -fsanitize=memory  -- ASan/g++ ise MISS karta!)
//   ==NNN==WARNING: MemorySanitizer: use-of-uninitialized-value
//       #0 ... in mode_uninit(int) 03_memory_bugs.cpp:106
//   (valgrind memcheck: "Use of uninitialised value of size 8" /
//    "Conditional jump ... depends on uninitialised value(s)")
//   FIX: `int vals[4] = {};`. -Wmaybe-uninitialized (needs -O1/-O2) bhi
//        aksar warn karta (03-VARIABLES/03).
//
// ./mb stackuaf (ASan, ASAN_OPTIONS=detect_stack_use_after_return=1  -- default on)
//   ==NNN==ERROR: AddressSanitizer: stack-use-after-return
//   READ of size 4 at 0x... thread T0
//       #0 ... in mode_stackuaf(int) 03_memory_bugs.cpp:118
//   Address is located in stack of thread T0 in frame
//       leak_local(int)              03_memory_bugs.cpp:113
//   FIX: local ka address kabhi return mat karo. Value return, ya caller
//        ka buffer/reference pass, ya heap + saaf ownership. GCC/Clang
//        -Wreturn-local-addr ise -O1+ pe warn karta (13-REFERENCES/09).
// ============================================================
