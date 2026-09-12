// 07_smartptr_benchmark.cpp
// ============================================================
// raw ptr vs unique_ptr vs shared_ptr -- MEASURED cost
// ============================================================
//   BENCHMARK -- -O2 ZAROORI:
//     g++ -std=c++20 -O2 07_smartptr_benchmark.cpp -o spb && ./spb
//     ya:  .\build.ps1 fast 17-RAII/examples/07_smartptr_benchmark.cpp
// ============================================================
//   Do cheezein measure karte hain:
//   A) create + destroy   -> unique_ptr == raw (zero overhead).
//                            make_shared = 1 alloc + control-block init.
//                            shared_ptr(new T) = 2 allocs.
//   B) COPY in a hot loop -> raw = ek mov (free). shared_ptr = ATOMIC inc + dec
//                            (thread-safe refcount ki cost). unique_ptr copy = nahi hoti.
// ============================================================

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>

using Clock = std::chrono::steady_clock;

struct Obj { std::int64_t a = 0, b = 0; };            // 16 bytes

static std::int64_t g_sink = 0;
static inline void sink(const void* p) { asm volatile("" : : "r"(p) : "memory"); }

template <class F>
static double time_ns_per(const char* label, long iters, F&& body) {
    auto t0 = Clock::now();
    for (long i = 0; i < iters; ++i) body(i);
    auto t1 = Clock::now();
    double ns = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    double per = ns / static_cast<double>(iters);
    std::printf("  %-34s %8.2f ns/op\n", label, per);
    return per;
}

int main() {
    const long N = 3'000'000;

    std::printf("=== A) create + destroy one Obj ===\n");
    double raw_c = time_ns_per("raw new + delete", N, [](long) {
        Obj* p = new Obj;
        sink(p);
        delete p;
    });
    double uniq_c = time_ns_per("make_unique<Obj>", N, [](long) {
        auto p = std::make_unique<Obj>();
        sink(p.get());
    });
    double mksh_c = time_ns_per("make_shared<Obj>", N, [](long) {
        auto p = std::make_shared<Obj>();
        sink(p.get());
    });
    double shnew_c = time_ns_per("shared_ptr<Obj>(new Obj)", N, [](long) {
        std::shared_ptr<Obj> p(new Obj);
        sink(p.get());
    });

    std::printf("\n  unique_ptr / raw : %.2fx   (== raw, zero overhead)\n", raw_c > 0 ? uniq_c / raw_c : 0);
    std::printf("  make_shared / raw : %.2fx   (+ control block)\n",       raw_c > 0 ? mksh_c / raw_c : 0);
    std::printf("  shared(new) / make_shared : %.2fx   (2 allocs vs 1)\n", mksh_c > 0 ? shnew_c / mksh_c : 0);

    std::printf("\n=== B) COPY an existing pointer in a hot loop ===\n");
    Obj* rawp = new Obj;
    auto up = std::make_unique<Obj>();
    auto sp = std::make_shared<Obj>();

    double raw_cp = time_ns_per("raw pointer copy", N, [&](long) {
        Obj* c = rawp;                 // just a mov
        sink(c);
    });
    double uniq_cp = time_ns_per("unique_ptr .get() (no copy)", N, [&](long) {
        Obj* c = up.get();             // unique_ptr copy hoti hi nahi -- .get() ek mov
        sink(c);
    });
    double sh_cp = time_ns_per("shared_ptr copy + destroy", N, [&](long) {
        std::shared_ptr<Obj> c = sp;   // ATOMIC ++ ; scope end -> ATOMIC --
        sink(c.get());
    });

    std::printf("\n  shared_ptr copy vs raw copy : %.1fx   (atomic inc+dec ki cost)\n",
                raw_cp > 0 ? sh_cp / raw_cp : 0);

    delete rawp;
    g_sink += sp.use_count();
    std::printf("  (sink %lld)\n", static_cast<long long>(g_sink));

    std::printf(
        "\n  Padhne ka tareeka:\n"
        "  - unique_ptr == raw (A: same ns/op; B: .get() ek mov). ZERO overhead.\n"
        "  - make_shared: raw se thoda mehnga (control block), par 1 alloc.\n"
        "  - shared_ptr(new): 2 allocs -> make_shared prefer karo.\n"
        "  - shared_ptr COPY: atomic refcount inc+dec -> raw copy se kai guna\n"
        "    (aur multi-thread mein contention -> aur bhi). Hot loop mein shared_ptr\n"
        "    by-value pass MAT karo -- const shared_ptr& ya raw T*/T& observe ke liye.\n");
    return 0;
}
