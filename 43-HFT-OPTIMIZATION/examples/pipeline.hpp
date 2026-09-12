// pipeline.hpp
// ============================================================
// Folder 43 ka BACKBONE: ek chhota tick-to-order pipeline, DO versions mein --
//
//   PipelineV0  = jaan-boojh kar NAIVE (par strawman nahi -- realistic naive):
//                 std::stringstream/std::stod parse, std::map<double> book,
//                 std::deque re-sum SMA, std::string order encode.
//
//   PipelineV3  = optimized: hand-rolled integer parse, fixed-point prices,
//                 flat-array book + cached top-of-book, ring-buffer running-sum
//                 SMA (no division), fixed-layout POD order encode.
//
// DONO ka input EK hi hota hai: ek packed ASCII blob (make_feed() se), taaki
// comparison apples-to-apples ho. Dono ka signal logic NUMERICALLY same hai
// (V3 integer arithmetic mein wahi cross-condition compute karta jo V0 float
// mein) -- 04_before_after.cpp is baat ko assert karta (agreement rate).
//
// Measurement: har pipeline `process_one(i, measure)` deta hai. measure=true pe
// 4 rdtsc checkpoints (parse / book / signal+encode) -- yeh STAGE ATTRIBUTION
// ke liye hai. rdtsc khud ~cost add karta (4 calls/tick) -> ABSOLUTE inflate
// hota; isliye "true" per-tick average ke liye measure=false loop ko ek hi
// steady_clock se time karo (02-establishing-baseline.md, trap: measurement
// overhead).
//
// SB kuch static/inline hai -> multiple .cpp ise include kar sakte bina ODR issue.
// ============================================================

#ifndef CPP_MASTERY_43_PIPELINE_HPP
#define CPP_MASTERY_43_PIPELINE_HPP

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
#  include <x86intrin.h>
#endif

