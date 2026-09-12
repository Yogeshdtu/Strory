// bench.cxx
// ============================================================
// Google Benchmark STYLE mein likha hua. Yeh file asli Google Benchmark
// ke saath bhi bilkul aise hi compile hoti -- sirf include badlo:
//
//     #include "minibench.hpp"                  // <- abhi (shim, no deps)
//     // #include <benchmark/benchmark.h>       // <- asli: -lbenchmark -lpthread
//
// Chalao:  ./build.ps1   (ya ./build.sh)
// ============================================================

#include "minibench.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <random>
#include <string>
#include <vector>

// ============================================================
//  1. The canonical first benchmark -- ek chhoti string banao
// ============================================================
static void BM_StringCreation(benchmark::State& state) {
    for (auto _ : state) {
        std::string s("hello");
        benchmark::DoNotOptimize(s);          // warna -O2 `s` ko delete kar de
    }
}
BENCHMARK(BM_StringCreation);

// ---- vs: ek string ko baar-baar copy karna ----
static void BM_StringCopy(benchmark::State& state) {
    std::string x(static_cast<std::size_t>(state.range(0)), 'x');
    for (auto _ : state) {
        std::string copy(x);
        benchmark::DoNotOptimize(copy.data());
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_StringCopy)->Arg(8)->Arg(64)->Arg(512)->Arg(4096);

// ============================================================
//  2. memcpy vs hand-rolled loop -- Range() se size sweep,
//     SetBytesProcessed se GB/s
// ============================================================
static void BM_memcpy(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    std::vector<char> src(n, 'a'), dst(n);
    for (auto _ : state) {
        std::memcpy(dst.data(), src.data(), n);
        benchmark::DoNotOptimize(dst.data());
        benchmark::ClobberMemory();           // dst likha gaya -- store retain karo
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_memcpy)->RangeMultiplier(8)->Range(64, 1 << 16);

static void BM_manual_copy(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    std::vector<char> src(n, 'a'), dst(n);
    for (auto _ : state) {
        for (std::size_t i = 0; i < n; ++i) dst[i] = src[i];
        benchmark::DoNotOptimize(dst.data());
        benchmark::ClobberMemory();
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_manual_copy)->RangeMultiplier(8)->Range(64, 1 << 16);

// ============================================================
//  3. PauseTiming/ResumeTiming -- per-iteration setup ko time se hatao
// ============================================================
static void BM_sort_shuffled(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    std::vector<int> v(n);
    std::iota(v.begin(), v.end(), 0);
    std::mt19937 rng(42);

    for (auto _ : state) {
        state.PauseTiming();                  // shuffle ko mat gino
        std::shuffle(v.begin(), v.end(), rng);
        state.ResumeTiming();

        std::sort(v.begin(), v.end());
        benchmark::DoNotOptimize(v.data());
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_sort_shuffled)->Arg(1 << 10)->Arg(1 << 14);

// ============================================================
//  4. Fixture -- SetUp ek baar data banata, har benchmark method use karta
// ============================================================
struct DataFixture : benchmark::Fixture {
    std::vector<std::uint64_t> data;
    void SetUp(const benchmark::State&) override {
        data.resize(1 << 16);
        std::mt19937_64 rng(7);
        for (auto& x : data) x = rng();
    }
    void TearDown(const benchmark::State&) override { data.clear(); }
};

BENCHMARK_F(DataFixture, SumReduce) {
    for (auto _ : state) {
        std::uint64_t s = 0;
        for (std::uint64_t x : data) s += x;
        benchmark::DoNotOptimize(s);          // warna poora loop gayab
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(data.size()));
}

BENCHMARK_F(DataFixture, XorReduce) {
    for (auto _ : state) {
        std::uint64_t s = 0;
        for (std::uint64_t x : data) s ^= x;
        benchmark::DoNotOptimize(s);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(data.size()));
}

// ============================================================
//  5. WRONG vs RIGHT -- barrier bhool jao to kya hota
// ============================================================
static void BM_reduce_NO_barrier(benchmark::State& state) {
    std::vector<int> v(1 << 14, 3);
    for (auto _ : state) {
        long sum = 0;
        for (int x : v) sum += x;
        // BUG: sum kahin use nahi -> -O2 loop delete -> ~0 ns/iter (jhooth)
        (void)sum;
    }
}
BENCHMARK(BM_reduce_NO_barrier);

static void BM_reduce_WITH_barrier(benchmark::State& state) {
    std::vector<int> v(1 << 14, 3);
    for (auto _ : state) {
        long sum = 0;
        for (int x : v) sum += x;
        benchmark::DoNotOptimize(sum);        // FIX
    }
}
BENCHMARK(BM_reduce_WITH_barrier);

BENCHMARK_MAIN();
