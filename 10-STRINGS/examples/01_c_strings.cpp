// 01_c_strings.cpp
// ============================================================
// C-strings -- char array + null terminator '\0', aur unke dangers
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_c_strings.cpp -o cs && ./cs
// ============================================================
// C-string = char array jismein aakhir mein '\0' (value 0) ho.
// String ki "length" = pehla '\0' milne tak ke chars.
// Sab <cstring> functions '\0' pe rukte hain -- agar '\0' na ho -> OOB.
// ============================================================

#include <cstring>      // strlen, strcpy, strncpy, strcmp, strcat
#include <iostream>

int main() {
    // ============================================================
    //  1. Char array + hidden '\0'
    // ============================================================
    std::cout << "===== 1. layout =====\n";
    char s[] = "hi";                 // {'h', 'i', '\0'} -- size 3, NOT 2!
    std::cout << "  \"hi\" ka array size = " << sizeof(s) << "  (h, i, aur '\\0')\n";
    std::cout << "  strlen(\"hi\")        = " << std::strlen(s) << "  ('\\0' count nahi hota)\n";

    for (std::size_t i = 0; i < sizeof(s); ++i)
        std::cout << "  s[" << i << "] = " << static_cast<int>(s[i])
                  << (s[i] == '\0' ? "  <- '\\0'\n" : "\n");

    // ============================================================
    //  2. strlen -- '\0' dhoondhta hai (O(n), har call pe scan)
    // ============================================================
    std::cout << "\n===== 2. strlen scans =====\n";
    const char* msg = "hello world";
    std::cout << "  strlen(\"hello world\") = " << std::strlen(msg)
              << "   (11 chars scan kiye)\n";
    std::cout << "  ⚠️ loop condition mein strlen mat likho -> O(n^2)\n";

    // ============================================================
    //  3. ⚠️ BUFFER OVERFLOW -- strcpy destination size check nahi karta
    // ============================================================
    std::cout << "\n===== 3. ⚠️ buffer overflow risks =====\n";
    char small[8];
    std::strcpy(small, "1234567");            // 7 chars + '\0' = 8 -> theek (bilkul fit)
    std::cout << "  strcpy(small[8], \"1234567\") -> \"" << small << "\"  (bilkul fit)\n";
    // std::strcpy(small, "this is way too long");   // ⚠️ OVERFLOW -- stack corrupt / crash

    // safer: strncpy -- par woh bhi '\0' guarantee nahi karta agar src poora bhar de
    char dst[8];
    std::strncpy(dst, "abcdefghij", sizeof(dst) - 1);
    dst[sizeof(dst) - 1] = '\0';               // ⚠️ MANUALLY '\0' lagana padta hai
    std::cout << "  strncpy + manual '\\0'      -> \"" << dst << "\"  (truncated, safe)\n";
    std::cout << "  RULE: C-strings mein har copy pe destination size khud track karo.\n";

    // ============================================================
    //  4. strcmp -- content compare (== pointers compare karta hai!)
    // ============================================================
    std::cout << "\n===== 4. compare =====\n";
    const char* a = "apple";
    const char* b = "apple";
    std::cout << "  (a == b)          -> " << (a == b ? "true" : "false")
              << "   <- POINTERS (literal merge ho sakta, guarantee nahi)\n";
    std::cout << "  strcmp(a, b) == 0 -> " << (std::strcmp(a, b) == 0 ? "true" : "false")
              << "   <- content\n";
    std::cout << "  strcmp(\"apple\", \"banana\") = " << std::strcmp("apple", "banana")
              << "   (negative -> apple < banana)\n";

    // ============================================================
    //  5. ⚠️ Missing '\0' -- undefined behaviour
    // ============================================================
    std::cout << "\n===== 5. ⚠️ missing terminator =====\n";
    char noTerm[3] = {'a', 'b', 'c'};          // NO '\0' -- yeh C-string NAHI hai
    std::cout << "  char x[3] = {'a','b','c'};  strlen/cout ispe -> OOB read\n";
    std::cout << "  (yahan print nahi kar rahe -- UB hota)\n";
    (void)noTerm;

    // ============================================================
    //  MORAL
    // ============================================================
    std::cout <<
        "\n-----------------------------------------------\n"
        "C-strings: fast, allocation-free, par MANUAL. Har operation mein\n"
        "  * destination buffer ka size khud check karo (overflow!)\n"
        "  * '\\0' terminator ensure karo\n"
        "  * strcmp/strncmp content compare ke liye (== nahi)\n"
        "  * strlen O(n) hai -- cache karo, loop condition mein mat\n"
        "Modern C++: std::string (owns + grows) ya std::string_view (borrow).\n"
        "C-strings sirf C APIs, embedded, ya jab har byte matter kare.\n";

    return 0;
}
