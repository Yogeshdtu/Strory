// 01_vector_deep.cpp
// ============================================================
// std::vector -- growth, capacity, reserve, iterator/reference invalidation
// ============================================================
//   NORMAL: g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_vector_deep.cpp -o vd && ./vd
//   BENCH : g++ -std=c++20 -O2 01_vector_deep.cpp -o vd && ./vd
// ============================================================
//   vector = contiguous heap buffer + size + capacity.
//   - push_back past capacity -> allocate bigger buffer (usually 2x), MOVE/COPY elements, free old
//   - reserve(n) -> one allocation up front, no reallocations while size <= n
//   - reallocation INVALIDATES all iterators, pointers, references into the vector
// ============================================================

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

using Clock = std::chrono::steady_clock;

int main() {
    std::printf("=== 1. growth pattern (capacity doubling) ===\n");
    {
        std::vector<int> v;
        std::size_t lastCap = 0;
        for (int i = 0; i < 20; ++i) {
            v.push_back(i);
            if (v.capacity() != lastCap) {
                std::printf("  size=%2zu -> capacity jumped to %2zu\n", v.size(), v.capacity());
                lastCap = v.capacity();
            }
        }
        std::printf("  (libstdc++ grows ~2x; some libs 1.5x)\n");
    }

    std::printf("\n=== 2. reserve vs no reserve (allocation count) ===\n");
    {
        // count reallocations by watching capacity changes
        std::vector<int> a; std::size_t reallocsA = 0, capA = 0;
        for (int i = 0; i < 100000; ++i) { a.push_back(i); if (a.capacity() != capA) { ++reallocsA; capA = a.capacity(); } }

        std::vector<int> b; b.reserve(100000); std::size_t reallocsB = 0, capB = b.capacity();
        for (int i = 0; i < 100000; ++i) { b.push_back(i); if (b.capacity() != capB) { ++reallocsB; capB = b.capacity(); } }

        std::printf("  no reserve : %zu reallocations for 100k push_backs\n", reallocsA);
        std::printf("  reserve    : %zu reallocations (1 allocation up front)\n", reallocsB);
    }

    std::printf("\n=== 3. reallocation INVALIDATES pointers/references ===\n");
    {
        std::vector<int> v{10, 20, 30};
        int*  p0     = &v[0];
        int&  first  = v[0];
        std::printf("  before grow: &v[0] = %p, first = %d\n", static_cast<void*>(p0), first);
        for (int i = 0; i < 1000; ++i) v.push_back(i);      // forces reallocation
        std::printf("  after  grow: &v[0] = %p  (old p0 = %p -> %s)\n",
                    static_cast<void*>(&v[0]), static_cast<void*>(p0),
                    (&v[0] == p0) ? "still valid" : "DANGLING -- points to freed buffer");
        // reading *p0 or `first` now is use-after-free (folder 13/14). Don't.
    }

    std::printf("\n=== 4. reserve does NOT change size; resize does ===\n");
    {
        std::vector<int> v;
        v.reserve(10);
        std::printf("  after reserve(10): size=%zu capacity=%zu\n", v.size(), v.capacity());
        v.resize(5);
        std::printf("  after resize(5)  : size=%zu capacity=%zu  (5 value-initialized 0s)\n", v.size(), v.capacity());
    }

    std::printf("\n=== 5. shrink_to_fit / clear ===\n");
    {
        std::vector<int> v(1000);
        v.clear();                                          // size 0, capacity UNCHANGED
        std::printf("  after clear()        : size=%zu capacity=%zu\n", v.size(), v.capacity());
        v.shrink_to_fit();                                  // non-binding request; usually frees
        std::printf("  after shrink_to_fit(): size=%zu capacity=%zu\n", v.size(), v.capacity());
        std::vector<int>().swap(v);                         // guaranteed release (swap with empty temp)
        std::printf("  after swap-with-temp : size=%zu capacity=%zu  (guaranteed free)\n", v.size(), v.capacity());
    }

    std::printf("\n=== 6. iterate: reserved vs unreserved fill time (-O2) ===\n");
    {
        const int N = 2'000'000;
        auto t0 = Clock::now();
        { std::vector<int> v; for (int i = 0; i < N; ++i) v.push_back(i); }
        auto t1 = Clock::now();
        { std::vector<int> v; v.reserve(N); for (int i = 0; i < N; ++i) v.push_back(i); }
        auto t2 = Clock::now();
        auto ms = [](auto a, auto b){ return std::chrono::duration<double,std::milli>(b-a).count(); };
        std::printf("  no reserve : %.2f ms   (reallocations + moves)\n", ms(t0, t1));
        std::printf("  reserve    : %.2f ms\n", ms(t1, t2));
    }

    std::printf(
        "\n"
        "  vector rules of thumb:\n"
        "  - final size pata ho -> reserve() -> zero reallocations\n"
        "  - reallocation ALL iterators/pointers/references todta hai -- unhe store mat karo across push_back\n"
        "  - clear() capacity nahi ghatata; shrink_to_fit() request hai; swap-with-temp = guaranteed free\n");
    return 0;
}
