// 04_dangling_reference.cpp
// ============================================================
// WARNING: DELIBERATE dangling-reference bugs -- chalao, dekho, phir fix samjho
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 04_dangling_reference.cpp -o dr && ./dr
//
//   Yeh file JAAN-BOOJH kar 2 warnings deti hai -- GCC khud dangling pakad leta
//   hai, aur wahi lesson hai:
//     -Wreturn-local-addr   -- BUG 1: local ka reference return          (line ~30)
//     -Wdangling-reference  -- BUG 3: const& bound to a temp via a call  (line ~46)
//
//   BUG 1 ko hum runtime pe CALL nahi karte -- reference-to-dead-local ko read
//   karna is toolchain pe seedha crash (SIGSEGV) hai. Bas compile-warning dekho.
//   BUG 2 aur BUG 3 chalte hain aur galat behaviour dikhate hain.
//
//   Asli diagnosis (stack-use-after-return / -scope) ASan se -- Linux/Clang/WSL:
//     g++ -std=c++20 -fsanitize=address,undefined -g 04_dangling_reference.cpp -o dr && ./dr
//   MinGW-w64 pe libasan nahi -- examples/README.md dekho.
// ============================================================

#include <iostream>
#include <vector>

// ---- BUG 1: local variable ka reference return (compile pe hi pakda jaata hai) ----
int& danglingFromLocal() {
    int local = 42;
    return local;          // WARNING -Wreturn-local-addr -- 'local' return pe mar jaata hai;
}                          // caller ke paas DEAD object ka alias reh jaata

// ---- BUG 3: lifetime extension ek function call ke through PROPAGATE nahi hoti ----
const int& passThrough(const int& r) { return r; }   // sirf aage bhej diya

// Stack ko "ganda" karne ke liye -- taaki mari hui temporary ki jagah reuse ho jaaye
__attribute__((noinline)) void stackNoise() {
    volatile int buf[64];
    for (int i = 0; i < 64; ++i) buf[i] = 0x2d2d2d2d;
    (void)buf;
}

// ---- FIX versions ----
int returnByValue() {                        // OK -- caller ko apni copy milti hai
    int local = 42;
    return local;
}
int& refIntoCaller(std::vector<int>& v) {    // OK -- reference us object ka jo
    return v[0];                             //      CALLER ke paas zinda hai
}

int main() {
    std::cout << "===== BUG 1: reference to a returned local =====\n";
    std::cout << "  danglingFromLocal() ko GCC ne compile pe hi pakad liya\n"
                 "  (-Wreturn-local-addr). Isliye ise chalate bhi nahi -- read = crash.\n"
              << std::flush;

    std::cout << "\n===== BUG 2: reference invalidated by vector realloc =====\n";
    {
        std::vector<int> v{10, 20, 30};
        int& first = v[0];                       // first -> v ke current buffer ka slot 0
        int* staleAddr = &first;
        std::cout << "  &first (bind ke waqt) = " << staleAddr << ", first = " << first << "\n";
        for (int i = 0; i < 1000; ++i) v.push_back(i);   // buffer grow -> naya block, purana free
        std::cout << "  1000x push_back ke baad:\n";
        std::cout << "    &first (stale)   = " << staleAddr << "\n";
        std::cout << "    &v[0]  (current) = " << &v[0] << "\n";
        std::cout << "    same? " << (staleAddr == &v[0] ? "yes"
                     : "NO  -> 'first' ab FREE ho chuki memory ko alias karti hai (UB)")
                  << "\n" << std::flush;
    }

    std::cout << "\n===== BUG 3: lifetime extension does NOT survive a call =====\n";
    {
        const int& r = passThrough(42);   // 42 ki temporary IS statement ke end pe marti hai;
        stackNoise();                     // extension yahan tak nahi pahunchti
        std::cout << "  expected 42, mila: " << r
                  << "   (build/luck pe depend -- UB. 'const int& r = 42;' hota to safe)\n"
                  << std::flush;
    }

    std::cout << "\n===== FIX =====\n";
    int good = returnByValue();
    std::cout << "  returnByValue()  -> " << good << "   (apni copy -- safe)\n";
    std::vector<int> v{100, 200};
    int& slot = refIntoCaller(v);
    slot = 999;
    std::cout << "  refIntoCaller(v) -> v[0] ab = " << v[0] << "   (v caller ke paas zinda)\n";
    const int& ok = 42;                    // yeh theek hai -- direct bind = lifetime extend
    std::cout << "  const int& ok = 42;  -> " << ok << "   (direct bind -> temp full scope tak jeeti)\n";

    std::cout <<
        "\n"
        "  Rule: reference apne referent se ZYADA nahi jeeni chahiye.\n"
        "  - local ka reference return mat karo -> value return karo\n"
        "  - jo container badal sakta hai, uske element ka reference mat pakdo\n"
        "  - 'const T& r = temp;' extend karta hai; 'const T& r = f(temp);' NAHI\n";
    return 0;
}
