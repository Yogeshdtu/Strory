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

    // ============================================================
    //  BENCHMARK: pool vs new/delete -- "burst" workload
    // ============================================================
    // 64 orders ek saath allocate + construct karo, phir sabko padho + free karo.
    // Asli feed jaisa: ek packet mein kai orders aate hain, phir chale jaate hain.
    //
    // ⚠️ Rule 2 ki kahani: pehle yahan `for (...) { p = allocate(); sink(p); deallocate(p); }`
    // tha -- har baar WAHI slot lo aur wapas do. `bench` ek local hai jiska address kahin
    // nahi gaya, isliye GCC 16.2 -O2 ne free_ ko register mein rakh liya aur poora
    // allocate+deallocate ek `mov [rsi], rdx` ban gaya (assembly mein dekha). Result:
    // "0.24 ns/op, ~140x" -- ek khaali loop vs asli allocator calls. `asm volatile` sink
    // memory ko clobber karta hai, par non-escaped local ke register wale free_ ko nahi.
    // Batch workload mein free list sach mein aage-peeche hoti hai -> asli kaam naapte hain.
    std::cout << "\n=== throughput: 64 ka burst, alloc+construct -> read+free (-O2 pe build karo) ===\n";
    constexpr int  BATCH  = 64;
    constexpr long ROUNDS = 100'000;                // 6.4M alloc+free pairs
    Order* ptrs[BATCH];
    std::uint64_t cs1 = 0, cs2 = 0;

    FixedPool bench(sizeof(Order), 1024);
    auto t0 = Clock::now();
    for (long r = 0; r < ROUNDS; ++r) {
        for (int j = 0; j < BATCH; ++j)                       // pool se lo + placement new
            ptrs[j] = new (bench.allocate()) Order{static_cast<std::uint64_t>(r + j), 100.0, 10, 'B'};
        for (int j = 0; j < BATCH; ++j) {                     // padho + pool ko wapas
            cs1 += ptrs[j]->id;
            bench.deallocate(ptrs[j]);                        // (Order trivially destructible -- ~Order() no-op)
        }
    }
    auto t1 = Clock::now();

    for (long r = 0; r < ROUNDS; ++r) {
        for (int j = 0; j < BATCH; ++j)                       // allocator se lo
            ptrs[j] = new Order{static_cast<std::uint64_t>(r + j), 100.0, 10, 'B'};
        for (int j = 0; j < BATCH; ++j) {
            cs2 += ptrs[j]->id;
            delete ptrs[j];                                   // allocator ko wapas
        }
    }
    auto t2 = Clock::now();

    const double pairs = static_cast<double>(ROUNDS) * BATCH;
    const double pp = std::chrono::duration<double, std::nano>(t1 - t0).count() / pairs;
    const double np = std::chrono::duration<double, std::nano>(t2 - t1).count() / pairs;

    std::cout << "  pool  alloc+free : " << pp << " ns/pair\n";
    std::cout << "  new   alloc+free : " << np << " ns/pair\n";
    std::cout << "  speedup          : " << (pp > 0 ? np / pp : np) << "x\n";
    std::cout << "  checksums equal  : " << (cs1 == cs2 ? "yes" : "NO") << "\n";

    std::cout <<
        "\n"
        "  Pool: do pointer moves. new/delete: allocator ka poora machinery (aur\n"
        "  kabhi-kabhi lock / OS call -> tail spike). Yeh sirf throughput dikhata hai;\n"
        "  asli jeet TAIL LATENCY hai -- 06_allocation_benchmark.cpp dekho.\n";
    return 0;
}
