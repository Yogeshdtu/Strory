// 03_temporaries.cpp
// ============================================================
// Temporary objects: kab bante, kab marte (full-expression ke end pe),
// aur `const T&` / `T&&` se LIFETIME EXTENSION — aur woh 4 cases jahan
// extension NAHI hoti (dangling).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow 03_temporaries.cpp -o tmp && ./tmp
//   (dangling cases: -O2 -fsanitize=address se aur clear dikhta — MinGW pe
//    fallback -D_GLIBCXX_ASSERTIONS)
// ============================================================

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

struct Loud {
    int id;
    static inline int n = 0;
    Loud() : id(++n) { std::printf("  Loud(%d) ctor\n", id); }
    ~Loud() { std::printf("  ~Loud(%d)\n", id); }
    Loud(const Loud&) : id(++n) { std::printf("  Loud(%d) copy\n", id); }
    int value() const { return id * 10; }
};

static Loud make_loud() { return Loud{}; }

int main() {
    // --------------------------------------------------------
    //  1. Temporary lives till the end of the FULL-EXPRESSION (the `;`)
    // --------------------------------------------------------
    std::puts("1. temporary lifetime = full-expression:");
    std::printf("  value = %d\n", make_loud().value());   // Loud banta, use hota, phir
    std::puts("  (^ ~Loud already chal chuka — ; se pehle)\n");

    // --------------------------------------------------------
    //  2. LIFETIME EXTENSION — `const T&` (ya `T&&`) ek temporary ko
    //     bind kare to temporary reference ke SCOPE tak jeeta hai.
    // --------------------------------------------------------
    std::puts("2. const T& extends the temporary to the ref's scope:");
    {
        const Loud& r = make_loud();       // temporary ~Loud yahan NAHI chalta
        std::printf("  r.value() = %d  (still alive)\n", r.value());
    }                                       // yahan chalta hai (scope end)
    std::puts("  (^ ~Loud block-end pe — extension ne bacha liya)\n");

    // --------------------------------------------------------
    //  3. Extension TRANSITIVE hai — temporary ke andar ke sub-object
    //     ka reference bhi poore temporary ko extend karta hai.
    // --------------------------------------------------------
    std::puts("3. binding a subobject reference also extends the whole temporary:");
    {
        const int& v = make_loud().id;      // poora Loud extend hota hai
        std::printf("  v = %d  (Loud still alive)\n", v);
    }
    std::puts("");

    // ========================================================
    //  NON-EXTENSION CASES — yahan temporary MAR jaata hai, ref DANGLES
    // ========================================================
    std::puts("=== dangling: extension NAHI hoti ===\n");

    // --- 4a. Function RETURN value is a reference to a temporary ---
    // const T& parameter/return: temporary caller ke full-expression tak
    // hi jeeta — function se bahar aate hi mar jaata.
    auto bad_ref = [](const Loud& x) -> const Loud& { return x; };
    std::puts("4a. returning the bound reference — temporary dies at the ;:");
    {
        const Loud& r = bad_ref(make_loud());   // ⚠️ temporary ; pe marta, r dangles
        // r.value() ko yahan use karna UB hota — isliye nahi kar rahe:
        (void)r;
        std::puts("  r is now DANGLING (temporary destroyed at the previous ;)");
    }
    std::puts("");

    // --- 4b. std::string_view / pointer INTO a temporary ---
    std::puts("4b. string_view into a temporary std::string:");
    {
        auto get_name = []{ return std::string("dynamically-built-name"); };
        std::string_view sv = get_name();       // ⚠️ temporary string ; pe marta
        // sv ab freed memory dekh raha hai. Print karna UB — skip:
        std::printf("  sv.size() reported as %zu (may be garbage / may 'work')\n", sv.size());
        std::puts("  -> string_view ko kabhi temporary se mat banao");
        std::string keep = get_name();          // ✅ apna copy rakho
        std::string_view ok(keep);
        std::printf("  ok = \"%.*s\" (owns backing storage)\n",
                    static_cast<int>(ok.size()), ok.data());
    }
    std::puts("");

    // --- 4c. Reference MEMBER bound to a temporary in a ctor ---
    std::puts("4c. reference member bound to a ctor argument temporary:");
    struct Holder {
        const Loud& ref;                        // ⚠️ danger: agar temporary se bind hua
        explicit Holder(const Loud& r) : ref(r) {}   // extension yahan APPLY NAHI hoti
        int peek() const { return ref.value(); }
    };
    {
        Holder h(make_loud());                   // temporary ; pe marta, h.ref dangles
        (void)h;
        std::puts("  h.ref is DANGLING — reference members don't extend ctor-arg temporaries");
    }
    std::puts("");

    // --- 4d. range-for over a temporary's SUBOBJECT (fixed in C++23) ---
    std::puts("4d. range-for over getObj().container :");
    struct Bag { std::vector<int> items{1, 2, 3}; };
    auto make_bag = []{ return Bag{}; };
    {
        // C++20: `for (int x : make_bag().items)` -> Bag temporary destroyed
        //        before the loop body in C++20 (fixed in C++23). Safe form:
        Bag b = make_bag();
        long s = 0;
        for (int x : b.items) s += x;
        std::printf("  sum = %ld  (materialized the Bag first — safe in every std)\n", s);
    }

    std::puts("\nSaar:");
    std::puts(" - temporary marta hai full-expression (;) ke end pe");
    std::puts(" - `const T&` / `T&&` LOCAL binding usse ref ke scope tak extend karta");
    std::puts(" - extension NAHI hoti: function return, ctor-mem-init, ke through");
    std::puts("   pass hone pe; string_view/pointer temporary se lena; (C++20) range-for subobject");
    return 0;
}