namespace pipe {

// ---------------------------------------------------------------
//  rdtsc harness -- SAME idiom as folders 35..42
// ---------------------------------------------------------------
static inline std::uint64_t tsc() {
#if defined(__x86_64__) || defined(_M_X64)
    _mm_lfence();
    const std::uint64_t t = __rdtsc();
    _mm_lfence();
    return t;
#else
    return 0;   // non-x86: stage attribution disabled, wall-clock still works
#endif
}

// keep(): optimizer ko value "use hui" dikhao, warna dead-code elimination
template <class T>
static inline void keep(T& v) {
    asm volatile("" : "+r,m"(v) : : "memory");
}

// ---------------------------------------------------------------
//  Feed generator -- deterministic. Ek packed ASCII blob banata:
//    "<T>,<S>,<PPPP.pp>,<QQQ>,<ID>\n"
//    T = A (add) | X (cancel)   S = B | S
//  Prices 2-decimal, ~98.00..102.00 ke aas-paas ek random-walk + slow drift
//  (drift isliye taaki SMA-crossover signal KABHI fire kare -- warna encode
//   stage kabhi chalti hi nahi aur usko measure nahi kar paate).
// ---------------------------------------------------------------
struct Feed {
    std::string           blob;      // saari lines, packed
    std::vector<std::uint32_t> off;  // off[i] = line i ka start; len = off[i+1]-off[i]
    std::uint32_t         max_id = 0;
    std::size_t           n_add = 0, n_cancel = 0;
};

// splitmix64 -- chhota, fast, deterministic
struct Rng {
    std::uint64_t s;
    explicit Rng(std::uint64_t seed) : s(seed) {}
    std::uint64_t next() {
        std::uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    std::uint32_t below(std::uint32_t n) { return static_cast<std::uint32_t>(next() % n); }
};

inline Feed make_feed(std::size_t n_msgs = 120000, std::uint64_t seed = 43) {
    Feed f;
    f.blob.reserve(n_msgs * 24);
    f.off.reserve(n_msgs + 1);

    Rng rng(seed);

    // generator ki apni bookkeeping: live order id -> (side, px_ticks, qty)
    struct GOrd { char side; std::int32_t px_ticks; std::int32_t qty; };
    std::unordered_map<std::uint32_t, GOrd> live;
    std::vector<std::uint32_t> live_ids;
    live.reserve(n_msgs);
    live_ids.reserve(n_msgs);

    std::int32_t mid_ticks = 10000;          // 100.00
    std::int32_t trend     = 0;               // momentum term, periodically resampled
    std::uint32_t trend_left = 1;             // msgs until next trend resample
    std::uint32_t next_id = 1;
    char line[48];

    auto emit = [&](const char* p, int len) {
        f.off.push_back(static_cast<std::uint32_t>(f.blob.size()));
        f.blob.append(p, static_cast<std::size_t>(len));
    };

    for (std::size_t i = 0; i < n_msgs; ++i) {
        // --- price process: random walk + a periodically-resampled MOMENTUM
        //     term. Momentum burst -> mid pulls away from its own trailing
        //     average -> SMA-crossover signal fires (encode stage exercised).
        if (--trend_left == 0) {
            trend = static_cast<std::int32_t>(rng.below(7)) - 3;   // -3..+3 ticks/msg
            trend_left = 120 + rng.below(280);
        }
        const std::int32_t step = static_cast<std::int32_t>(rng.below(5)) - 2;   // -2..+2 ticks
        const std::int32_t pull = (mid_ticks - 10000) / 200;       // soft mean-revert (no hard pin)
        mid_ticks += step + trend - pull;
        if (mid_ticks < 9200)  mid_ticks = 9200;                   // wide safety clamp only
        if (mid_ticks > 10800) mid_ticks = 10800;

        const bool do_cancel = (!live_ids.empty()) && (rng.below(100) < 32u);
        int len;
        if (do_cancel) {
            const std::uint32_t k  = rng.below(static_cast<std::uint32_t>(live_ids.size()));
            const std::uint32_t id = live_ids[k];
            const GOrd o = live[id];
            len = std::snprintf(line, sizeof line, "X,%c,%d.%02d,%d,%u\n",
                                o.side,
                                o.px_ticks / 100, o.px_ticks % 100,
                                o.qty, id);
            emit(line, len);
            // swap-remove from live_ids
            live_ids[k] = live_ids.back();
            live_ids.pop_back();
            live.erase(id);
            ++f.n_cancel;
        } else {
            const char side = (rng.below(2) == 0) ? 'B' : 'S';
            const std::int32_t skew = static_cast<std::int32_t>(rng.below(9));   // 0..8 ticks off mid
            std::int32_t px = (side == 'B') ? (mid_ticks - skew) : (mid_ticks + skew);
            if (px < 9000)  px = 9000;
            if (px > 10999) px = 10999;
            const std::int32_t qty = 1 + static_cast<std::int32_t>(rng.below(999));
            const std::uint32_t id = next_id++;
            len = std::snprintf(line, sizeof line, "A,%c,%d.%02d,%d,%u\n",
                                side, px / 100, px % 100, qty, id);
            emit(line, len);
            live[id] = GOrd{side, px, qty};
            live_ids.push_back(id);
            f.max_id = id;
            ++f.n_add;
        }
        (void) len;
    }
    f.off.push_back(static_cast<std::uint32_t>(f.blob.size()));   // sentinel
    return f;
}

// ---------------------------------------------------------------
//  Signal / pipeline shared constants
// ---------------------------------------------------------------
constexpr std::size_t   kWindow    = 64;      // SMA window (power of two -> V3 no divide)
constexpr std::int64_t  kNum       = 2001;    // threshold ratio = kNum/kDen = 1.0005 (0.05%)
constexpr std::int64_t  kDen       = 2000;
constexpr std::size_t   kCooldown  = 3;       // fire ke baad itne msgs tak dobara nahi
                                              // (chhota -> signal-hold ke dauraan re-quote,
                                              //  encode stage ko theek coverage milta)

struct StageNs {
    double parse = 0, book = 0, signal = 0, total = 0;
    std::uint64_t ticks = 0;
};

// =============================================================
//  V0 -- NAIVE (realistic-naive, not strawman)
// =============================================================
class PipelineV0 {
public:
    explicit PipelineV0(const Feed& f) : feed_(f) {}

