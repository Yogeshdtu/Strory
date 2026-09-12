// 08_preprocessor_demo.cpp
// ============================================================
// Preprocessor ka poora tour (lesson 02, 03): object/function macros,
// # stringize, ## paste, __VA_ARGS__ / __VA_OPT__, predefined macros,
// __has_include, _Pragma, X-macros, aur macro ke classic traps (measured).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow 08_preprocessor_demo.cpp -o pp && ./pp
//   dekho preprocessor output:  g++ -std=c++20 -E 08_preprocessor_demo.cpp | less
// ============================================================

#include <cstdio>
#include <initializer_list>

// ============================================================
//  1. Object-like aur function-like macros
// ============================================================
#define TICK_SIZE 5                       // object-like: pure text substitution
#define SQ_BAD(x)  x * x                  // ⚠️ no parens — trap neeche
#define SQ(x)     ((x) * (x))             // ✅ har argument + poora body paren mein
#define MAX(a, b) ((a) < (b) ? (b) : (a)) // ⚠️ args ka double-eval — trap neeche

// ============================================================
//  2. # (stringize) aur ## (token paste)
// ============================================================
#define STR(x)   #x                       // x ko "..." string literal banao
#define XSTR(x)  STR(x)                    // pehle x ko expand, phir stringize
#define CAT(a, b) a##b                     // do tokens ko jodo -> ek identifier

#define LOG_TAG "core"
#define WHERE()  __FILE__ ":" XSTR(__LINE__)   // "file.cpp:42"  (compile-time concat)

// ============================================================
//  3. Variadic macro + __VA_OPT__ (C++20) — trailing comma problem solve
// ============================================================
#define LOGF(fmt, ...) \
    std::printf("[%s] " fmt "%s\n", LOG_TAG __VA_OPT__(,) __VA_ARGS__, "")
//                                              ^ __VA_OPT__(,) => comma sirf jab args hon

// ============================================================
//  4. X-macro — ek list, kai jagah expand (enum + names ek saath sync)
// ============================================================
#define ORDER_TYPES(X) \
    X(Market)          \
    X(Limit)           \
    X(Stop)            \
    X(StopLimit)

enum class OrderType {
#define X(name) name,
    ORDER_TYPES(X)
#undef X
};

static const char* order_type_name(OrderType t) {
    switch (t) {
#define X(name) case OrderType::name: return #name;
        ORDER_TYPES(X)
#undef X
    }
    return "?";
}

// ============================================================
//  5. Conditional compilation + __has_include
// ============================================================
#if defined(__has_include)
#  if __has_include(<charconv>)
#    define HAVE_CHARCONV 1
#  else
#    define HAVE_CHARCONV 0
#  endif
#else
#  define HAVE_CHARCONV 0
#endif

int main() {
    // --------------------------------------------------------
    //  A. predefined macros
    // --------------------------------------------------------
    std::puts("A. predefined macros:");
    std::printf("   __FILE__      = %s\n", __FILE__);
    std::printf("   __LINE__      = %d\n", __LINE__);
    std::printf("   __func__      = %s\n", __func__);
    std::printf("   __DATE__/TIME = %s %s\n", __DATE__, __TIME__);
    std::printf("   __cplusplus   = %ld\n", static_cast<long>(__cplusplus));
    std::printf("   __STDC_HOSTED__ = %d\n", __STDC_HOSTED__);
#ifdef __GNUC__
    std::printf("   __GNUC__      = %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif
#ifdef _WIN32
    std::puts("   _WIN32 defined (Windows target)");
#endif

    // --------------------------------------------------------
    //  B. stringize / paste / WHERE()
    // --------------------------------------------------------
    std::puts("\nB. # and ## :");
    std::printf("   STR(hello)       = %s\n", STR(hello));
    std::printf("   XSTR(TICK_SIZE)  = %s   (expands macro first)\n", XSTR(TICK_SIZE));
    std::printf("   STR(TICK_SIZE)   = %s   (no expand)\n", STR(TICK_SIZE));
    int CAT(order_, id) = 42;             // -> int order_id = 42;
    std::printf("   CAT(order_, id)  -> variable value = %d\n", order_id);
    std::printf("   WHERE()          = %s\n", WHERE());

    // --------------------------------------------------------
    //  C. variadic + __VA_OPT__
    // --------------------------------------------------------
    std::puts("\nC. LOGF (variadic):");
    LOGF("no args");
    LOGF("with args: %d %s", 7, "seven");

    // --------------------------------------------------------
    //  D. X-macro: enum + names never drift apart
    // --------------------------------------------------------
    std::puts("\nD. X-macro enum<->name:");
    for (auto t : { OrderType::Market, OrderType::Limit, OrderType::Stop, OrderType::StopLimit })
        std::printf("   %d -> %s\n", static_cast<int>(t), order_type_name(t));

    // --------------------------------------------------------
    //  E. __has_include
    // --------------------------------------------------------
    std::printf("\nE. HAVE_CHARCONV = %d\n", HAVE_CHARCONV);

    // --------------------------------------------------------
    //  F. _Pragma operator (macro ke andar pragma daal sakte)
    // --------------------------------------------------------
    _Pragma("GCC diagnostic push")
    _Pragma("GCC diagnostic ignored \"-Wunused-variable\"")
    int deliberately_unused = 0;
    _Pragma("GCC diagnostic pop")
    (void)deliberately_unused;
    std::puts("\nF. _Pragma: is region mein -Wunused-variable band tha");

    // --------------------------------------------------------
    //  G. TRAPS — measured
    // --------------------------------------------------------
    std::puts("\nG. macro traps:");

    // Trap 1: missing parens -> precedence toot-ta hai
    std::printf("   SQ_BAD(1+2) = %d   (chahiye 9, milta 1+2*1+2 = %d)\n",
                SQ_BAD(1 + 2), SQ_BAD(1 + 2));
    std::printf("   SQ(1+2)     = %d   (sahi)\n", SQ(1 + 2));

    // Trap 2: argument ka double-evaluation (side effects 2 baar)
    int a = 5, b = 3;
    int m = MAX(a++, b++);               // a++ ya b++ me se ek DO baar evaluate hoga
    std::printf("   MAX(a++,b++): result=%d  a=%d b=%d   (a ya b 2 baar badha!)\n", m, a, b);

    // Trap 3: 10 / SQ_BAD(2) => 10 / 2*2 => (10/2)*2 => 10, not 10/4
    std::printf("   10 / SQ_BAD(2) = %d   (chahiye 2, milta %d)\n",
                10 / SQ_BAD(2), 10 / SQ_BAD(2));

    std::puts("\nSaar: function-like macro = blind text substitution.");
    std::puts(" - Har parameter aur poora body () mein wrap karo.");
    std::puts(" - Arguments ko body mein EK baar use karo (ya inline function use karo).");
    std::puts(" - `constexpr` function / `enum` / `inline` variable > macro jahan possible ho.");
    return 0;
}
