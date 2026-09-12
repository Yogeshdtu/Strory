#pragma once

// Cross-platform export macro. Windows DLL ko har public symbol pe
// __declspec(dllexport/dllimport) chahiye; ELF (.so) pe default sab visible
// (ya -fvisibility=hidden + __attribute__((visibility("default")))).
#if defined(_WIN32)
#  if defined(GREET_BUILD)
#    define GREET_API __declspec(dllexport)
#  else
#    define GREET_API __declspec(dllimport)
#  endif
#else
#  define GREET_API __attribute__((visibility("default")))
#endif

namespace greet {
GREET_API const char* hello();
GREET_API int         bump();      // internal counter — shared library state
}  // namespace greet
