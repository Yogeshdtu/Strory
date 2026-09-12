#pragma once
#include <cstdint>
#include <string>

namespace engine {
std::int64_t fnv1a(const std::string& s);      // hash
std::string  reverse(std::string s);
int          run();                             // demo driver
}  // namespace engine
