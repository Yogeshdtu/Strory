// 03_raii_unwinding.cpp
// ============================================================
// Stack unwinding + RAII: throw hote hi har fully-constructed local ka
// dtor ULTE order mein chalta hai. Isliye lock/file/alloc RAII se safe.
// Plus: ctor ke beech throw (partially-constructed object ka dtor NAHI),
// aur dtor-throws-during-unwinding => std::terminate (guarded demo).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow 03_raii_unwinding.cpp -o ru && ./ru
// ============================================================

#include <cstdio>
#include <string>
#include <stdexcept>
#include <exception>   // std::uncaught_exceptions

// ---- ek generic "resource" jo ctor/dtor pe log karta hai ----
struct Resource {
    std::string tag;
    explicit Resource(std::string t) : tag(std::move(t)) {
        std::printf("  + acquire  %s\n", tag.c_str());
    }
    ~Resource() {
        // std::uncaught_exceptions() > 0 => hum unwinding ke beech mein hain
        std::printf("  - release  %s%s\n", tag.c_str(),
                    std::uncaught_exceptions() > 0 ? "   (unwinding ke dauraan)" : "");
    }
    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;
};

// ---- manual (RAII ke bina) — throw pe cleanup CHHOOT jaata hai ----
static void manual_cleanup_bad() {
    std::puts("\n[manual, RAII ke bina]");
    char* buf = new char[64];              // raw alloc
    std::puts("  malloc'd buf");
    if (true) throw std::runtime_error("beech mein error");
    delete[] buf;                          // <-- yeh line kabhi nahi chalti => LEAK
    std::puts("  freed buf (never printed)");
}

// ---- RAII wale objects ke saath — throw pe sab release ----
static void raii_good() {
    std::puts("\n[RAII ke saath — nested scopes]");
    Resource file("market_data.log");
    {
        Resource lock("book_mutex");
        Resource mem("order_pool_page");
        std::puts("  ... kaam karte hue error aa gaya ...");
        throw std::runtime_error("parse fail");
        // yahan se aage kuch nahi; mem, lock, file — teenon dtors chalenge
    }
}

// ---- ctor ke beech throw: jo members BAN chuke unke dtors chalte hain,
//      par object KHUD "bana hi nahi" isliye uska dtor NAHI chalta ----
struct HalfBuilt {
    Resource a;
    Resource b;   // <-- iska ctor throw karega
    Resource c;   // yeh kabhi construct nahi hoga
    HalfBuilt() : a("member-a"), b(make_b()), c("member-c") {}
    static std::string make_b() { throw std::runtime_error("member b ka ctor fail"); }
};

// ---- dtor jo throw karta hai: agar yeh UNWINDING ke beech chala to
//      std::terminate. Default OFF rakha hai (arg do to on).
struct DangerousDtor {
    bool armed;
    explicit DangerousDtor(bool a) : armed(a) {}
    ~DangerousDtor() noexcept(false) {
        if (armed && std::uncaught_exceptions() > 0) {
            std::puts("  ~DangerousDtor: unwinding ke beech throw... => std::terminate");
            throw std::runtime_error("dtor threw during unwinding");
        }
    }
};

int main() {
    // --------------------------------------------------------
    //  1. manual cleanup — leak
    // --------------------------------------------------------
    try { manual_cleanup_bad(); }
    catch (const std::exception& e) { std::printf("  caught: %s  (buf LEAK ho gaya)\n", e.what()); }

    // --------------------------------------------------------
    //  2. RAII — throw pe deterministic release, ulte order mein
    // --------------------------------------------------------
    try { raii_good(); }
    catch (const std::exception& e) { std::printf("  caught: %s\n", e.what()); }
    std::puts("  ^ note: release order = mem, lock, file (construction ka ulta)");

    // --------------------------------------------------------
    //  3. ctor ke beech throw — sirf constructed members release hote
    // --------------------------------------------------------
    std::puts("\n[ctor ke beech throw]");
    try {
        HalfBuilt hb;
        (void)hb;
    } catch (const std::exception& e) {
        std::printf("  caught: %s\n", e.what());
        std::puts("  ^ 'a' release hua, 'c' banaya hi nahi tha, ~HalfBuilt NAHI chala");
    }

    // --------------------------------------------------------
    //  4. dtor-throws-during-unwinding => terminate (guarded)
    // --------------------------------------------------------
    std::puts("\n[dtor throws during unwinding — demo OFF by default]");
    std::puts("  code padho: DangerousDtor{true} ko ek try-block mein rakh ke");
    std::puts("  bahar se throw karoge to program std::terminate karega.");
    {
        DangerousDtor safe(false);   // armed=false => kuch nahi hoga
        (void)safe;
    }
    // Asli demo (comment hata ke dekho — program abort karega):
    // try {
    //     DangerousDtor boom(true);
    //     throw std::runtime_error("outer");   // unwinding shuru -> ~DangerousDtor throw -> terminate
    // } catch (...) {}

    std::puts("\nSaar: cleanup ko HAMESHA destructor mein daalo (RAII).");
    std::puts("Rule: destructors ko throw NAHI karna chahiye (noexcept by default).");
    return 0;
}
