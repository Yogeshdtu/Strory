// 06_inline_asm.cpp
// ============================================================
// GCC extended inline assembly. Syntax:
//
//   asm [volatile] ( "template"
//                    : output operands      // "=r"(x), "+r"(y), "=m"(z)
//                    : input operands       // "r"(a),  "i"(5),  "m"(w)
//                    : clobbers );          // "cc", "memory", "rax", ...
//
// Constraints: r=any GP reg, m=memory, i=immediate, =write, +read-write,
//              a/b/c/d=rax/rbx/rcx/rdx, "cc"=flags clobbered, "memory"=barrier.
//
// ⚠️ Aapko inline asm kabhi-kabhi hi chahiye:
//   - special instructions jinke liye intrinsic nahi (`cpuid`, `rdtsc` variants,
//     `pause`, MSR/port I/O in kernel)
//   - the zero-instruction optimization barrier (`DoNotOptimize`)
//   - a tiny hot sequence the compiler consistently mis-schedules (rare, measure)
// Warna: intrinsics (`<immintrin.h>`) use karo -- portable, compiler
// optimize kar sakta, aur galti ka scope kam.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 06_inline_asm.cpp -o iasm && ./iasm
// ============================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

// ---- 1. the optimization barrier (this repo's `keep()` / DoNotOptimize) ----
template <class T>
static inline void DoNotOptimize(T& v) {
    asm volatile("" : "+r,m"(v) : : "memory");   // 0 instructions; "escape" v
}

// ---- 2. a real instruction: CPUID (no intrinsic that's this direct) ----
struct Regs { unsigned a, b, c, d; };
static Regs cpuid(unsigned leaf, unsigned subleaf) {
    Regs r{};
    asm volatile("cpuid"
                 : "=a"(r.a), "=b"(r.b), "=c"(r.c), "=d"(r.d)  // outputs in eax/ebx/ecx/edx
                 : "a"(leaf), "c"(subleaf));                     // inputs
    return r;
}

// ---- 3. rdtsc, hand-written (shows EDX:EAX combine) ----
static std::uint64_t rdtsc() {
    unsigned lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));      // low->eax, high->edx
    return (static_cast<std::uint64_t>(hi) << 32) | lo;
}

// ---- 4. a small computed sequence: (x*5 + 1) via lea, forced ----
// NOTE: default GCC inline asm = AT&T syntax. `lea 1(%1,%1,4), %0` means
//       %0 = 1 + %1 + %1*4  (disp(base, index, scale)). Intel form would be
//       `lea %0, [%1 + %1*4 + 1]` and needs -masm=intel.
static std::uint64_t times5_plus1(std::uint64_t x) {
    std::uint64_t r;
    asm("lea 1(%1,%1,4), %0" : "=r"(r) : "r"(x));
    return r;
}

// ---- 5. spin hint: `pause` (intrinsic exists as _mm_pause, shown for contrast) ----
static inline void cpu_relax() { asm volatile("pause" ::: "memory"); }

int main() {
    // 1. barrier
    std::uint64_t acc = 0;
    for (int i = 0; i < 1000; ++i) { acc += static_cast<std::uint64_t>(i) * static_cast<std::uint64_t>(i); DoNotOptimize(acc); }
    std::printf("1. barrier: acc = %llu (loop not deleted)\n", (unsigned long long)acc);

    // 2. cpuid: vendor string is in EBX,EDX,ECX of leaf 0
    Regs v = cpuid(0, 0);
    char vendor[13];
    std::memcpy(vendor + 0, &v.b, 4);
    std::memcpy(vendor + 4, &v.d, 4);
    std::memcpy(vendor + 8, &v.c, 4);
    vendor[12] = '\0';
    std::printf("2. cpuid leaf 0 vendor : \"%s\"  (max leaf %u)\n", vendor, v.a);
    Regs f1 = cpuid(1, 0);
    std::printf("   leaf 1: SSE2=%d  AVX=%d  (edx bit26 / ecx bit28)\n",
                (f1.d >> 26) & 1, (f1.c >> 28) & 1);

    // 3. rdtsc delta over a tiny loop
    std::uint64_t t0 = rdtsc();
    volatile std::uint64_t sink = 0;
    for (int i = 0; i < 1000; ++i) sink += static_cast<std::uint64_t>(i);
    std::uint64_t t1 = rdtsc();
    std::printf("3. rdtsc: ~%llu ticks for a 1000-iter add loop (unfenced -- noisy)\n",
                (unsigned long long)(t1 - t0));

    // 4. lea trick
    std::printf("4. times5_plus1(20) = %llu  (should be 101)\n",
                (unsigned long long)times5_plus1(20));

    // 5. pause (just call it; used in spin-wait loops -- folder 28)
    for (int i = 0; i < 4; ++i) cpu_relax();
    std::puts("5. `pause` x4 executed (spin-loop hint)");

    std::puts("\nRead the asm: ./build.ps1 asm 34-ASSEMBLY/examples/06_inline_asm.cpp");
    std::puts(" - DoNotOptimize : the `asm volatile(\"\")` produces ZERO instructions");
    std::puts(" - cpuid         : `mov eax,LEAF ; xor ecx,ecx ; cpuid ; mov ...`");
    std::puts(" - rdtsc         : `rdtsc ; sal rdx,32 ; or rax,rdx`");
    std::puts(" - times5_plus1  : a single `lea reg, [reg + reg*4 + 1]`");
    std::puts("\nRule: prefer intrinsics (<immintrin.h>, __builtin_*). Inline asm only");
    std::puts("for what has no intrinsic, or the barrier. Wrong constraints/clobbers =");
    std::puts("silent corruption that -O2 exposes.");
    return 0;
}
