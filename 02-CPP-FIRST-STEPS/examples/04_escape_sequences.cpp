// 04_escape_sequences.cpp
// ============================================================
// Escape sequences aur string literals
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 04_escape_sequences.cpp -o esc && ./esc
// ============================================================

#include <iostream>

int main() {
    std::cout << "========== NEWLINE (\\n) ==========\n";
    // `\n` EK character hai (ASCII 10), do nahi. Backslash agle character
    // ka matlab badal deta hai -- isko "escape sequence" kehte hain.
    std::cout << "Pehli line\nDoosri line\nTeesri line\n";

    std::cout << "\n========== TAB (\\t) ==========\n";
    // `\t` = ASCII 9. Yeh agle TAB STOP tak jaata hai (usually har 8 columns).
    // Isliye alignment tab tak hi kaam karti hai jab tak text chhota ho.
    std::cout << "Naam\tUmar\tCity\n";
    std::cout << "Ram\t25\tDelhi\n";
    std::cout << "Priyanka\t30\tMumbai\n";
    std::cout << "^^ dhyaan do: 'Priyanka' lamba hai isliye alignment bigda\n";
    std::cout << "   (proper alignment ke liye <iomanip> ka setw -- folder 04)\n";

    std::cout << "\n========== QUOTES ==========\n";
    // Double quote ko string ke andar likhne ke liye escape karna padta hai,
    // warna compiler samajhta hai ki string yahin khatam ho gayi.
    std::cout << "Usne kaha \"Namaste\"\n";
    std::cout << "Single quote: \' (yahan escape optional tha)\n";

    std::cout << "\n========== BACKSLASH (\\\\) ==========\n";
    // Ek actual backslash likhne ke liye do backslash chahiye.
    // Yeh Windows paths ka classic problem hai.
    std::cout << "Windows path: C:\\Users\\Rahul\\Documents\n";
    std::cout << "Regex: \\d{3}-\\d{4}\n";

    std::cout << "\n========== RAW STRINGS (C++11) ==========\n";
    // R"(...)"  -- iske andar KUCH bhi escape nahi hota. Jo likha, wahi.
    // Yeh paths, regex, JSON, SQL ke liye life-saver hai.
    std::cout << R"(Windows path: C:\Users\Rahul\Documents)" << "\n";
    std::cout << R"(Regex: \d{3}-\d{4})" << "\n";
    std::cout << R"(Yahan \n bhi literally dikhta hai, newline nahi banta)" << "\n";

    std::cout << "\n========== MULTI-LINE RAW STRING ==========\n";
    // Raw string mein actual newlines bhi as-is aate hain.
    std::cout << R"({
    "symbol": "NIFTY",
    "price": 21500,
    "path": "C:\data\feed.log"
})" << "\n";

    std::cout << "\n========== SIZES (null terminator!) ==========\n";
    // Har string literal ke aakhir mein compiler automatic `\0` lagata hai.
    // Isliye sizeof hamesha visible characters + 1 hota hai.
    std::cout << "sizeof(\"\")      = " << sizeof("")      << "  (sirf \\0)\n";
    std::cout << "sizeof(\"A\")     = " << sizeof("A")     << "  ('A' + \\0)\n";
    std::cout << "sizeof(\"Hello\") = " << sizeof("Hello") << "  (5 chars + \\0)\n";
    std::cout << "sizeof(\"a\\n\")   = " << sizeof("a\n")   << "  (\\n EK char hai!)\n";
    std::cout << "sizeof(\"\\\\\")     = " << sizeof("\\")   << "  (backslash bhi ek char)\n";
    std::cout << "sizeof('A')     = " << sizeof('A')     << "  (char literal, string nahi)\n";

    std::cout << "\n========== 'A' vs \"A\" ==========\n";
    // 'A'  -> CHARACTER literal, type char, 1 byte
    // "A"  -> STRING literal, type const char[2], 2 bytes ('A' aur '\0')
    // Yeh bilkul alag cheezein hain!
    char c = 'A';                 // ✅ theek hai
    // char bad = "A";            // ❌ error: cannot convert const char* to char
    const char* s = "A";          // ✅ pointer to string literal

    std::cout << "'A' as char: " << c << "\n";
    std::cout << "\"A\" as string: " << s << "\n";

    std::cout << "\n========== CHARACTERS ARE NUMBERS ==========\n";
    // Har char actually ek chhota integer hai (ASCII value).
    std::cout << "'A' ka number: " << static_cast<int>('A') << "\n";
    std::cout << "'a' ka number: " << static_cast<int>('a') << "\n";
    std::cout << "'0' ka number: " << static_cast<int>('0') << "\n";
    std::cout << "'a' - 'A'    = " << ('a' - 'A')
              << "  <- case conversion ka raaz\n";
    std::cout << "'7' - '0'    = " << ('7' - '0')
              << "  <- char ko digit banane ka raaz\n";

    // Dhyaan do: `c + 1` ka result INT hai, char nahi.
    // Kyunki char arithmetic mein int mein "promote" ho jaata hai.
    // (Integer promotion -- folder 03 mein detail.)
    std::cout << "'A' + 1 as int:  " << ('A' + 1) << "\n";
    std::cout << "'A' + 1 as char: " << static_cast<char>('A' + 1) << "\n";

    std::cout << "\n========== HEX / OCTAL ESCAPES ==========\n";
    // \xHH = hex escape, \NNN = octal escape
    std::cout << "\x48\x65\x6C\x6C\x6F" << "  <- hex escapes se 'Hello'\n";
    std::cout << "\110\145\154\154\157" << "  <- octal escapes se 'Hello'\n";

    std::cout << "\n========== ADJACENT CONCATENATION ==========\n";
    // Do string literals agar side-by-side hon, compiler unhe jod deta hai.
    // Lambi strings todne ke liye kaam aata hai.
    std::cout << "Yeh " "ek " "hi " "string " "hai\n";
    std::cout << "Ek bahut lambi line ko "
                 "kai lines mein todna "
                 "padhne mein aasan hai.\n";

    std::cout << "\n========== \\n vs std::endl ==========\n";
    // `\n`         -> sirf newline character daalta hai        [FAST]
    // `std::endl`  -> newline + BUFFER FLUSH (ek syscall!)     [SLOW]
    //
    // 99% cases mein `\n` use karo.
    // `endl` sirf tab jab aapko output turant chahiye (e.g. crash se pehle).
    //
    // HFT relevance: logging hot path mein `endl` disaster hai --
    // har line pe ek write() syscall (~500ns+). Isliye HFT mein logging
    // asynchronous hoti hai. Folder 41 mein detail.
    std::cout << "Yeh \\n se aayi\n";
    std::cout << "Yeh endl se aayi" << std::endl;
    std::cout << "(output same dikhta hai, par endl ne extra syscall kiya)\n";

    return 0;
}
