// 07_simple_pool.cpp
// ============================================================
// Ek chhota FIXED-SIZE memory pool -- O(1) allocate/free, koi syscall nahi
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 07_simple_pool.cpp -o pool && ./pool
//   Benchmark ke liye:  .\build.ps1 fast 14-MEMORY/examples/07_simple_pool.cpp
// ============================================================
//   Idea: ek baar mein ek bada block (arena) allocate karo. Use N fixed-size
//   slots mein baanto. Free slots ki ek "free list" rakho (har free slot ke
//   andar hi agle free slot ka pointer likh do -- extra memory nahi).
//
//     allocate() -> free list ka head nikaalo, head ko aage badhao   (O(1))
//     free(p)    -> p ko free list ke aage lagao                     (O(1))
//
//   Trade-off: sirf ek fixed size (blockSize) deta hai; capacity fixed hai;
//   thread-safe nahi (per-thread pool banao). Yeh HFT order/event objects ka
//   classic pattern hai -- allocation ki tail latency poori tarah hataata hai.
// ============================================================

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <new>
#include <vector>

class FixedPool {
public:
    FixedPool(std::size_t blockSize, std::size_t blockCount)
        : blockSize_(roundUp(blockSize < sizeof(void*) ? sizeof(void*) : blockSize,
                             alignof(std::max_align_t))),
          blockCount_(blockCount),
          arena_(blockSize_ * blockCount_) {
        // free list banao: har slot ke andar agle slot ka address
        free_ = nullptr;
        for (std::size_t i = blockCount_; i-- > 0; ) {
            void* slot = arena_.data() + i * blockSize_;
            *reinterpret_cast<void**>(slot) = free_;
            free_ = slot;
        }
    }

    void* allocate() noexcept {
        if (!free_) return nullptr;                 // pool full
        void* p = free_;
        free_ = *reinterpret_cast<void**>(p);       // head = next
        ++inUse_;
        return p;
    }

    void deallocate(void* p) noexcept {
        if (!p) return;
        *reinterpret_cast<void**>(p) = free_;       // p -> old head
        free_ = p;
        --inUse_;
    }

    std::size_t inUse() const noexcept { return inUse_; }
    std::size_t capacity() const noexcept { return blockCount_; }

private:
    static std::size_t roundUp(std::size_t n, std::size_t a) noexcept {
        return (n + a - 1) / a * a;
    }

    std::size_t blockSize_;
    std::size_t blockCount_;
    std::vector<std::uint8_t> arena_;               // ek hi allocation
    void*       free_ = nullptr;
    std::size_t inUse_ = 0;
};

// ---- demo object ----
struct Order {
    std::uint64_t id;
    double        price;
    std::uint32_t qty;
    char          side;
};

using Clock = std::chrono::steady_clock;
static void sink(void* p) { asm volatile("" : : "r"(p) : "memory"); }

int main() {
    std::cout << "=== correctness ===\n";
    FixedPool pool(sizeof(Order), 4);
    std::cout << "  capacity = " << pool.capacity() << " blocks of " << sizeof(Order) << " B\n";

    void* a = pool.allocate();
    void* b = pool.allocate();
    Order* oa = new (a) Order{1, 100.5, 10, 'B'};   // placement new -- pool ki memory pe construct
    Order* ob = new (b) Order{2, 101.0, 5,  'S'};
    std::cout << "  oa: id=" << oa->id << " price=" << oa->price << "  @ " << a << "\n";
    std::cout << "  ob: id=" << ob->id << " price=" << ob->price << "  @ " << b << "\n";
    std::cout << "  inUse = " << pool.inUse() << "\n";

    oa->~Order();                                   // placement new -> manual destructor
    pool.deallocate(a);
    void* c = pool.allocate();                      // 'a' wala slot wapas milega
    std::cout << "  free(a) then allocate() -> " << c << (c == a ? "  (same slot reused)\n" : "\n");

    ob->~Order();
    pool.deallocate(b);
    pool.deallocate(c);

    void* d1 = pool.allocate(); void* d2 = pool.allocate();
    void* d3 = pool.allocate(); void* d4 = pool.allocate();
    void* d5 = pool.allocate();                     // capacity 4 -> 5th = nullptr
    std::cout << "  5th allocate on capacity-4 pool -> " << (d5 ? "ptr" : "nullptr (pool full)") << "\n";
    pool.deallocate(d1); pool.deallocate(d2); pool.deallocate(d3); pool.deallocate(d4);

    // ---- benchmark: pool vs new/delete ----
    std::cout << "\n=== throughput (build with -O2) ===\n";
    const long REPS = 5'000'000;

    FixedPool bench(sizeof(Order), 1024);
    auto t0 = Clock::now();
    for (long i = 0; i < REPS; ++i) {
        void* p = bench.allocate();
        sink(p);
        bench.deallocate(p);
    }
    auto t1 = Clock::now();

    auto t2 = Clock::now();
    for (long i = 0; i < REPS; ++i) {
        void* p = ::operator new(sizeof(Order));
        sink(p);
        ::operator delete(p);
    }
    auto t3 = Clock::now();

    auto ns = [](Clock::time_point x, Clock::time_point y) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(y - x).count();
    };
    const double pp = static_cast<double>(ns(t0, t1)) / static_cast<double>(REPS);
    const double np = static_cast<double>(ns(t2, t3)) / static_cast<double>(REPS);

    std::cout << "  pool  alloc+free : " << pp << " ns/op\n";
    std::cout << "  new   alloc+free : " << np << " ns/op\n";
    std::cout << "  speedup          : " << (pp > 0 ? np / pp : np) << "x\n";

    std::cout <<
        "\n"
        "  Pool: ek pointer swap. new/delete: allocator ka poora machinery (aur\n"
        "  kabhi-kabhi lock / OS call -> tail spike). Yeh sirf throughput dikhata hai;\n"
        "  asli jeet TAIL LATENCY hai -- 06_allocation_benchmark.cpp dekho.\n";
    return 0;
}
