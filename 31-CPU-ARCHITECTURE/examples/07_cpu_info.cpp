// 07_cpu_info.cpp
// ============================================================
// CPUID instruction se CPU khud batata hai woh kaun hai aur kya kar sakta.
// Vendor string, brand string, family/model, aur feature bits (SSE/AVX/
// AVX2/AVX-512/BMI/FMA/POPCNT/RDTSCP/invariant-TSC ...).
//
// HFT: startup pe yeh detect karke (a) sahi SIMD code-path chuno,
// (b) assert karo ki required features hain (warna hard-fail), (c) log
// karo taaki "kaunse box pe kya build chala" pata rahe.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 07_cpu_info.cpp -o cpuinfo && ./cpuinfo
// ============================================================

#include <cstdint>
#include <cstdio>
#include <cstring>
#if defined(__GNUC__)
#  include <cpuid.h>
#endif

struct Regs { unsigned a, b, c, d; };

static Regs cpuid(unsigned leaf, unsigned sub = 0) {
    Regs r{0, 0, 0, 0};
#if defined(__GNUC__)
    __get_cpuid_count(leaf, sub, &r.a, &r.b, &r.c, &r.d);
#else
    (void)leaf; (void)sub;
#endif
    return r;
}

static void feat(const char* name, bool have) {
    std::printf("  %-14s %s\n", name, have ? "yes" : "-");
}

int main() {
#if !defined(__GNUC__)
    std::puts("needs GCC/Clang <cpuid.h>");
    return 0;
#else
    // ---- leaf 0: vendor + max standard leaf ----
    Regs r0 = cpuid(0);
    char vendor[13];
    std::memcpy(vendor + 0, &r0.b, 4);
    std::memcpy(vendor + 4, &r0.d, 4);
    std::memcpy(vendor + 8, &r0.c, 4);
    vendor[12] = '\0';
    std::printf("vendor            : %s   (max standard leaf 0x%X)\n", vendor, r0.a);

    // ---- leaf 1: family/model/stepping + feature bits (ecx, edx) ----
    Regs r1 = cpuid(1);
    unsigned stepping =  r1.a        & 0xF;
    unsigned model    = (r1.a >> 4)  & 0xF;
    unsigned family   = (r1.a >> 8)  & 0xF;
    unsigned ext_model = (r1.a >> 16) & 0xF;
    unsigned ext_fam   = (r1.a >> 20) & 0xFF;
    if (family == 0xF) family += ext_fam;
    if (family == 0x6 || family == 0xF) model += (ext_model << 4);
    std::printf("family/model/step : %u / %u / %u\n", family, model, stepping);

    // ---- brand string: leaves 0x80000002..4 ----
    Regs be = cpuid(0x80000000);
    if (be.a >= 0x80000004) {
        char brand[49];
        Regs b2 = cpuid(0x80000002), b3 = cpuid(0x80000003), b4 = cpuid(0x80000004);
        std::memcpy(brand + 0,  &b2, 16);
        std::memcpy(brand + 16, &b3, 16);
        std::memcpy(brand + 32, &b4, 16);
        brand[48] = '\0';
        // brand string aksar leading spaces ke saath aata
        const char* p = brand; while (*p == ' ') ++p;
        std::printf("brand             : %s\n", p);
    }

    std::puts("\nfeatures:");
    feat("SSE2",    (r1.d & (1u << 26)) != 0);
    feat("SSE4.1",  (r1.c & (1u << 19)) != 0);
    feat("SSE4.2",  (r1.c & (1u << 20)) != 0);
    feat("POPCNT",  (r1.c & (1u << 23)) != 0);
    feat("AES",     (r1.c & (1u << 25)) != 0);
    feat("AVX",     (r1.c & (1u << 28)) != 0);
    feat("FMA",     (r1.c & (1u << 12)) != 0);
    feat("RDRAND",  (r1.c & (1u << 30)) != 0);
    feat("RDTSCP",  (cpuid(0x80000001).d & (1u << 27)) != 0);

    // ---- leaf 7 sub 0: AVX2, BMI, AVX-512 ----
    Regs r7 = cpuid(7, 0);
    feat("BMI1",     (r7.b & (1u << 3))  != 0);
    feat("BMI2",     (r7.b & (1u << 8))  != 0);
    feat("AVX2",     (r7.b & (1u << 5))  != 0);
    feat("AVX512F",  (r7.b & (1u << 16)) != 0);
    feat("AVX512BW", (r7.b & (1u << 30)) != 0);
    feat("AVX512VL", (r7.b & (1u << 31)) != 0);

    // ---- leaf 0x80000007: invariant TSC (folder 29 file 16) ----
    Regs ri = cpuid(0x80000007);
    feat("invariant-TSC", (ri.d & (1u << 8)) != 0);

    std::puts("\nKya seekha:");
    std::puts(" - CPUID(leaf, subleaf) -> eax/ebx/ecx/edx. Leaf 0 = vendor, 1 = family +");
    std::puts("   common features, 7/0 = newer (AVX2/BMI/AVX-512), 0x80000002-4 = brand.");
    std::puts(" - HFT startup checklist: assert required features (e.g. AVX2 + FMA +");
    std::puts("   invariant-TSC); pick the SIMD path; log vendor/model/microcode so a");
    std::puts("   perf regression can be tied to a hardware/BIOS change.");
    std::puts(" - GCC function multiversioning (`__attribute__((target_clones(...)))` /");
    std::puts("   `target(...)`) does this dispatch for you -- but you still want an");
    std::puts("   explicit startup assert + log, not a silent fallback.");
    std::puts(" - `-march=native` bakes in THIS box's features -> binary won't run (or");
    std::puts("   #UD-crashes) on an older CPU. Build for the deployment target; keep a");
    std::puts("   runtime check for the mismatch.");
    return 0;
#endif
}
