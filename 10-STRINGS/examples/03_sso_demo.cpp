// 03_sso_demo.cpp
// ============================================================
// SSO -- Small String Optimization: chhoti strings HEAP pe nahi jaatin
// ============================================================
//   ⚠️ ISE -O0 YA -O1 PE CHALAO:
//        g++ -std=c++20 -O0 -g 03_sso_demo.cpp -o sso && ./sso
//   -O2 pe GCC `std::string` ki short-lived local allocation ko poori tarah
//   ELIDE kar deta hai (C++14 new-expression elision allowance) -- tab yeh
//   demo "0 allocations" dikhaega. Woh khud ek sabak hai (folder 33), par
//   SSO threshold dekhne ke liye -O0 chahiye.
// ============================================================
// std::string ke andar ek chhota buffer hota hai (libstdc++: 15 chars +
// '\0' = 16 bytes). Us size tak ki string INLINE rehti hai -- koi
// `new`/`malloc` nahi. Us se badi -> heap allocation.
//
// Hum global `operator new` ko override karke count karenge ki EXACTLY
// kis length pe pehli heap allocation hoti hai.
// ============================================================

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <new>
#include <string>

static std::size_t g_allocCount = 0;
static std::size_t g_allocBytes = 0;

void* operator new(std::size_t n) {
    ++g_allocCount;
    g_allocBytes += n;
    if (void* p = std::malloc(n)) return p;
    throw std::bad_alloc{};
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

int main() {
    std::cout << "std::string ka sizeof = " << sizeof(std::string)
              << " bytes  (SSO buffer + size + capacity/ptr)\n\n";
    std::cout << "len | heap allocations while building a string of that length\n";
    std::cout << "----+-------------------------------------------------------\n";

    for (std::size_t len = 0; len <= 30; ++len) {
        g_allocCount = 0;
        g_allocBytes = 0;
        {
            std::string s(len, 'a');
            volatile char sink = s.empty() ? '0' : s[0];
            (void)sink;
        }
        const std::size_t n = g_allocCount, bytes = g_allocBytes;   // read before next cout
        std::cout << std::setw(3) << len << " | " << n << " alloc";
        if (n) std::cout << "  (" << bytes << " bytes)   <- HEAP";
        else   std::cout << "               <- inline (SSO)";
        std::cout << "\n";
    }

    std::cout << "\nDekha: libstdc++ pe len <= 15 -> 0 allocations (inline).\n"
                 "       len == 16 pe pehli heap allocation.\n"
                 "(libc++ ka SSO threshold 22; MSVC ka 15. Number implementation-specific.)\n\n";

    // ---- capacity growth (geometric ~2x) ----
    std::cout << "append loop -- capacity growth:\n";
    std::string g;
    std::size_t lastCap = g.capacity();
    std::cout << "  start: size=0 cap=" << lastCap << "\n";
    for (int i = 0; i < 5000; ++i) {
        g.push_back('x');
        if (g.capacity() != lastCap) {
            std::cout << "  size=" << std::setw(5) << g.size()
                      << "  cap " << lastCap << " -> " << g.capacity()
                      << "  (x" << std::fixed << std::setprecision(2)
                      << static_cast<double>(g.capacity()) / static_cast<double>(lastCap) << ")\n";
            lastCap = g.capacity();
        }
    }

    std::cout << "\n  -> reserve(n) se yeh saari reallocations bach jaati hain (lesson 07).\n"
                 "  HFT: chhote symbols/keys SSO se allocation-free; bade buffers ek baar\n"
                 "  reserve karke reuse. Folder 36.\n";

    return 0;
}
