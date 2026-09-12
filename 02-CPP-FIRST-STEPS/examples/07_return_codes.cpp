// 07_return_codes.cpp
// ============================================================
// Exit codes aur program termination
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 07_return_codes.cpp -o retcode
//   ./retcode
//   echo "Exit code: $?"
// ============================================================

#include <iostream>
#include <cstdlib>   // EXIT_SUCCESS, EXIT_FAILURE, std::exit

struct Resource {
    const char* name;
    Resource(const char* n) : name(n) {
        std::cout << "  [+] " << name << " khula\n";
    }
    ~Resource() {
        std::cout << "  [-] " << name << " band hua (destructor)\n";
    }
};

// Global object -- iska destructor `main` ke BAAD chalta hai
Resource globalRes("GLOBAL-RESOURCE");

int main() {
    std::cout << "=== main() shuru ===\n";

    Resource localRes("LOCAL-RESOURCE");

    // ---------------------------------------------------------
    // EXIT CODE KA MATLAB
    // ---------------------------------------------------------
    // Yeh number OPERATING SYSTEM ko jaata hai.
    //
    //    0        = success (sab theek)
    //    non-zero = error (kuch galat hua)
    //
    // Yeh ULTA lagta hai (0 = "false" jaisa), par yeh Unix convention hai:
    //   - success ka sirf EK tareeka hota hai        -> 0
    //   - fail hone ke KAI tareeke hote hain          -> 1, 2, 3, ...
    //
    // Scripts aur build systems isi se decide karte hain ki aage badhein ya nahi:
    //   ./run_tests && ./deploy      <- deploy sirf tab jab tests pass hon
    //   ./build || echo "fail!"      <- message sirf tab jab build fail ho
    //
    // HFT relevance: monitoring systems exit code se pata karte hain ki
    // trading process crash hua ya cleanly band hua. Galat exit code =
    // alert nahi bajega = system silently down.

    std::cout << "\nPortable constants:\n";
    std::cout << "  EXIT_SUCCESS = " << EXIT_SUCCESS << "\n";
    std::cout << "  EXIT_FAILURE = " << EXIT_FAILURE << "\n";

    std::cout << "\nCommon exit codes:\n";
    std::cout << "    0   = success\n";
    std::cout << "    1   = general error\n";
    std::cout << "  127   = command not found\n";
    std::cout << "  130   = Ctrl+C se maara gaya (128 + SIGINT[2])\n";
    std::cout << "  139   = segmentation fault (128 + SIGSEGV[11])\n";

    // ---------------------------------------------------------
    // ⚠️ std::exit() vs return -- BAHUT IMPORTANT FARK
    // ---------------------------------------------------------
    //
    //   return 0;           -> LOCAL objects ke destructors CHALTE hain  ✅
    //   std::exit(0);       -> LOCAL objects ke destructors NAHI chalte  ❌
    //                          (globals ke chalte hain, buffers flush hote hain)
    //   std::quick_exit(0); -> kuch nahi chalta
    //   std::abort();       -> crash, core dump, kuch nahi chalta
    //
    // std::exit() se aapke RAII objects clean nahi honge --
    // files corrupt ho sakti hain, locks release nahi honge,
    // network connections dangling reh sakte hain.
    //
    // TRY KARO: neeche wali line uncomment karo aur output compare karo.
    // Aap dekhoge ki "LOCAL-RESOURCE band hua" print NAHI hoga.
    //
    // std::exit(5);

    std::cout << "\n=== main() se return kar rahe hain ===\n";

    // `return` best hai -- sab destructors chalte hain, sab cleanup hota hai.
    //
    // NOTE: `main` akela function hai jisme `return` OPTIONAL hai.
    //       Agar aap na likho, compiler apne aap `return 0;` laga deta hai.
    return 0;
}
// Yahan LOCAL-RESOURCE marta hai (main ka scope khatam)
// Phir GLOBAL-RESOURCE marta hai (main ke BAAD)
