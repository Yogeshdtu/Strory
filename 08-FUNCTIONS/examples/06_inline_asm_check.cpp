// 06_inline_asm_check.cpp
// ============================================================
// INLINING -- compiler function call ko hata deta hai. Assembly + benchmark.
// ============================================================
//   Assembly dekho (yahi asli demo hai):
//     g++ -std=c++20 -O2 -S -masm=intel 06_inline_asm_check.cpp -o - | c++filt | less
//     (ya: ./build.ps1 asm 08-FUNCTIONS/examples/06_inline_asm_check.cpp)
//
//   Benchmark (call overhead):
//     g++ -std=c++20 -O2 06_inline_asm_check.cpp -o ia && ./ia
// ============================================================
// `inline` keyword ka asli matlab: "is definition ko multiple TUs mein
// rehne do (ODR exception)". Woh compiler ko inline karne ko MAJBOOR nahi
// karta -- aur compiler bina keyword ke bhi inline kar sakta hai.
//
// Asli inlining decision compiler `-O1`+ pe khud leta hai (function chhota?
// hot? sirf ek jagah se call?). Hum niche noinline se force karke difference
// dekhenge.
// ============================================================

#include <chrono>
#include <cstdint>
#include <iostream>

// Chhota function -- compiler -O2 pe ise call-site pe inline kar dega
static int addInline(int a, int b) {
    return a + b;
}

// Wahi function, par inlining FORCE-DISABLED -> har call pe asli `call` instruction
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int addNoInline(int a, int b) {
    return a + b;
}

#if defined(__GNUC__) || defined(__clang__)
static inline void keep(std::int64_t& x) { asm volatile("" : "+r"(x) : : ); }
#else
static volatile std::int64_t g_sink;
static inline void keep(std::int64_t& x) { g_sink = x; }
#endif

int main() {
    constexpr int N = 200'000'000;

    auto ms = [](auto a, auto b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };

    // ---- inline-able version ----
    std::int64_t s1 = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) { s1 += addInline(i, 1); keep(s1); }
    auto t1 = std::chrono::steady_clock::now();

    // ---- forced real call version ----
    std::int64_t s2 = 0;
    for (int i = 0; i < N; ++i) { s2 += addNoInline(i, 1); keep(s2); }
    auto t2 = std::chrono::steady_clock::now();

    std::cout << "N = " << N << " calls\n\n";
    std::cout << "  addInline   (compiler inlines) : " << ms(t0, t1) << " ms\n";
    std::cout << "  addNoInline (real `call` each) : " << ms(t1, t2) << " ms\n";
    if (ms(t0, t1) > 0.0)
        std::cout << "  ratio: " << (ms(t1, t2) / ms(t0, t1)) << "x\n";
    std::cout << "  per-call overhead ~ "
              << ((ms(t1, t2) - ms(t0, t1)) * 1e6 / N) << " ns\n";
    std::cout << "  (checksums " << (s1 == s2 ? "match" : "DIFFER") << ")\n";

    std::cout <<
        "\n"
        "  * `call`/`ret` + register setup + optimization-barrier (compiler dono\n"
        "    taraf ka code alag nahi mila sakta) -- yeh sab call ki cost hai.\n"
        "  * Chhoti hot functions inline hone se yeh sab gayab, AUR caller ke\n"
        "    saath optimize hoti hain (constant folding, CSE).\n"
        "  * Assembly mein dekho: `-O2 -S` -> addInline ka koi `call` nahi;\n"
        "    addNoInline ke liye `call addNoInline(int, int)`.\n"
        "  * `inline` keyword != speed. Compiler heuristics + `-O2` karte hain.\n"
        "    `[[gnu::always_inline]]` / `[[gnu::noinline]]` se force kar sakte ho.\n"
        "  * Trade-off: bahut aggressive inlining -> code bloat -> I-cache pressure.\n"
        "  Folder 08 lesson 10, folder 33 (compiler opt) mein poora.\n";

    return 0;
}
