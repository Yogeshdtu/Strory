// 03_value_categories.cpp
// ============================================================
// Value categories -- lvalue / prvalue / xvalue  (glvalue, rvalue)
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 03_value_categories.cpp -o vc && ./vc
// ============================================================
//                      expression
//              .------------'------------.
//          glvalue                     rvalue
//        .----'----.               .-----'-----.
//     lvalue     xvalue  <-- xvalue is in BOTH -->  prvalue
//
//   lvalue  : has identity, can't be moved from   (named var, *p, a[i], s.m, ++i)
//   prvalue : no identity, "pure" temporary/value (42, a+b, foo(), T{})
//   xvalue  : has identity, CAN be moved from     (std::move(x), foo() where foo returns T&&)
//   glvalue = lvalue | xvalue   (has identity)
//   rvalue  = prvalue | xvalue  (movable)
// ============================================================

#include <iostream>
#include <string>
#include <utility>

// Overload set that reports which value category the argument bound to.
// (non-const lvalue ref preferred for lvalues; rvalue ref for rvalues; const& is the fallback)
void probe(std::string&)        { std::cout << "  -> non-const lvalue          (bound to std::string&)\n"; }
void probe(const std::string&)  { std::cout << "  -> const lvalue OR const rvalue -> no move  (bound to const std::string&)\n"; }
void probe(std::string&&)       { std::cout << "  -> rvalue (movable)          (bound to std::string&&)\n"; }

std::string makeStr() { return "made"; }          // returns a prvalue
std::string&& passThrough(std::string&& s) { return std::move(s); }   // returns an xvalue

int main() {
    std::string x = "x";
    const std::string cx = "cx";
    std::string arr[2];

    std::cout << "=== lvalues (named, addressable, NOT movable implicitly) ===\n";
    std::cout << "x            "; probe(x);              // lvalue
    std::cout << "cx           "; probe(cx);             // const lvalue
    std::cout << "arr[0]       "; probe(arr[0]);         // lvalue
    std::cout << "++x-like     "; { std::string& r = x; probe(r); }   // lvalue (reference is an lvalue)

    std::cout << "\n=== prvalues (pure temporaries / literals / by-value returns) ===\n";
    std::cout << "std::string{} "; probe(std::string{});          // prvalue
    std::cout << "x + \"!\"      "; probe(x + "!");                // prvalue (operator+ returns by value)
    std::cout << "makeStr()     "; probe(makeStr());              // prvalue

    std::cout << "\n=== xvalues (have identity but explicitly movable) ===\n";
    std::cout << "std::move(x)  "; probe(std::move(x));           // xvalue (cast of lvalue to T&&)
    std::cout << "passThrough(std::move(x)) "; probe(passThrough(std::move(x)));  // xvalue (returns T&&)
    std::cout << "std::move(cx) "; probe(std::move(cx));          // xvalue BUT const -> binds to const& (can't move)

    std::cout << "\n=== decltype trick: (expr) with extra parens ===\n";
    // decltype(x)   -> declared type: std::string
    // decltype((x)) -> value category: lvalue -> std::string&
    std::cout << "  std::is_same<decltype(x),   std::string  > : "
              << std::is_same_v<decltype(x), std::string> << "\n";
    std::cout << "  std::is_same<decltype((x)), std::string& > : "
              << std::is_same_v<decltype((x)), std::string&> << "   (extra parens -> value category)\n";
    std::cout << "  std::is_same<decltype(std::move(x)), std::string&&> : "
              << std::is_same_v<decltype(std::move(x)), std::string&&> << "   (xvalue -> T&&)\n";

    std::cout <<
        "\n"
        "  lvalue : naam hai, address le sakte, implicitly move nahi hota\n"
        "  prvalue: literal / by-value return / T{} -- pure value, koi identity nahi\n"
        "  xvalue : std::move(lvalue) ya T&&-returning call -- identity + movable\n"
        "  rvalue = prvalue + xvalue  (yeh T&& parameter se bind hote)\n"
        "  Move ctor/assign RVALUES ko consume karti hain -> resource steal.\n";
    return 0;
}
