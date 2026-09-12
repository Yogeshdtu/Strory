// 07_crtp_policy.cpp
// ============================================================
// CRTP (static polymorphism) + policy-based design.
//   CRTP: base<Derived> calls into Derived -- dispatch resolved at
//         compile time, zero vtable, fully inlinable.
//   Policy: compose behaviour by picking template arguments, each
//           policy's code inlined -> no indirection.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 07_crtp_policy.cpp -o cp && ./cp
//   BENCH: g++ -std=c++20 -O2 07_crtp_policy.cpp -o cp && ./cp
// ============================================================

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

using Clock = std::chrono::steady_clock;

// ============================================================
//  1. CRTP -- static polymorphism
// ============================================================
template <class Derived>
struct Shape {
    double area()      const { return static_cast<const Derived*>(this)->area_impl(); }
    double perimeter() const { return static_cast<const Derived*>(this)->perimeter_impl(); }
    void   describe()  const {
        std::printf("  area=%.2f perimeter=%.2f\n", area(), perimeter());
    }
};

struct Circle : Shape<Circle> {
    double r;
    explicit Circle(double radius) : r(radius) {}
    double area_impl()      const { return 3.14159265358979 * r * r; }
    double perimeter_impl() const { return 2.0 * 3.14159265358979 * r; }
};

struct Square : Shape<Square> {
    double s;
    explicit Square(double side) : s(side) {}
    double area_impl()      const { return s * s; }
    double perimeter_impl() const { return 4.0 * s; }
};

// ============================================================
//  2. Policy-based design -- compose behaviour via template args
// ============================================================
struct ThrowOnFull   { static void full()  { std::printf("  [policy] would throw\n"); } };
struct IgnoreOnFull  { static void full()  { std::printf("  [policy] silently ignored\n"); } };

struct CountStats    { std::uint64_t pushes = 0; void onPush() { ++pushes; } void dump() { std::printf("  [stats] pushes=%llu\n", (unsigned long long)pushes); } };
struct NoStats       { void onPush() {} void dump() {} };

template <class T, std::size_t Cap, class FullPolicy, class StatsPolicy>
class Buffer : private StatsPolicy {
    T data_[Cap];
    std::size_t size_ = 0;
public:
    void push(const T& x) {
        if (size_ == Cap) { FullPolicy::full(); return; }
        data_[size_++] = x;
        StatsPolicy::onPush();
    }
    std::size_t size() const { return size_; }
    void dumpStats() { StatsPolicy::dump(); }
};

// ============================================================
//  3. CRTP vs virtual -- measured
// ============================================================
struct VBase { virtual ~VBase() = default; virtual std::int64_t f(std::int64_t x) const = 0; };
struct VMul3 : VBase { std::int64_t f(std::int64_t x) const override { return x * 3 + 1; } };
struct VAdd7 : VBase { std::int64_t f(std::int64_t x) const override { return x + 7; } };

template <class D>
struct CBase { std::int64_t f(std::int64_t x) const { return static_cast<const D*>(this)->f_impl(x); } };
struct CImpl : CBase<CImpl> { std::int64_t f_impl(std::int64_t x) const { return x * 3 + 1; } };

int main() {
    std::printf("=== 1. CRTP static polymorphism ===\n");
    Circle c{2.0};
    Square q{3.0};
    c.describe();
    q.describe();
    std::printf("  sizeof(Circle) = %zu  (no vptr -- CRTP base is empty)\n", sizeof(Circle));

    std::printf("\n=== 2. policy-based Buffer ===\n");
    Buffer<int, 2, ThrowOnFull, CountStats> b1;
    b1.push(1); b1.push(2); b1.push(3);   // 3rd -> FullPolicy::full()
    b1.dumpStats();
    Buffer<int, 2, IgnoreOnFull, NoStats> b2;
    b2.push(1); b2.push(2); b2.push(3);
    std::printf("  b2 size=%zu (NoStats -> dump is a no-op)\n", b2.size());

    std::printf("\n=== 3. CRTP vs virtual dispatch (-O2) ===\n");
    {
        const long N = 40'000'000;
        volatile std::int64_t sink = 0;

        // a HETEROGENEOUS vector of base pointers -> the compiler can't devirtualize
        std::vector<std::unique_ptr<VBase>> objs;
        for (int k = 0; k < 1024; ++k)
            if (k & 1) objs.push_back(std::make_unique<VMul3>());
            else       objs.push_back(std::make_unique<VAdd7>());

        auto t0 = Clock::now();
        std::int64_t acc = 0;
        for (long i = 0; i < N; ++i)
            acc += objs[static_cast<std::size_t>(i) & 1023]->f(i & 0xffff);   // real vtable indirect call
        sink += acc;
        auto t1 = Clock::now();

        CImpl ci;
        acc = 0;
        for (long i = 0; i < N; ++i) acc += ci.f(i & 0xffff);                 // inlined -- just the arithmetic
        sink += acc;
        auto t2 = Clock::now();

        auto ns = [&](auto a, auto b){
            return std::chrono::duration<double, std::nano>(b - a).count() / static_cast<double>(N);
        };
        std::printf("  virtual (heterogeneous) : %.2f ns/call\n", ns(t0, t1));
        std::printf("  CRTP                    : %.2f ns/call\n", ns(t1, t2));
        std::printf("  (sink %lld)\n", static_cast<long long>(sink));
        std::printf("  note: if the concrete type is VISIBLE at the call site, the compiler\n"
                    "        devirtualizes and virtual == CRTP. The gap appears at a real\n"
                    "        polymorphic boundary (a base pointer whose target it can't see).\n");
    }

    std::printf(
        "\n"
        "  CRTP: the base casts `this` to Derived and calls Derived's method -- resolved\n"
        "  at compile time, inlined, no vptr (sizeof unchanged). Policies: each policy\n"
        "  class's members inline into the host; you pick behaviour by TYPE, not by a\n"
        "  runtime branch. Cost: it's all in the type -> more instantiations, harder errors.\n");
    return 0;
}
