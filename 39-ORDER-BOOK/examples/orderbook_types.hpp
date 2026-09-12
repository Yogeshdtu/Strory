// orderbook_types.hpp
// ============================================================
// Shared type aliases -- V1/V2/V3 sab isi ko include karte, taaki
// 07_comparison_suite.cpp (jo teeno headers ek saath include karta)
// mein koi conflicting redefinition na ho.
// ============================================================
#pragma once

#include <cstdint>

using Price   = std::int64_t;   // integer ticks (37/08) -- double NAHI
using Qty     = std::uint32_t;
using OrderId = std::uint64_t;
