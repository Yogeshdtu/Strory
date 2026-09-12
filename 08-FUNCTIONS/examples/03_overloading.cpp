// 03_overloading.cpp
// ============================================================
// Function overloading -- ek naam, kai signatures. Overload RESOLUTION.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 03_overloading.cpp -o ov && ./ov
// ============================================================
// Compiler har call ke liye "best match" chunta hai, is order mein:
//   1. Exact match (koi conversion nahi / sirf trivial: T -> const T)
//   2. Promotion (char/short -> int, float -> double)
//   3. Standard conversion (int -> double, int -> bool, D* -> B*)
//   4. User-defined conversion (constructor / operator T)
//   ... koi ek best na mile / do equal ho -> AMBIGUOUS (compile error)
// ============================================================

#include <iostream>
#include <string>

// ---- alag parameter TYPE ----
void show(int x)          { std::cout << "  show(int)     -> " << x << "\n"; }
void show(double x)       { std::cout << "  show(double)  -> " << x << "\n"; }
void show(const char* s)  { std::cout << "  show(char*)   -> " << s << "\n"; }
void show(const std::string& s) { std::cout << "  show(string&) -> " << s << "\n"; }

// ---- alag parameter COUNT ----
int area(int side)             { return side * side; }          // square
int area(int w, int h)         { return w * h; }                // rectangle

// ---- const se overload (pointers/references pe) ----
void access(int* p)        { std::cout << "  access(int*)       (modifiable)\n"; (void)p; }
void access(const int* p)  { std::cout << "  access(const int*) (read-only)\n"; (void)p; }

int main() {
    std::cout << "===== 1. EXACT MATCH =====\n";
    show(42);            // show(int)
    show(3.14);          // show(double)
    show("hello");       // show(const char*)
    show(std::string("world"));   // show(const string&)

    std::cout << "\n===== 2. PROMOTION (char/short/float -> int/double) =====\n";
    char c = 'A';
    short s = 100;
    show(c);            // char -> int  : show(int)
    show(s);            // short -> int : show(int)
    float f = 2.5f;
    show(f);            // float -> double : show(double)

    std::cout << "\n===== 3. STANDARD CONVERSION =====\n";
    show(true);         // bool -> int : show(int)
    show('x' + 1);      // 'x'+1 already int : show(int)
    // show(10L);       // ⚠️ long -> int aur long -> double dono conversion -> AMBIGUOUS (section 6)

    std::cout << "\n===== 4. COUNT se overload =====\n";
    std::cout << "  area(5)    = " << area(5) << "   (square)\n";
    std::cout << "  area(3, 4) = " << area(3, 4) << "   (rectangle)\n";

    std::cout << "\n===== 5. const se overload =====\n";
    int value = 10;
    const int frozen = 20;
    access(&value);     // int*
    access(&frozen);    // const int*

    std::cout << "\n===== 6. ⚠️ AMBIGUOUS calls (comment hataao -> compile error) =====\n";
    // show(0.5f);  // float -> double ya float -> ... yahan double clear hai, theek
    //
    // Yeh ambiguous hai -- 5L int aur double DONO ko "standard conversion" chahiye,
    // koi behtar nahi:
    //     void g(int); void g(double);
    //     g(5L);        // ❌ error: call of overloaded 'g(long)' is ambiguous
    //
    // Classic: 0 aur nullptr
    //     void h(int); void h(char*);
    //     h(0);         // h(int) -- 0 int hai
    //     h(NULL);      // ⚠️ platform pe depend -- isliye nullptr use karo
    //     h(nullptr);   // h(char*) -- nullptr ka type std::nullptr_t
    std::cout << "  (details lesson 08 mein -- yahan comment mein 3 classic cases hain)\n";

    std::cout << "\n===== 7. Overload vs default arg -- alag cheezein =====\n";
    std::cout << "  area(int) aur area(int,int) -- 2 alag functions (overload)\n";
    std::cout << "  ek function `int area(int w, int h = -1)` -- 1 function, default arg (lesson 07)\n";

    return 0;
}
