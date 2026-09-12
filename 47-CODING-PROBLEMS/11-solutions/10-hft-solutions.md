# 10 — HFT: worked solutions

Poore code — parsing, book ops, risk gate, latency histogram. Baaki
`10-hft-problems.md` ke `<details>` blocks mein. Runnable versions:
`../examples/10_order_book_ops.cpp` aur `46-INTERVIEW-PREP/examples/`.

---

## 2 — ASCII decimal price → int64 ticks (scale 100)

```cpp
#include <cstdint>
#include <optional>
#include <string_view>

std::optional<std::int64_t> parse_price(std::string_view s) {
    constexpr std::int64_t kScale = 100;    // 2 decimal places
    constexpr int          kDec   = 2;
    if (s.empty()) return std::nullopt;

    std::size_t i = 0;
    std::int64_t sign = 1;
    if (s[i] == '-') { sign = -1; ++i; }
    else if (s[i] == '+') { ++i; }

    std::int64_t ip = 0;
    bool any = false;
    for (; i < s.size() && s[i] >= '0' && s[i] <= '9'; ++i) {
        int d = s[i] - '0';
        if (ip > (INT64_MAX - d) / 10) return std::nullopt;   // overflow guard, BEFORE mul
        ip = ip * 10 + d;
        any = true;
    }
    std::int64_t frac = 0, fscale = 1;
    if (i < s.size() && s[i] == '.') {
        ++i;
        for (int k = 0; k < kDec && i < s.size() && s[i] >= '0' && s[i] <= '9'; ++k, ++i) {
            frac = frac * 10 + (s[i] - '0');
            fscale *= 10;
            any = true;
        }
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;   // truncate extra digits
    }
    if (!any || i != s.size()) return std::nullopt;              // trailing garbage
    frac *= kScale / fscale;                                     // pad missing decimals
    return sign * (ip * kScale + frac);
}
// "123.45" -> 12345 ; "123.4" -> 12340 ; "123" -> 12300 ; "12x" -> nullopt
```

Single pass, no float (float parse → rounding → exact price compares break).
Overflow checked **before** the multiply. Extra fractional digits truncated (not
rounded) — match your venue's convention.

---

## 3 + 4 — Price-indexed book: add + cancel with O(1) BBO

```cpp
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

class Book {
    static constexpr std::int32_t kLevels = 4096;      // price band around a reference
    std::array<std::int64_t, kLevels> bid_qty_{};
    std::array<std::int64_t, kLevels> ask_qty_{};
    std::int32_t best_bid_ = -1;                        // tick index, -1 = none
    std::int32_t best_ask_ = kLevels;

    struct Loc { bool is_bid; std::int32_t tick; std::int64_t qty; bool live; };
    std::vector<Loc> loc_;                              // by order id (dense)
public:
    std::uint32_t add(bool is_bid, std::int32_t tick, std::int64_t qty) {
        if (tick < 0 || tick >= kLevels) return UINT32_MAX;   // outside band -> reject
        (is_bid ? bid_qty_ : ask_qty_)[tick] += qty;
        if (is_bid) { if (tick > best_bid_) best_bid_ = tick; }
        else        { if (tick < best_ask_) best_ask_ = tick; }
        loc_.push_back({is_bid, tick, qty, true});
        return static_cast<std::uint32_t>(loc_.size() - 1);
    }
    void cancel(std::uint32_t id) {
        if (id >= loc_.size() || !loc_[id].live) return;
        Loc& o = loc_[id];
        o.live = false;
        auto& side = o.is_bid ? bid_qty_ : ask_qty_;
        side[o.tick] -= o.qty;
        if (side[o.tick] == 0) {                        // level emptied — maybe rewalk BBO
            if (o.is_bid && o.tick == best_bid_)
                while (best_bid_ >= 0 && bid_qty_[best_bid_] == 0) --best_bid_;
            if (!o.is_bid && o.tick == best_ask_)
                while (best_ask_ < kLevels && ask_qty_[best_ask_] == 0) ++best_ask_;
        }
    }
    std::optional<std::int32_t> best_bid() const {
        return best_bid_ >= 0 ? std::optional{best_bid_} : std::nullopt;
    }
    std::optional<std::int32_t> best_ask() const {
        return best_ask_ < kLevels ? std::optional{best_ask_} : std::nullopt;
    }
};
```

`add` / `cancel` are `O(1)` except the rare BBO rewalk, which is bounded because
books are dense near the top (a few ticks). `id → Loc` gives `O(1)` cancel with
no search. Prices are integer ticks — no float compares anywhere.

---

## 8 — Pre-trade risk gate (ordered, fail-fast)

```cpp
#include <cstdint>

struct Limits {
    std::int64_t max_qty, max_pos, max_notional, price_band;
    std::int32_t rate_limit;
};
struct RiskState {
    std::int64_t position    = 0;
    std::int64_t exposure    = 0;
    std::int32_t msgs_window = 0;
    std::int64_t ref_price   = 0;
};

enum class Reject { Ok, Qty, Band, Rate, Position, Notional };

Reject check(const Limits& L, const RiskState& S, std::int64_t px, std::int64_t qty, int side) {
    if (qty > L.max_qty)                                   return Reject::Qty;       // 1 cmp
    if (std::llabs(px - S.ref_price) > L.price_band)       return Reject::Band;      // 1 cmp
    if (S.msgs_window >= L.rate_limit)                     return Reject::Rate;      // counter
    const std::int64_t signed_qty = side > 0 ? qty : -qty;
    if (std::llabs(S.position + signed_qty) > L.max_pos)   return Reject::Position;  // add+cmp
    if (px * qty + S.exposure > L.max_notional)            return Reject::Notional;  // mul (costliest)
    return Reject::Ok;
}
```

Cheapest + most-likely-to-fire checks first (fail fast, and each branch is
near-100% predictable since almost everything passes). The multiply for notional
is last. Rate limit ≠ kill switch: this throttles a single order; a kill switch
is a latched global stop. Budget: < 20 ns.

---

## 10 — Latency histogram (log-linear buckets, O(1) record)

```cpp
#include <array>
#include <cstdint>
#include <bit>

class LatencyHist {
    static constexpr int kSub = 4;                       // 16 sub-buckets per octave
    static constexpr int kBuckets = 64 << kSub;
    std::array<std::uint64_t, kBuckets> counts_{};
public:
    void record(std::uint64_t ns) {
        if (ns == 0) { ++counts_[0]; return; }
        const int msb = 63 - std::countl_zero(ns);       // floor(log2)
        const int sub = static_cast<int>((ns >> (msb - kSub)) & ((1 << kSub) - 1));
        const int idx = (msb << kSub) | sub;
        if (idx < kBuckets) ++counts_[idx];              // one array bump — O(1)
    }
    std::uint64_t percentile(double p) const {           // OFF the hot path
        std::uint64_t total = 0;
        for (auto c : counts_) total += c;
        std::uint64_t target = static_cast<std::uint64_t>(p * static_cast<double>(total));
        std::uint64_t acc = 0;
        for (int i = 0; i < kBuckets; ++i) {
            acc += counts_[i];
            if (acc >= target) {
                const int msb = i >> kSub, sub = i & ((1 << kSub) - 1);
                return (std::uint64_t{1} << msb) | (std::uint64_t(sub) << (msb - kSub));
            }
        }
        return 0;
    }
};
```

`record` = compute a bucket index (a few bit ops) + one increment. No branchy
logic, no allocation, fixed ~few-KB array. Percentiles computed later, off the
trading thread. This is the HdrHistogram idea in ~20 lines.
