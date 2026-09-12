// 06_broken_on_purpose.cpp
// ============================================================
//  ⚠️  IS FILE MEIN JAAN-BOOJH KAR 8 GALTIYAN HAIN  ⚠️
// ============================================================
//
// AAPKA KAAM:
//   1. Compile karo:
//        g++ -std=c++20 -Wall -Wextra 06_broken_on_purpose.cpp -o broken
//   2. SIRF PEHLA error padho (baaki ignore karo!)
//   3. Us ek error ko fix karo
//   4. Dobara compile karo
//   5. Repeat karo jab tak clean build na ho
//
// HAR ERROR KE LIYE NOTE KARO:
//   - Error message kya tha?
//   - Kis STAGE ka error tha?
//       (1) Preprocessor  (2) Parser  (3) Semantic  (4) Linker  (5) Runtime
//   - Kaise fix kiya?
//
// Answers file ke sabse neeche hain -- pehle KHUD try karo!
// ============================================================


// ---- GALTI 1 ----
#include <iostrem>


// ---- GALTI 2 ----
int helperFunction();


// ---- GALTI 3 ----
struct Point {
    int x;
    int y;
}


// ---- GALTI 4 ----
void main() {

    // ---- GALTI 5 ----
    int x = 10

    // ---- GALTI 6 ----
    cout << "x ki value: " << x << "\n";

    // ---- GALTI 7 ----
    if (x > 5);
        std::cout << "x bada hai\n";

    // ---- GALTI 8 ----
    std::cout << helperFunction() << "\n";

    return 0;
}


// ============================================================
//                      A N S W E R S
//                (pehle khud try karo!)
// ============================================================
//
// GALTI 1  [STAGE 1: PREPROCESSOR]
//   #include <iostrem>
//   Spelling galat hai. Sahi: <iostream>
//   Error: "fatal error: iostrem: No such file or directory"
//   Note: "fatal" ka matlab compiler ne turant haar maan li, aage padha hi nahi.
//
// GALTI 2  [STAGE 4: LINKER]
//   int helperFunction();
//   Yeh sirf DECLARATION hai -- definition kahin nahi hai.
//   Compiler khush ho jayega (declaration mil gayi), par LINKER fail karega.
//   Error: "undefined reference to `helperFunction()'"
//   Fix: definition add karo, jaise:
//        int helperFunction() { return 42; }
//
// GALTI 3  [STAGE 2: PARSER]
//   struct Point { ... }
//   Struct definition ke baad SEMICOLON zaroori hai.
//   Fix: `};` likho
//   Yaad rakhne ka rule:
//     - type define kar rahe ho (class/struct/enum/union) -> ; lagao
//     - code define kar rahe ho (function/if/loop)        -> ; mat lagao
//
// GALTI 4  [STAGE 2: SEMANTIC]
//   void main()
//   main HAMESHA int return karta hai. `void main()` standard ke khilaaf hai.
//   Error: "'::main' must return 'int'"
//   Fix: `int main()`
//
// GALTI 5  [STAGE 2: PARSER]
//   int x = 10
//   Semicolon missing.
//   🔑 IMPORTANT: Error AGLI line pe dikhega, is line pe nahi!
//   Jab "expected ';'" dikhe -- HAMESHA UPAR WALI LINE DEKHO.
//
// GALTI 6  [STAGE 2: SEMANTIC]
//   cout << ...
//   `std::` missing hai. cout `std` namespace ke andar hai.
//   Error: "'cout' was not declared in this scope; did you mean 'std::cout'?"
//   Fix: `std::cout`
//
// GALTI 7  [WARNING -- SILENT BUG!]
//   if (x > 5);
//   Extra semicolon ne `if` ka body khali kar diya (null statement).
//   Agli line ab `if` ke BAHAR hai -- woh HAMESHA chalegi.
//
//   ⚠️ YEH COMPILE HO JAYEGA! Koi error nahi. Bas galat kaam karega.
//   -Wall ke saath warning milegi:
//      "warning: suggest braces around empty body in an 'if' statement"
//
//   ISI WAJAH SE WARNINGS HAMESHA ON RAKHNI HAIN.
//   Fix: `if (x > 5) { std::cout << "x bada hai\n"; }`
//
// GALTI 8  [GALTI 2 KA HI NATEEJA]
//   helperFunction() ko call kiya, par uski definition nahi hai.
//   Galti 2 fix karne se yeh bhi theek ho jayegi.
//
// ============================================================
//  SEEKHNE WALI BAATEIN
// ============================================================
//  1. Ek galti se KAI errors aa sakte hain. Pehla fix karo, dobara compile karo.
//  2. Missing `;` ka error AGLI line pe dikhta hai.
//  3. Compiler ke "did you mean...?" suggestions aksar SAHI hote hain.
//  4. Linker errors alag dikhte hain -- "undefined reference", `ld` ya `collect2`.
//  5. Kuch galtiyan ERROR nahi, WARNING deti hain -- aur wahi sabse khatarnaak hain.
//     Isliye: -Wall -Wextra HAMESHA.
// ============================================================
