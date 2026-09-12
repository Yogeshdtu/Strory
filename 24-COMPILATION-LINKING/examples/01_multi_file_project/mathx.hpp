// mathx.hpp — DECLARATIONS only (+ constexpr/inline jinke body ki har TU ko zaroorat ho)
// ============================================================
// Header ka kaam: "yeh cheezein exist karti hain, inka signature yeh hai."
// Definitions mathx.cxx mein — taaki har TU jo yeh header include kare,
// unhe recompile na karna pade, aur ODR na toote.
// ============================================================
#pragma once
#include <cstdint>

namespace mathx {

// ---- non-inline function DECLARATIONS (definitions mathx.cxx mein) ----
std::int64_t gcd(std::int64_t a, std::int64_t b);
std::int64_t lcm(std::int64_t a, std::int64_t b);
bool         is_prime(std::int64_t n);

// ---- constexpr: body header mein hona hi chahiye (har TU ko compile-time chahiye) ----
constexpr std::int64_t square(std::int64_t x) { return x * x; }

// ---- inline function: body header mein, par ODR-safe (vague linkage) ----
inline std::int64_t clamp_nonneg(std::int64_t x) { return x < 0 ? 0 : x; }

// ---- extern variable DECLARATION (definition mathx.cxx mein — exactly once) ----
extern const char* const kVersion;

}  // namespace mathx
