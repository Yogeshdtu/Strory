// 02_constructors.cpp
// ============================================================
// Saare constructor types -- default, parameterized, delegating,
// = default, = delete, copy
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 02_constructors.cpp -o ctor && ./ctor
// ============================================================
//   Constructor = ek special member function jo object ke banne ke waqt chalti
//   hai. Naam class jaisa, koi return type nahi. Kaam: members ko valid state
//   mein laana.
// ============================================================

#include <iostream>
#include <string>

class Widget {
    int         id_;
    std::string name_;
    double      weight_;

public:
    // ---- 1. parameterized constructor ----
    Widget(int id, std::string name, double weight)
        : id_(id), name_(std::move(name)), weight_(weight) {
        std::cout << "  [param ctor] id=" << id_ << "\n";
    }

    // ---- 2. delegating constructor -- doosre ctor ko call karta ----
    explicit Widget(int id)
        : Widget(id, "unnamed", 1.0) {          // param ctor ko delegate
        std::cout << "  [delegating ctor] id=" << id_ << "\n";
    }

    // ---- 3. default constructor -- = default se compiler wala maango ----
    Widget() : Widget(0) {                       // yeh bhi delegate karta hai
        std::cout << "  [default ctor]\n";
    }

    // ---- 4. copy constructor -- explicitly likha (dekhne ke liye) ----
    Widget(const Widget& other)
        : id_(other.id_), name_(other.name_ + " (copy)"), weight_(other.weight_) {
        std::cout << "  [copy ctor] from id=" << other.id_ << "\n";
    }

    // copy assignment ko = default (rule-of-3 preview -- folder 18)
    Widget& operator=(const Widget&) = default;

    void print() const {
        std::cout << "    Widget{id=" << id_ << ", name=\"" << name_
                  << "\", weight=" << weight_ << "}\n";
    }
};

// ---- 5. = delete -- ek type jise copy NAHI kiya ja sakta ----
class Unique {
    int token_;
public:
    explicit Unique(int t) : token_(t) {}
    Unique(const Unique&)            = delete;   // copy ctor banned
    Unique& operator=(const Unique&) = delete;   // copy assignment banned
    int token() const { return token_; }
};

int main() {
    std::cout << "=== default ctor ===\n";
    Widget a;                       a.print();

    std::cout << "\n=== single-arg (delegating) ctor ===\n";
    Widget b{42};                   b.print();

    std::cout << "\n=== param ctor ===\n";
    Widget c{7, "gear", 2.5};       c.print();

    std::cout << "\n=== copy ctor ===\n";
    Widget d = c;                   d.print();

    std::cout << "\n=== = delete ===\n";
    Unique u{100};
    std::cout << "  u.token() = " << u.token() << "\n";
    // Unique v = u;                // ❌ compile ERROR -- copy ctor deleted
    // Unique w{1}; w = u;          // ❌ compile ERROR -- copy assignment deleted
    std::cout << "  Unique copy karne ki koshish -> compile error (by design)\n";

    std::cout <<
        "\n"
        "  Widget()            : Widget(0) {}   -- default -> delegates\n"
        "  Widget(int id)      : Widget(id, ...) -- delegates to param ctor\n"
        "  Widget(int,str,dbl) : member init list -- asli kaam yahan\n"
        "  Widget(const Widget&)                 -- copy ctor\n"
        "  X(const X&) = delete                  -- copy banned\n";
    return 0;
}
