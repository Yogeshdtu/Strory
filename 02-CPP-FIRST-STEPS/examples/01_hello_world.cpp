// 01_hello_world.cpp
// ============================================================
// AAPKA PEHLA C++ PROGRAM
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 01_hello_world.cpp -o hello
//   ./hello
// ============================================================

// `#` batata hai: yeh line PREPROCESSOR ke liye hai, compiler ke liye nahi.
// `include` ka matlab: "is file ka poora content yahan paste kar do".
// `<iostream>` = "input output stream" -- ismein cout, cin, cerr milte hain.
// Dhyaan do: is line ke aakhir mein semicolon NAHI hai.
#include <iostream>

// `int`  -> yeh function ek integer return karega
// `main` -> special naam: program ka entry point (yahan se aapka code shuru hota hai)
// `()`   -> parameters ki list (khali = koi parameter nahi leta)
// `{`    -> function ka body yahan se shuru
int main() {

    // `std`   -> namespace ka naam ("standard"). Poori standard library ismein hai.
    // `::`    -> scope resolution operator: "std ke andar wala"
    // `cout`  -> ek OBJECT hai (function NAHI), jo screen se juda hua hai
    // `<<`    -> stream insertion operator (yeh overloaded operator hai, arrow nahi)
    // `"..."` -> string literal (type: const char[13])
    // `\n`    -> EK character hai (newline, ASCII 10), do characters nahi
    // `;`     -> statement yahan khatam
    std::cout << "Hello World\n";

    // `return` -> function se bahar niklo aur yeh value do
    // `0`      -> exit code. 0 = success. Yeh value OPERATING SYSTEM ko jaati hai.
    //             Terminal mein `echo $?` se dekh sakte ho.
    //
    // NOTE: `main` akela function hai jisme `return` OPTIONAL hai.
    //       Agar aap na likho, compiler apne aap `return 0;` laga deta hai.
    //       Baaki har non-void function mein `return` zaroori hai.
    return 0;

// `}` -> function ka body khatam. Iske baad semicolon NAHI lagta.
//        (Lekin class/struct/enum ke baad LAGTA hai -- yeh fark yaad rakhna.)
}
