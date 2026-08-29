// 09_char_demo.cpp
// ============================================================
// char = ek chhota integer
// ============================================================
#include <iostream>
#include <cctype>
#include <climits>
#include <cstdint>

int main() {
    std::cout << "===== CHAR EK NUMBER HAI =====\n";
    char c = 'A';
    std::cout << "'A' as char: " << c << "\n";
    std::cout << "'A' as int:  " << static_cast<int>(c) << "\n";
    std::cout << "sizeof(char) = " << sizeof(char) << " (HAMESHA 1, guaranteed)\n";

    std::cout << "\n===== TEEN JAADUI FACTS =====\n";
    std::cout << "'a' - 'A' = " << ('a' - 'A')
              << "   -> case conversion ka raaz\n";
    std::cout << "'7' - '0' = " << ('7' - '0')
              << "    -> char digit se int digit\n";
    std::cout << "'A' + 1   = " << static_cast<char>('A' + 1)
              << "    -> letters consecutive hain\n";

    std::cout << "\n===== INTEGER PROMOTION =====\n";
    char a = 10, b = 20;
    std::cout << "sizeof(a)     = " << sizeof(a)     << "\n";
    std::cout << "sizeof(a + b) = " << sizeof(a + b)
              << "   <- char + char ka result INT hai!\n";
    std::cout << "'A' + 1 (bina cast) = " << ('A' + 1)
              << "  <- number, character nahi\n";

    std::cout << "\n===== ⚠️ SIGNEDNESS =====\n";
    std::cout << "CHAR_MIN = " << CHAR_MIN << ", CHAR_MAX = " << CHAR_MAX << "\n";
    std::cout << "Is platform pe `char` " << (CHAR_MIN < 0 ? "SIGNED" : "UNSIGNED")
              << " hai\n";
    std::cout << "(x86 pe signed, ARM pe aksar unsigned -- IMPLEMENTATION DEFINED!)\n\n";

    std::cout << "C++ mein TEEN alag char types hain:\n";
    std::cout << "  char           -- signedness implementation-defined\n";
    std::cout << "  signed char    -- guaranteed -128..127\n";
    std::cout << "  unsigned char  -- guaranteed 0..255\n";
    std::cout << "(`char` `signed char` ka alias NAHI hai -- teen alag types)\n";

    std::cout << "\n===== YEH BUG BANATA HAI =====\n";
    // Market data parsing ka classic bug:
    char signedBuf[1]   = { static_cast<char>(0xFF) };
    unsigned char ubuf[1] = { 0xFF };

    std::cout << "Byte 0xFF ko padha:\n";
    std::cout << "  char se:          " << static_cast<int>(signedBuf[0])
              << "   <- GALAT (sign extension)\n";
    std::cout << "  unsigned char se: " << static_cast<int>(ubuf[0])
              << "  <- SAHI\n";
    std::cout << "\nRAW BYTES ke liye HAMESHA uint8_t / unsigned char / std::byte use karo!\n";

    std::cout << "\n===== CLASSIFICATION (<cctype>) =====\n";
    char tests[] = {'A', 'z', '5', ' ', '!', '\n'};
    const char* names[] = {"'A'", "'z'", "'5'", "' '", "'!'", "'\\n'"};

    for (std::size_t i = 0; i < sizeof(tests); ++i) {
        // ⚠️ IMPORTANT: <cctype> functions ko unsigned char cast karke do.
        //    Negative char value pe woh UNDEFINED BEHAVIOUR hai.
        auto uc = static_cast<unsigned char>(tests[i]);
        std::cout << names[i] << " (" << static_cast<int>(uc) << "): ";
        if (std::isalpha(uc)) std::cout << "alpha ";
        if (std::isdigit(uc)) std::cout << "digit ";
        if (std::isspace(uc)) std::cout << "space ";
        if (std::ispunct(uc)) std::cout << "punct ";
        if (std::isupper(uc)) std::cout << "upper ";
        if (std::islower(uc)) std::cout << "lower ";
        if (std::isprint(uc)) std::cout << "printable ";
        std::cout << "\n";
    }

    std::cout << "\n===== CASE CONVERSION =====\n";
    const char* word = "Hello World 123";
    std::cout << "Original:  " << word << "\n";
    std::cout << "Upper:     ";
    for (const char* p = word; *p != '\0'; ++p)
        std::cout << static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
    std::cout << "\nLower:     ";
    for (const char* p = word; *p != '\0'; ++p)
        std::cout << static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
    std::cout << "\n";
    // Dhyaan do: `*p != '\0'` -- string literal ke aakhir mein null terminator hai.
    // Yeh C-string traversal ka classic pattern hai (folder 10 mein detail).

    std::cout << "\n===== PRINTABLE ASCII =====\n";
    for (int i = 32; i < 127; ++i) {
        std::cout << i << "='" << static_cast<char>(i) << "' ";
        if ((i - 31) % 8 == 0) std::cout << "\n";
    }
    std::cout << "\n";

    return 0;
}
