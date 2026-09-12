// 04_virtual_destructor.cpp
// ============================================================
// WARNING: Missing virtual destructor -> partial destruction -> LEAK / UB
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 04_virtual_destructor.cpp -o vd && ./vd
//
//   Jaan-boojh kar 1 warning: -Wdelete-non-virtual-dtor
//   (base class ke paas virtual functions hain par dtor virtual NAHI, aur usse
//    delete ho raha hai)
//
//   `delete basePtr;` jahan basePtr ek Derived ko point karta hai:
//   - base dtor VIRTUAL   -> ~Derived() phir ~Base()  -> sab members freed  ✅
//   - base dtor non-virtual -> sirf ~Base()           -> Derived ke members LEAK, UB  ❌
//
//   MinGW pe ASan nahi -> counted operator new/delete se leak DIKHTA hai.
// ============================================================

#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>

namespace { long g_new = 0, g_del = 0; }
void* operator new(std::size_t n)      { ++g_new; void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc{}; return p; }
void  operator delete(void* p) noexcept { if (p) { ++g_del; std::free(p); } }
void  operator delete(void* p, std::size_t) noexcept { if (p) { ++g_del; std::free(p); } }
void* operator new[](std::size_t n)     { return ::operator new(n); }
void  operator delete[](void* p) noexcept { ::operator delete(p); }
void  operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

// ---- BAD: non-virtual destructor in a polymorphic base ----
struct BadBase {
    virtual void process() { std::printf("  BadBase::process\n"); }
    ~BadBase() { std::printf("  ~BadBase()\n"); }         // ⚠️ NOT virtual
};
struct BadDerived : BadBase {
    std::vector<int> buffer_;                              // owns heap memory
    std::string      label_;
    BadDerived() : buffer_(500, 7), label_("a fairly long label that won't fit in SSO buffer") {}
    void process() override { std::printf("  BadDerived::process (buffer %zu)\n", buffer_.size()); }
    ~BadDerived() { std::printf("  ~BadDerived()  (buffer + label freed)\n"); }
};

// ---- GOOD: virtual destructor ----
struct GoodBase {
    virtual void process() { std::printf("  GoodBase::process\n"); }
    virtual ~GoodBase() { std::printf("  ~GoodBase()\n"); }   // ✅ virtual
};
struct GoodDerived : GoodBase {
    std::vector<int> buffer_;
    std::string      label_;
    GoodDerived() : buffer_(500, 7), label_("a fairly long label that won't fit in SSO buffer") {}
    void process() override { std::printf("  GoodDerived::process (buffer %zu)\n", buffer_.size()); }
    ~GoodDerived() override { std::printf("  ~GoodDerived()  (buffer + label freed)\n"); }
};

int main() {
    std::printf("=== BAD: non-virtual base destructor ===\n");
    {
        BadBase* p = new BadDerived;
        p->process();
        delete p;             // ⚠️ -Wdelete-non-virtual-dtor -- sirf ~BadBase() chalega
    }                         //     BadDerived::buffer_ aur label_ ke destructors SKIP -> leak

    std::printf("\n=== GOOD: virtual base destructor ===\n");
    {
        GoodBase* p = new GoodDerived;
        p->process();
        delete p;             // ~GoodDerived() -> ~GoodBase() -> sab freed
    }

    std::printf("\n[alloc counter]  operator new: %ld   operator delete: %ld   outstanding: %ld\n",
                g_new, g_del, g_new - g_del);
    if (g_new != g_del)
        std::printf("[LEAK] %ld block(s) never freed -- BadDerived ka buffer_/label_ (non-virtual dtor)\n",
                    g_new - g_del);

    std::printf(
        "\nRule: agar class polymorphic hai (koi virtual method) aur usse Base* se\n"
        "delete hoga -> destructor VIRTUAL banao. Warna:\n"
        "  - `public virtual ~Base() = default;`  (normal case)\n"
        "  - ya `protected: ~Base();` non-virtual  (Base* se delete rok do)\n");
    return 0;
}
