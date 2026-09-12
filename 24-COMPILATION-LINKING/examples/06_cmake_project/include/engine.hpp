#pragma once
#include <string>

namespace engine {
// ENGINE_FAST_PATH CMake option se define hota hai (target_compile_definitions)
const char* build_flavor();
std::string shout(std::string s);   // uppercase + "!"
}  // namespace engine
