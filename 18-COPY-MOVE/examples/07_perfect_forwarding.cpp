// 07_perfect_forwarding.cpp
// ============================================================
// Forwarding references + std::forward -- value category preserve karna
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 07_perfect_forwarding.cpp -o pf && ./pf
// ============================================================
//   template <class T> void f(T&& x)   -- yeh FORWARDING reference hai (T deduced)
//     lvalue do  -> T = U&,  x : U&    (reference collapsing: U& && -> U&)
//     rvalue do  -> T = U,   x : U&&
//   std::forward<T>(x)  -- x ko uski ORIGINAL value category ke saath aage bhejta:
//     T = U&  -> forward gives an lvalue
//     T = U   -> forward gives an rvalue
//   Isse ek generic wrapper bina extra copy/move ke arguments pass kar sakta hai.
// ============================================================

#include <iostream>
#include <string>
#include <utility>

struct Widget {
    Widget()                       { std::cout << "  Widget()          default ctor\n"; }
    Widget(const Widget&)          { std::cout << "  Widget(const&)     COPY ctor\n"; }
    Widget(Widget&&) noexcept      { std::cout << "  Widget(&&)         MOVE ctor\n"; }
};

// The "consumer" -- overloaded on value category so we can see what arrived
void sink(const Widget&) { std::cout << "  sink(const Widget&)  <- got an LVALUE\n"; }
void sink(Widget&&)      { std::cout << "  sink(Widget&&)       <- got an RVALUE\n"; }

// ---- WITHOUT forwarding: always passes an lvalue (x is a named parameter) ----
template <class T>
void relayBad(T&& x) {
    sink(x);                          // x is a NAMED variable -> an lvalue, ALWAYS
}

// ---- WITH forwarding: preserves the caller's value category ----
template <class T>
void relayGood(T&& x) {
    sink(std::forward<T>(x));         // lvalue in -> lvalue out ; rvalue in -> rvalue out
}

// ---- a generic factory: perfectly forward ctor args ----
template <class T, class... Args>
T make(Args&&... args) {
    return T(std::forward<Args>(args)...);   // no extra copy/move of the args
}

int main() {
    Widget lv;                        // an lvalue Widget

    std::cout << "=== relayBad (no forward): rvalue argument still arrives as lvalue ===\n";
    std::cout << "  relayBad(lv):        "; relayBad(lv);
    std::cout << "  relayBad(Widget{}):  "; { Widget tmp; relayBad(std::move(tmp)); }
    //                                         ^ even a moved-in rvalue -> sink(const Widget&)

    std::cout << "\n=== relayGood (std::forward): value category preserved ===\n";
    std::cout << "  relayGood(lv):        "; relayGood(lv);
    std::cout << "  relayGood(Widget{}):  "; relayGood(Widget{});

    std::cout << "\n=== reference collapsing ===\n";
    std::cout << "  T&&  where T = Widget&   ->  Widget& && -> Widget&   (lvalue ref)\n";
    std::cout << "  T&&  where T = Widget    ->  Widget&&               (rvalue ref)\n";
    std::cout << "  Rule: koi bhi & ho -> result &.  Sirf && + && -> &&.\n";

    std::cout << "\n=== generic factory: forward ctor args (no double move) ===\n";
    std::string src = "a reasonably long source string value";
    std::cout << "  make<std::string>(std::move(src)):\n";
    std::string dst = make<std::string>(std::move(src));   // src's buffer -> forwarded -> moved into dst
    std::cout << "    dst = \"" << dst << "\"\n";
    std::cout << "    src = \"" << src << "\"   (moved-from -- forwarded as rvalue, then moved)\n";

    std::cout <<
        "\n"
        "  Forwarding reference: `T&&` jahan T deduce hota hai (ya `auto&&`).\n"
        "  std::forward<T>(x): x ko uski asli value category ke saath aage bhejta.\n"
        "  Bina forward -> named param hamesha lvalue -> generic wrappers ek extra copy karte.\n";
    return 0;
}