    // ek message process karo. measure=true -> rdtsc stage checkpoints.
    void process_one(std::size_t i, bool measure) {
        const std::uint32_t a = feed_.off[i];
        const std::uint32_t b = feed_.off[i + 1];
        const std::string line = feed_.blob.substr(a, b - a);   // naive: copy

        std::uint64_t t0 = 0, t1 = 0, t2 = 0, t3 = 0;
        if (measure) t0 = tsc();

        // ---- STAGE 1: parse (stringstream-style split + stod/stoi) ----
        char   type = 0, side = 0;
        double price = 0.0;
        int    qty = 0;
        std::uint32_t id = 0;
        parse_naive(line, type, side, price, qty, id);
        if (measure) t1 = tsc();

        // ---- STAGE 2: book apply (std::map<double> + std::list) ----
        if (type == 'A') {
            add_(side, price, id, qty);
        } else {
            cancel_(id);
        }
        if (measure) t2 = tsc();

        // ---- STAGE 3: signal + (maybe) encode ----
        signal_and_encode_(qty);
        if (measure) t3 = tsc();

        if (measure) {
            ns_.parse  += static_cast<double>(t1 - t0);
            ns_.book   += static_cast<double>(t2 - t1);
            ns_.signal += static_cast<double>(t3 - t2);
            ns_.total  += static_cast<double>(t3 - t0);
            ns_.ticks  += 1;
        }
    }

    std::size_t orders() const { return orders_; }
    const StageNs& stage_ticks() const { return ns_; }
    // fire hua kis msg index pe -- agreement check ke liye
    const std::vector<std::size_t>& fire_idx() const { return fire_idx_; }
    std::size_t cur_index() const { return cur_i_; }
    void set_index(std::size_t i) { cur_i_ = i; }

private:
    static void parse_naive(const std::string& line, char& type, char& side,
                            double& price, int& qty, std::uint32_t& id) {
        // "T,S,PPPP.pp,QQ,ID\n"  -- find/substr/stod/stoi
        std::size_t p0 = 0;
        std::size_t p1 = line.find(',', p0);
        type = line.substr(p0, p1 - p0)[0];
        p0 = p1 + 1; p1 = line.find(',', p0);
        side = line.substr(p0, p1 - p0)[0];
        p0 = p1 + 1; p1 = line.find(',', p0);
        price = std::stod(line.substr(p0, p1 - p0));
        p0 = p1 + 1; p1 = line.find(',', p0);
        qty = std::stoi(line.substr(p0, p1 - p0));
        p0 = p1 + 1; p1 = line.find('\n', p0);
        id = static_cast<std::uint32_t>(std::stoul(line.substr(p0, p1 - p0)));
    }

    void add_(char side, double price, std::uint32_t id, int qty) {
        id_px_[id]   = price;
        id_side_[id] = side;
        if (side == 'B') bids_[price].push_back({id, qty});
        else             asks_[price].push_back({id, qty});
    }
    void cancel_(std::uint32_t id) {
        auto it = id_px_.find(id);
        if (it == id_px_.end()) return;
        const double price = it->second;
        const char   side  = id_side_[id];
        if (side == 'B') {
            auto lit = bids_.find(price);
            if (lit != bids_.end()) {
                erase_from_list(lit->second, id);
                if (lit->second.empty()) bids_.erase(lit);
            }
        } else {
            auto lit = asks_.find(price);
            if (lit != asks_.end()) {
                erase_from_list(lit->second, id);
                if (lit->second.empty()) asks_.erase(lit);
            }
        }
        id_px_.erase(it);
        id_side_.erase(id);
    }
    static void erase_from_list(std::list<std::pair<std::uint32_t, int>>& lst, std::uint32_t id) {
        for (auto it = lst.begin(); it != lst.end(); ++it) {
            if (it->first == id) { lst.erase(it); return; }
        }
    }

    void signal_and_encode_(int qty_hint) {
        if (bids_.empty() || asks_.empty()) return;
        const double bb  = bids_.begin()->first;
        const double ba  = asks_.begin()->first;
        const double mid = (bb + ba) / 2.0;                 // <-- division

        hist_.push_back(mid);
        if (hist_.size() > kWindow) hist_.pop_front();
        if (hist_.size() < kWindow) return;

        double sum = 0.0;
        for (double v : hist_) sum += v;                    // <-- re-sum every tick
        const double sma = sum / static_cast<double>(hist_.size());   // <-- division

        if (cooldown_ > 0) { --cooldown_; return; }

        const double ratio = static_cast<double>(kNum) / static_cast<double>(kDen);
        if (mid > sma * ratio) {
            char buf[64];
            const int n = std::snprintf(buf, sizeof buf, "NEW,B,%.2f,%d\n", mid, qty_hint);
            out_.emplace_back(buf, static_cast<std::size_t>(n < 0 ? 0 : n));   // <-- string alloc
            ++orders_;
            fire_idx_.push_back(cur_i_);
            cooldown_ = kCooldown;
        }
    }

