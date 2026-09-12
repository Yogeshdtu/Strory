// mathx.hpp -- declaration only (definition in mathx.cxx, another TU)
#pragma once
#include <cstdint>

// Chhota, hot, par DOOSRE translation unit mein defined. Bina LTO ke
// `main` isse sirf ek opaque `call` ke through use kar sakta -- na inline,
// na constant-fold, na vectorize across the boundary.
std::uint32_t hot_transform(std::uint32_t x) noexcept;
