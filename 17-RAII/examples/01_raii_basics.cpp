// 01_raii_basics.cpp
// ============================================================
// RAII -- Resource Acquisition Is Initialization
//   acquire in constructor, release in destructor, cleanup scope ke saath
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_raii_basics.cpp -o raii && ./raii
// ============================================================
//   Idea: resource ka lifetime ek OBJECT ke lifetime se baandh do.
//   Object scope se nikla -> destructor chala -> resource released.
//   - normal return, early return, break, exception -- har raste pe cleanup.
//   - manual `cleanup()` call karne ki zaroorat hi nahi.
// ============================================================

#include <iostream>
#include <string>

// Ek chhota RAII guard -- ctor mein "acquire", dtor mein "release" (yahan sirf print)
class TraceGuard {
    std::string name_;
public:
    explicit TraceGuard(std::string name) : name_(std::move(name)) {
        std::cout << "  + acquire  " << name_ << "\n";
    }
    ~TraceGuard() {
        std::cout << "  - release  " << name_ << "\n";
    }
    // RAII guards aksar non-copyable hote (ownership unique) -- folder 18
    TraceGuard(const TraceGuard&)            = delete;
    TraceGuard& operator=(const TraceGuard&) = delete;
};

void normalFlow() {
    std::cout << "normalFlow():\n";
    TraceGuard a{"A"};
    TraceGuard b{"B"};
    std::cout << "  ... doing work ...\n";
}   // yahan: dtor B, phir dtor A  (reverse order -- folder 15/16)

void earlyReturn(bool bail) {
    std::cout << "earlyReturn(bail=" << std::boolalpha << bail << "):\n";
    TraceGuard g{"G"};
    if (bail) {
        std::cout << "  bailing out early\n";
        return;                 // dtor G FIR BHI chalta hai
    }
    std::cout << "  finished normally\n";
}   // dtor G

void nestedScopes() {
    std::cout << "nestedScopes():\n";
    TraceGuard outer{"outer"};
    {
        TraceGuard inner{"inner"};
        std::cout << "  inside inner block\n";
    }                           // dtor inner -- YAHAN, block ke `}` pe
    std::cout << "  back in outer, inner already gone\n";
}   // dtor outer

void inALoop() {
    std::cout << "inALoop():\n";
    for (int i = 0; i < 3; ++i) {
        TraceGuard t{"iter-" + std::to_string(i)};
        std::cout << "    loop body " << i << "\n";
    }                           // har iteration ke end pe dtor -- ek naya object har baar
}

int main() {
    normalFlow();
    std::cout << "\n";
    earlyReturn(false);
    std::cout << "\n";
    earlyReturn(true);
    std::cout << "\n";
    nestedScopes();
    std::cout << "\n";
    inALoop();

    std::cout <<
        "\n"
        "  RAII: resource ka lifetime == object ka scope.\n"
        "  Constructor acquire, destructor release. Har exit path pe automatic.\n"
        "  Isliye C++ mein 'finally' block nahi hai -- destructor hi 'finally' hai.\n";
    return 0;
}
