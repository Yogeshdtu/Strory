// 07_dispatch_benchmark.cpp
// ============================================================
// Dispatch cost: virtual vs direct vs CRTP vs std::variant -- MEASURED
// ============================================================
//   BENCHMARK -- -O2 ZAROORI:
//     g++ -std=c++20 -O2 07_dispatch_benchmark.cpp -o db && ./db
//     ya:  .\build.ps1 fast 16-OOP/examples/07_dispatch_benchmark.cpp
// ============================================================
//   virtual call  = load vptr -> load slot -> INDIRECT call. Compiler INLINE nahi
//                   kar sakta (dynamic type unknown) -> aur no cross-call opt.
//   direct call   = known function -> inline -> often just a few FLOPs.
//   CRTP          = compile-time polymorphism -> static dispatch -> inline (direct jaisa).
//   std::variant  = tagged union -> visit = switch on index -> har arm inline ho sakta.
// ============================================================

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <variant>
#include <vector>

using Clock = std::chrono::steady_clock;
static double g_sink = 0;

// ---------- virtual hierarchy ----------
struct Shape {
    virtual ~Shape() = default;
    virtual double area() const = 0;
};
struct VCircle : Shape { double r; explicit VCircle(double r_) : r(r_) {} double area() const override { return 3.14159265358979 * r * r; } };
struct VSquare : Shape { double s; explicit VSquare(double s_) : s(s_) {} double area() const override { return s * s; } };
struct VTri    : Shape { double b, h; VTri(double b_, double h_) : b(b_), h(h_) {} double area() const override { return 0.5 * b * h; } };

// ---------- plain (monomorphic direct) ----------
struct PCircle { double r; double area() const { return 3.14159265358979 * r * r; } };

// ---------- CRTP ----------
template <class Derived>
struct ShapeCRTP {
    double area() const { return static_cast<const Derived*>(this)->areaImpl(); }
};
struct CCircle : ShapeCRTP<CCircle> {
    double r;
    explicit CCircle(double r_) : r(r_) {}
    double areaImpl() const { return 3.14159265358979 * r * r; }
};

// ---------- variant ----------
using VarShape = std::variant<VCircle, VSquare, VTri>;

int main() {
    const std::size_t N     = 100000;
    const int         REPS  = 500;

    // --- build datasets (runtime-decided types -> compiler devirtualize na kar sake) ---
    std::vector<std::unique_ptr<Shape>> vpoly;
    std::vector<PCircle>                pmono;
    std::vector<CCircle>                crtp;
    std::vector<VarShape>              variants;
    vpoly.reserve(N); pmono.reserve(N); crtp.reserve(N); variants.reserve(N);

    std::uint64_t seed = 12345;
    auto rng = [&] { seed = seed * 6364136223846793005ULL + 1442695040888963407ULL; return (seed >> 33); };

    for (std::size_t i = 0; i < N; ++i) {
        double v = 1.0 + static_cast<double>(i % 7);
        switch (rng() % 3) {
            case 0: vpoly.push_back(std::make_unique<VCircle>(v));   variants.emplace_back(VCircle{v});      break;
            case 1: vpoly.push_back(std::make_unique<VSquare>(v));   variants.emplace_back(VSquare{v});      break;
            default:vpoly.push_back(std::make_unique<VTri>(v, v+1)); variants.emplace_back(VTri{v, v + 1});  break;
        }
        pmono.push_back(PCircle{v});
        crtp.push_back(CCircle{v});
    }

    auto run = [&](const char* label, auto&& body) {
        auto t0 = Clock::now();
        double acc = 0;
        for (int r = 0; r < REPS; ++r) {
            acc += body();
            asm volatile("" : "+r"(acc) : : "memory");     // barrier -- loop hoist rok
        }
        auto t1 = Clock::now();
        g_sink += acc;
        double ns = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
        double perCall = ns / (static_cast<double>(REPS) * static_cast<double>(N));
        std::printf("  %-26s  %8.3f ns/call\n", label, perCall);
        return perCall;
    };

    std::printf("N = %zu shapes, %d reps  (per-call = total / (N*reps))\n\n", N, REPS);

    double dv = run("virtual  (unique_ptr)", [&] {
        double s = 0; for (const auto& p : vpoly) s += p->area(); return s;
    });
    double dd = run("direct   (monomorphic)", [&] {
        double s = 0; for (const auto& c : pmono) s += c.area(); return s;
    });
    double dc = run("CRTP     (static poly)", [&] {
        double s = 0; for (const auto& c : crtp) s += c.area(); return s;
    });
    double dvar = run("std::variant + visit", [&] {
        double s = 0; for (const auto& v : variants) s += std::visit([](const auto& x) { return x.area(); }, v); return s;
    });

    std::printf("\n  virtual / direct  ratio : %.2fx\n", dd > 0 ? dv / dd : 0.0);
    std::printf("  variant / direct  ratio : %.2fx\n", dd > 0 ? dvar / dd : 0.0);
    std::printf("  CRTP    / direct  ratio : %.2fx\n", dd > 0 ? dc / dd : 0.0);
    std::printf("  (sink %.1f)\n", g_sink);

    std::printf(
        "\n  Padhne ka tareeka:\n"
        "  - direct / CRTP  : compiler inline karta -> ~1-2 FLOPs/call, fastest.\n"
        "  - std::variant   : switch on type index, arms inline ho sakte -> beech mein.\n"
        "  - virtual        : indirect call, no inline, no cross-call opt -> slowest\n"
        "                     (aur data-dependent -> branch/BTB misses tail badhaate).\n"
        "  HFT: hot dispatch = CRTP / variant / function table, virtual nahi (file 12, 13).\n");
    return 0;
}
