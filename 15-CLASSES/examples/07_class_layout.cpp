// 07_class_layout.cpp
// ============================================================
// Class ka memory layout -- sizeof, member offsets, padding,
// empty class, "no vtable" (no virtual)
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 07_class_layout.cpp -o cl && ./cl
// ============================================================
//   Non-virtual class ka layout = uske data members ka layout (struct jaisa).
//   - private/public se layout NAHI badalta (bas access check).
//   - Methods object mein jagah NAHI lete (code alag, .text mein).
//   - Empty class ka sizeof >= 1 (har object ka unique address hona chahiye).
//   - Koi `virtual` nahi -> koi vptr nahi -> koi hidden 8 bytes nahi (folder 16).
// ============================================================

#include <cstddef>
#include <iostream>
#include <type_traits>

struct Empty {};                                  // koi data nahi

class WithMethodsOnly {                            // sirf methods, koi data nahi
public:
    void doThing() const {}
    int  compute() const { return 42; }
};

// Layout demos -- struct (public) taaki offsetof(...) bahar se naam le sake.
// (private/public se layout badalta NAHI -- niche Accessors class isse dikhati hai.)
struct BadLayout {                                 // padding se bloat
    char   a_ = 0;   // 1  + 7 padding
    double b_ = 0;   // 8
    char   c_ = 0;   // 1  + 7 padding
};

struct GoodLayout {                                // members descending alignment order
    double b_ = 0;   // 8
    char   a_ = 0;   // 1
    char   c_ = 0;   // 1  + 6 padding
};

class Accessors {                                  // methods + 2 ints -> sirf 8 bytes
    int x_;
    int y_;
public:
    Accessors(int x, int y) : x_(x), y_(y) {}
    int  x() const { return x_; }
    int  y() const { return y_; }
    void setX(int v) { x_ = v; }
    int  sum() const { return x_ + y_; }
};

int main() {
    std::cout << "sizeof(Empty)           = " << sizeof(Empty)
              << "   (>=1 -- unique address guarantee)\n";
    std::cout << "sizeof(WithMethodsOnly) = " << sizeof(WithMethodsOnly)
              << "   (methods object mein jagah nahi lete)\n";
    std::cout << "sizeof(Accessors)       = " << sizeof(Accessors)
              << "   (2 int = 8; 4 methods se koi fark nahi)\n\n";

    std::cout << "sizeof(BadLayout)  = " << sizeof(BadLayout)
              << "   (char,double,char -> 1+7pad + 8 + 1+7pad = 24)\n";
    std::cout << "sizeof(GoodLayout) = " << sizeof(GoodLayout)
              << "   (double,char,char -> 8 + 1 + 1 + 6pad = 16)\n\n";

    std::cout << "offsets (GoodLayout):\n";
    std::cout << "  b_ @ " << offsetof(GoodLayout, b_) << "\n";
    std::cout << "  a_ @ " << offsetof(GoodLayout, a_) << "\n";
    std::cout << "  c_ @ " << offsetof(GoodLayout, c_) << "\n\n";

    // ---- do Empty objects ke alag addresses ----
    Empty e1, e2;
    std::cout << "&e1 != &e2 : " << std::boolalpha << (&e1 != &e2)
              << "   (isiliye sizeof(Empty) >= 1)\n";

    std::cout << "\n";
    std::cout << "is_polymorphic<Accessors> : " << std::boolalpha
              << std::is_polymorphic_v<Accessors> << "   (no virtual -> no vptr -> layout == data)\n";

    std::cout <<
        "\n"
        "  Non-virtual class ka sizeof = data members + padding (struct jaisa).\n"
        "  private/public -> layout pe asar NAHI (compile-time access check).\n"
        "  Methods, static members -> per-object storage NAHI.\n"
        "  Empty class -> 1 byte (address uniqueness). Base ke roop mein 0 (EBO -- folder 16/18).\n"
        "  virtual aate hi +8 bytes (vptr) -- woh folder 16 mein.\n";
    return 0;
}
