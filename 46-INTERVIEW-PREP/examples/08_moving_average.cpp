// 08_moving_average.cpp
// ============================================================
// HFT CLASSIC: a fixed-window moving average (SMA) that updates in O(1)
// per tick -- ring buffer + running sum, NO re-scan of the window.
// Then: a division-free "is the price above SMA * (1 + thr)" check.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 08_moving_average.cpp -o t && ./t
// ============================================================
// INTERVIEWER KYA DEKH RAHA:
//   - O(1) update: keep a running sum, subtract the leaving element,
//     add the entering one -- do NOT loop the window every tick
//   - ring buffer with power-of-two size (& mask, no %)
//   - integer / fixed-point (no float drift over millions of updates)
//   - turning `mid > sma * (1 + thr)` into an integer cross-multiply so
//     there is NO division on the hot path
// ============================================================

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>

template <std::size_t Window>
class MovingAverage {
    static_assert((Window & (Window - 1)) == 0, "Window must be a power of two");
    static constexpr std::size_t kMask = Window - 1;

public:
    // push a new sample (integer, e.g. a fixed-point price in ticks)
    void push(std::int64_t x) {
        const std::size_t slot = count_ & kMask;
        if (count_ >= Window) sum_ -= ring_[slot];   // element leaving the window
        sum_ += x;
        ring_[slot] = x;
        ++count_;
    }

    bool warm() const { return count_ >= Window; }
    std::int64_t sum() const { return sum_; }

    // SMA as a rational: sum_ / n. Avoid computing it as a division on the
    // hot path; expose the pieces for cross-multiplication.
    std::int64_t window_count() const {
        return static_cast<std::int64_t>(count_ < Window ? count_ : Window);
    }
    // Only for tests / cold path: the actual average, integer-truncated.
    std::int64_t average_trunc() const {
        const std::int64_t n = window_count();
        return n ? sum_ / n : 0;
    }

private:
    std::array<std::int64_t, Window> ring_{};
    std::int64_t sum_ = 0;
    std::uint64_t count_ = 0;
};

// Division-free signal: is `mid` above SMA by more than thr_num/thr_den?
//   mid > (sum/n) * (1 + thr_num/thr_den)
//   mid > (sum/n) * (thr_den + thr_num)/thr_den
//   mid * n * thr_den > sum * (thr_den + thr_num)            <-- all integer, no division
static bool above_sma_by(std::int64_t mid, std::int64_t sum, std::int64_t n,
                         std::int64_t thr_num, std::int64_t thr_den) {
    // caution in real code: guard against i64 overflow (use __int128 or
    // bound the inputs). Here mid, sum, n, thr_* are all small.
    return mid * n * thr_den > sum * (thr_den + thr_num);
}

int main() {
    MovingAverage<4> ma;
    assert(!ma.warm());

    ma.push(10);                 // window: [10]
    ma.push(20);                 // [10,20]
    ma.push(30);                 // [10,20,30]
    assert(!ma.warm());
    assert(ma.sum() == 60);
    ma.push(40);                 // [10,20,30,40]  -> warm
    assert(ma.warm());
    assert(ma.sum() == 100);
    assert(ma.average_trunc() == 25);

    ma.push(50);                 // 10 leaves, 50 enters -> [20,30,40,50]
    assert(ma.sum() == 140);
    assert(ma.average_trunc() == 35);

    ma.push(60);                 // [30,40,50,60]
    assert(ma.sum() == 180);
    assert(ma.average_trunc() == 45);

    // division-free threshold check vs a float reference
    {
        // window sum 180, n 4 -> SMA 45. thr = 2/100 = 2%. 45 * 1.02 = 45.9
        const std::int64_t sum = ma.sum(), n = ma.window_count();
        assert(!above_sma_by(45, sum, n, 2, 100));   // 45   > 45.9 ? no
        assert(!above_sma_by(45, sum, n, 2, 100));
        assert(above_sma_by(46, sum, n, 2, 100));    // 46   > 45.9 ? yes
        assert(above_sma_by(50, sum, n, 2, 100));    // 50   > 45.9 ? yes

        // cross-check the cross-multiplication against float for a range
        for (std::int64_t mid = 40; mid <= 60; ++mid) {
            const double sma = static_cast<double>(sum) / static_cast<double>(n);
            const bool ref = static_cast<double>(mid) > sma * (1.0 + 2.0 / 100.0);
            assert(above_sma_by(mid, sum, n, 2, 100) == ref);
        }
    }

    // running sum stays exact over many updates (no float drift), and
    // equals a brute-force sum of the last Window samples.
    {
        constexpr std::int64_t kW = 8;
        MovingAverage<8> big;
        for (std::int64_t i = 1; i <= 100000; ++i) big.push(i);
        std::int64_t manual = 0;                       // last 8 values: 99993..100000
        for (std::int64_t v = 100000 - kW + 1; v <= 100000; ++v) manual += v;
        assert(big.sum() == manual);
        assert(big.average_trunc() == manual / kW);
    }

    std::printf("08_moving_average: ALL PASS\n");
    return 0;
}

// ============================================================
// O(1) UPDATE: sum_ += entering; if the window was full, sum_ -= leaving.
// Never loop the window. The ring buffer + a monotonic count_ (mask only
// on the index) means no %, no ABA, no wasted slot.
//
// DIVISION-FREE SIGNAL: `mid > sma*(1+thr)` with sma = sum/n becomes
// `mid*n*thr_den > sum*(thr_den+thr_num)` -- pure integer multiply/
// compare, zero `idiv` on the hot path. Verify with `perf annotate`
// that no `idiv` appears. Guard for i64 overflow in real code
// (__int128 or bounded inputs). (folders 43/10, 44 SpreadCrossStrategy)
// ============================================================
