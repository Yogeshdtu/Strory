// 03_comments_demo.cpp
// ============================================================
// Comment ke saare styles
// ============================================================

/*
 * ============================================================
 *  FILE HEADER COMMENT
 *  Purpose : comment styles dikhana
 *  Note    : Preprocessor yeh saare comments HATA deta hai --
 *            compiler ko yeh kabhi dikhte hi nahi.
 *            Verify karo: g++ -E 03_comments_demo.cpp | tail -20
 * ============================================================
 */

#include <iostream>

/**
 * @brief Do numbers ka sum nikaalta hai.
 *
 * Yeh Doxygen-style documentation comment hai. Doxygen jaise tools
 * isse automatic HTML documentation bana dete hain. Bade projects mein
 * yeh standard practice hai.
 *
 * @param a  pehla number
 * @param b  doosra number
 * @return   a aur b ka sum
 *
 * @note Yeh function abhi bahut simple hai
 */
int add(int a, int b) {
    return a + b;
}

int main() {
    // ---------- Single line comment ----------
    int x = 10;    // yeh inline comment hai -- line ke end tak

    /* ---------- Multi-line comment ---------- */

    /* Yeh comment
       kai lines mein
       fail sakta hai */

    // ---------- ACHHE vs BURE comments ----------

    // ❌ BURA comment -- code hi dohra raha hai, kuch naya nahi bata raha
    int y = 20;    // y ko 20 set karo

    // ✅ ACHHA comment -- "KYUN" bata raha hai
    // Prices ko integer paise mein rakhte hain, double mein nahi.
    // Kyunki floating point mein 0.1 + 0.2 != 0.3 hota hai, aur financial
    // calculations mein woh rounding error crores ka nuksaan kar sakta hai.
    long long priceInPaise = 10050;   // Rs 100.50

    std::cout << "x = " << x << ", y = " << y << "\n";
    std::cout << "price (paise) = " << priceInPaise << "\n";
    std::cout << "add(3, 4) = " << add(3, 4) << "\n";

    // ---------- Code ko temporarily band karna ----------
    // std::cout << "Yeh line chalegi nahi\n";
    //
    // ⚠️ NOTE: Commented-out code ko permanently mat chhodo.
    //          Git use karo -- purana code history mein safe rehta hai.
    //          Dead commented code sirf confusion banata hai.

    // ---------- Special markers (convention) ----------
    // TODO:  yahan input validation add karni hai
    // FIXME: negative prices handle nahi ho rahe
    // HACK:  temporary fix, proper solution baad mein
    // NOTE:  yeh assumption pe based hai ki price kabhi negative nahi hoga
    // PERF:  yeh loop optimize ho sakta hai
    //
    // Dhoondhne ke liye:  grep -rn "TODO\|FIXME" .

    // ---------- NESTED COMMENT KA TRAP ----------
    //
    // Multi-line comments NEST nahi hote. Yeh GALAT hai:
    //
    //     SLASH-STAR bahar
    //        SLASH-STAR andar STAR-SLASH
    //        yeh line ab CODE ban gayi -- error!
    //     STAR-SLASH
    //
    // Kyunki PEHLA "STAR-SLASH" poore comment ko band kar deta hai,
    // aur uske baad ka text code ban jaata hai.
    //
    // (Isliye maine yahan literal symbols ki jagah naam likhe hain --
    //  warna yeh file khud compile na hoti! Compiler `-Wcomment` warning
    //  bhi deta hai jab woh comment ke andar SLASH-STAR dekhta hai.)
    //
    // Bade blocks comment karne ke SAFE tareeke:
    //   1. `//` use karo (VS Code mein Ctrl+/ se ek saath ho jaata hai)
    //   2. `#if 0 ... #endif` use karo -- neeche demo hai


#if 0
    // Yeh sab kuch preprocessor ignore kar dega -- comments bhi.
    // Yeh nested comments ke liye safe tareeka hai.
    std::cout << "Yeh kabhi compile nahi hoga\n";
    /* aur yeh comment bhi safe hai */
#endif

    return 0;
}
