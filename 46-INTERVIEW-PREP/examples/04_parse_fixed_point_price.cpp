// 04_parse_fixed_point_price.cpp
// ============================================================
// HFT CLASSIC: parse an ASCII decimal price like "12345.67" into an
// integer number of ticks (scale 100) WITHOUT floating point.
// Then format it back. Handle edge cases.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 04_parse_fixed_point_price.cpp -o t && ./t
// ============================================================
// INTERVIEWER KYA DEKH RAHA:
//   - "why not std::stod?" -> float can't represent 0.01 exactly;
//     price comparison must be EXACT; stod also does locale/rounding work
//     and (from a std::string) allocates.
//   - a clean single-pass const char* walk, no allocation
//   - edge cases: no decimal point ("100" -> 10000), fewer/more decimals
//     than the scale, leading zeros, sign, trailing garbage, overflow
//   - fixed-point discipline: 2 decimal places -> multiply integer part
//     by 100, add the 2 fractional digits
// ============================================================

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

// scale = 100  (2 decimal places -> price in "cents"/ticks)
static constexpr std::int64_t kScale     = 100;
static constexpr int          kDecimals  = 2;

// Parse [begin, end) as a decimal price into ticks. Returns nullopt on
// malformed input or overflow. Accepts an optional leading '-'.
static std::optional<std::int64_t> parse_price(const char* begin, const char* end) {
    if (begin == end) return std::nullopt;

    const char* p = begin;
    bool neg = false;
    if (*p == '-') { neg = true; ++p; if (p == end) return std::nullopt; }

    std::int64_t int_part = 0;
    bool saw_int_digit = false;
    while (p != end && *p >= '0' && *p <= '9') {
        saw_int_digit = true;
        const int d = *p - '0';
        if (int_part > (INT64_MAX - d) / 10) return std::nullopt;   // overflow guard
        int_part = int_part * 10 + d;
        ++p;
    }
    if (!saw_int_digit) return std::nullopt;

    std::int64_t frac = 0;
    int frac_digits = 0;
    if (p != end && *p == '.') {
        ++p;
        while (p != end && *p >= '0' && *p <= '9') {
            if (frac_digits < kDecimals) {                          // take first kDecimals
                frac = frac * 10 + (*p - '0');
                ++frac_digits;
            }
            // extra fractional digits beyond scale: ignore (truncate).
            // (An interviewer may want rounding -- mention the choice.)
            ++p;
        }
    }
    if (p != end) return std::nullopt;                              // trailing garbage

    // pad missing fractional digits: "12.3" -> frac 3, need 30
    while (frac_digits < kDecimals) { frac *= 10; ++frac_digits; }

    // ticks = int_part * scale + frac
    if (int_part > (INT64_MAX - frac) / kScale) return std::nullopt;
    std::int64_t ticks = int_part * kScale + frac;
    return neg ? -ticks : ticks;
}

static std::optional<std::int64_t> parse_price(std::string_view s) {
    return parse_price(s.data(), s.data() + s.size());
}

// Format ticks back to "int.frac" with exactly kDecimals places.
static std::string format_price(std::int64_t ticks) {
    std::string out;
    if (ticks < 0) { out.push_back('-'); ticks = -ticks; }
    const std::int64_t ip = ticks / kScale;
    const std::int64_t fp = ticks % kScale;
    out += std::to_string(ip);
    out.push_back('.');
    // zero-pad fractional to kDecimals
    std::string f = std::to_string(fp);
    while (static_cast<int>(f.size()) < kDecimals) f.insert(f.begin(), '0');
    out += f;
    return out;
}

int main() {
    // happy path
    assert(parse_price("12345.67") == 1234567);
    assert(parse_price("0.01")     == 1);
    assert(parse_price("100")      == 10000);      // no decimal point
    assert(parse_price("100.")     == 10000);      // trailing dot, no frac digits
    assert(parse_price("12.3")     == 1230);       // pad to 2 places
    assert(parse_price("00012.30") == 1230);       // leading zeros
    assert(parse_price("12.3456")  == 1234);       // truncate extra frac digits
    assert(parse_price("-5.50")    == -550);

    // malformed
    assert(!parse_price("").has_value());
    assert(!parse_price("-").has_value());
    assert(!parse_price(".5").has_value());        // no integer digit
    assert(!parse_price("1.2.3").has_value());     // trailing garbage after 2nd dot
    assert(!parse_price("12x3").has_value());
    assert(!parse_price("abc").has_value());

    // overflow
    assert(!parse_price("99999999999999999999.00").has_value());

    // round-trip
    for (std::int64_t t : {0LL, 1LL, 99LL, 100LL, 1234567LL, -550LL}) {
        auto r = parse_price(format_price(t));
        assert(r.has_value() && *r == t);
    }

    std::printf("04_parse_fixed_point_price: ALL PASS\n");
    return 0;
}

// ============================================================
// WHY FIXED-POINT: 0.1 + 0.2 != 0.3 in binary float. A price grid is
// exact multiples of a tick -> represent it as an integer count of
// ticks. Comparisons (best bid vs order price), arithmetic (notional =
// price * qty), and hashing are then exact. std::stod would also do
// locale handling + rounding + (from std::string) an allocation.
// (folders 03, 43/09, 44)
//
// PERFORMANCE NOTE: at -O2 a hand shift-and-add parse over a const char*
// is a few ns/field; std::from_chars is the standard alloc-free option
// and is competitive. Measure. (folder 43/13)
// ============================================================
