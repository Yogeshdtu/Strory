#include "calc.hpp"
namespace calc {
std::int64_t add(std::int64_t a, std::int64_t b) { return a + b; }
const char*  build_id() { return "calc-static-" __DATE__; }
}  // namespace calc
