// 01_struct_basics.cpp
// ============================================================
// struct -- related data ko ek unit mein bandhna
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_struct_basics.cpp -o sb && ./sb
// ============================================================

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// ------------------------------------------------------------
//  struct definition -- ek naya TYPE ban gaya
// ------------------------------------------------------------
struct Point {
    double x;
    double y;
};   // <- SEMICOLON zaroori (type define kar rahe ho)

struct Order {
    std::uint64_t id;
    std::string   symbol;
    double        price;
    std::int64_t  qty;
    bool          isBuy;
};

// struct ko function mein use karo -- pass by const& (copy bachao)
double distance(const Point& a, const Point& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// struct return karo (RVO se free -- folder 08 lesson 04)
Point midpoint(const Point& a, const Point& b) {
    return { (a.x + b.x) / 2.0, (a.y + b.y) / 2.0 };
}

int main() {
    // ============================================================
    //  1. Instance banana + members access (dot operator)
    // ============================================================
    std::cout << "===== 1. basics =====\n";
    Point p;                          // ⚠️ members UNINITIALIZED (garbage)
    p.x = 3.0;
    p.y = 4.0;
    std::cout << "  p = (" << p.x << ", " << p.y << ")\n";

    Point origin{0.0, 0.0};           // aggregate init -- members in order
    std::cout << "  distance(p, origin) = " << distance(p, origin) << "\n";

    Point mid = midpoint(p, origin);
    std::cout << "  midpoint = (" << mid.x << ", " << mid.y << ")\n";

    // ============================================================
    //  2. Bigger struct -- ek "record"
    // ============================================================
    std::cout << "\n===== 2. Order record =====\n";
    Order o{1001, "AAPL", 192.34, 100, true};
    std::cout << "  #" << o.id << "  " << o.symbol << "  "
              << o.qty << " @ " << o.price << "  " << (o.isBuy ? "BUY" : "SELL") << "\n";

    o.price = 193.00;                 // members mutable hain
    o.qty += 50;
    std::cout << "  after edit: " << o.qty << " @ " << o.price << "\n";

    // ============================================================
    //  3. VALUE SEMANTICS -- copy independent hoti hai
    // ============================================================
    std::cout << "\n===== 3. value semantics =====\n";
    Order copy = o;                   // poori copy (id, symbol string bhi copy)
    copy.qty = 999;
    std::cout << "  o.qty = " << o.qty << "   copy.qty = " << copy.qty
              << "   (independent)\n";

    // ============================================================
    //  4. Array / vector of structs
    // ============================================================
    std::cout << "\n===== 4. containers of structs =====\n";
    std::vector<Point> path = {{0, 0}, {1, 2}, {3, 3}, {6, 1}};
    double total = 0.0;
    for (std::size_t i = 1; i < path.size(); ++i)
        total += distance(path[i - 1], path[i]);
    std::cout << "  path length = " << total << "\n";

    // ============================================================
    //  5. sizeof -- members ka jod (+ padding, lesson 05)
    // ============================================================
    std::cout << "\n===== 5. sizeof =====\n";
    std::cout << "  sizeof(Point) = " << sizeof(Point) << "  (2 * 8)\n";
    std::cout << "  sizeof(Order) = " << sizeof(Order)
              << "  (uint64 + string + double + int64 + bool + padding)\n";

    std::cout << "\n  struct = 'yeh cheezein ek saath rehti hain'. Dot se access,\n"
                 "  value semantics (copy = full copy), function ko const& se do.\n";

    return 0;
}