    const Feed& feed_;
    std::map<double, std::list<std::pair<std::uint32_t, int>>, std::greater<double>> bids_;
    std::map<double, std::list<std::pair<std::uint32_t, int>>>                        asks_;
    std::map<std::uint32_t, double> id_px_;
    std::map<std::uint32_t, char>   id_side_;
    std::deque<double>              hist_;
    std::vector<std::string>        out_;
    std::size_t orders_   = 0;
    std::size_t cooldown_ = 0;
    std::size_t cur_i_    = 0;
    std::vector<std::size_t> fire_idx_;
    StageNs ns_;
};

// =============================================================
//  V3 -- OPTIMIZED
// =============================================================
class PipelineV3 {
public:
    explicit PipelineV3(const Feed& f) : feed_(f) {
        id_loc_.assign(f.max_id + 1, Loc{});
        ring_.fill(0);
    }

    void process_one(std::size_t i, bool measure) {
        const char* p   = feed_.blob.data() + feed_.off[i];

        std::uint64_t t0 = 0, t1 = 0, t2 = 0, t3 = 0;
        if (measure) t0 = tsc();

        // ---- STAGE 1: parse (hand-rolled, integer, fixed-point) ----
        // format is generator-controlled -> no per-field validation here
        // (real feed handler validates: 38-MARKET-DATA / 09-fixed-point.md).
        const char type = p[0];
        const char side = p[2];
        p += 4;                                    // skip "T,S,"
        std::int32_t px_ticks = 0;
        while (*p != '.') { px_ticks = px_ticks * 10 + (*p - '0'); ++p; }
        px_ticks *= 100;
        ++p;                                       // skip '.'
        px_ticks += (p[0] - '0') * 10 + (p[1] - '0');
        p += 2;
        ++p;                                       // skip ','
        std::int32_t qty = 0;
        while (*p != ',') { qty = qty * 10 + (*p - '0'); ++p; }
        ++p;
        std::uint32_t id = 0;
        while (*p != '\n') { id = id * 10 + static_cast<std::uint32_t>(*p - '0'); ++p; }
        if (measure) t1 = tsc();

        // ---- STAGE 2: book apply (flat array + cached top-of-book) ----
        const std::size_t idx = static_cast<std::size_t>(px_ticks - kBase);
        if (type == 'A') {
            if (side == 'B') {
                bid_qty_[idx] += qty;
                if (idx > best_bid_ || best_bid_ == kNoBid) best_bid_ = idx;
            } else {
                ask_qty_[idx] += qty;
                if (idx < best_ask_) best_ask_ = idx;
            }
            id_loc_[id] = Loc{static_cast<std::int32_t>(idx), qty, side};
        } else {
            const Loc l = id_loc_[id];
            const std::size_t li = static_cast<std::size_t>(l.idx);
            if (l.side == 'B') {
                bid_qty_[li] -= l.qty;
                if (bid_qty_[li] == 0 && li == best_bid_) rewalk_bid_();
            } else {
                ask_qty_[li] -= l.qty;
                if (ask_qty_[li] == 0 && li == best_ask_) rewalk_ask_();
            }
        }
        if (measure) t2 = tsc();

        // ---- STAGE 3: signal + (maybe) encode ----
        signal_and_encode_(qty);
        if (measure) t3 = tsc();

        if (measure) {
            ns_.parse  += static_cast<double>(t1 - t0);
            ns_.book   += static_cast<double>(t2 - t1);
            ns_.signal += static_cast<double>(t3 - t2);
            ns_.total  += static_cast<double>(t3 - t0);
            ns_.ticks  += 1;
        }
    }

    std::size_t orders() const { return orders_; }
    const StageNs& stage_ticks() const { return ns_; }
    const std::vector<std::size_t>& fire_idx() const { return fire_idx_; }
    std::size_t cur_index() const { return cur_i_; }
    void set_index(std::size_t i) { cur_i_ = i; }

private:
    static constexpr std::int32_t kBase   = 9000;             // 90.00 in ticks
    static constexpr std::size_t  kLevels = 4096;             // 90.00 .. 130.95
    static constexpr std::size_t  kNoBid  = static_cast<std::size_t>(-1);

