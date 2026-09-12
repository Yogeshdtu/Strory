// 06_variant.cpp
// ============================================================
// std::variant -- type-safe tagged union (C++17)
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 06_variant.cpp -o var && ./var
// ============================================================
// std::variant<A, B, C> = "in mein se EXACTLY ek". Active type khud track
// karta hai, destructor sahi chalata hai, galat access pe throw karta hai.
// Raw union + manual tag ka safe replacement.
// ============================================================

#include <iostream>
#include <string>
#include <variant>
#include <vector>

// ek "market event" -- 3 mein se koi ek
struct Quote  { double bid, ask; };
struct Trade  { double price; long qty; };
struct Reject { std::string reason; };

using Event = std::variant<Quote, Trade, Reject>;

// ---- std::visit -- har alternative ke liye ek handler ----
struct EventPrinter {
    void operator()(const Quote& q)  const { std::cout << "  Quote  " << q.bid << " / " << q.ask << "\n"; }
    void operator()(const Trade& t)  const { std::cout << "  Trade  " << t.qty << " @ " << t.price << "\n"; }
    void operator()(const Reject& r) const { std::cout << "  Reject (" << r.reason << ")\n"; }
};

// double buy notional across a stream
double buyNotional(const std::vector<Event>& events) {
    double total = 0.0;
    for (const Event& e : events) {
        // overload set inline (C++ "overloaded" idiom -- yahan explicit)
        if (const Trade* t = std::get_if<Trade>(&e))
            total += t->price * static_cast<double>(t->qty);
    }
    return total;
}

int main() {
    // ============================================================
    //  1. Banana + which type -- index() / holds_alternative
    // ============================================================
    std::cout << "===== 1. basics =====\n";
    Event e = Quote{192.30, 192.34};
    std::cout << "  e.index() = " << e.index()
              << "  holds Quote? " << std::boolalpha << std::holds_alternative<Quote>(e) << "\n";

    e = Trade{192.32, 100};                 // reassign to a different alternative
    std::cout << "  after reassign: index = " << e.index()
              << "  holds Trade? " << std::holds_alternative<Trade>(e) << "\n";

    // ============================================================
    //  2. Access -- get / get_if / visit
    // ============================================================
    std::cout << "\n===== 2. access =====\n";
    try {
        std::cout << "  std::get<Quote>(e) -> ";
        auto q = std::get<Quote>(e);        // e holds Trade -> throws
        std::cout << q.bid << "\n";
    } catch (const std::bad_variant_access&) {
        std::cout << "throw std::bad_variant_access  (e holds Trade, not Quote)\n";
    }

    if (const Trade* t = std::get_if<Trade>(&e))   // ✅ no-throw check
        std::cout << "  get_if<Trade> -> " << t->qty << " @ " << t->price << "\n";

    std::visit(EventPrinter{}, e);          // ✅ dispatch on active type

    // ============================================================
    //  3. Stream processing
    // ============================================================
    std::cout << "\n===== 3. event stream =====\n";
    std::vector<Event> stream = {
        Quote{100.0, 100.1},
        Trade{100.05, 500},
        Reject{"price band"},
        Trade{100.10, 300},
        Quote{100.2, 100.3},
    };
    for (const Event& ev : stream) std::visit(EventPrinter{}, ev);
    std::cout << "  buy notional (all trades) = " << buyNotional(stream) << "\n";

    // ============================================================
    //  4. variant vs raw union
    // ============================================================
    std::cout << "\n===== 4. variant vs union =====\n";
    std::cout << "  sizeof(Event) = " << sizeof(Event)
              << "  (biggest alternative + discriminant + alignment)\n";
    std::cout <<
        "  ✅ variant: active type tracked, correct destructor, bad access -> throw,\n"
        "     std::string alternative safely handled, std::visit exhaustive dispatch.\n"
        "  ⚠️ union: koi tag nahi (khud track karo), non-trivial members = manual\n"
        "     ctor/dtor, wrong-member read = UB. Sirf POD wire layouts / bit views ke liye.\n"
        "  Cost: variant ka access ~ ek branch (index check). Hot path pe visit\n"
        "  ka dispatch inline ho jaata hai -O2 pe -- virtual call se sasta.\n";

    return 0;
}
