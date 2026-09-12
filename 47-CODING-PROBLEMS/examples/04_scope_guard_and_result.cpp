// 04_scope_guard_and_result.cpp
// ============================================================
// Folder 47 file 04: RAII ScopeGuard (arbitrary cleanup, dismiss-able) +
// Result<T,E> (expected-lite: value OR error, no exceptions, no heap).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g -O0 04_scope_guard_and_result.cpp -o t && ./t
// ============================================================

#include <cassert>
#include <cstdio>
#include <new>
#include <string>
#include <utility>

// ---- B8: ScopeGuard --------------------------------------------
template <class F>
class ScopeGuard {
    F    f_;
    bool active_ = true;
public:
    explicit ScopeGuard(F f) : f_(std::move(f)) {}
    ScopeGuard(ScopeGuard&& o) noexcept : f_(std::move(o.f_)), active_(o.active_) { o.active_ = false; }
    ScopeGuard(const ScopeGuard&)            = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    ScopeGuard& operator=(ScopeGuard&&)      = delete;
    ~ScopeGuard() { if (active_) f_(); }
    void dismiss() noexcept { active_ = false; }
};
template <class F> ScopeGuard(F) -> ScopeGuard<F>;

// ---- C5: Result<T,E> -----------------------------------------
enum class Err { None, BadInput, Overflow };

template <class T, class E>
class Result {
    union { T ok_; E err_; };
    bool has_;
public:
    Result(T v)          : ok_(std::move(v)),  has_(true)  {}
    Result(E e, int)     : err_(std::move(e)), has_(false) {}
    Result(const Result& o) : has_(o.has_) {
        if (has_) ::new (&ok_) T(o.ok_);
        else      ::new (&err_) E(o.err_);
    }
    Result& operator=(const Result&) = delete;
    ~Result() { if (has_) ok_.~T(); else err_.~E(); }

    bool     has_value() const { return has_; }
    const T& value()     const { return ok_; }
    const E& error()     const { return err_; }
    T        value_or(T alt) const { return has_ ? ok_ : alt; }

    template <class Fn>
    auto map(Fn fn) const -> Result<decltype(fn(ok_)), E> {
        using R = Result<decltype(fn(ok_)), E>;
        return has_ ? R(fn(ok_)) : R(err_, 0);
    }
};

static Result<int, Err> parse_small_int(const std::string& s) {
    if (s.empty()) return {Err::BadInput, 0};
    long v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return {Err::BadInput, 0};
        v = v * 10 + (c - '0');
        if (v > 1000000) return {Err::Overflow, 0};
    }
    return static_cast<int>(v);
}

int main() {
    // ---- ScopeGuard: runs on every exit path ----
    int cleaned = 0;
    {
        ScopeGuard g{[&] { ++cleaned; }};
        assert(cleaned == 0);
    }
    assert(cleaned == 1);                       // ran at scope exit

    // ---- ScopeGuard: dismiss cancels it ----
    int cleaned2 = 0;
    {
        ScopeGuard g{[&] { ++cleaned2; }};
        g.dismiss();                            // "committed" — don't roll back
    }
    assert(cleaned2 == 0);

    // ---- ScopeGuard: move transfers responsibility ----
    int cleaned3 = 0;
    {
        ScopeGuard outer{[&] { ++cleaned3; }};
        { ScopeGuard inner{std::move(outer)}; }  // inner runs it here
        assert(cleaned3 == 1);
    }
    assert(cleaned3 == 1);                        // outer was dismissed by the move

    // ---- Result: value path ----
    auto r1 = parse_small_int("123");
    assert(r1.has_value() && r1.value() == 123);
    auto doubled = r1.map([](int x) { return x * 2; });
    assert(doubled.has_value() && doubled.value() == 246);

    // ---- Result: error paths ----
    auto r2 = parse_small_int("12x");
    assert(!r2.has_value() && r2.error() == Err::BadInput);
    assert(r2.value_or(-1) == -1);
    auto r2m = r2.map([](int x) { return x + 1; });     // error passes through
    assert(!r2m.has_value() && r2m.error() == Err::BadInput);

    auto r3 = parse_small_int("99999999");
    assert(!r3.has_value() && r3.error() == Err::Overflow);

    std::puts("04_scope_guard_and_result: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - ScopeGuard turns "cleanup on every path" into one RAII object. dismiss()
//     is the commit. The move ctor hands the duty to the new object and
//     dismisses the source (so it runs exactly once).
//   - Cleanup callables must not throw (dtor -> std::terminate).
//   - Result<T,E>: tagged union, no heap, no exception unwinding cost. Hot-path
//     error handling. C++23 ships this as std::expected with and_then/transform.
//   - The `E e, int` ctor tag disambiguates the error overload from the value
//     overload when T and E could both accept the same argument.
// ============================================================
