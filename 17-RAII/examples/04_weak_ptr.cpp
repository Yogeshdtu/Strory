// 04_weak_ptr.cpp
// ============================================================
// std::weak_ptr -- non-owning observer; shared_ptr CYCLE todna
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 04_weak_ptr.cpp -o wp && ./wp
// ============================================================
//   Problem: do shared_ptr ek doosre ko rakhein -> refcount kabhi 0 nahi ->
//            dono LEAK (memory kabhi free nahi).
//   Fix:     ek direction ko weak_ptr banao -> woh strong count nahi badhata ->
//            cycle toot jaata -> destructors chalte.
//   weak_ptr use karne ke liye: .lock() -> shared_ptr (ya empty agar object gaya)
// ============================================================

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>

namespace { long g_new = 0, g_del = 0; }
void* operator new(std::size_t n)      { ++g_new; void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc{}; return p; }
void  operator delete(void* p) noexcept { if (p) { ++g_del; std::free(p); } }
void  operator delete(void* p, std::size_t) noexcept { if (p) { ++g_del; std::free(p); } }

// ---- BAD: parent <-> child dono shared_ptr -> cycle -> leak ----
namespace bad {
struct Node {
    std::string             name;
    std::shared_ptr<Node>   other;                 // ⚠️ strong -- cycle banega
    explicit Node(std::string n) : name(std::move(n)) { std::cout << "  bad::Node(" << name << ") ctor\n"; }
    ~Node() { std::cout << "  bad::Node(" << name << ") dtor\n"; }
};
void run() {
    auto a = std::make_shared<Node>("A");
    auto b = std::make_shared<Node>("B");
    a->other = b;                                   // A -> B  (B count 2)
    b->other = a;                                   // B -> A  (A count 2)
    std::cout << "  a.use_count=" << a.use_count() << ", b.use_count=" << b.use_count() << "\n";
}   // a, b scope se gaye -> counts 2 -> 1 (kabhi 0 nahi) -> NO dtor -> LEAK
}

// ---- GOOD: back-edge weak_ptr -> cycle nahi ----
namespace good {
struct Node {
    std::string           name;
    std::shared_ptr<Node> next;                     // forward: strong (owns)
    std::weak_ptr<Node>   prev;                     // back: WEAK (observes, doesn't own)
    explicit Node(std::string n) : name(std::move(n)) { std::cout << "  good::Node(" << name << ") ctor\n"; }
    ~Node() { std::cout << "  good::Node(" << name << ") dtor\n"; }
};
void run() {
    auto a = std::make_shared<Node>("A");
    auto b = std::make_shared<Node>("B");
    a->next = b;                                    // A owns B  (b count 2)
    b->prev = a;                                    // B observes A  (a count STILL 1 -- weak)
    std::cout << "  a.use_count=" << a.use_count() << ", b.use_count=" << b.use_count() << "\n";

    // weak_ptr use: lock() -> temporary shared_ptr
    if (auto p = b->prev.lock()) std::cout << "  b->prev.lock() -> " << p->name << "\n";
    else                         std::cout << "  b->prev expired\n";
}   // a gaya -> A count 0 -> ~A -> a->next reset -> B count 0 -> ~B.  NO leak
}

int main() {
    std::cout << "=== BAD: shared_ptr cycle ===\n";
    long b0n = g_new, b0d = g_del;
    bad::run();
    std::cout << "  [bad] new=" << (g_new - b0n) << " delete=" << (g_del - b0d)
              << "  -> outstanding " << (g_new - b0n) - (g_del - b0d) << "  (LEAK -- no dtors ran)\n";

    std::cout << "\n=== GOOD: weak_ptr back-edge ===\n";
    long g0n = g_new, g0d = g_del;
    good::run();
    std::cout << "  [good] new=" << (g_new - g0n) << " delete=" << (g_del - g0d)
              << "  -> outstanding " << (g_new - g0n) - (g_del - g0d) << "  (clean -- dtors ran)\n";

    std::cout << "\n=== weak_ptr after object destroyed ===\n";
    std::weak_ptr<int> w;
    {
        auto s = std::make_shared<int>(42);
        w = s;
        std::cout << "  in scope : w.expired()=" << std::boolalpha << w.expired()
                  << ", *w.lock()=" << *w.lock() << "\n";
    }   // s gone -> object destroyed
    std::cout << "  out of scope : w.expired()=" << w.expired()
              << ", w.lock() is " << (w.lock() ? "valid" : "empty") << "\n";

    std::cout <<
        "\n"
        "  weak_ptr: 'main dekhta hoon par own nahi karta'.\n"
        "  Cycles ke liye (parent<->child, observer lists), aur caches ke liye.\n"
        "  Access: .lock() -> shared_ptr; check karo empty to nahi.\n";
    return 0;
}
