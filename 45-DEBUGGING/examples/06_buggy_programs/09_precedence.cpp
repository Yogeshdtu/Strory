// 09_precedence.cpp  --  ismein EK bug hai.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 09_precedence.cpp -o t && ./t
// Expected: "net pay = 1025"   Actual: "net pay = 2025".
// ============================================================
#include <cstdio>

// gross = rate*hours + bonus.  Phir 50% advance kaat ke net do:
//   net = (rate*hours + bonus) / 2
static long net_pay(long rate, long hours, long bonus) {
    return rate * hours + bonus / 2;          // <-- dekho yahan
}

int main() {
    // rate 50, hours 40 -> 2000;  + bonus 50 -> gross 2050;  net = 2050/2 = 1025
    std::printf("net pay = %ld\n", net_pay(50, 40, 50));
    return 0;
}

// ============================================================
// BUG:     Operator precedence. `/` `+` se pehle bind karta hai, isliye
//            rate*hours + bonus/2  ==  (rate*hours) + (bonus/2)
//                                  ==  2000 + 25  ==  2025
//          Intent tha `(rate*hours + bonus) / 2` == 2050/2 == 1025.
//          Sirf `bonus` aadha ho raha hai, poora gross nahi.
// SYMPTOM: Number "thoda sa" galat -- crash nahi, exception nahi, chup
//          logic bug. Sirf hand-computed expected value se milane pe pakda
//          jaata. Chhote bonus pe error chhota (aasani se miss ho jaata).
// TOOL:    Koi sanitizer nahi pakadta -- yeh well-defined, bas GALAT.
//          (1) Unit test with hand-computed expected: EXPECT_EQ(net_pay(
//              50,40,50), 1025). (2) gdb: `print rate*hours + bonus/2`
//              (2025) vs `print (rate*hours + bonus)/2` (1025) -> alag ->
//              precedence suspect. (3) `-Wall` yahan chup (pure arithmetic);
//              `&`/`|` vs `==` hota to `-Wparentheses` bolta.
// FIX:     Parentheses se intent likho:
//            return (rate * hours + bonus) / 2;
//          Rule: `+ - * / % << >> & | ^` mix karo aur order pe zaraa bhi
//          shak ho -> brackets lagao. Reader ko precedence table yaad
//          nahi karni chahiye (05-OPERATORS/09).
// ============================================================
