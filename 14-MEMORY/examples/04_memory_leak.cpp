// 04_memory_leak.cpp
// ============================================================
// WARNING: DELIBERATE memory leaks -- allocate karke free nahi karte
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 04_memory_leak.cpp -o leak && ./leak
//
//   Yeh program crash nahi karta, "galat" output bhi nahi deta -- leak ka yahi
//   khatra hai: chup-chaap memory badhti rehti hai. Long-running process
//   (exchange gateway, server) dheere-dheere saari RAM kha jaata hai -> OOM kill.
//
//   Is file mein global `operator new`/`delete` override karke ek counter rakha
//   hai -- taaki leak is platform pe bhi DIKHE (MinGW pe LSan/Valgrind nahi).
//
//   Linux pe asli tools:
//     valgrind --leak-check=full ./leak
//     g++ -fsanitize=address -g 04_memory_leak.cpp && ./a.out    # LeakSanitizer exit pe report
// ============================================================

#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>

// ---- allocation counter (global new/delete override) ----
namespace {
    long   g_new_calls    = 0;
    long   g_delete_calls = 0;
    size_t g_bytes_req     = 0;
}

void* operator new(std::size_t n) {
    ++g_new_calls;
    g_bytes_req += n;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc{};
}
void operator delete(void* p) noexcept        { if (p) { ++g_delete_calls; std::free(p); } }
void operator delete(void* p, std::size_t) noexcept { if (p) { ++g_delete_calls; std::free(p); } }
// array-new/-delete is MinGW libstdc++ pe alag hai -- inhe bhi counted `operator new`
// se route karo, warna `new T[]` allocations chhup jaayenge.
void* operator new[](std::size_t n)              { return ::operator new(n); }
void  operator delete[](void* p) noexcept        { ::operator delete(p); }
void  operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

// ---- leaky functions ----
void leakOne() {
    int* p = new int(123);      // ⚠️ delete nahi
    *p = 456;
    // function return -> 'p' (stack) gaya, heap block ka koi naam nahi bacha -> leak
}

void leakInLoop(int times) {
    for (int i = 0; i < times; ++i) {
        auto* row = new double[128];   // ⚠️ 1 KB har iteration, kabhi delete[] nahi
        row[0] = static_cast<double>(i);
        (void)row;
    }
}

void leakViaContainerOfPointers() {
    std::vector<std::string*> v;
    for (int i = 0; i < 5; ++i)
        v.push_back(new std::string("row-" + std::to_string(i)));  // ⚠️ v destroy hoga,
    // par v ke andar ke `std::string*` delete nahi honge -> 5 string leaks
}

// ---- exit pe report ----
struct LeakReport {
    ~LeakReport() {
        std::fprintf(stderr,
            "\n[leak report] operator new: %ld   operator delete: %ld   "
            "outstanding blocks: %ld   (~%zu bytes requested total)\n",
            g_new_calls, g_delete_calls, g_new_calls - g_delete_calls, g_bytes_req);
        if (g_new_calls != g_delete_calls)
            std::fprintf(stderr, "[leak report] LEAK: %ld block(s) never freed\n",
                         g_new_calls - g_delete_calls);
    }
};
namespace { LeakReport g_report; }   // iska dtor sabse aakhir mein chalta hai

int main() {
    std::puts("=== leaking on purpose ===");

    leakOne();
    std::puts("  leakOne()                    -> 1 int leaked");

    leakInLoop(1000);
    std::puts("  leakInLoop(1000)             -> 1000 double[128] blocks leaked (~1 MB)");

    leakViaContainerOfPointers();
    std::puts("  leakViaContainerOfPointers() -> 5 std::string leaked (vector frees the");
    std::puts("                                  pointer array, NOT the pointees)");

    std::puts("\nprogram normally exit ho raha hai -- neeche leak report dekho.");
    std::puts("(Fix: har new ka delete; ya raw new hi mat use karo -- vector<int>,");
    std::puts(" vector<string>, unique_ptr. Folder 17 RAII.)");
    return 0;
}
