// 02_switch_demo.cpp
// ============================================================
// switch / case / break / default / fallthrough
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 02_switch_demo.cpp -o sw && ./sw
// ============================================================
// Yeh dikhata hai:
//   1. basic switch -- ek value ko kai constants se match karna
//   2. break kyun zaroori -- warna FALLTHROUGH
//   3. jaan-boojh kar fallthrough -> [[fallthrough]] se document karo
//   4. default -- "in mein se koi nahi"
//   5. case ke andar variable -> { } block chahiye
//   6. enum class pe switch -> compiler missing case warn karta hai
//   7. C++ mein string pe switch NAHI hota
// ============================================================

#include <iostream>
#include <string_view>

enum class Side { Buy, Sell };

// enum class pe switch: koi `default` nahi rakha, taaki -Wswitch
// hamein missing case pe warn kare (compile-time safety).
std::string_view sideName(Side s) {
    switch (s) {
        case Side::Buy:  return "BUY";
        case Side::Sell: return "SELL";
    }
    return "?";   // sab enumerators handle hain, par compiler ko return chahiye
}

int main() {
    // ============================================================
    //  1. BASIC switch
    // ============================================================
    std::cout << "===== 1. BASIC switch =====\n";
    // switch ki value INTEGRAL ya ENUM honi chahiye (int, char, enum...).
    // Har `case` label ek COMPILE-TIME CONSTANT hai.
    for (const int day : {1, 3, 7, 9}) {
        std::cout << "  day " << day << " -> ";
        switch (day) {
            case 1: std::cout << "Monday\n";    break;
            case 2: std::cout << "Tuesday\n";   break;
            case 3: std::cout << "Wednesday\n"; break;
            case 7: std::cout << "Sunday\n";    break;
            default: std::cout << "(invalid)\n"; break;
        }
    }

    // ============================================================
    //  2. ⚠️ break BHOOLNE PE FALLTHROUGH
    // ============================================================
    std::cout << "\n===== 2. FALLTHROUGH (break missing) =====\n";
    // `case` sirf ENTRY POINT hai. Ek baar andar aaye, to `break`,
    // `return` ya block ke end tak SAB kuch chalta hai -- agle
    // case labels ko ignore karke.
    // (break missing -> -Wimplicit-fallthrough warning. Hum yahan locally
    //  silence kar rahe hain sirf bug ka ASAR dikhane ke liye. -Wall
    //  ke saath compiler ise KHUD pakad leta hai -- wahi bachav hai.)
    const int level = 1;
    std::cout << "  level " << level << " unlocks: ";
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
    switch (level) {
        case 1: std::cout << "[basic] ";      // break nahi -> neeche gir gaya
        case 2: std::cout << "[intermediate] ";
        case 3: std::cout << "[advanced] ";
                break;
        default: std::cout << "(none)";
    }
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    std::cout << "\n  (level 1 hone pe bhi teenon chhap gaye -- yeh bug hai\n"
                 "   agar aap sirf [basic] chahte the)\n";

    // ============================================================
    //  3. JAAN-BOOJH KAR fallthrough -> [[fallthrough]]
    // ============================================================
    std::cout << "\n===== 3. INTENTIONAL fallthrough =====\n";
    // Kai cases ko ek jaisa treat karna ho:
    for (const char c : {'a', 'E', 'x', '7'}) {
        std::cout << "  '" << c << "' -> ";
        switch (c) {
            case 'a': case 'e': case 'i': case 'o': case 'u':
            case 'A': case 'E': case 'I': case 'O': case 'U':
                // labels ke beech koi code nahi -> yeh "clean" grouping hai,
                // compiler isko warn nahi karta.
                std::cout << "vowel\n";
                break;

            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                std::cout << "digit\n";
                break;

            default:
                std::cout << "consonant / other\n";
                break;
        }
    }

    // Jab labels ke BEECH code ho aur aap phir bhi girna chahte ho:
    std::cout << "  --- [[fallthrough]] demo ---\n";
    for (const int n : {0, 1, 2}) {
        std::cout << "  n=" << n << ": ";
        switch (n) {
            case 0:
                std::cout << "zero ";
                [[fallthrough]];   // "yeh jaan-boojh kar hai" -- warning silence
            case 1:
                std::cout << "<=1 ";
                break;
            default:
                std::cout << "big ";
                break;
        }
        std::cout << "\n";
    }

    // ============================================================
    //  4. ⚠️ case ke andar VARIABLE -> { } block chahiye
    // ============================================================
    std::cout << "\n===== 4. case + local variable =====\n";
    const int opcode = 2;
    switch (opcode) {
        case 1: {
            const int bonus = 10;          // { } ke bina: "jump bypasses init" error
            std::cout << "  op1, bonus=" << bonus << "\n";
            break;
        }
        case 2: {
            const int bonus = 25;
            std::cout << "  op2, bonus=" << bonus << "\n";
            break;
        }
        default:
            std::cout << "  unknown op\n";
            break;
    }

    // ============================================================
    //  5. enum class pe switch -- compile-time completeness
    // ============================================================
    std::cout << "\n===== 5. switch on enum class =====\n";
    std::cout << "  Side::Buy  -> " << sideName(Side::Buy)  << "\n";
    std::cout << "  Side::Sell -> " << sideName(Side::Sell) << "\n";
    // Agar Side mein 'Cancel' add karein aur sideName() mein case na daalein,
    // -Wall (-> -Wswitch) warning dega: "enumeration value 'Cancel' not handled".
    // Yeh switch ka bada faayda hai if-else chain ke mukable.

    // ============================================================
    //  6. C++ mein STRING pe switch nahi hota
    // ============================================================
    std::cout << "\n===== 6. string pe switch? NAHI =====\n";
    std::cout << "  switch sirf integral/enum pe. Strings ke liye if-else,\n"
                 "  ya std::unordered_map<std::string, ...>, ya hashing.\n";

    return 0;
}
