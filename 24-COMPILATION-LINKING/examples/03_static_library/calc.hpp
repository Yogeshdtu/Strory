#pragma once
#include <cstdint>

namespace calc {
std::int64_t add(std::int64_t a, std::int64_t b);
std::int64_t mul(std::int64_t a, std::int64_t b);
std::int64_t dot(const std::int64_t* xs, const std::int64_t* ys, std::size_t n);
const char*  build_id();
}  // namespace calc
