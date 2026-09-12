// 03_shared_ptr.cpp
// ============================================================
// std::shared_ptr -- shared ownership via reference counting
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 03_shared_ptr.cpp -o sp && ./sp
// ============================================================
//   shared_ptr<T> = pointer + pointer-to-CONTROL-BLOCK (refcounts).
//   - copy  -> strong count ++   (ATOMIC -- thread-safe, par cost)
//   - destroy/reset -> strong count --   ; count 0 -> object deleted
//   - control block strong==0 && weak==0 pe free hota hai
//   sizeof(shared_ptr) == 2 * sizeof(void*)  (16 on 64-bit)
// ============================================================

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>

// ---- allocation counter (make_shared 1 alloc vs shared_ptr(new T) 2 allocs) ----
namespace { long g_new = 0, g_del = 0; }
void* operator new(std::size_t n)      { ++g_new; void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc{}; return p; }
void  operator delete(void* p) noexcept { if (p) { ++g_del; std::free(p); } }
void  operator delete(void* p, std::size_t) noexcept { if (p) { ++g_del; std::free(p); } }

struct Resource {
    int id;
    explicit Resource(int i) : id(i) { std::cout << "  Resource(" << id << ") ctor\n"; }
    ~Resource()                      { std::cout << "  Resource(" << id << ") dtor\n"; }
};

void observe(std::shared_ptr<Resource> s) {          // BY VALUE -> ek aur copy -> count ++
    std::cout << "  observe(): id=" << s->id << ", use_count=" << s.use_count() << "\n";
}   // s scope end -> count --

int main() {
    std::cout << "=== 1. refcount lifecycle ===\n";
    std::shared_ptr<Resource> a = std::make_shared<Resource>(1);
    std::cout << "  after make_shared     : use_count = " << a.use_count() << "\n";   // 1
    {
        std::shared_ptr<Resource> b = a;             // copy -> count 2
        std::cout << "  after copy to b       : use_count = " << a.use_count() << "\n"; // 2
        std::shared_ptr<Resource> c = b;             // copy -> count 3
        std::cout << "  after copy to c       : use_count = " << a.use_count() << "\n"; // 3
        observe(a);                                   // by value -> 4 inside, back to 3 after
        std::cout << "  after observe()       : use_count = " << a.use_count() << "\n"; // 3
    }   // b, c destroyed -> count back to 1
    std::cout << "  after inner scope     : use_count = " << a.use_count() << "\n";   // 1

    std::cout << "\n=== 2. reset ===\n";
    a.reset();                                        // count 1 -> 0 -> Resource(1) deleted
    std::cout << "  after a.reset()       : a is " << (a ? "set" : "null") << "\n";

    std::cout << "\n=== 3. make_shared (1 alloc) vs shared_ptr(new T) (2 allocs) ===\n";
    long base_new = g_new, base_del = g_del;
    {
        auto x = std::make_shared<Resource>(2);      // object + control block -> ONE allocation
    }
    long ms_new = g_new - base_new, ms_del = g_del - base_del;
    std::cout << "  make_shared<Resource>(2)   -> " << ms_new << " new, " << ms_del << " delete\n";

    base_new = g_new; base_del = g_del;
    {
        std::shared_ptr<Resource> y(new Resource{3}); // object alloc + separate control block alloc -> TWO
    }
    long sp_new = g_new - base_new, sp_del = g_del - base_del;
    std::cout << "  shared_ptr(new Resource{3}) -> " << sp_new << " new, " << sp_del << " delete\n";
    std::cout << "  -> make_shared: 1 alloc, better locality. Prefer it.\n";

    std::cout << "\n=== 4. sizeof ===\n";
    std::cout << "  sizeof(Resource*)          = " << sizeof(Resource*) << "\n";
    std::cout << "  sizeof(unique_ptr<Resource>) = " << sizeof(std::unique_ptr<Resource>) << "\n";
    std::cout << "  sizeof(shared_ptr<Resource>) = " << sizeof(std::shared_ptr<Resource>)
              << "   (ptr + control-block ptr)\n";

    std::printf("\n[alloc] total new=%ld delete=%ld\n", g_new, g_del);
    std::cout <<
        "  shared_ptr use jab ownership GENUINELY shared ho (kai owners, unpredictable\n"
        "  lifetime). Warna unique_ptr (zero cost) -- ya value semantics.\n";
    return 0;
}
