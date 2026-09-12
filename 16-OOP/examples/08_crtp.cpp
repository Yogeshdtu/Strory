// 08_crtp.cpp
// ============================================================
// CRTP -- Curiously Recurring Template Pattern -- static polymorphism
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 08_crtp.cpp -o crtp && ./crtp
// ============================================================
//   template <class Derived>
//   struct Base { void interface() { static_cast<Derived*>(this)->impl(); } };
//   struct Thing : Base<Thing> { void impl() { ... } };
//
//   Base ka code Derived ke impl ko call karta hai -- COMPILE TIME pe resolved.
//   -> no vptr, no indirect call, fully inlinable. "polymorphism" bina virtual.
//   Trade-off: koi runtime heterogeneous container nahi (har Base<D> alag type).
// ============================================================

#include <cstdio>
#include <string>
#include <vector>

// ---------- 1. Static interface / mixin ----------
template <class Derived>
struct Printable {
    void print() const {
        std::printf("  %s\n", static_cast<const Derived*>(this)->toString().c_str());
    }
};

struct Point : Printable<Point> {
    int x, y;
    Point(int x_, int y_) : x(x_), y(y_) {}
    std::string toString() const { return "Point(" + std::to_string(x) + ", " + std::to_string(y) + ")"; }
};

struct Tag : Printable<Tag> {
    std::string label;
    explicit Tag(std::string l) : label(std::move(l)) {}
    std::string toString() const { return "Tag<" + label + ">"; }
};

// ---------- 2. CRTP for shared behaviour (comparison from a single op) ----------
template <class Derived>
struct Ordered {
    friend bool operator>(const Derived& a, const Derived& b)  { return b < a; }
    friend bool operator<=(const Derived& a, const Derived& b) { return !(b < a); }
    friend bool operator>=(const Derived& a, const Derived& b) { return !(a < b); }
    // Derived sirf `operator<` de -- baaki 3 yahan se
};

struct Version : Ordered<Version> {
    int major, minor;
    Version(int mj, int mn) : major(mj), minor(mn) {}
    friend bool operator<(const Version& a, const Version& b) {
        return (a.major != b.major) ? a.major < b.major : a.minor < b.minor;
    }
};

// ---------- 3. CRTP static dispatch (virtual-jaisa, zero cost) ----------
template <class Derived>
struct Strategy {
    double evaluate(double px) const {
        return static_cast<const Derived*>(this)->signal(px);   // compile-time dispatch
    }
};

struct MeanRevert : Strategy<MeanRevert> {
    double mid;
    explicit MeanRevert(double m) : mid(m) {}
    double signal(double px) const { return (mid - px) * 0.5; }
};
struct Momentum : Strategy<Momentum> {
    double last;
    explicit Momentum(double l) : last(l) {}
    double signal(double px) const { return (px - last) * 0.3; }
};

// Ek templated consumer -- kis strategy ke saath, compile time pe bind
template <class S>
double backtest(const Strategy<S>& strat, const std::vector<double>& prices) {
    double pnl = 0;
    for (double p : prices) pnl += strat.evaluate(p);   // evaluate() fully inlines to signal()
    return pnl;
}

int main() {
    std::printf("=== 1. static interface (Printable mixin) ===\n");
    Point p{3, 4};
    Tag   t{"urgent"};
    p.print();                       // Printable<Point>::print -> Point::toString  (inlined)
    t.print();

    std::printf("\n=== 2. derive 4 comparison ops from operator< ===\n");
    Version a{1, 5}, b{2, 0};
    std::printf("  a < b  : %d\n", a < b);
    std::printf("  a > b  : %d\n", a > b);     // from Ordered<Version>
    std::printf("  a <= b : %d\n", a <= b);
    std::printf("  b >= a : %d\n", b >= a);

    std::printf("\n=== 3. static-dispatch strategies (no vptr) ===\n");
    std::vector<double> prices{100.0, 101.5, 99.0, 102.0, 98.5};
    MeanRevert mr{100.5};
    Momentum   mo{100.0};
    std::printf("  MeanRevert backtest pnl = %.3f\n", backtest(mr, prices));
    std::printf("  Momentum   backtest pnl = %.3f\n", backtest(mo, prices));

    std::printf(
        "\n"
        "  CRTP: Base<Derived> me static_cast<Derived*>(this) -> compile-time dispatch\n"
        "  -> no vptr (sizeof me +0), no indirect call, fully inlinable\n"
        "  Trade-off: har Base<D> alag type -> vector<Base*> jaisa heterogeneous nahi\n"
        "  Use jab: type compile time pe pata ho (strategies, mixins, policies, expression templates)\n");
    return 0;
}
