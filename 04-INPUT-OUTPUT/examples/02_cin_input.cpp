// 02_cin_input.cpp
// ============================================================
// std::cin, >> operator, aur classic traps
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 02_cin_input.cpp -o cin && ./cin
//
// TEST KARO:
//   - Sahi input do
//   - Galat input do ("abc" jahan number chahiye)
//   - Ctrl+D (Linux/Mac) ya Ctrl+Z (Windows) se EOF do
// ============================================================

#include <iostream>
#include <string>
#include <limits>

int main() {
    std::cout << "===== 1. BASIC INPUT =====\n";
    int age = 0;
    std::cout << "Aapki umar likho: ";

    // `>>` kya karta hai:
    //   1. Leading whitespace SKIP karta hai (space, tab, newline)
    //   2. Type ke liye jitne characters valid hain, utne padhta hai
    //   3. Pehle INVALID character pe RUK jaata hai (use buffer mein chhod deta hai)
    //   4. Convert karke variable mein daal deta hai
    std::cin >> age;

    // Galat input pe kya hota hai:
    //   - failbit set ho jaata hai
    //   - age = 0 set ho jaata hai (C++11 se)
    //   - galat text BUFFER MEIN HI PADA REHTA HAI  <- yeh sabse important
    //   - har aage ka >> turant fail hoga
    if (std::cin.fail()) {
        std::cout << "\n⚠️ Galat input mila!\n";
        std::cout << "  fail() = " << std::cin.fail() << "\n";
        std::cout << "  eof()  = " << std::cin.eof()  << "\n";
        std::cout << "  age    = " << age << "  (default set ho gaya)\n";

        // RECOVERY -- DONO cheezein zaroori hain:
        std::cin.clear();      // 1. error flags saaf karo
                               //    (bina iske stream kuch padhega hi nahi)
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                               // 2. galat input buffer se hatao
                               //    (bina iske dobara wahi galat input padhega)
        std::cout << "  Recovery ho gayi.\n";
    } else {
        std::cout << "Aap " << age << " saal ke ho\n";
    }

    // ============================================================
    //  2. ⚠️ THE NEWLINE TRAP -- har beginner ko kaatta hai
    // ============================================================
    std::cout << "\n===== 2. THE NEWLINE TRAP =====\n";
    std::cout << "Poora naam likho: ";

    // `>>` ne '\n' ko BUFFER MEIN CHHOD diya tha.
    // Agar hum seedha getline karein, woh turant us '\n' ko padh lega
    // aur KHALI string dega.
    //
    // Isliye pehle buffer saaf karna padta hai:
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::string fullName;
    std::getline(std::cin, fullName);

    std::cout << "Naam: [" << fullName << "]\n";
    std::cout << "(agar yeh khali hai, ignore() line comment ho gayi hai)\n";

    // ============================================================
    //  3. >> vs getline
    // ============================================================
    std::cout << "\n===== 3. >> vs getline =====\n";
    std::cout << "  cin >> str   -> pehle SPACE pe ruk jaata hai\n";
    std::cout << "  getline(...) -> poori LINE padhta hai\n";
    std::cout << "\n  Input 'Rahul Kumar Sharma' pe:\n";
    std::cout << "    >>      -> \"Rahul\"\n";
    std::cout << "    getline -> \"Rahul Kumar Sharma\"\n";

    // ============================================================
    //  4. MULTIPLE VALUES -- chaining
    // ============================================================
    std::cout << "\n===== 4. MULTIPLE VALUES =====\n";
    std::cout << "Do numbers likho (space ya newline se alag): ";
    int a = 0, b = 0;
    if (std::cin >> a >> b) {
        std::cout << a << " + " << b << " = " << (a + b) << "\n";
    } else {
        std::cout << "Galat input\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    // ============================================================
    //  5. ROBUST INPUT LOOP -- yeh pattern yaad kar lo
    // ============================================================
    std::cout << "\n===== 5. ROBUST INPUT LOOP =====\n";
    int number = 0;
    while (true) {
        std::cout << "Ek number likho (1-100): ";

        if (std::cin >> number) {
            // Parse ho gaya. Ab buffer ka bacha hua kachra bhi saaf karo.
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            if (number >= 1 && number <= 100) {
                std::cout << "✅ Accepted: " << number << "\n";
                break;
            }
            std::cout << "⚠️ Range 1-100 hona chahiye.\n";
            continue;
        }

        // Parse fail hua. Pehle EOF check karo!
        // Bina is check ke, Ctrl+D pe INFINITE LOOP ban jaayega --
        // kyunki EOF ke baad clear() karke bhi kuch nahi milega.
        if (std::cin.eof()) {
            std::cout << "\nInput khatam ho gaya. Bye.\n";
            return 0;
        }

        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "⚠️ Galat input. Sirf number likho.\n";
    }

    std::cout << "\n===== DONE =====\n";
    return 0;
}
