// 05_scope_demo.cpp
// ============================================================
// Blocks, scope, lifetime, aur destruction order
// ============================================================
// Yeh example RAII (folder 17) ka foundation hai.
// Dhyaan se dekho ki objects kab BANTE hain aur kab MARTE hain.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow 05_scope_demo.cpp -o scope && ./scope
// ============================================================

#include <iostream>

// ---------------------------------------------------------------
// Tracer: ek chhota helper jo batata hai ki object kab bana, kab mara.
// (`struct` abhi detail mein nahi padha -- folder 11 aur 15 mein aayega.
//  Abhi bas itna samjho ki yeh do "special functions" define kar raha hai:
//  ek jo object banne pe chalta hai, ek jo object marne pe.)
// ---------------------------------------------------------------
struct Tracer {
    const char* name;

    // CONSTRUCTOR -- object BANTE waqt automatically chalta hai
    Tracer(const char* n) : name(n) {
        std::cout << "    [+] " << name << " BANA\n";
    }

    // DESTRUCTOR -- object MARTE waqt automatically chalta hai
    // Yeh AUTOMATIC hai. Aapko kuch call nahi karna padta.
    ~Tracer() {
        std::cout << "    [-] " << name << " MARA\n";
    }
};

// Global variable -- poore program mein zinda, DATA section mein
int globalVar = 100;

// Global object -- iska constructor main() se PEHLE chalega
Tracer globalTracer("GLOBAL-OBJECT");

void demoFunction() {
    std::cout << "\n--- demoFunction() ke andar ---\n";
    Tracer funcLocal("function-local");
    std::cout << "  global yahan bhi dikhta hai: " << globalVar << "\n";
    std::cout << "--- demoFunction() se return ---\n";
}   // <- funcLocal yahan marta hai

int main() {
    std::cout << "\n=== main() shuru hua ===\n";
    std::cout << "(dhyaan do: GLOBAL-OBJECT pehle hi ban chuka tha!)\n\n";

    Tracer outer("outer");
    int x = 10;

    // ---------------------------------------------------------
    // NESTED BLOCK -- naya scope
    // ---------------------------------------------------------
    {
        std::cout << "\n--- block 1 mein ghuse ---\n";
        Tracer inner1("inner-1");
        int y = 20;

        // Andar wala block bahar ka sab dekh sakta hai
        std::cout << "  x (bahar se) = " << x << "\n";
        std::cout << "  y (yahin ka) = " << y << "\n";

        {
            std::cout << "\n--- block 2 (aur andar) ---\n";
            Tracer inner2("inner-2");

            // SHADOWING: yeh naya `x` bahar wale `x` ko chhupa deta hai.
            // Legal hai, par CONFUSING hai. -Wshadow warning dega.
            // Real code mein isse bacho -- alag naam do.
            int x = 999;
            std::cout << "  shadowed x = " << x << "  <- andar wala\n";
            std::cout << "--- block 2 se nikal rahe hain ---\n";
        }   // <- inner2 aur shadowed x yahan marte hain

        std::cout << "\n  wapas block 1 mein, x = " << x << "  <- bahar wala wapas\n";
        std::cout << "--- block 1 se nikal rahe hain ---\n";
    }   // <- inner1 aur y yahan marte hain

    // std::cout << y;    // ❌ ERROR: 'y' was not declared in this scope
                          //    y apne block ke saath mar chuka hai.
                          //    Uncomment karke error dekho!

    demoFunction();

    // ---------------------------------------------------------
    // DESTRUCTION ORDER -- ULTA hota hai (LIFO)
    // ---------------------------------------------------------
    std::cout << "\n--- teen objects banate hain ---\n";
    {
        Tracer a("A");
        Tracer b("B");
        Tracer c("C");
        std::cout << "  ab block khatam hoga -- dekho order ULTA hai:\n";
    }
    // Output: C mara, phir B, phir A.
    //
    // Ulta kyun? Kyunki `c` shayad `b` pe depend karta ho, aur `b` shayad `a` pe.
    // Isliye jo BAAD mein bana, woh PEHLE marta hai. Yeh guarantee hai.

    // ---------------------------------------------------------
    // EARLY RETURN pe bhi destructors chalte hain
    // ---------------------------------------------------------
    std::cout << "\n--- early return / exception safety ---\n";
    {
        Tracer safe("SAFE-RESOURCE");
        std::cout << "  agar yahan return ya exception ho jaata,\n";
        std::cout << "  tab bhi SAFE-RESOURCE ka destructor chalta.\n";
        std::cout << "  YEH RAII KA POORA POINT HAI.\n";
    }

    std::cout << "\n=== main() khatam ho raha hai ===\n";
    return 0;
}   // <- outer yahan marta hai, phir GLOBAL-OBJECT (main ke BAAD)
