// 06_exception_safety.cpp
// ============================================================
// RAII + exceptions -- stack unwinding har acquired resource ko release karta hai
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 06_exception_safety.cpp -o es && ./es
// ============================================================
//   Jab `throw` hota hai:
//   - stack unwind hota hai -- har FULLY-CONSTRUCTED local ka destructor chalta
//   - RAII objects (unique_ptr, lock_guard, apne guards) -> resource released
//   - raw `new` + manual `delete` -> agar beech mein throw -> `delete` MISS -> LEAK
// ============================================================

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>

namespace { long g_new = 0, g_del = 0; }
void* operator new(std::size_t n)      { ++g_new; void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc{}; return p; }
void  operator delete(void* p) noexcept { if (p) { ++g_del; std::free(p); } }
void  operator delete(void* p, std::size_t) noexcept { if (p) { ++g_del; std::free(p); } }

struct Res {
    int id;
    explicit Res(int i) : id(i) { std::cout << "    + Res(" << id << ")\n"; }
    ~Res()                      { std::cout << "    - Res(" << id << ")\n"; }
};

// ---- BAD: raw new + manual delete -> throw ke beech mein -> leak ----
void rawVersion(bool boom) {
    std::cout << "  rawVersion(boom=" << std::boolalpha << boom << "):\n";
    Res* a = new Res{1};
    Res* b = new Res{2};
    if (boom) throw std::runtime_error("boom!");   // ⚠️ a, b ka delete MISS -> LEAK
    delete b;
    delete a;
}

// ---- GOOD: unique_ptr -> throw pe bhi destructors chalte ----
void raiiVersion(bool boom) {
    std::cout << "  raiiVersion(boom=" << std::boolalpha << boom << "):\n";
    auto a = std::make_unique<Res>(10);
    auto b = std::make_unique<Res>(11);
    if (boom) throw std::runtime_error("boom!");   // ✅ unwinding -> ~b, ~a chalte -> no leak
    // koi manual delete nahi
}

static void report(const char* label, long n0, long d0) {
    long out = (g_new - n0) - (g_del - d0);
    std::printf("    [%s] new=%ld delete=%ld  outstanding=%ld%s\n",
                label, g_new - n0, g_del - d0, out, out ? "  <- LEAK" : "");
}

int main() {
    std::cout << "=== raw new/delete, NO exception (works) ===\n";
    { long n = g_new, d = g_del; rawVersion(false); report("raw ok", n, d); }

    std::cout << "\n=== raw new/delete, WITH exception (LEAKS) ===\n";
    { long n = g_new, d = g_del;
      try { rawVersion(true); } catch (const std::exception& e) { std::cout << "  caught: " << e.what() << "\n"; }
      report("raw throw", n, d);
    }

    std::cout << "\n=== RAII (unique_ptr), NO exception ===\n";
    { long n = g_new, d = g_del; raiiVersion(false); report("raii ok", n, d); }

    std::cout << "\n=== RAII (unique_ptr), WITH exception (still clean) ===\n";
    { long n = g_new, d = g_del;
      try { raiiVersion(true); } catch (const std::exception& e) { std::cout << "  caught: " << e.what() << "\n"; }
      report("raii throw", n, d);
    }

    std::printf("\n[total] new=%ld delete=%ld  outstanding=%ld\n", g_new, g_del, g_new - g_del);
    std::cout <<
        "  Raw new/delete: throw ke beech = leak. Har early-exit path pe delete\n"
        "  yaad rakhna practically impossible.\n"
        "  RAII: destructor GUARANTEED chalta (return, break, exception -- sab).\n"
        "  Isliye modern C++ mein raw owning `new` code smell hai.\n";
    return 0;
}
