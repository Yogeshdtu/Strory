// shared.hpp — ⚠️ JAAN-BOOJH KE GALAT
// ============================================================
// Is header mein ek NON-INLINE function ki DEFINITION (body) hai.
// Jo bhi TU isse include karega, usme `venue_name` ka ek definition
// aa jaayega -> 2+ TUs -> One Definition Rule VIOLATION -> link error
// "multiple definition of venue_name()".
//
// SAHI: yahan sirf `const char* venue_name();` (declaration) rakho,
// body ek .cxx mein. Ya `inline` lagao (vague linkage).
// ============================================================
#pragma once

const char* venue_name() {          // ❌ non-inline definition in a header
    return "NYSE";
}
