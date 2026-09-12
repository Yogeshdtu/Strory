// 06_operator_overload.cpp
// ============================================================
// Operator overloading -- Money class (cents mein, no float)
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 06_operator_overload.cpp -o oo && ./oo
// ============================================================
//   Operator overloading = apni types ke liye +, -, ==, <<, ... define karna.
//   Rules:
//   - Kam se kam ek operand user-defined type ho.
//   - Precedence / arity / associativity NAHI badal sakte.
//   - `+=` member; `+` free function (symmetric conversions ke liye).
//   - Comparisons: C++20 `<=>` ek se saare relational operators mil jaate.
//   - `<<` free function (left operand `std::ostream&` hai).
// ============================================================

#include <compare>
#include <cstdint>
#include <iostream>

class Money {
    std::int64_t cents_ = 0;

public:
    Money() = default;
    explicit Money(std::int64_t cents) : cents_(cents) {}
    static Money fromRupees(std::int64_t r, std::int64_t p = 0) { return Money{r * 100 + p}; }

    std::int64_t cents() const { return cents_; }

    // ---- compound assignment: MEMBER (baayan operand hamesha Money) ----
    Money& operator+=(Money rhs) { cents_ += rhs.cents_; return *this; }
    Money& operator-=(Money rhs) { cents_ -= rhs.cents_; return *this; }
    Money& operator*=(std::int64_t k) { cents_ *= k; return *this; }

    // ---- unary minus ----
    Money operator-() const { return Money{-cents_}; }

    // ---- prefix / postfix ++ (ek paisa) -- dikhane ke liye ----
    Money& operator++()    { ++cents_; return *this; }          // prefix: ++m
    Money  operator++(int) { Money old = *this; ++cents_; return old; }  // postfix: m++

    // ---- comparisons: ek default <=> se ==, !=, <, <=, >, >= sab ----
    auto operator<=>(const Money&) const = default;
    bool operator==(const Money&) const = default;
};

// ---- binary + / - : FREE functions, compound assignment ke terms mein ----
Money operator+(Money a, Money b) { a += b; return a; }
Money operator-(Money a, Money b) { a -= b; return a; }
Money operator*(Money a, std::int64_t k) { a *= k; return a; }
Money operator*(std::int64_t k, Money a) { a *= k; return a; }   // symmetric: 3 * m

// ---- stream output: FREE function, left operand std::ostream& ----
std::ostream& operator<<(std::ostream& os, Money m) {
    std::int64_t c = m.cents();
    const char* sign = (c < 0) ? "-" : "";
    if (c < 0) c = -c;
    os << sign << (c / 100) << '.';
    if (c % 100 < 10) os << '0';
    return os << (c % 100);
}

int main() {
    Money a = Money::fromRupees(100, 50);      // 100.50
    Money b = Money::fromRupees(25);           // 25.00

    std::cout << "a        = " << a << "\n";
    std::cout << "b        = " << b << "\n";
    std::cout << "a + b    = " << (a + b) << "\n";
    std::cout << "a - b    = " << (a - b) << "\n";
    std::cout << "3 * b    = " << (3 * b) << "\n";
    std::cout << "-a       = " << (-a) << "\n";

    a += b;
    std::cout << "a += b   -> a = " << a << "\n";

    Money c = Money::fromRupees(10);
    std::cout << "c++ returns " << c++ << ", then c = " << c << "\n";
    std::cout << "++c returns " << ++c << ", and c = " << c << "\n";

    std::cout << std::boolalpha;
    std::cout << "\na == b : " << (a == b) << "\n";
    std::cout << "a  > b : " << (a  > b) << "\n";
    std::cout << "a != b : " << (a != b) << "\n";
    std::cout << "b <= a : " << (b <= a) << "\n";

    std::cout <<
        "\n"
        "  +=  member  |  +  free (a += b; return a)\n"
        "  <<  free (ostream& left)\n"
        "  <=> = default  ->  <, <=, >, >=, ==, != sab mil gaye\n"
        "  Money(int) explicit -> galti se int se Money nahi banega\n";
    return 0;
}
