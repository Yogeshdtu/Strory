// 05_std_move_demo.cpp
// ============================================================
// std::move -- yeh sirf ek CAST hai. Woh khud kuch move NAHI karta.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 05_std_move_demo.cpp -o sm && ./sm
//   (jaan-boojh kar 1 warning: -Wpessimizing-move -- section 4, makeBad lambda.
//    Wahi lesson hai: `return std::move(x)` copy elision ko rok deta hai.)
// ============================================================
//   std::move(x)  ==  static_cast<remove_reference_t<decltype(x)>&&>(x)
//   -- ek lvalue ko rvalue (xvalue) mein "cast" karta hai.
//   Actual resource transfer tab hota hai jab ek MOVE ctor / MOVE assign
//   us rvalue ko consume karti hai.
// ============================================================

#include <iostream>
#include <string>
#include <utility>
#include <vector>

struct Tag {
    std::string name;
    explicit Tag(std::string n) : name(std::move(n)) {}
    Tag(const Tag& o) : name(o.name)            { std::cout << "  COPY Tag(" << name << ")\n"; }
    Tag(Tag&& o) noexcept : name(std::move(o.name)) { std::cout << "  MOVE Tag(" << name << ")\n"; }
};

int main() {
    std::cout << "=== 1. std::move by itself does NOTHING ===\n";
    std::string s = "hello world this is a long string past SSO";
    auto&& r = std::move(s);            // just a cast -> rvalue reference to s. NO transfer.
    std::cout << "  after std::move(s): s = \"" << s << "\"  (UNCHANGED -- nothing consumed it)\n";
    std::cout << "  s.size() = " << s.size() << "\n";
    (void)r;

    std::cout << "\n=== 2. transfer happens when a MOVE ctor consumes the rvalue ===\n";
    std::string t = std::move(s);       // string's MOVE ctor runs -> steals s's buffer
    std::cout << "  after std::string t = std::move(s):\n";
    std::cout << "    t = \"" << t << "\"\n";
    std::cout << "    s = \"" << s << "\"  (moved-from -- valid but unspecified, usually empty)\n";
    std::cout << "    s.size() = " << s.size() << "\n";

    std::cout << "\n=== 3. std::move on a CONST -> silently COPIES ===\n";
    const Tag ct{"const-tag"};
    Tag a = std::move(ct);              // std::move(ct) is `const Tag&&` -> move ctor can't bind -> COPY ctor
    std::cout << "  (const rvalue -> copy ctor chosen, not move)\n";
    (void)a;

    std::cout << "\n=== 4. std::move in `return` -> PESSIMIZATION ===\n";
    std::cout << "  Tag makeGood() { Tag x{\"g\"}; return x;            }   -> NRVO (zero copy/move)\n";
    std::cout << "  Tag makeBad () { Tag x{\"b\"}; return std::move(x); }   -> forces a MOVE (NRVO disabled)\n";
    auto makeGood = [] { Tag x{"good"}; return x; };
    auto makeBad  = [] { Tag x{"bad"};  return std::move(x); };
    std::cout << "  makeGood(): "; Tag g = makeGood();   (void)g;   // no COPY/MOVE line printed (NRVO)
    std::cout << "  makeBad() : "; Tag b = makeBad();    (void)b;   // "MOVE Tag(bad)" printed

    std::cout << "\n=== 5. std::move into a container (the real use) ===\n";
    std::vector<Tag> v;
    Tag big{"push-me"};
    v.push_back(big);                   // COPY (big is an lvalue)
    v.push_back(std::move(big));        // MOVE (explicit rvalue)
    std::cout << "  big.name = \"" << big.name << "\"  (moved-from)\n";

    std::cout <<
        "\n"
        "  std::move  = cast to rvalue. Zero runtime cost. Nothing is moved by it.\n"
        "  Move HAPPENS when a move ctor/assign takes that rvalue and steals resources.\n"
        "  Gotchas: std::move on const -> copy;  std::move in return -> kills (N)RVO.\n";
    return 0;
}
