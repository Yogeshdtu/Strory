#include "engine.hpp"
#include <cctype>
#include <algorithm>

namespace engine {

const char* build_flavor() {
#ifdef ENGINE_FAST_PATH
    return "fast-path";
#else
    return "normal";
#endif
}

std::string shout(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    s.push_back('!');
    return s;
}

}  // namespace engine
