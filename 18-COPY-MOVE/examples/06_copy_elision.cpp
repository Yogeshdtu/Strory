// 06_copy_elision.cpp
// ============================================================
// Copy elision -- RVO, NRVO, guaranteed elision (C++17)
// ============================================================
//   NORMAL:
//     g++ -std=c++20 -O2 06_copy_elision.cpp -o ce && ./ce
//   ELISION OFF (dekhne ke liye ki kya elide hota tha):
//     g++ -std=c++20 -O2 -fno-elide-constructors 06_copy_elision.cpp -o ce_off && ./ce_off
// ============================================================
//   RVO  (Return Value Optimization) : `return T{...};` -- prvalue -> caller ki jagah
//                                       mein SEEDHA construct. C++17: GUARANTEED (zero copy/move).
//   NRVO (Named RVO)                  : `T x; ...; return x;` -- named local -> caller ki jagah.
//                                       Allowed, NOT guaranteed. Practically GCC/Clang karte hain.
// ============================================================

#include <cstdio>

struct Tracer {
    int id;
    explicit Tracer(int i) : id(i) { std::printf("  Tracer(%d)      ctor\n", id); }
    Tracer(const Tracer& o) : id(o.id) { std::printf("  Tracer(%d)      COPY ctor\n", id); }
    Tracer(Tracer&& o) noexcept : id(o.id) { std::printf("  Tracer(%d)      MOVE ctor\n", id); }
    ~Tracer() { std::printf("  ~Tracer(%d)\n", id); }
};

// RVO: return a prvalue directly
Tracer makeRVO() {
    return Tracer{1};                 // C++17: constructed IN the caller's storage. 0 copy/move.
}

// NRVO: return a named local
Tracer makeNRVO() {
    Tracer x{2};
    return x;                          // named local -> NRVO (GCC/Clang elide). Fallback: implicit move.
}

// NOT elidable AND not implicitly-moved: `which ? a : b` is a conditional expression,
// not a plain name -> the "implicit move on return" rule doesn't apply -> full COPY.
Tracer makeConditional(bool which) {
    Tracer a{3};
    Tracer b{4};
    return which ? a : b;             // NRVO impossible (2 candidates) + not a name -> COPY ctor
}

// pass a prvalue by value -- guaranteed elision (no temp materialized)
void takeByValue(Tracer t) {
    std::printf("    takeByValue: id=%d\n", t.id);
}

int main() {
    std::printf("=== makeRVO()  (prvalue return -- guaranteed elision C++17) ===\n");
    Tracer r = makeRVO();             // NORMAL: just "Tracer(1) ctor".  -fno-elide: + MOVE + ~temp

    std::printf("\n=== makeNRVO() (named return -- NRVO, not guaranteed) ===\n");
    Tracer n = makeNRVO();            // NORMAL: just "Tracer(2) ctor".  -fno-elide: + MOVE + ~temp

    std::printf("\n=== makeConditional(true) (not elidable, not a name -> COPY) ===\n");
    Tracer c = makeConditional(true);// "Tracer(3) ctor", "Tracer(4) ctor", COPY, ~4, ~3

    std::printf("\n=== takeByValue(makeRVO()) (prvalue arg -- guaranteed elision) ===\n");
    takeByValue(makeRVO());           // NORMAL: "Tracer(1) ctor" then "~Tracer(1)".  1 object total.

    std::printf(
        "\n"
        "  NORMAL build: prvalue returns/args -> ZERO copy/move (C++17 guaranteed).\n"
        "  Named locals -> NRVO (compiler courtesy; GCC/Clang do it).\n"
        "  -fno-elide-constructors: har jagah extra MOVE ctor + temp dtor dikhega --\n"
        "  yeh batata hai elision ne kitna kaam bachaya.\n"
        "  Isliye: `return T{...};` ya `return localVar;` likho. `return std::move(x)` MAT.\n");
    return 0;
}
