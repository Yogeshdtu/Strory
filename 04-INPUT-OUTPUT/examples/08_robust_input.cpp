// 08_robust_input.cpp
// ============================================================
// Bulletproof input handling
// ============================================================
// User GALAT input dega. Hamesha. Ya galti se, ya jaan-boojh kar.
// Yeh file dikhati hai ki usse kaise handle karein bina crash ya
// infinite loop ke.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 08_robust_input.cpp -o robust && ./robust
//
// TEST KARO:
//   abc                  <- galat type
//   12abc                <- trailing garbage
//   99999999999999999999 <- overflow
//   (khali line)         <- bas Enter
//   -5                   <- range se bahar
//   Ctrl+D               <- EOF
// ============================================================

#include <iostream>
#include <string>
#include <string_view>
#include <optional>
#include <charconv>
#include <limits>
#include <vector>
#include <cstdint>

// ============================================================
//  APPROACH 1: cin >> ke saath (traditional)
// ============================================================
// Galat input pe DO cheezein karni padti hain:
//   1. clear()  -- error flags saaf karo (warna stream kuch padhega hi nahi)
//   2. ignore() -- galat input buffer se hatao (warna dobara wahi padhega)
//
// Sirf ek karoge to INFINITE LOOP ban jaayega.
std::optional<int> readIntViaStream(std::string_view prompt) {
    while (true) {
        std::cout << prompt;

        int value = 0;
        if (std::cin >> value) {
            // ✅ Parse ho gaya. Ab line ka bacha hua kachra bhi saaf karo,
            //    warna woh agle read ko confuse karega.
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return value;
        }

        // ⚠️ EOF check PEHLE karo!
        // Bina iske Ctrl+D pe INFINITE LOOP hoga -- kyunki EOF ke baad
        // clear() karke bhi kuch nahi milega.
        if (std::cin.eof()) {
            std::cout << "\n(EOF mila)\n";
            return std::nullopt;
        }

        std::cin.clear();                                                    // 1
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');  // 2
        std::cout << "  ⚠️ Galat input. Sirf number likho.\n";
    }
}

// ============================================================
//  APPROACH 2: getline + from_chars (RECOMMENDED)
// ============================================================
// Faayde:
//   - stream KABHI fail state mein nahi jaata
//   - poori line HAMESHA consume hoti hai (koi leftover nahi)
//   - trailing garbage DETECT hota hai ("12abc" reject)
//   - from_chars allocate nahi karta, exception nahi throw karta
//   - sabse tez
std::optional<std::int64_t> parseInt(std::string_view s) {
    // Leading/trailing whitespace trim karo
    // (from_chars whitespace skip NAHI karta)
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back()  == ' ' || s.back()  == '\t' || s.back() == '\r'))
        s.remove_suffix(1);

    if (s.empty()) return std::nullopt;

    std::int64_t value = 0;
    const auto* first = s.data();
    const auto* last  = s.data() + s.size();

    const auto [ptr, ec] = std::from_chars(first, last, value);

    if (ec == std::errc::invalid_argument)    return std::nullopt;  // parse hi nahi hua
    if (ec == std::errc::result_out_of_range) return std::nullopt;  // overflow
    if (ptr != last)                          return std::nullopt;  // trailing garbage

    return value;
}

std::optional<std::int64_t> readIntSafe(std::string_view prompt) {
    std::string line;
    while (true) {
        std::cout << prompt;

        if (!std::getline(std::cin, line)) {
            std::cout << "\n(EOF mila)\n";
            return std::nullopt;
        }

        if (const auto value = parseInt(line)) {
            return value;
        }

        std::cout << "  ⚠️ '" << line << "' valid number nahi hai.\n";
    }
}

// ============================================================
//  Range validation
// ============================================================
std::optional<std::int64_t> readIntInRange(std::string_view prompt,
                                           std::int64_t minVal,
                                           std::int64_t maxVal) {
    while (true) {
        const auto value = readIntSafe(prompt);
        if (!value) return std::nullopt;              // EOF

        if (*value >= minVal && *value <= maxVal) return value;

        std::cout << "  ⚠️ Value " << minVal << " se " << maxVal
                  << " ke beech honi chahiye.\n";
    }
}

