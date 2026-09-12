// 07_atoi_edge_cases.cpp
// ============================================================
// CLASSIC: implement "string to int" (like C's atoi / LeetCode's myAtoi)
// and get EVERY edge case right. This question is entirely about edge
// cases and overflow -- the happy path is trivial.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 07_atoi_edge_cases.cpp -o t && ./t
// ============================================================
// INTERVIEWER KYA DEKH RAHA:
//   - leading whitespace, optional sign, digits, stop at first non-digit
//   - INT overflow -> clamp to INT_MIN / INT_MAX (no signed-overflow UB!)
//   - empty / all-whitespace / sign-only / "+-2" / "  -0012a34" / ""
//   - detecting overflow BEFORE it happens (compare against limit/10)
//   - do NOT rely on wraparound; do NOT use a wider type as a crutch
//     without saying so
// ============================================================

#include <cassert>
#include <climits>
#include <cstdio>
#include <string_view>

// Semantics like LeetCode 8 "String to Integer (atoi)":
//   skip leading spaces, optional +/-, read digits, stop at non-digit,
//   clamp to [INT_MIN, INT_MAX].
static int my_atoi(std::string_view s) {
    std::size_t i = 0;
    const std::size_t n = s.size();

    while (i < n && s[i] == ' ') ++i;                 // 1. leading spaces

    int sign = 1;
    if (i < n && (s[i] == '+' || s[i] == '-')) {      // 2. optional sign
        sign = (s[i] == '-') ? -1 : 1;
        ++i;
    }

    int result = 0;
    while (i < n && s[i] >= '0' && s[i] <= '9') {     // 3. digits
        const int d = s[i] - '0';

        // 4. overflow check BEFORE multiplying/adding (no UB):
        //    if result*10 + d would exceed INT_MAX, clamp now and stop.
        //    (INT_MIN's magnitude is INT_MAX + 1, but this same bound still
        //     clamps "-2147483648" to INT_MIN correctly -- it returns before
        //     computing the magnitude that wouldn't fit in an int.)
        if (result > (INT_MAX - d) / 10) {
            return (sign == 1) ? INT_MAX : INT_MIN;
        }
        result = result * 10 + d;
        ++i;
    }
    return sign * result;
}

int main() {
    // happy path
    assert(my_atoi("42") == 42);
    assert(my_atoi("   -042") == -42);
    assert(my_atoi("4193 with words") == 4193);
    assert(my_atoi("words and 987") == 0);           // non-digit first -> 0
    assert(my_atoi("+1") == 1);

    // empty / whitespace / sign-only
    assert(my_atoi("") == 0);
    assert(my_atoi("   ") == 0);
    assert(my_atoi("-") == 0);
    assert(my_atoi("+") == 0);
    assert(my_atoi("+-12") == 0);                    // second sign is a non-digit
    assert(my_atoi("  -0012a34") == -12);

    // zero forms
    assert(my_atoi("0") == 0);
    assert(my_atoi("-0") == 0);
    assert(my_atoi("0000123") == 123);

    // overflow -> clamp (NOT wraparound, NOT UB)
    assert(my_atoi("2147483647") == INT_MAX);
    assert(my_atoi("2147483648") == INT_MAX);
    assert(my_atoi("99999999999999") == INT_MAX);
    assert(my_atoi("-2147483648") == INT_MIN);
    assert(my_atoi("-2147483649") == INT_MIN);
    assert(my_atoi("-99999999999999") == INT_MIN);

    std::printf("07_atoi_edge_cases: ALL PASS  (INT range [%d, %d])\n", INT_MIN, INT_MAX);
    return 0;
}

// ============================================================
// KEY POINT: the overflow check is done BEFORE the arithmetic, by
// comparing against (INT_MAX - d) / 10. Doing `result = result*10 + d`
// first and then checking is signed-overflow UB -- the compiler may
// assume it can't happen and delete your check. (folders 23/13, 45/12 D1)
//
// In real code: std::from_chars (alloc-free, returns an error code on
// overflow / bad input) is the right tool. This exercise is about
// demonstrating you can reason through the edge cases and avoid UB.
// ============================================================
