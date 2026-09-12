// 01_if_else.cpp
// ============================================================
// if / else / else-if -- program ka pehla FAISLA
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_if_else.cpp -o ifelse && ./ifelse
// ============================================================
// Yeh dikhata hai:
//   1. if -- condition bool honi chahiye
//   2. if / else
//   3. else-if chain (aur woh switch nahi hai)
//   4. truthiness (int/pointer -> bool) aur explicit likhne ki salah
//   5. braces kyon matter karte hain (silent bug)
//   6. guard clause -- nesting kam karna
//   7. ternary -- jab if ek VALUE chahiye
// ============================================================

#include <iostream>
#include <string>
#include <string_view>

// ------------------------------------------------------------
// Guard clause demo ke liye do versions -- ek nested, ek flat.
// ------------------------------------------------------------
std::string_view classifyNested(int age) {
    std::string_view result;
    if (age >= 0) {
        if (age < 13) {
            result = "child";
        } else {
            if (age < 20) {
                result = "teen";
            } else {
                result = "adult";
            }
        }
    } else {
        result = "invalid";
    }
    return result;
}

std::string_view classifyGuard(int age) {
    if (age < 0)   return "invalid";   // pehle galat cases nikaal do
    if (age < 13)  return "child";     // ab har line ka matlab saaf hai
    if (age < 20)  return "teen";
    return "adult";                    // bacha hua case
}

int main() {
    std::cout << std::boolalpha;

    // ============================================================
    //  1. BASIC if -- condition ek bool expression hai
    // ============================================================
    std::cout << "===== 1. BASIC if =====\n";
    const int temperature = 45;

    if (temperature > 40) {
        // Yeh block SIRF tab chalta hai jab condition true ho
        std::cout << "  Bahut garmi hai (" << temperature << ")\n";
    }

    // condition khud ek value hai -- (temperature > 40) -> true/false
    std::cout << "  (temperature > 40) ka result = " << (temperature > 40) << "\n";

    // ============================================================
    //  2. if / else -- do raaste, exactly ek chalega
    // ============================================================
    std::cout << "\n===== 2. if / else =====\n";
    const int number = 7;

    if (number % 2 == 0) {
        std::cout << "  " << number << " even hai\n";
    } else {
        std::cout << "  " << number << " odd hai\n";
    }

    // ============================================================
    //  3. else-if CHAIN -- upar se neeche, pehla true jeet-ta hai
    // ============================================================
    std::cout << "\n===== 3. else-if CHAIN =====\n";
    // Yaad rakho: `else if` koi alag keyword nahi. Yeh bas
    //   else { if (...) { ... } }
    // hai -- braces optional isliye ek line mein aa jaata hai.
    for (const int score : {95, 82, 71, 40}) {
        char grade;
        if (score >= 90)      grade = 'A';
        else if (score >= 80) grade = 'B';
        else if (score >= 70) grade = 'C';
        else                  grade = 'F';

        std::cout << "  score " << score << " -> " << grade << "\n";
    }
    // ⚠️ Order matter karta hai: agar `score >= 70` sabse upar hota,
    //    to 95 bhi 'C' ban jaata (pehla match jeet-ta hai).

    // ============================================================
    //  4. TRUTHINESS -- non-bool bhi condition ban jaata hai
    // ============================================================
    std::cout << "\n===== 4. TRUTHINESS =====\n";
    // if () ke andar jo bhi ho, usko bool mein convert kiya jaata hai:
    //   0, 0.0, nullptr        -> false
    //   baaki sab (non-zero)   -> true
    const int zero = 0;
    const int negative = -1;
    const int* nullPtr = nullptr;

    std::cout << "  if (0)      -> " << (zero ? "true" : "false") << "\n";
    std::cout << "  if (-1)     -> " << (negative ? "true" : "false")
              << "   <- non-zero, isliye true (sign se matlab nahi)\n";
    std::cout << "  if (nullptr)-> " << (nullPtr ? "true" : "false") << "\n";

    // ✅ Salah: intent explicit likho -- condition self-documenting ho
    const int itemCount = 3;
    if (itemCount != 0) {                 // "count zero nahi hai"  -- clear
        std::cout << "  cart mein " << itemCount << " items\n";
    }
    // `if (itemCount)` bhi chalta, par padhne wale ko sochna padta hai.
    // Exception: bool variable seedha likho -> if (isReady), if (!v.empty())

    // ============================================================
    //  5. ⚠️ BRACES -- chhodne pe silent bug
    // ============================================================
    std::cout << "\n===== 5. BRACES KA TRAP =====\n";
    const bool loggedIn = false;

    // Bina braces ke, `if` sirf AGLI EK statement ko control karta hai.
    if (loggedIn)
        std::cout << "  welcome back\n";
    std::cout << "  <- yeh line if ke BAHAR hai, hamesha chalti hai\n";

    // Agar aap yahan do statements "if ke andar" samajh rahe the -- galti.
    // Isliye is course mein: HAMESHA braces lagao, chahe ek line ho.
    if (loggedIn) {
        std::cout << "  (kabhi nahi)\n";
    }

    // ⚠️ "Dangling else": else hamesha SABSE PAAS wale if se judta hai.
    // (Yeh block jaan-boojh kar galat-style hai -- -Wdangling-else warning
    //  isi ko pakad-ta hai. Hum yahan locally silence kar rahe hain sirf
    //  demo dikhane ke liye; aap apne code mein aisa MAT likho.)
    const int x = 5;
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-else"
#endif
    if (x > 0)
        if (x > 10)
            std::cout << "  bada\n";
        else
            std::cout << "  x 0 aur 10 ke beech (else INNER if se juda!)\n";
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    // Braces se irada saaf karo -- iske badle likho:
    //   if (x > 0) { if (x > 10) {...} else {...} }

    // ============================================================
    //  6. GUARD CLAUSE -- nesting flatten karo
    // ============================================================
    std::cout << "\n===== 6. GUARD CLAUSE =====\n";
    for (const int age : {-4, 8, 16, 30}) {
        std::cout << "  age " << age
                  << "  nested=" << classifyNested(age)
                  << "  guard=" << classifyGuard(age) << "\n";
    }
    std::cout << "  (dono ka output same -- par guard version padhna aasan)\n";

    // ============================================================
    //  7. TERNARY -- jab if se ek VALUE chahiye
    // ============================================================
    std::cout << "\n===== 7. TERNARY (?:) =====\n";
    const int a = 10, b = 20;

    // if-else statement value nahi deta, isliye const nahi bana sakte.
    // ?: ek EXPRESSION hai -> const initialise ho jaata hai.
    const int larger = (a > b) ? a : b;
    std::cout << "  const int larger = (a > b) ? a : b;  -> " << larger << "\n";

    const int count = 1;
    std::cout << "  " << count << " item" << (count == 1 ? "" : "s") << "\n";

    // ============================================================
    //  HFT relevance
    // ============================================================
    // Hot path (har market data message pe chalne wala code) mein:
    //   - sabse sasta aur sabse selective check PEHLE rakho
    //   - taaki zyada tar messages jaldi `return` ho jaayein
    //   - guard clauses hi yeh style hai: "galat cases pehle nikaalo"
    //
    //   if (msg.type != Expected)  return;   // 1 integer compare, aksar true
    //   if (!inTradingHours())     return;
    //   if (!riskCheck(msg))       return;
    //   process(msg);                         // yahan tak kam messages pahunchte hain
    std::cout << "\n(HFT: hot path mein guard clauses -- galat cases pehle nikaalo)\n";

    return 0;
}