    struct Loc { std::int32_t idx = 0; std::int32_t qty = 0; char side = 0; };

    struct OrderOut { std::uint8_t side; std::int64_t px_ticks; std::int32_t qty; };

    void rewalk_bid_() {
        std::size_t i = best_bid_;
        while (i != kNoBid && bid_qty_[i] == 0) --i;   // wraps to SIZE_MAX at 0 -> kNoBid
        best_bid_ = i;
    }
    void rewalk_ask_() {
        std::size_t i = best_ask_;
        while (i < kLevels && ask_qty_[i] == 0) ++i;
        best_ask_ = i;
    }

    void signal_and_encode_(std::int32_t qty_hint) {
        if (best_bid_ == kNoBid || best_ask_ >= kLevels) return;
        const std::int64_t bb   = static_cast<std::int64_t>(best_bid_) + kBase;
        const std::int64_t ba   = static_cast<std::int64_t>(best_ask_) + kBase;
        const std::int64_t sum2 = bb + ba;                  // == 2 * mid_ticks (no divide)

        ring_sum_ += sum2 - ring_[pos_];
        ring_[pos_] = sum2;
        pos_ = (pos_ + 1) & (kWindow - 1);
        if (filled_ < kWindow) ++filled_;
        if (filled_ < kWindow) return;      // match V0: evaluate on the W-th sample

        if (cooldown_ > 0) { --cooldown_; return; }

        // V0: mid > sma * (kNum/kDen)
        //   mid       = sum2 / 2
        //   sma       = (ring_sum_ / kWindow) / 2
        //   cross  <=>  sum2 * kWindow * kDen  >  ring_sum_ * kNum       (all integer)
        const std::int64_t lhs = sum2 * static_cast<std::int64_t>(kWindow) * kDen;
        const std::int64_t rhs = ring_sum_ * kNum;
        if (lhs > rhs) {
            out_.side     = static_cast<std::uint8_t>('B');
            out_.px_ticks = sum2 / 2;
            out_.qty      = qty_hint;
            keep(out_);                                     // "sent" -- no alloc, no string
            ++orders_;
            fire_idx_.push_back(cur_i_);
            cooldown_ = kCooldown;
        }
    }

    const Feed& feed_;
    std::array<std::int64_t, kLevels> bid_qty_{};
    std::array<std::int64_t, kLevels> ask_qty_{};
    std::vector<Loc> id_loc_;
    std::size_t best_bid_ = kNoBid;
    std::size_t best_ask_ = kLevels;

    std::array<std::int64_t, kWindow> ring_{};
    std::int64_t ring_sum_ = 0;
    std::size_t  pos_ = 0, filled_ = 0;

    OrderOut     out_{};
    std::size_t  orders_   = 0;
    std::size_t  cooldown_ = 0;
    std::size_t  cur_i_    = 0;
    std::vector<std::size_t> fire_idx_;
    StageNs ns_;
};

// ---------------------------------------------------------------
//  Shared helpers: TSC calibration + percentile report
// ---------------------------------------------------------------
// tsc ticks -> ns.  busy-wait ~120ms, (tsc delta) / (ns delta).
inline double calibrate_tpns() {
    namespace ch = std::chrono;
    const auto c0 = ch::steady_clock::now();
    const std::uint64_t r0 = tsc();
    volatile std::uint64_t spin = 0;
    while (ch::duration_cast<ch::milliseconds>(ch::steady_clock::now() - c0).count() < 120)
        spin = spin + 1;
    const std::uint64_t r1 = tsc();
    const auto c1 = ch::steady_clock::now();
    const double dt_ns = static_cast<double>(ch::duration_cast<ch::nanoseconds>(c1 - c0).count());
    const double dticks = static_cast<double>(r1 - r0);
    return (dt_ns > 0.0) ? (dticks / dt_ns) : 1.0;
}

// nearest-rank percentile over an ALREADY-SORTED vector
inline double pct(const std::vector<double>& v, double p) {
    if (v.empty()) return 0.0;
    std::size_t i = static_cast<std::size_t>(p / 100.0 * static_cast<double>(v.size()));
    if (i >= v.size()) i = v.size() - 1;
    return v[i];
}

}  // namespace pipe

#endif  // CPP_MASTERY_43_PIPELINE_HPP
