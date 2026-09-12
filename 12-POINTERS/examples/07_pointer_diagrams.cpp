// 07_pointer_diagrams.cpp
// ============================================================
// Memory state ko har step pe print karo -- pointers ko "dekho"
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 07_pointer_diagrams.cpp -o pd && ./pd
// ============================================================
// Har step pe: variable ki value, uska address, aur pointer kya
// point karta hai -- ek chhote table ke roop mein.
// ============================================================

#include <cstdint>
#include <iomanip>
#include <iostream>

// address ko chhota, readable offset mein badlo (base se relative)
static std::uintptr_t g_base = 0;
static long rel(const void* p) {
    auto v = reinterpret_cast<std::uintptr_t>(p);
    if (g_base == 0) g_base = v;
    return static_cast<long>(v) - static_cast<long>(g_base);
}

static void show(const char* step, int a, const void* aAddr,
                 int b, const void* bAddr,
                 const int* p, const void* pAddr) {
    std::cout << "  " << std::left << std::setw(22) << step << " | ";
    std::cout << "a=" << std::setw(3) << a << "@" << std::setw(4) << rel(aAddr) << "  ";
    std::cout << "b=" << std::setw(3) << b << "@" << std::setw(4) << rel(bAddr) << "  ";
    std::cout << "p@" << std::setw(4) << rel(pAddr) << " -> ";
    if (p == nullptr)          std::cout << "nullptr";
    else if (p == aAddr)       std::cout << "a  (*p=" << *p << ")";
    else if (p == bAddr)       std::cout << "b  (*p=" << *p << ")";
    else                       std::cout << "?  (*p=" << *p << ")";
    std::cout << "\n";
}

int main() {
    std::cout << "  (addresses shown as offsets from the first variable)\n\n";
    std::cout << "  step                   | a               b               p\n";
    std::cout << "  -----------------------+---------------------------------------------\n";

    int a = 10;
    int b = 20;
    int* p = nullptr;
    show("int a=10, b=20; p=null", a, &a, b, &b, p, &p);

    p = &a;
    show("p = &a;", a, &a, b, &b, p, &p);

    *p = 15;
    show("*p = 15;", a, &a, b, &b, p, &p);

    p = &b;
    show("p = &b;", a, &a, b, &b, p, &p);

    *p += 100;
    show("*p += 100;", a, &a, b, &b, p, &p);

    a = *p;
    show("a = *p;", a, &a, b, &b, p, &p);

    p = nullptr;
    show("p = nullptr;", a, &a, b, &b, p, &p);

    // ============================================================
    //  Pointer to pointer
    // ============================================================
    std::cout << "\n===== pointer to pointer =====\n";
    int x = 7;
    int* px = &x;
    int** ppx = &px;
    std::cout << "  x   = " << x << "   @" << rel(&x) << "\n";
    std::cout << "  px  -> x        @" << rel(&px) << "   *px  = " << *px << "\n";
    std::cout << "  ppx -> px       @" << rel(&ppx) << "  *ppx = px,  **ppx = " << **ppx << "\n";
    **ppx = 42;
    std::cout << "  **ppx = 42;  ->  x = " << x << "\n";

    return 0;
}
