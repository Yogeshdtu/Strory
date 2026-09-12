// 05_expected.cpp
// ============================================================
// std::expected<T,E> (C++23) — exceptions ke bina "result ya error"
// ek hi value mein. Monadic and_then / transform / or_else se error
// handling ki plumbing khatam.
//
// Yeh repo default -std=c++20 pe build hota hai jahan <expected> nahi
// hai — isliye ek chhota Expected<T,E> khud bana rakha hai (bilkul
// wahi API). C++23 pe asli std::expected use hota hai. Program batata
// hai kaunsa active hai.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow 05_expected.cpp -o exp && ./exp
//   g++ -std=c++23 -Wall -Wextra -Wshadow 05_expected.cpp -o exp   # asli std::expected
// ============================================================

#include <cstdio>
#include <string>
#include <string_view>
#include <version>     // __cpp_lib_expected

// ---- error type: ek plain enum, koi exception nahi ----
enum class ParseErr { empty, not_a_number, out_of_range, negative };

static const char* to_str(ParseErr e) {
    switch (e) {
        case ParseErr::empty:        return "empty input";
        case ParseErr::not_a_number: return "not a number";
        case ParseErr::out_of_range: return "out of range";
        case ParseErr::negative:     return "negative not allowed";
    }
    return "?";
}

// ============================================================
//  Expected<T,E> — C++23 ho to alias, warna mini implementation
// ============================================================
#if defined(__cpp_lib_expected)
  #include <expected>
  template <class T, class E> using Expected = std::expected<T, E>;
  template <class E>          using Unexpect = std::unexpected<E>;
  static constexpr const char* kImpl = "std::expected (C++23)";
#else
  #include <utility>
  #include <new>
  // Minimal: bilkul utna hi jitna is example ko chahiye.
  template <class E>
  struct Unexpect { E err; };

  template <class T, class E>
  class Expected {
      union { T val_; E err_; };
      bool has_;
  public:
      Expected(const T& v)  : val_(v),  has_(true)  {}
      Expected(T&& v)       : val_(std::move(v)), has_(true) {}
      Expected(Unexpect<E> u) : err_(std::move(u.err)), has_(false) {}
      Expected(const Expected& o) : has_(o.has_) {
          if (has_) new (&val_) T(o.val_); else new (&err_) E(o.err_);
      }
      ~Expected() { if (has_) val_.~T(); else err_.~E(); }
      Expected& operator=(const Expected&) = delete;

      bool has_value() const { return has_; }
      explicit operator bool() const { return has_; }
      const T& value() const { return val_; }
      const T& operator*() const { return val_; }
      const E& error() const { return err_; }
      T value_or(T alt) const { return has_ ? val_ : alt; }

      // monadic: F :: T -> Expected<U,E>
      template <class F>
      auto and_then(F&& f) const -> decltype(f(val_)) {
          if (has_) return f(val_);
          return Unexpect<E>{err_};
      }
      // monadic: F :: T -> U   (wraps back into Expected<U,E>)
      template <class F>
      auto transform(F&& f) const -> Expected<decltype(f(val_)), E> {
          if (has_) return f(val_);
          return Unexpect<E>{err_};
      }
      // monadic: F :: E -> Expected<T,E>
      template <class F>
      Expected or_else(F&& f) const {
          if (has_) return *this;
          return f(err_);
      }
  };
  static constexpr const char* kImpl = "mini Expected<T,E> (C++20 fallback)";
#endif

// helper: error banane ke liye
template <class E>
static Unexpect<E> fail(E e) { return Unexpect<E>{e}; }

// ============================================================
//  Ab actual logic — har step Expected lautata hai, koi throw nahi
// ============================================================

// "42" -> 42 ; galat -> ParseErr
static Expected<long long, ParseErr> parse_int(std::string_view s) {
    if (s.empty()) return fail(ParseErr::empty);
    long long n = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return fail(ParseErr::not_a_number);
        long long digit = c - '0';
        if (n > (9'000'000'000'000'000'000LL - digit) / 10) return fail(ParseErr::out_of_range);
        n = n * 10 + digit;
    }
    return n;
}

// business rule: price 0 se bada hona chahiye, aur <= 1_000_000 ticks
static Expected<long long, ParseErr> validate_price(long long ticks) {
    if (ticks <= 0)            return fail(ParseErr::negative);
    if (ticks > 1'000'000)     return fail(ParseErr::out_of_range);
    return ticks;
}

// pipeline: string -> parse -> validate -> (ticks * tick_value)
static Expected<long long, ParseErr> price_in_paise(std::string_view s) {
    return parse_int(s)
        .and_then(validate_price)
        .transform([](long long ticks) { return ticks * 5; });   // 1 tick = 5 paise
}

int main() {
    std::printf("impl: %s\n\n", kImpl);

    const std::string_view inputs[] = { "2500", "0", "abc", "", "9999999", "500" };

    std::puts("string       -> result");
    std::puts("-----------------------------------------");
    for (std::string_view in : inputs) {
        auto r = price_in_paise(in);
        if (r) std::printf("  %-10.*s -> OK   %lld paise\n",
                           static_cast<int>(in.size()), in.data(), r.value());
        else   std::printf("  %-10.*s -> ERR  %s\n",
                           static_cast<int>(in.size()), in.data(), to_str(r.error()));
    }

    // --- value_or: error pe default ---
    std::puts("\nvalue_or (error -> fallback):");
    std::printf("  parse(\"xx\").value_or(-1) = %lld\n",
                parse_int("xx").value_or(-1));

    // --- or_else: error ko recover / translate karo ---
    std::puts("\nor_else (empty ko 0 maano):");
    auto recovered = parse_int("").or_else([](ParseErr) -> Expected<long long, ParseErr> {
        return 0LL;
    });
    std::printf("  parse(\"\").or_else(->0) = %lld\n", recovered.value_or(-999));

    std::puts("\nSaar: har function 'T ya E' lautata hai. Caller `if (r)` se check");
    std::puts("karta hai. Koi try/catch nahi, koi hidden control flow nahi —");
    std::puts("error path bhi normal code path jitna hi predictable.");
    return 0;
}
