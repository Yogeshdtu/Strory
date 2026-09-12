// 10_pmr_demo.cpp
// ============================================================
// std::pmr -- polymorphic allocators; allocation-free containers
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 10_pmr_demo.cpp -o pmr && ./pmr
// ============================================================
//   std::pmr::vector<T> etc. take a `memory_resource*` at RUNTIME (not a template param).
//   Resources:
//     monotonic_buffer_resource  -- bump allocator; deallocate is a no-op; freed en masse
//     unsynchronized_pool_resource -- size-bucketed pools, single-thread
//     new_delete_resource()      -- default: ::operator new/delete
//     null_memory_resource()     -- throws on any allocation (assert "no allocation happened")
// ============================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory_resource>
#include <new>
#include <string>
#include <vector>

// count global ::operator new so we can PROVE pmr avoided the heap
namespace { long g_global_new = 0; }
void* operator new(std::size_t n)      { ++g_global_new; void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc{}; return p; }
void  operator delete(void* p) noexcept { std::free(p); }
void  operator delete(void* p, std::size_t) noexcept { std::free(p); }

int main() {
    std::printf("=== 1. plain std::vector -> heap allocations ===\n");
    {
        long before = g_global_new;
        std::vector<int> v;
        for (int i = 0; i < 1000; ++i) v.push_back(i);
        std::printf("  std::vector<int> 1000 push_backs -> %ld global ::operator new calls\n",
                    g_global_new - before);
    }

    std::printf("\n=== 2. pmr::vector on a STACK buffer -> ZERO heap allocations ===\n");
    {
        std::byte buffer[64 * 1024];                                        // 64 KB on the stack
        std::pmr::monotonic_buffer_resource pool{buffer, sizeof(buffer)};   // bump-allocate from `buffer`
        long before = g_global_new;

        std::pmr::vector<int> v{&pool};
        for (int i = 0; i < 1000; ++i) v.push_back(i);

        std::pmr::vector<std::pmr::string> names{&pool};
        for (int i = 0; i < 50; ++i) names.emplace_back("a fairly long name that would normally heap-allocate");

        std::printf("  pmr::vector<int> (1000) + pmr::vector<pmr::string> (50 long strings)\n");
        std::printf("  -> %ld global ::operator new calls  (all memory came from the stack buffer)\n",
                    g_global_new - before);
        std::printf("  buffer bytes used so far: ~%zu / %zu\n",
                    sizeof(buffer) - 0, sizeof(buffer));   // (monotonic_buffer_resource doesn't expose used bytes)
    }   // pool destructs -> nothing to free (memory was on the stack)

    std::printf("\n=== 3. null_memory_resource -- assert 'no allocation' ===\n");
    {
        std::byte buffer[256];
        std::pmr::monotonic_buffer_resource pool{buffer, sizeof(buffer), std::pmr::null_memory_resource()};
        //                                                              ^ upstream = null -> if we exceed 256 B, THROW
        std::pmr::vector<int> v{&pool};
        try {
            for (int i = 0; i < 1000; ++i) v.push_back(i);   // will exceed 256 bytes -> bad_alloc from null upstream
            std::printf("  (unexpectedly fit)\n");
        } catch (const std::bad_alloc&) {
            std::printf("  exceeded the 256-byte buffer -> null upstream threw bad_alloc (as designed)\n");
            std::printf("  -> use this to statically-ish guarantee a code path does no allocation\n");
        }
    }

    std::printf("\n=== 4. reuse a monotonic buffer across iterations (release) ===\n");
    {
        std::byte buffer[8 * 1024];
        std::pmr::monotonic_buffer_resource pool{buffer, sizeof(buffer)};
        long before = g_global_new;
        for (int iter = 0; iter < 100; ++iter) {
            std::pmr::vector<int> scratch{&pool};
            for (int i = 0; i < 200; ++i) scratch.push_back(i);      // per-iteration scratch work
            pool.release();                                          // reset the bump pointer -> reuse the buffer
        }
        std::printf("  100 iterations of 200-int scratch, release() each time -> %ld global new\n",
                    g_global_new - before);
    }

    std::printf(
        "\n"
        "  std::pmr: container ka TYPE badle bina uski memory strategy badlo.\n"
        "  monotonic_buffer_resource + stack/static buffer -> per-request / per-event\n"
        "  scratch work with ZERO heap allocation. release() to reuse.\n"
        "  Yeh folder 14 ke arena/pool ka standard, composable roop hai.\n");
    return 0;
}
