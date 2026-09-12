// 01_exceptions_basics.cpp
// ============================================================
// throw / try / catch ka poora mechanics: catch order, catch-by-ref,
// rethrow, what(), aur throw hote hi stack unwinding (dtors chalte hain).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow 01_exceptions_basics.cpp -o eb && ./eb
// ============================================================

#include <cstdio>
#include <stdexcept>   // std::runtime_error, std::out_of_range, std::logic_error
#include <string>
#include <exception>   // std::exception, std::current_exception, std::rethrow_exception

// ============================================================
//  1. Ek chhota RAII guard — sirf yeh dikhane ke liye ki throw
//     ke waqt destructor DETERMINISTICALLY chalta hai (unwinding).
// ============================================================
struct Guard {
    std::string name;
    explicit Guard(std::string n) : name(std::move(n)) {
        std::printf("      Guard(%s) banaya\n", name.c_str());
    }
    ~Guard() {
        std::printf("      ~Guard(%s) — scope chhoda (unwinding ya normal)\n", name.c_str());
    }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
};

// ============================================================
//  2. Apni exception type — std::runtime_error se derive.
//     what() ready-made milta hai (message store ho jaata hai).
// ============================================================
struct PriceError : std::runtime_error {
    long bad_price;
    PriceError(const std::string& msg, long p)
        : std::runtime_error(msg), bad_price(p) {}
};

// deep() throw karta hai. Beech mein Guard objects hain — unke dtors
// throw ke raaste mein chalenge.
static void deep(int level) {
    Guard g("deep-level-" + std::to_string(level));
    if (level == 0) {
        std::printf("      deep(0): ab throw karte hain\n");
        throw PriceError("price band se bahar", 10123);
    }
    deep(level - 1);
    std::printf("      deep(%d): yeh line NAHI chhpegi (throw ne skip kiya)\n", level);
}

int main() {
    // --------------------------------------------------------
    //  A. Basic try / catch — catch by CONST REFERENCE (hamesha).
    //     By value karoge to slicing + ek extra copy.
    // --------------------------------------------------------
    std::puts("A. basic throw/catch:");
    try {
        throw std::out_of_range("index 7 >= size 3");
    } catch (const std::out_of_range& e) {
        std::printf("   pakda: out_of_range: %s\n", e.what());
    }

    // --------------------------------------------------------
    //  B. Catch order — DERIVED pehle, BASE baad mein.
    //     Agar base (std::exception) upar likh dein to derived
    //     handlers kabhi nahi chalenge (-Wexception warn bhi karta).
    // --------------------------------------------------------
    std::puts("\nB. catch order (derived before base):");
    try {
        throw PriceError("tick size galat", 42);
    } catch (const PriceError& e) {                 // <-- sabse specific
        std::printf("   PriceError: %s (bad_price=%ld)\n", e.what(), e.bad_price);
    } catch (const std::runtime_error& e) {         // yeh bhi match karta, par upar wala jeeta
        std::printf("   runtime_error: %s\n", e.what());
    } catch (const std::exception& e) {             // sabse general
        std::printf("   exception: %s\n", e.what());
    }

    // --------------------------------------------------------
    //  C. Stack unwinding — throw se catch tak har local object ka
    //     destructor ULTE order mein chalta hai. Yahi RAII ka dil hai.
    // --------------------------------------------------------
    std::puts("\nC. unwinding — deep(3) throw karega, saare ~Guard chalenge:");
    try {
        deep(3);
    } catch (const PriceError& e) {
        std::printf("   main ne pakda: %s (bad_price=%ld)\n", e.what(), e.bad_price);
    }

    // --------------------------------------------------------
    //  D. Rethrow — `throw;` (bina argument) CURRENT exception ko
    //     aage bhejta hai (copy nahi, wahi object). Logging + rethrow
    //     ke liye classic pattern.
    // --------------------------------------------------------
    std::puts("\nD. rethrow:");
    try {
        try {
            throw std::logic_error("invariant toota");
        } catch (const std::exception& e) {
            std::printf("   inner: log kiya (%s), ab rethrow\n", e.what());
            throw;                                   // <-- wahi exception, aage
        }
    } catch (const std::logic_error& e) {
        std::printf("   outer: dobara pakda: %s\n", e.what());
    }

    // --------------------------------------------------------
    //  E. catch-all `...` — type nahi pata / C library callback /
    //     top-level safety net. e ka access nahi milta yahan.
    // --------------------------------------------------------
    std::puts("\nE. catch-all:");
    try {
        throw 42;                                   // int throw karna legal hai (par mat karo)
    } catch (const std::exception&) {
        std::puts("   exception branch (yeh nahi)");
    } catch (...) {
        std::puts("   catch(...) ne pakda — type unknown");
    }

    // --------------------------------------------------------
    //  F. std::current_exception / rethrow_exception — exception ko
    //     ek variable (std::exception_ptr) mein "pakad ke rakho",
    //     baad mein / doosre thread pe rethrow karo.
    // --------------------------------------------------------
    std::puts("\nF. exception_ptr (store now, rethrow later):");
    std::exception_ptr saved;
    try {
        throw std::runtime_error("baad mein handle karna");
    } catch (...) {
        saved = std::current_exception();           // type-erased pakad
        std::puts("   exception ko exception_ptr mein rakh diya");
    }
    // ... kahin aur, baad mein ...
    try {
        if (saved) std::rethrow_exception(saved);
    } catch (const std::exception& e) {
        std::printf("   restore karke pakda: %s\n", e.what());
    }

    std::puts("\nmain normally return kar raha hai (exit 0).");
    return 0;
}

/*
Expected output (shape):

A. basic throw/catch:
   pakda: out_of_range: index 7 >= size 3

B. catch order (derived before base):
   PriceError: tick size galat (bad_price=42)

C. unwinding — deep(3) throw karega, saare ~Guard chalenge:
      Guard(deep-level-3) banaya
      Guard(deep-level-2) banaya
      Guard(deep-level-1) banaya
      Guard(deep-level-0) banaya
      deep(0): ab throw karte hain
      ~Guard(deep-level-0) — scope chhoda (unwinding ya normal)
      ~Guard(deep-level-1) — scope chhoda (unwinding ya normal)
      ~Guard(deep-level-2) — scope chhoda (unwinding ya normal)
      ~Guard(deep-level-3) — scope chhoda (unwinding ya normal)
      main ne pakda: price band se bahar (bad_price=10123)
   ...
*/