// ============================================================
//  Non-empty string
// ============================================================
std::optional<std::string> readNonEmptyLine(std::string_view prompt) {
    std::string line;
    while (true) {
        std::cout << prompt;
        if (!std::getline(std::cin, line)) return std::nullopt;

        // Trailing \r hata do (Windows files / terminals ke liye)
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.find_first_not_of(" \t") != std::string::npos) return line;

        std::cout << "  ⚠️ Khali nahi ho sakta.\n";
    }
}

int main() {
    std::cout << "══════════════════════════════════════════════════\n";
    std::cout << "  ROBUST INPUT DEMO\n";
    std::cout << "══════════════════════════════════════════════════\n";
    std::cout << "Yeh test karo:\n";
    std::cout << "  abc                   <- galat type\n";
    std::cout << "  12abc                 <- trailing garbage\n";
    std::cout << "  99999999999999999999  <- overflow\n";
    std::cout << "  (khali line)          <- bas Enter\n";
    std::cout << "  Ctrl+D                <- EOF\n";
    std::cout << "══════════════════════════════════════════════════\n\n";

    // ============================================================
    //  parseInt() ka offline test -- bina user input ke
    // ============================================================
    std::cout << "===== parseInt() TEST CASES =====\n";
    const std::vector<std::string> tests = {
        "42", "-17", "0", "  42  ",
        "abc", "12abc", "", "  ", "3.14",
        "99999999999999999999",           // overflow
        "9223372036854775807",            // int64 max -- valid
    };

    for (const auto& t : tests) {
        const auto r = parseInt(t);
        std::cout << "  " << (r ? "✅" : "❌") << "  [" << t << "]"
                  << std::string(t.size() < 24 ? 24 - t.size() : 1, ' ')
                  << "-> ";
        if (r) std::cout << *r << "\n";
        else   std::cout << "REJECTED\n";
    }

    // ============================================================
    //  Interactive
    // ============================================================
    std::cout << "\n===== APPROACH 1: cin >> (traditional) =====\n";
    if (const auto v = readIntViaStream("Ek number likho: ")) {
        std::cout << "  ✅ Mila: " << *v << "\n";
    } else {
        std::cout << "  Input khatam. Bye.\n";
        return 0;
    }

    std::cout << "\n===== APPROACH 2: getline + from_chars =====\n";
    if (const auto v = readIntSafe("Ek aur number: ")) {
        std::cout << "  ✅ Mila: " << *v << "\n";
    } else {
        std::cout << "  Input khatam. Bye.\n";
        return 0;
    }

    std::cout << "\n===== RANGE VALIDATION =====\n";
    if (const auto v = readIntInRange("Umar (1-120): ", 1, 120)) {
        std::cout << "  ✅ Umar: " << *v << "\n";
    } else {
        std::cout << "  Input khatam. Bye.\n";
        return 0;
    }

    std::cout << "\n===== NON-EMPTY STRING =====\n";
    if (const auto s = readNonEmptyLine("Aapka naam: ")) {
        std::cout << "  ✅ Namaste, " << *s << "!\n";
    } else {
        std::cout << "  Input khatam. Bye.\n";
        return 0;
    }

    std::cout << "\n══════════════════════════════════════════════════\n";
    std::cout << "KYA SEEKHA:\n";
    std::cout << "  1. Galat input pe stream FAIL state mein jaata hai\n";
    std::cout << "  2. Recovery ke liye clear() AUR ignore() -- dono chahiye\n";
    std::cout << "  3. EOF check zaroori hai, warna infinite loop\n";
    std::cout << "  4. getline + from_chars zyada robust hai:\n";
    std::cout << "       - stream kabhi fail nahi hota\n";
    std::cout << "       - trailing garbage pakda jaata hai\n";
    std::cout << "       - no allocation, no exceptions (HFT-friendly)\n";
    std::cout << "  5. stoi() \"12abc\" ko 12 maan leta hai -- from_chars nahi\n";
    std::cout << "══════════════════════════════════════════════════\n";

    return 0;
}
