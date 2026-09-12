// 10_auto_demo.cpp
// ============================================================
// auto: fayde aur traps
// ============================================================
#include <iostream>
#include <vector>
#include <string>
#include <map>

// Ek object jo apni copies count karta hai -- copy trap dikhane ke liye
struct Tracked {
    std::string data;
    static int copyCount;

    explicit Tracked(std::string d) : data(std::move(d)) {}
    Tracked(const Tracked& o) : data(o.data) { ++copyCount; }
    Tracked& operator=(const Tracked&) = default;
};
int Tracked::copyCount = 0;

int main() {
    std::cout << "===== BASIC DEDUCTION =====\n";
    auto a = 5;             // int
    auto b = 5.0;           // double
    auto c = 5.0f;          // float
    auto d = 'x';           // char
    auto e = true;          // bool
    auto f = 5u;            // unsigned int
    auto g = 5L;            // long

    std::cout << "auto a = 5;     sizeof = " << sizeof(a) << "  (int)\n";
    std::cout << "auto b = 5.0;   sizeof = " << sizeof(b) << "  (double)\n";
    std::cout << "auto c = 5.0f;  sizeof = " << sizeof(c) << "  (float)\n";
    std::cout << "auto d = 'x';   sizeof = " << sizeof(d) << "  (char)\n";
    std::cout << "auto e = true;  sizeof = " << sizeof(e) << "  (bool)\n";
    std::cout << "auto f = 5u;    sizeof = " << sizeof(f) << "  (unsigned)\n";
    std::cout << "auto g = 5L;    sizeof = " << sizeof(g) << "  (long)\n";

    std::cout << "\n===== ⚠️ TRAP 1: STRING LITERAL =====\n";
    auto s = "hello";
    std::cout << "auto s = \"hello\";  sizeof(s) = " << sizeof(s)
              << "  <- POINTER ka size!\n";
    std::cout << "s ka type: const char*, NOT std::string\n";
    // s.length();              // ❌ compile error
    auto s2 = std::string("hello");
    std::cout << "auto s2 = std::string(\"hello\");  length = " << s2.length()
              << "  ✅\n";

    std::cout << "\n===== ⚠️ TRAP 2: const/& DROP HO JAATE HAIN =====\n";
    const int ci = 5;
    auto x1 = ci;               // int -- const GAYA
    x1 = 99;                    // ✅ modify ho gaya
    std::cout << "const int ci = 5;\n";
    std::cout << "auto x1 = ci;  x1 = 99;  -> x1 = " << x1
              << ", ci = " << ci << "  (const drop hua, copy bani)\n";

    const auto x2 = ci;         // const int  ✅
    // x2 = 99;                 // ❌ error
    std::cout << "const auto x2 = ci;  -> const preserve hua\n";
    (void)x2;

    int y = 5;
    int& ref = y;
    auto  r1 = ref;             // int  -- reference GAYA, copy bani
    auto& r2 = ref;             // int& -- reference preserve
    r1 = 100;
    std::cout << "\nint y = 5; int& ref = y;\n";
    std::cout << "auto  r1 = ref;  r1 = 100;  -> y = " << y
              << "  (copy thi, y nahi badla)\n";
    r2 = 200;
    std::cout << "auto& r2 = ref;  r2 = 200;  -> y = " << y
              << "  (reference thi, y badla)\n";

    std::cout << "\n===== ⚠️ TRAP 3: LOOPS MEIN ANCHAHI COPIES =====\n";
    std::vector<Tracked> items;
    items.emplace_back("data-1");
    items.emplace_back("data-2");
    items.emplace_back("data-3");

    Tracked::copyCount = 0;
    for (auto item : items) { (void)item; }
    std::cout << "for (auto item : items)        -> "
              << Tracked::copyCount << " copies  ⚠️\n";

    Tracked::copyCount = 0;
    for (const auto& item : items) { (void)item; }
    std::cout << "for (const auto& item : items) -> "
              << Tracked::copyCount << " copies  ✅\n";
    std::cout << "\nHFT hot path mein yeh REAL latency hai.\n";
    std::cout << "RULE: loops mein `const auto&` default banao.\n";

    std::cout << "\n===== ✅ AUTO KAB USEFUL HAI =====\n";
    std::map<std::string, std::vector<int>> data;
    data["orders"] = {1, 2, 3};
    data["trades"] = {4, 5};

    // Bina auto -- yeh likhna padta:
    // std::map<std::string, std::vector<int>>::const_iterator it = data.begin();
    auto it = data.begin();
    std::cout << "auto it = data.begin();  -- lamba type likhne se bacha\n";
    std::cout << "First key: " << it->first << "\n";

    // Structured bindings (C++17) -- auto ke saath aur bhi clean
    std::cout << "\nStructured bindings (C++17):\n";
    for (const auto& [key, values] : data) {
        std::cout << "  " << key << ": ";
        for (int v : values) std::cout << v << " ";
        std::cout << "\n";
    }

    std::cout << "\n===== ❌ AUTO KAB NAHI =====\n";
    std::cout << "auto result = calculate();   <- reader ko type pata nahi chalta\n";
    std::cout << "int64_t result = calculate();<- ✅ explicit jab type important ho\n";

    std::cout << "\n===== SUMMARY =====\n";
    std::cout << "✅ auto it = container.begin();\n";
    std::cout << "✅ for (const auto& x : container)\n";
    std::cout << "✅ auto ptr = std::make_unique<Order>();\n";
    std::cout << "❌ auto s = \"text\";        (const char*, not string)\n";
    std::cout << "❌ for (auto x : bigObjects)  (copies!)\n";
    std::cout << "❌ auto x = ci;              (const drop)\n";

    return 0;
}
