// 05_initialization.cpp
// ============================================================
// Saare initialization forms aur unke fark
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 05_initialization.cpp -o init && ./init
// ============================================================

#include <iostream>
#include <vector>
#include <string>

struct Point { int x; int y; };

struct Timer {
    Timer() { std::cout << "  Timer constructor chala\n"; }
};

struct Config {
    int  width;
    int  height;
    bool fullscreen;
};

int main() {
    std::cout << "===== SAARE FORMS =====\n";

    int a = 5;          // copy initialization
    int b(5);           // direct initialization
    int c{5};           // direct-list (brace) initialization   ⭐ RECOMMENDED
    int d = {5};        // copy-list initialization
    int e{};            // value initialization -> 0
    auto f = 5;         // auto deduction

    std::cout << "int a = 5;   -> " << a << "\n";
    std::cout << "int b(5);    -> " << b << "\n";
    std::cout << "int c{5};    -> " << c << "\n";
    std::cout << "int d = {5}; -> " << d << "\n";
    std::cout << "int e{};     -> " << e << "   <- value-initialized to 0\n";
    std::cout << "auto f = 5;  -> " << f << "\n";

    // ============================================================
    //  BRACES NARROWING ROKTE HAIN -- yeh sabse bada fayda hai
    // ============================================================
    std::cout << "\n===== NARROWING PROTECTION =====\n";

    int truncated = 3.99;         // ✅ compile hota hai -- SILENT data loss
    std::cout << "int truncated = 3.99;  -> " << truncated
              << "   <- .99 CHUPCHAP gaya\n";

    // int safe{3.99};            // ❌ COMPILE ERROR: narrowing conversion
    std::cout << "int safe{3.99};        -> COMPILE ERROR (narrowing)\n";
    std::cout << "   ^ braces ne bug pakad liya!\n";

    int big = 300;
    char smallChar = static_cast<char>(big);   // explicit cast se allowed
    std::cout << "\nchar c = 300;   -> " << static_cast<int>(smallChar)
              << "   <- wrap around, silent\n";
    // char safeChar{big};        // ❌ COMPILE ERROR
    std::cout << "char c{300};    -> COMPILE ERROR\n";

    // Exception: agar value COMPILE TIME pe pata ho aur fit ho jaye:
    char ok{100};                 // ✅ 100 char mein fit hai
    std::cout << "char c{100};    -> " << static_cast<int>(ok)
              << "  ✅ (compile time pe pata hai ki fit hai)\n";

    // ============================================================
    //  VALUE INITIALIZATION -- har type ke liye sensible default
    // ============================================================
    std::cout << "\n===== VALUE INITIALIZATION {} =====\n";
    int         vi{};
    double      vd{};
    bool        vb{};
    char        vc{};
    int*        vp{};
    std::string vs{};

    std::cout << "int{}    -> " << vi << "\n";
    std::cout << "double{} -> " << vd << "\n";
    std::cout << "bool{}   -> " << std::boolalpha << vb << std::noboolalpha << "\n";
    std::cout << "char{}   -> " << static_cast<int>(vc) << " ('\\0')\n";
    std::cout << "int*{}   -> " << static_cast<void*>(vp) << " (nullptr)\n";
    std::cout << "string{} -> \"" << vs << "\" (empty, size " << vs.size() << ")\n";
    std::cout << "\n{} templates mein bahut useful hai -- har T ke liye kaam karta hai.\n";

    // ============================================================
    //  ⚠️ MOST VEXING PARSE
    // ============================================================
    std::cout << "\n===== MOST VEXING PARSE =====\n";
    std::cout << "Timer t1;   ->\n";
    Timer t1;                     // ✅ object banta hai

    std::cout << "Timer t2();  -> (kuch nahi hua!)\n";
    // Timer t2();                // ⚠️ yeh FUNCTION DECLARATION hai, object nahi!
                                  //    "t2 ek function hai jo Timer return karta hai"

    std::cout << "Timer t3{};  ->\n";
    Timer t3{};                   // ✅ object banta hai -- braces se ambiguity nahi

    std::cout << "\nBraces se yeh problem hi nahi aati -- ek aur reason {} use karne ka.\n";
    (void)t1; (void)t3;           // unused warning se bachne ke liye

    // ============================================================
    //  ⚠️ VECTOR TRAP -- braces ka apna gotcha
    // ============================================================
    std::cout << "\n===== VECTOR TRAP =====\n";
    std::vector<int> v1(5, 10);   // 5 elements, sab 10
    std::vector<int> v2{5, 10};   // 2 elements: 5 aur 10   ⚠️ BILKUL ALAG!
    std::vector<int> v3(5);       // 5 elements, sab 0
    std::vector<int> v4{5};       // 1 element: 5

    auto printVec = [](const char* name, const std::vector<int>& v) {
        std::cout << name << " size=" << v.size() << "  { ";
        for (int x : v) std::cout << x << " ";
        std::cout << "}\n";
    };

    printVec("vector<int> v1(5, 10);", v1);
    printVec("vector<int> v2{5, 10};", v2);
    printVec("vector<int> v3(5);    ", v3);
    printVec("vector<int> v4{5};    ", v4);

    std::cout << "\nRULE: {} initializer_list constructor ko PREFER karta hai.\n";
    std::cout << "Containers mein size dena ho -> () use karo.\n";

    // ============================================================
    //  AGGREGATE INITIALIZATION
    // ============================================================
    std::cout << "\n===== AGGREGATE INIT (structs/arrays) =====\n";
    Point p1{10, 20};
    Point p2{};                   // dono 0
    Point p3{10};                 // x=10, y=0 (baaki value-initialized)

    std::cout << "Point p1{10, 20}; -> (" << p1.x << ", " << p1.y << ")\n";
    std::cout << "Point p2{};       -> (" << p2.x << ", " << p2.y << ")\n";
    std::cout << "Point p3{10};     -> (" << p3.x << ", " << p3.y << ")\n";

    int arr1[5]{1, 2, 3, 4, 5};
    int arr2[5]{};                // sab 0
    int arr3[5]{1, 2};            // {1, 2, 0, 0, 0}

    auto printArr = [](const char* name, const int (&a)[5]) {
        std::cout << name << " { ";
        for (int x : a) std::cout << x << " ";
        std::cout << "}\n";
    };
    printArr("int arr[5]{1,2,3,4,5};", arr1);
    printArr("int arr[5]{};         ", arr2);
    printArr("int arr[5]{1,2};      ", arr3);

    // ============================================================
    //  DESIGNATED INITIALIZERS (C++20)
    // ============================================================
    std::cout << "\n===== DESIGNATED INITIALIZERS (C++20) =====\n";
    Config cfg{.width = 1920, .height = 1080, .fullscreen = true};
    std::cout << "Config{.width=1920, .height=1080, .fullscreen=true}\n";
    std::cout << "  -> " << cfg.width << "x" << cfg.height
              << " fullscreen=" << std::boolalpha << cfg.fullscreen
              << std::noboolalpha << "\n";

    Config cfg2{.width = 800};    // baaki value-initialized
    std::cout << "Config{.width=800}  -> " << cfg2.width << "x" << cfg2.height
              << " fullscreen=" << std::boolalpha << cfg2.fullscreen
              << std::noboolalpha << "\n";
    std::cout << "\n⚠️ C++ mein order SAME hona chahiye jaise struct mein --\n";
    std::cout << "   re-order karna error hai (C se strict).\n";

    // ============================================================
    //  ⚠️ UNINITIALIZED
    // ============================================================
    std::cout << "\n===== UNINITIALIZED (KABHI MAT KARNA) =====\n";
    // int garbage;               // ⚠️ local -- GARBAGE value, UB to read
    static int staticZero;        // ✅ static -- automatically 0
    std::cout << "int local;        -> GARBAGE (UB to read)\n";
    std::cout << "static int s;     -> " << staticZero << "  (auto zero)\n";
    std::cout << "int x{};          -> 0  ✅ safe\n";
    std::cout << "\nGOLDEN RULE: hamesha initialize karo. {} sabse safe hai.\n";

    return 0;
}
