// 05_stack_overflow.cpp
// ============================================================
//  ⚠️  YEH PROGRAM JAAN-BOOJH KAR CRASH HOTA HAI  ⚠️
// ============================================================
//  Yeh COMPILE theek hota hai. Chalane par STACK OVERFLOW se crash karta
//  hai (Linux/Mac: "Segmentation fault", Windows: exit code 0xC00000FD).
//
//  Maqsad: dekhna ki
//    1. bina BASE CASE ke recursion infinite hai
//    2. har call ek stack frame khaata hai
//    3. stack ki ek LIMIT hai (Linux default ~8 MB, Windows ~1 MB)
//    4. limit paar -> OS process ko maar deta hai (graceful nahi)
//
//  RUN:
//    g++ -std=c++20 -O0 -g 05_stack_overflow.cpp -o so && ./so
//    echo $?          # Linux: 139 (128+SIGSEGV=11)  Windows: -1073741571
//
//  ⚠️ NOTE: `make folder` / `checkall` sirf COMPILE karte hain -> yeh "OK"
//     dikhega. Isko HAATH se chalao aur crash dekho.
// ============================================================

#include <iostream>

// ------------------------------------------------------------
//  Har call:
//   - ~1 KB ka local buffer (frame ko bada aur "real" banata hai --
//     warna compiler tail-call optimize karke loop bana sakta hai)
//   - buffer ko touch karta hai (compiler optimize na kar sake)
//   - har 5000 depth pe progress print
//   - phir KHUD KO call -- koi base case NAHI
// ------------------------------------------------------------
void goDeeper(int depth) {
    volatile char frameHog[1024];       // is frame ka ~1 KB
    frameHog[0] = static_cast<char>(depth & 0xFF);
    frameHog[1023] = frameHog[0];

    if (depth % 500 == 0) {
        std::cout << "  depth = " << depth
                  << "   (~" << depth << " KB stack use -- har frame ~1 KB)\n";
        std::cout.flush();               // crash se pehle output nikal jaaye
    }

    goDeeper(depth + 1);                 // ⚠️ NO BASE CASE -- yeh kabhi return nahi karta

    // yeh line kabhi execute nahi hoti:
    std::cout << frameHog[0];
}

int main() {
    std::cout << "Stack ko bharna shuru... (crash aane wala hai)\n";
    std::cout << "Har frame ~1 KB. Dekhte hain kitni door tak pahunchte hain.\n\n";
    std::cout.flush();

    goDeeper(0);

    // yahan kabhi nahi pahunchte
    std::cout << "\nYeh line kabhi print nahi hogi.\n";
    return 0;
}

// ============================================================
//  KYA SEEKHNA HAI
// ============================================================
//  1. RECURSION KO HAMESHA EK BASE CASE CHAHIYE. `if (n <= 0) return;`
//     jaisa kuch -- jo call chain ko rok de.
//
//  2. Stack overflow ka crash "clean" nahi hota -- koi exception nahi,
//     koi "index out of range" message nahi. OS bas process ko maar deta hai
//     (SIGSEGV). Debugger (gdb) mein backtrace hazaaron identical frames
//     dikhaayega -- yahi signature hai.
//
//  3. Stack size:
//       Linux:   ulimit -s   (default 8192 KB), pthread_attr_setstacksize se badla ja sakta hai
//       Windows: linker /STACK option (default 1 MB)
//     Deep recursion chahiye to -> iteration mein badlo, ya explicit
//     std::stack<T> heap pe use karo, ya thread ko bada stack do.
//
//  4. Tail-call optimization (TCO): agar recursive call function ka
//     AAKHRI kaam ho aur compiler `-O2` pe TCO kare, to woh recursion ko
//     loop bana deta hai -> koi overflow nahi. C++ TCO GUARANTEE nahi karta
//     (Scheme ke ulat). Isi wajah se yahan `frameHog` aur `-O0` -- taaki
//     crash reliably dikhe.
//
//  5. Sahi version:
//       void goDeeper(int depth) {
//           if (depth >= 1'000'000) return;   // BASE CASE
//           goDeeper(depth + 1);
//       }
//     ...par 10 lakh frames abhi bhi overflow kar sakte hain. Deep kaam
//     iteration se karo.
// ============================================================
