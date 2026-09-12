// 03_initializer_list.cpp
// ============================================================
// Member initializer list vs assignment -- aur MEMBER ORDER trap
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 03_initializer_list.cpp -o il && ./il
//   (jaan-boojh kar warnings: -Wreorder + -Wuninitialized -- neeche BadOrder dekho)
// ============================================================
//   Init list  : `: a_(x), b_(y)`   -> members SEEDHA construct hote hain
//   Assignment : `{ a_ = x; }`      -> pehle default-construct, PHIR assign (2 kaam)
//
//   Rule: members constructor body chalne se PEHLE hi ban chuke hote hain.
//   Body mein `=` sirf ASSIGNMENT hai, initialization nahi.
// ============================================================

#include <iostream>
#include <string>

// Kaun-kaunsa kaam hua, trace karne ke liye
struct Tracer {
    std::string tag;
    explicit Tracer(std::string t) : tag(std::move(t)) { std::cout << "  ctor  " << tag << "\n"; }
    Tracer(const Tracer& o) : tag(o.tag) { std::cout << "  copy  " << tag << "\n"; }
    Tracer& operator=(const Tracer& o) { std::cout << "  ASSIGN " << tag << " = " << o.tag << "\n"; tag = o.tag; return *this; }
};

// ---- BAD: assignment in body ----
class ViaAssignment {
    Tracer a_;
    Tracer b_;
public:
    ViaAssignment(const Tracer& a, const Tracer& b) : a_("a_default"), b_("b_default") {
        a_ = a;                 // default-construct ho chuka; ab assign (extra kaam)
        b_ = b;
    }
};

// ---- GOOD: member initializer list ----
class ViaInitList {
    Tracer a_;
    Tracer b_;
public:
    ViaInitList(const Tracer& a, const Tracer& b) : a_(a), b_(b) {}   // seedha copy-construct
};

// ---- const / reference members: init list ke bina compile hi nahi hota ----
class NeedsInitList {
    const int   limit_;
    int&        counterRef_;
public:
    NeedsInitList(int limit, int& counter) : limit_(limit), counterRef_(counter) {}
    // : galti se { limit_ = limit; } likho -> ERROR (const ko assign nahi, ref bind nahi)
    int limit() const { return limit_; }
};

// ---- MEMBER ORDER trap: members DECLARATION order mein init hote hain,
//      init-list mein likhe order mein NAHI ----
class BadOrder {
    int a_;
    int b_;
public:
    // hum chahte the b_ pehle, phir a_ = b_ + 1. Par a_ pehle declare hua hai,
    // to a_ PEHLE init hoga -- us waqt b_ abhi garbage hai!
    explicit BadOrder(int x) : b_(x), a_(b_ + 1) {}   // ⚠️ -Wreorder + a_ garbage-based
    void print() const { std::cout << "  BadOrder{a_=" << a_ << ", b_=" << b_ << "}  (a_ galat!)\n"; }
};

class GoodOrder {
    int a_;
    int b_;
public:
    explicit GoodOrder(int x) : a_(x + 1), b_(x) {}   // a_ ko b_ pe depend mat karao
    void print() const { std::cout << "  GoodOrder{a_=" << a_ << ", b_=" << b_ << "}\n"; }
};

int main() {
    Tracer ta{"A"}, tb{"B"};

    std::cout << "\n=== via assignment (body) -- extra kaam ===\n";
    ViaAssignment v1{ta, tb};       // ctor a_default, ctor b_default, ASSIGN x2

    std::cout << "\n=== via init list -- seedha ===\n";
    ViaInitList v2{ta, tb};         // copy x2, bas

    std::cout << "\n=== const / reference members ===\n";
    int counter = 10;
    NeedsInitList n{5, counter};
    std::cout << "  n.limit() = " << n.limit() << "  (const member -- init list se hi set hua)\n";

    std::cout << "\n=== member ORDER trap ===\n";
    BadOrder(5).print();            // a_ = garbage + 1  (b_ abhi set nahi tha jab a_ bana)
    GoodOrder(5).print();           // a_ = 6, b_ = 5

    std::cout <<
        "\n"
        "  1. Init list use karo (assignment nahi) -- ek kaam vs do.\n"
        "  2. const / reference / no-default members ke liye init list ZAROORI.\n"
        "  3. Members DECLARATION order mein init hote hain -- init-list ka order\n"
        "     bas cosmetic hai. Ek member ko doosre pe depend mat karao.\n";
    return 0;
}
