// 04_dangling_view.cpp  --  ismein EK bug hai.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 04_dangling_view.cpp -o t && ./t
// Expected: "first word: hello"   Actual: garbage / empty / crash.
// ============================================================
#include <cstdio>
#include <string>
#include <string_view>

// input string ka pehla shabd lautao
static std::string_view first_word(const std::string& line) {
    std::string trimmed = line.substr(0, line.find(' '));   // <-- local
    return trimmed;                                         // <-- dekho yahan
}

int main() {
    std::string sentence = "hello world foo";
    std::string_view w = first_word(sentence);
    std::printf("first word: %.*s\n", static_cast<int>(w.size()), w.data());
    return 0;
}

// ============================================================
// BUG:     `first_word` ek `std::string_view` return karta jo local
//          `trimmed` ke buffer ko point karta. Function return hote hi
//          `trimmed` destroy -> `w` ab freed/reused memory ko dekh raha
//          (dangling view). `string_view` khud data OWN nahi karta.
// SYMPTOM: Kabhi "hello" (memory abhi overwrite nahi hui), kabhi garbage,
//          kabhi empty, kabhi crash. UB.
// TOOL:    ASan -> "stack-use-after-return" / "heap-use-after-free" jab
//          `w.data()` padha jaata. Clang `-Wdangling` / `-Wreturn-stack-
//          address` kabhi pakadta (yeh case aksar warning se nikal jaata --
//          isi liye khatarnaak). AddressSanitizer sabse pakka.
// FIX:     Owning type return karo:
//            static std::string first_word(const std::string& line) {
//                return line.substr(0, line.find(' '));
//            }
//          Ya agar view hi chahiye: input `line` ka hi substring-view
//          lautao (`std::string_view(line).substr(...)`) -- tab view caller
//          ke live string ko point karta, local ko nahi. Rule: view/span/
//          pointer kabhi bhi apne se chhoti-zindagi wali cheez ko point
//          na kare (13-REFERENCES/09, 10-STRINGS, 22-MODERN-CPP).
// ============================================================
