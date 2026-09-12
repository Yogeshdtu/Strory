// 08_namespaces.cpp
// ============================================================
// Apne namespaces banana: naam ki takkar rokna, nesting, alias,
// using-declaration vs using-directive, anonymous aur inline namespace,
// aur ADL (argument-dependent lookup) -- sab ek program mein.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g 08_namespaces.cpp -o ns && ./ns
// ============================================================

#include <iostream>
#include <sstream>
#include <string>

// ============================================================
//  1. DO NAMESPACE, EK HI FUNCTION NAAM -- koi takkar nahi
// ============================================================
// Do exchanges ke lot size alag hain. Dono function ka naam `lot_size` hai,
// par alag namespace mein hain, isliye compiler ke liye yeh do alag functions hain.
namespace nse {
    int lot_size() { return 50; }
}

namespace bse {
    int lot_size() { return 25; }
}

// ============================================================
//  2. NAMESPACE DOBARA KHOLNA -- ek namespace kai jagah likha ja sakta hai
// ============================================================
// `nse` upar ban chuka hai. Yahan phir se khola aur ek naya function joda.
// Bade projects mein ek hi namespace kai files mein faila hota hai (std bhi aisa hi hai).
namespace nse {
    int tick_paise() { return 5; }
}

// ============================================================
//  3. NESTED NAMESPACE -- C++17 ka chhota syntax
// ============================================================
// Purana tareeka:  namespace exch { namespace nse { namespace fo { ... } } }
// C++17 se:        namespace exch::nse::fo { ... }
namespace exch::nse::fo {
    int expiry_day() { return 4; }   // Thursday = 4 (0 = Sunday)
}

// ============================================================
//  4. GLOBAL NAMESPACE aur `::`
// ============================================================
int limit = 1000;                    // global namespace mein

// ============================================================
//  5. ANONYMOUS (UNNAMED) NAMESPACE -- sirf is file ke liye
// ============================================================
// Iske andar ki cheezein is .cpp file ke bahar dikhti hi nahi (internal linkage).
// Doosri file mein same naam ka helper ho, to linker error nahi aayega.
// Poori detail: folder 24 file 05 (linkage).
namespace {
    int file_private_counter = 0;
    void bump() { ++file_private_counter; }
}

// ============================================================
//  6. INLINE NAMESPACE -- versioning
// ============================================================
// `inline namespace v2` ke members seedha `api::` se bhi mil jaate hain.
// Purana version `api::v1::` likh ke abhi bhi mil sakta hai.
// Libraries isse ABI versioning karti hain (folder 25 file 14).
namespace api {
    namespace v1 {
        int version() { return 1; }
    }
    inline namespace v2 {
        int version() { return 2; }
    }
}

int main() {
    // ============================================================
    //  1. qualified naam -- namespace::function
    // ============================================================
    std::cout << "== 1. same naam, alag namespace ==\n";
    std::cout << "  nse::lot_size() = " << nse::lot_size() << "\n";
    std::cout << "  bse::lot_size() = " << bse::lot_size() << "\n";

    // ============================================================
    //  2. reopened namespace ka naya member
    // ============================================================
    std::cout << "\n== 2. reopened namespace ==\n";
    std::cout << "  nse::tick_paise() = " << nse::tick_paise() << "\n";

    // ============================================================
    //  3. nested + ALIAS -- lamba naam chhota karo
    // ============================================================
    std::cout << "\n== 3. nested namespace + alias ==\n";
    std::cout << "  exch::nse::fo::expiry_day() = " << exch::nse::fo::expiry_day() << "\n";
    namespace fo = exch::nse::fo;    // alias: sirf naya naam, koi copy nahi
    std::cout << "  fo::expiry_day()            = " << fo::expiry_day() << "\n";

    // ============================================================
    //  4. `::` se global cheez -- jab local naam usse chhupa de
    // ============================================================
    std::cout << "\n== 4. global :: ==\n";
    int limit = 10;                  // ⚠️ -Wshadow warning JAAN-BOOJH KAR: local `limit` ne global ko chhupa diya
    std::cout << "  limit   (local)  = " << limit << "\n";
    std::cout << "  ::limit (global) = " << ::limit << "\n";

    // ============================================================
    //  5. USING-DECLARATION vs USING-DIRECTIVE -- dono SCOPE ke andar
    // ============================================================
    std::cout << "\n== 5. using-declaration vs using-directive ==\n";
    {
        using nse::lot_size;         // DECLARATION: sirf EK naam laaya
        std::cout << "  using nse::lot_size;   lot_size() = " << lot_size() << "\n";
    }                                // yahan scope khatam -> `lot_size` ab phir se unknown
    {
        using namespace bse;         // DIRECTIVE: bse ke SAARE naam dikhne lage
        std::cout << "  using namespace bse;   lot_size() = " << lot_size() << "\n";
    }
    // Agar ek hi scope mein `using namespace nse;` aur `using namespace bse;` dono likho
    // aur phir `lot_size()` call karo:
    //     error: call of overloaded 'lot_size()' is ambiguous
    // Isliye using-directive ko chhote scope mein rakho, aur header file mein KABHI mat likho.

    // ============================================================
    //  6. anonymous namespace ki cheez -- is file mein normal naam jaisi
    // ============================================================
    std::cout << "\n== 6. anonymous namespace ==\n";
    bump();
    bump();
    std::cout << "  file_private_counter = " << file_private_counter << "\n";

    // ============================================================
    //  7. inline namespace -- default version
    // ============================================================
    std::cout << "\n== 7. inline namespace ==\n";
    std::cout << "  api::version()     = " << api::version() << "   (inline v2)\n";
    std::cout << "  api::v1::version() = " << api::v1::version() << "\n";
    std::cout << "  api::v2::version() = " << api::v2::version() << "\n";

    // ============================================================
    //  8. ADL -- argument-dependent lookup
    // ============================================================
    // Neeche `getline` ke aage `std::` NAHI likha, phir bhi compile hota hai. Kyun?
    // Argument `in` ka type std::istringstream hai -> woh `std` namespace mein hai ->
    // compiler `getline` ko `std` mein bhi dhoondhta hai. Isi ko ADL kehte hain.
    std::cout << "\n== 8. ADL ==\n";
    std::istringstream in("BUY 100 RELIANCE");
    std::string side;
    getline(in, side, ' ');          // ADL ne std::getline dhoondha
    std::cout << "  getline(in, side, ' ') -> side = \"" << side << "\"\n";
    // ADL HAR argument ka type dekhta hai. `getline(some_int, side)` likhoge to bhi
    // `side` (std::string) ki wajah se std::getline MIL jaayega -- phir error aayega
    // "no matching function", "not declared" nahi. Error message padh ke farq samjho.

    // `std::cout << side` bhi ADL hi hai: string ke liye `operator<<` ek free function hai
    // jo `std` mein rehta hai. Neeche wahi call bina operator syntax ke:
    std::string sym = "RELIANCE";
    std::operator<<(std::cout, "  explicit std::operator<< call: ");
    std::operator<<(std::cout, sym) << "\n";
}
