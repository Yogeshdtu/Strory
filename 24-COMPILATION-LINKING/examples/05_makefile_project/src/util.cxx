#include "engine.hpp"
#include <algorithm>

namespace engine {
std::string reverse(std::string s) {
    std::reverse(s.begin(), s.end());
    return s;
}
}  // namespace engine
