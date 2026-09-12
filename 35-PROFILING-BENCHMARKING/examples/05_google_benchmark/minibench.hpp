// minibench.hpp
// ============================================================
// Google Benchmark ka ~150-line SHIM. Google Benchmark library is box pe
// installed nahi -- par uska API surface itna standard hai ki hum ek
// chhota compatible version bana sakte hain, taaki:
//   (a) yeh example bina kisi dependency ke compile + chale
//   (b) ASLI Google Benchmark pe switch karna ek line ka kaam ho:
//
//        // #include "minibench.hpp"
//        #include <benchmark/benchmark.h>        // aur -lbenchmark -lpthread
//
//   ...baaki poora bench.cxx bina change kiye chalega.
//
// Kya support karta:
//   for (auto _ : state) { ... }         -- the iteration loop
//   benchmark::DoNotOptimize(x) / ClobberMemory()
//   state.range(0), state.SetItemsProcessed(n), state.SetBytesProcessed(n)
//   state.PauseTiming() / ResumeTiming()
//   BENCHMARK(fn) [->Arg(x)] [->Range(lo,hi)] [->RangeMultiplier(m)]
//   BENCHMARK_F(Fixture, Name) + struct Fixture : minibench::Fixture
//   BENCHMARK_MAIN()
//
// Kya NAHI (asli GB deta, shim nahi): multi-threaded benchmarks, statistical
// repeats/stddev, --benchmark_filter, JSON output, manual timing, templates,
// asymptotic complexity. Inke liye asli library lagao.
// ============================================================

