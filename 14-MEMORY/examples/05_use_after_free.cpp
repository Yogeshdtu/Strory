// 05_use_after_free.cpp
// ============================================================
// WARNING: DELIBERATE use-after-free + double-free -- runtime UB demo
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 05_use_after_free.cpp -o uaf && ./uaf
//
//   Yeh file undefined behavior karti hai. Aksar chal jaati hai (freed memory
//   abhi allocator ke paas hai, OS ko wapas nahi gayi) par:
//     - value garbage / stale ho sakti hai
//     - agar block reuse ho gaya to kisi AUR ka data corrupt hota hai
//     - double-free allocator ke metadata ko tod ke crash / heap corruption
//
//   Linux pe asli diagnosis:
//     g++ -std=c++20 -fsanitize=address -g 05_use_after_free.cpp -o uaf && ./uaf
//   -> "heap-use-after-free" / "attempting double-free", exact line + free stack.
//   MinGW-w64 pe ASan nahi -- examples/README.md.
// ============================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace { long g_new = 0, g_del = 0; }
void* operator new(std::size_t n)      { ++g_new; void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc{}; return p; }
void  operator delete(void* p) noexcept { if (p) { ++g_del; std::free(p); } }
void  operator delete(void* p, std::size_t) noexcept { if (p) { ++g_del; std::free(p); } }
void* operator new[](std::size_t n)     { return ::operator new(n); }
void  operator delete[](void* p) noexcept { ::operator delete(p); }
void  operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

static std::uintptr_t U(const void* p) { return reinterpret_cast<std::uintptr_t>(p); }

int main() {
    std::printf("=== BUG 1: read after free ===\n");
    {
        int* p = new int(1234);
        std::printf("  before free: *p = %d   @ %#llx\n", *p, (unsigned long long)U(p));
        delete p;                                // block wapas allocator ko
        std::printf("  after  free: *p = %d   (stale -- UB. abhi wahi bytes, par p invalid)\n", *p);
        // p ab dangling pointer hai (folder 12 file 12). Ise nullptr karna chahiye tha.
    }

    std::printf("\n=== BUG 2: freed block gets reused ===\n");
    {
        char* a = new char[16];
        std::strcpy(a, "AAAAAAAAAAAAAAA");
        std::uintptr_t oldAddr = U(a);
        std::printf("  a = \"%s\"   @ %#llx\n", a, (unsigned long long)oldAddr);
        delete[] a;                              // freed

        char* b = new char[16];                  // allocator wahi block de sakta hai
        std::strcpy(b, "BBBBBBBBBBBBBBB");
        std::printf("  b @ %#llx   %s\n", (unsigned long long)U(b),
                    (U(b) == oldAddr) ? "<- SAME block as freed 'a'!" : "(alag block is baar)");
        std::printf("  ab 'a' ke through padhna 'b' ka data dega (silent corruption): a=\"%s\"\n",
                    (U(b) == oldAddr) ? b : "(varies)");
        delete[] b;
    }

    std::printf("\n=== BUG 3: double free ===\n");
    {
        int* p = new int(9);
        delete p;
        // delete p;      // ⚠️ double-free -- yahan ENABLE karo to aksar crash / heap-corruption abort.
        std::printf("  (double `delete p;` comment-out hai -- enable karke dekho: crash/abort)\n");
        std::printf("  Fix pattern: delete ke baad `p = nullptr;` (nullptr ko delete karna SAFE hai)\n");
    }

    std::printf("\n[alloc] new=%ld delete=%ld\n", g_new, g_del);
    std::printf(
        "\nRules:\n"
        "  - delete ke baad pointer ko use mat karo (nullptr set karo)\n"
        "  - ek block ko do baar delete mat karo\n"
        "  - ownership clear rakho -- ek hi jagah free kare (RAII / unique_ptr, folder 17)\n");
    return 0;
}
