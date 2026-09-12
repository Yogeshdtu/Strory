// stats.hpp — ek aur module ka interface
// ============================================================
#pragma once
#include <cstdint>
#include <span>

namespace stats {

double mean(std::span<const double> xs);
double stddev(std::span<const double> xs);          // population stddev
std::int64_t count_primes_upto(std::int64_t n);     // mathx par depend karta hai

}  // namespace stats
