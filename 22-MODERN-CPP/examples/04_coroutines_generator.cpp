// 04_coroutines_generator.cpp
// ============================================================
// C++20 coroutines -- a minimal `Generator<T>` (lazy sequence via
// co_yield). Shows the promise type, the handle, and lazy pull.
// NOTE: C++20 has no std::generator (that's C++23). We hand-roll one.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g 04_coroutines_generator.cpp -o co && ./co
// ============================================================

#include <coroutine>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <string>
#include <utility>

// ------------------------------------------------------------
//  A pull-based generator: the coroutine runs only when the
//  consumer asks for the next value.
// ------------------------------------------------------------
template <class T>
class Generator {
public:
    struct promise_type {
        T current_{};
        std::exception_ptr err_{};

        Generator get_return_object() {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }   // lazy: don't run until first pull
        std::suspend_always final_suspend()   noexcept { return {}; }
        std::suspend_always yield_value(T v) noexcept { current_ = std::move(v); return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { err_ = std::current_exception(); }
    };

    using handle = std::coroutine_handle<promise_type>;

    explicit Generator(handle h) : h_(h) {}
    Generator(Generator&& o) noexcept : h_(std::exchange(o.h_, {})) {}
    Generator& operator=(Generator&&) = delete;
    Generator(const Generator&) = delete;
    ~Generator() { if (h_) h_.destroy(); }

    // returns false when the coroutine is done
    bool next() {
        if (!h_ || h_.done()) return false;
        h_.resume();
        if (h_.promise().err_) std::rethrow_exception(h_.promise().err_);
        return !h_.done();
    }
    const T& value() const { return h_.promise().current_; }

private:
    handle h_{};
};

// ---- a coroutine that yields the first n Fibonacci numbers ----
Generator<std::uint64_t> fib(int n) {
    std::uint64_t a = 0, b = 1;
    for (int i = 0; i < n; ++i) {
        co_yield a;                       // hand `a` to the consumer, suspend here
        std::uint64_t c = a + b; a = b; b = c;
    }
}

// ---- a coroutine that yields lines split from a string ----
Generator<std::string> split_lines(std::string text) {
    std::size_t start = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            co_yield text.substr(start, i - start);
            start = i + 1;
        }
    }
}

int main() {
    std::printf("=== 1. Fibonacci generator (pull 10) ===\n  ");
    {
        auto g = fib(10);
        while (g.next()) std::printf("%llu ", static_cast<unsigned long long>(g.value()));
        std::printf("\n");
    }

    std::printf("\n=== 2. lazy: values produced only on demand ===\n");
    {
        auto g = fib(1000000);            // would overflow if fully evaluated -- but we only pull 5
        for (int i = 0; i < 5 && g.next(); ++i)
            std::printf("  fib[%d] = %llu\n", i, static_cast<unsigned long long>(g.value()));
        std::printf("  (stopped after 5 -- the coroutine is suspended, not run to completion)\n");
    }

    std::printf("\n=== 3. line splitter ===\n");
    {
        auto g = split_lines("alpha\nbeta\ngamma\ndelta");
        int i = 0;
        while (g.next()) std::printf("  line %d: \"%s\"\n", i++, g.value().c_str());
    }

    std::printf(
        "\n"
        "  A coroutine is a function with co_yield / co_await / co_return. The compiler\n"
        "  builds a state machine + a `promise_type` you define (get_return_object,\n"
        "  initial/final_suspend, yield_value, ...). `co_yield v` stores v in the promise\n"
        "  and suspends; the consumer resumes the handle to continue. State lives in a\n"
        "  heap frame (unless the compiler elides it). C++23 adds std::generator.\n");
    return 0;
}
