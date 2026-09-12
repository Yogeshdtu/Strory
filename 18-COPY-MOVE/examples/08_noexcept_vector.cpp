// 08_noexcept_vector.cpp
// ============================================================
// noexcept move -- std::vector growth ka behaviour (MEASURED)
// ============================================================
//   BENCHMARK -- -O2:
//     g++ -std=c++20 -O2 08_noexcept_vector.cpp -o nev && ./nev
// ============================================================
//   std::vector grow karte waqt elements ko naye buffer mein le jaata hai.
//   - move ctor `noexcept`      -> vector elements ko MOVE karta (O(1) each) -- fast
//   - move ctor NOT noexcept    -> vector COPY karta (strong exception guarantee ke liye) -- slow
//   (agar move throw ho jaaye realloc ke beech mein to vector half-broken state pe hota;
//    isse bachne ke liye standard non-noexcept move ke saath copy karta hai.)
// ============================================================

#include <chrono>
#include <cstring>
#include <iostream>
#include <type_traits>
#include <vector>

using Clock = std::chrono::steady_clock;

// Same guts; only difference is `noexcept` on the move ctor.
struct WithNoexcept {
    std::size_t n_    = 0;
    int*        data_ = nullptr;
    explicit WithNoexcept(std::size_t n) : n_(n), data_(new int[n]) { std::memset(data_, 1, n * sizeof(int)); }
    WithNoexcept(const WithNoexcept& o) : n_(o.n_), data_(new int[o.n_]) { std::memcpy(data_, o.data_, n_ * sizeof(int)); }
    WithNoexcept(WithNoexcept&& o) noexcept : n_(o.n_), data_(o.data_) { o.n_ = 0; o.data_ = nullptr; }
    WithNoexcept& operator=(WithNoexcept&&) noexcept = default;
    WithNoexcept& operator=(const WithNoexcept&) = delete;
    ~WithNoexcept() { delete[] data_; }
};

struct WithoutNoexcept {
    std::size_t n_    = 0;
    int*        data_ = nullptr;
    explicit WithoutNoexcept(std::size_t n) : n_(n), data_(new int[n]) { std::memset(data_, 1, n * sizeof(int)); }
    WithoutNoexcept(const WithoutNoexcept& o) : n_(o.n_), data_(new int[o.n_]) { std::memcpy(data_, o.data_, n_ * sizeof(int)); }
    WithoutNoexcept(WithoutNoexcept&& o) /* NO noexcept */ : n_(o.n_), data_(o.data_) { o.n_ = 0; o.data_ = nullptr; }
    ~WithoutNoexcept() { delete[] data_; }
};

template <class T>
double fillAndTime(const char* label, int count, std::size_t elemSize) {
    std::vector<T> v;                              // NO reserve -> forces reallocations as it grows
    auto t0 = Clock::now();
    for (int i = 0; i < count; ++i)
        v.push_back(T{elemSize});                  // each realloc: move (noexcept) or copy (not)
    auto t1 = Clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::printf("  %-42s %8.2f ms\n", label, ms);
    return ms;
}

int main() {
    std::printf("is_nothrow_move_constructible:\n");
    std::printf("  WithNoexcept    : %d\n", std::is_nothrow_move_constructible_v<WithNoexcept>);
    std::printf("  WithoutNoexcept : %d\n\n", std::is_nothrow_move_constructible_v<WithoutNoexcept>);

    const int         COUNT = 200000;
    const std::size_t ELEM  = 256;                 // 256 ints = 1 KB per element buffer

    std::printf("push_back %d elements (%zu-int buffer each), no reserve:\n", COUNT, ELEM);
    double a = fillAndTime<WithNoexcept>   ("noexcept move    -> vector MOVES on realloc", COUNT, ELEM);
    double b = fillAndTime<WithoutNoexcept>("non-noexcept move -> vector COPIES on realloc", COUNT, ELEM);

    std::printf("\n  non-noexcept / noexcept : %.2fx  (the copies on every realloc)\n", a > 0 ? b / a : 0);

    std::printf(
        "\n"
        "  Same class, ek me `noexcept` extra. Woh vector ko batata hai 'move safe hai'\n"
        "  -> realloc pe elements MOVE hote (pointer steal). Bina noexcept -> vector\n"
        "  strong guarantee rakhne ke liye COPY karta (poora buffer memcpy) -> bahut dheema.\n"
        "  RULE: move ctor / move assign ko HAMESHA `noexcept` mark karo (agar sach me no-throw).\n");
    return 0;
}