#ifndef MINIBENCH_HPP
#define MINIBENCH_HPP

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace minibench {

// ---- optimization barriers (same as Google Benchmark's) ----------------
template <class T>
inline void DoNotOptimize(const T& v) { asm volatile("" : : "r,m"(v) : "memory"); }
template <class T>
inline void DoNotOptimize(T& v) { asm volatile("" : "+r,m"(v) : : "memory"); }
inline void ClobberMemory() { asm volatile("" : : : "memory"); }

using Clock = std::chrono::steady_clock;

// ---- config knobs -----------------------------------------------------
inline constexpr double  kMinSeconds = 0.25;         // har benchmark itna chalao (min)
inline constexpr int64_t kMaxIters   = int64_t{1} << 28;
inline constexpr int64_t kStartIters = 1024;

// ============================================================
//  State -- ek benchmark run ka context. `for (auto _ : state)` ke
//  through iterate hota; timer khud manage karta.
// ============================================================
class State {
public:
    explicit State(int64_t range0) : range0_(range0) {}

    struct Sentinel {};
    struct Iterator {
        State* st;
        bool operator!=(Sentinel) const { return st->KeepRunning(); }
        Iterator& operator++() { return *this; }
        struct Value {};
        Value operator*() const { return {}; }
    };
    Iterator begin() { StartTimer(); return Iterator{this}; }
    Sentinel end()   { return {}; }

    int64_t range(int i = 0) const { (void)i; return range0_; }

    // real Google Benchmark ka public accessor -- iteration count
    int64_t iterations() const { return iters_done_; }

    void SetItemsProcessed(int64_t n) { items_ = n; }
    void SetBytesProcessed(int64_t n) { bytes_ = n; }
    void SetLabel(const std::string& s) { label_ = s; }

    // timing ke beech mein setup/teardown ke liye
    void PauseTiming() {
        paused_accum_ += Clock::now() - seg_start_;
    }
    void ResumeTiming() {
        seg_start_ = Clock::now();
    }

    // ---- internals (macros/runner use karte) ----
    int64_t   _iterations() const { return iters_done_; }
    double    _seconds()    const {
        return std::chrono::duration<double>(active_ - paused_accum_).count();
    }
    int64_t   _items()  const { return items_; }
    int64_t   _bytes()  const { return bytes_; }
    const std::string& _label() const { return label_; }

private:
    void StartTimer() {
        t0_ = Clock::now();
        seg_start_ = t0_;
        paused_accum_ = Clock::duration::zero();
        iters_done_ = 0;
        target_ = kStartIters;
    }
    bool KeepRunning() {
        if (iters_done_ < target_) { ++iters_done_; return true; }
        // current target hit -- enough time beeta?
        active_ = Clock::now() - t0_;
        double secs = std::chrono::duration<double>(active_ - paused_accum_).count();
        if (secs < kMinSeconds && target_ < kMaxIters) {
            target_ *= 4;
            ++iters_done_;
            return true;
        }
        return false;                                // done -- loop khatam
    }

    int64_t range0_;
    int64_t items_ = 0, bytes_ = 0;
    std::string label_;
    Clock::time_point t0_{}, seg_start_{};
    Clock::duration   active_{}, paused_accum_{};
    int64_t iters_done_ = 0, target_ = kStartIters;
};

// ============================================================
//  Fixture -- asli GB ke benchmark::Fixture jaisa (SetUp/TearDown)
// ============================================================
class Fixture {
public:
    virtual ~Fixture() = default;
    virtual void SetUp(const State&) {}
    virtual void TearDown(const State&) {}
};

// ============================================================
//  Registry
// ============================================================
struct Case {
    std::string name;
    std::function<void(State&)> fn;
    std::vector<int64_t> args;                       // empty -> ek run, range=0
};

inline std::vector<Case>& registry() {
    static std::vector<Case> r;
    return r;
}

class Registrar {
public:
    Registrar(const char* name, std::function<void(State&)> fn) {
        registry().push_back(Case{name, std::move(fn), {}});
        idx_ = registry().size() - 1;
    }
    Registrar* Arg(int64_t a) { registry()[idx_].args.push_back(a); return this; }
    Registrar* RangeMultiplier(int64_t m) { mult_ = m; return this; }
    Registrar* Range(int64_t lo, int64_t hi) {
        for (int64_t v = lo; v <= hi; v *= mult_) registry()[idx_].args.push_back(v);
        return this;
    }
    Registrar* Iterations(int64_t) { return this; }  // shim: ignore (adaptive)
    Registrar* Name(const char* n) { registry()[idx_].name = n; return this; }

private:
    std::size_t idx_ = 0;
    int64_t mult_ = 8;
};

// ============================================================
//  Runner
// ============================================================
inline int RunAll() {
    std::printf("%-34s %14s %13s %16s\n", "Benchmark", "iters", "ns/iter", "throughput");
    std::printf("%s\n", std::string(80, '-').c_str());
    for (const Case& c : registry()) {
        std::vector<int64_t> args = c.args.empty() ? std::vector<int64_t>{0} : c.args;
        for (int64_t a : args) {
            State st(a);
            c.fn(st);
            const double secs = st._seconds();
            const int64_t iters = st._iterations();
            const double nspi = iters ? secs * 1e9 / static_cast<double>(iters) : 0.0;

            std::string label = c.name;
            if (!c.args.empty()) label += "/" + std::to_string(a);

            char thru[48] = "";
            if (st._items())
                std::snprintf(thru, sizeof thru, "%9.3f M items/s",
                              static_cast<double>(st._items()) / secs / 1e6);
            else if (st._bytes())
                std::snprintf(thru, sizeof thru, "%9.3f GB/s",
                              static_cast<double>(st._bytes()) / secs / 1e9);

            std::printf("%-34s %14lld %13.3f %16s  %s\n",
                        label.c_str(), static_cast<long long>(iters), nspi, thru,
                        st._label().c_str());
        }
    }
    std::puts("\n(minibench shim -- ns/iter mein loop+timer overhead ~1 predicted");
    std::puts(" branch/iter shaamil hai. Asli Google Benchmark: repeats + stddev +");
    std::puts(" CPU-vs-wall + JSON. Switch: #include <benchmark/benchmark.h>, -lbenchmark.)");
    return 0;
}

}  // namespace minibench

// ---- API ko `benchmark::` naam do (asli GB jaisa) --------------------
namespace benchmark = minibench;

// ============================================================
//  Macros
// ============================================================
#define MB_CONCAT_(a, b) a##b
#define MB_CONCAT(a, b) MB_CONCAT_(a, b)

#define BENCHMARK(fn) \
    static ::minibench::Registrar* MB_CONCAT(mb_reg_, __LINE__) = \
        (new ::minibench::Registrar(#fn, fn))

#define BENCHMARK_MAIN() \
    int main() { return ::minibench::RunAll(); }

// BENCHMARK_F(Fix, Name) { body with `state` }  -- fixture + method body
#define BENCHMARK_F(Fixture, Name)                                             \
    struct MB_CONCAT(Name, _fx) : Fixture {                                    \
        void Run(::minibench::State& state);                                   \
    };                                                                        \
    static void MB_CONCAT(Name, _thunk)(::minibench::State& st) {              \
        MB_CONCAT(Name, _fx) f;                                               \
        f.SetUp(st);                                                          \
        f.Run(st);                                                            \
        f.TearDown(st);                                                       \
    }                                                                         \
    static ::minibench::Registrar* MB_CONCAT(mb_freg_, __LINE__) =            \
        (new ::minibench::Registrar(#Fixture "/" #Name,                        \
                                    MB_CONCAT(Name, _thunk)));                 \
    void MB_CONCAT(Name, _fx)::Run(::minibench::State& state)

#endif  // MINIBENCH_HPP
