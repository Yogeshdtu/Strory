// 04_string_view.cpp
// ============================================================
// std::string_view -- non-owning read-only VIEW into char data
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 04_string_view.cpp -o sv && ./sv
// ============================================================
// string_view = { const char* ptr; size_t len; }  -- 16 bytes, no ownership,
// NO copy, NO allocation. std::string ka span<const char> equivalent.
// Read-only string PARAMETERS ke liye best. Par dangling ka dhyaan.
// ============================================================

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

// EK function -- literal, std::string, char*, substring -- sab bina copy
std::size_t countVowels(std::string_view s) {
    std::size_t n = 0;
    for (char c : s)
        if (std::string_view("aeiouAEIOU").find(c) != std::string_view::npos) ++n;
    return n;
}

// tokenizer -- input string ke andar VIEWS return karta hai (zero copy)
std::vector<std::string_view> splitView(std::string_view s, char delim) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    while (start <= s.size()) {
        std::size_t pos = s.find(delim, start);
        if (pos == std::string_view::npos) { out.push_back(s.substr(start)); break; }
        out.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

int main() {
    // ============================================================
    //  1. Ek function, ki sources -- koi copy nahi
    // ============================================================
    std::cout << "===== 1. no-copy parameter =====\n";
    const char* cstr = "programming";
    std::string str = "hello world";
    std::cout << "  countVowels(\"programming\") = " << countVowels(cstr) << "\n";
    std::cout << "  countVowels(std::string)    = " << countVowels(str) << "\n";
    std::cout << "  countVowels(str.substr...)  = " << countVowels(std::string_view(str).substr(0, 5)) << "\n";
    std::cout << "  countVowels(\"literal\")      = " << countVowels("literal") << "\n";

    // ============================================================
    //  2. substr / remove_prefix / remove_suffix -- sab VIEWS
    // ============================================================
    std::cout << "\n===== 2. cheap slicing =====\n";
    std::string_view sv = "  [payload]  ";
    std::cout << "  original     : \"" << sv << "\" (" << sv.size() << ")\n";
    sv.remove_prefix(sv.find_first_not_of(' '));
    sv.remove_suffix(sv.size() - sv.find_last_not_of(' ') - 1);
    std::cout << "  trimmed      : \"" << sv << "\" (" << sv.size() << ")\n";
    std::cout << "  substr(1, 7) : \"" << sv.substr(1, 7) << "\"   (no allocation)\n";

    // ============================================================
    //  3. Tokenize -- zero-copy split
    // ============================================================
    std::cout << "\n===== 3. zero-copy split =====\n";
    std::string csv = "AAPL,192.34,1000,BUY";
    for (std::string_view field : splitView(csv, ','))
        std::cout << "  field: \"" << field << "\"\n";
    std::cout << "  (4 views into `csv` -- 0 new strings allocated)\n";

    // ============================================================
    //  4. ⚠️ DANGLING -- #1 string_view bug
    // ============================================================
    std::cout << "\n===== 4. ⚠️ dangling traps =====\n";
    // Dhyaan: GCC 16.2 `-Wall -Wextra` in dangling patterns pe KOI warning nahi deta (chala ke
    // dekha, lesson 09). Isliye (a)/(b) comment mein hain aur (c) ka read comment kiya hai --
    // chalane pe UB hota. Linux pe ASan (-fsanitize=address) inhe runtime pe pakadta hai.

    //  (a) temporary ka view
    // std::string_view bad = std::string("temp") + "!";   // ⚠️ temp gone after ;
    std::cout << "  (a) sv = std::string(\"x\") + \"y\";  -> temp destroyed, sv dangling\n";

    //  (b) function jo apne local ka view lautaye
    // std::string_view f() { std::string local = "hi"; return local; }  // ⚠️
    std::cout << "  (b) return string_view of a local std::string -> dangling\n";

    //  (c) view string se zyada jee gaya
    std::string_view later;
    {
        std::string temp = "block-scoped";
        later = temp;
    }   // temp destroyed
    // std::cout << later;   // ⚠️ UB
    std::cout << "  (c) view assigned inside a block, used after -> dangling\n";

    //  (d) .data() null-terminated NAHI hota (dangling nahi, par C API ke saath utna hi khatarnak)
    std::string_view piece = std::string_view("hello world").substr(0, 5);  // "hello"
    std::cout << "  (d) piece.data() -> points to 'h' but NEXT char is ' ', not '\\0'.\n"
                 "      C API ko piece.data() mat do -- std::string(piece).c_str() use karo.\n";
    (void)piece;

    std::cout <<
        "\n  RULE: string_view apne SOURCE se zyada zinda nahi rehna chahiye.\n"
        "  Function PARAMETER: safe (caller ka data call ke doraan zinda).\n"
        "  MEMBER / stored / returned: soch-samajh ke (ownership kaun rakhta hai?).\n"
        "  Null-terminator ki zaroorat -> std::string(sv), string_view nahi.\n";

    return 0;
}
