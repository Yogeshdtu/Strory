// 01_calculator.cpp  --  49-PROJECTS beginner P1
// ============================================================
// Expression evaluator: precedence + parentheses via the shunting-yard
// algorithm. Returns std::optional (nullopt on any error, incl. /0).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -g -O0 01_calculator.cpp -o t && ./t
// ============================================================

#include <cassert>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

int precedence(char op) {
    switch (op) {
        case '+': case '-': return 1;
        case '*': case '/': return 2;
        default:            return 0;
    }
}

bool apply(std::vector<double>& vals, char op) {
    if (vals.size() < 2) return false;
    const double b = vals.back(); vals.pop_back();
    const double a = vals.back(); vals.pop_back();
    switch (op) {
        case '+': vals.push_back(a + b); return true;
        case '-': vals.push_back(a - b); return true;
        case '*': vals.push_back(a * b); return true;
        case '/':
            if (b == 0.0) return false;          // divide-by-zero -> error, not crash
            vals.push_back(a / b);
            return true;
        default: return false;
    }
}

std::optional<double> evaluate(std::string_view expr) {
    std::vector<double> vals;
    std::vector<char>   ops;
    bool expect_operand = true;                  // for detecting unary minus / bad input

    for (std::size_t i = 0; i < expr.size(); ) {
        const char c = expr[i];
        if (c == ' ' || c == '\t') { ++i; continue; }

        if ((c >= '0' && c <= '9') || c == '.') {
            std::size_t j = i;
            while (j < expr.size() && ((expr[j] >= '0' && expr[j] <= '9') || expr[j] == '.')) ++j;
            try {
                vals.push_back(std::stod(std::string(expr.substr(i, j - i))));
            } catch (...) { return std::nullopt; }
            i = j;
            expect_operand = false;
        } else if (c == '(') {
            ops.push_back(c);
            ++i;
            expect_operand = true;
        } else if (c == ')') {
            while (!ops.empty() && ops.back() != '(') {
                if (!apply(vals, ops.back())) return std::nullopt;
                ops.pop_back();
            }
            if (ops.empty()) return std::nullopt;   // unbalanced ')'
            ops.pop_back();                          // pop '('
            expect_operand = false;
            ++i;
        } else if (c == '+' || c == '-' || c == '*' || c == '/') {
            if (expect_operand) {
                if (c == '-') { vals.push_back(0.0); }   // unary minus -> 0 - x
                else if (c == '+') { /* unary plus: no-op */ ++i; continue; }
                else return std::nullopt;                // *,/ with no left operand
            }
            while (!ops.empty() && ops.back() != '(' &&
                   precedence(ops.back()) >= precedence(c)) {
                if (!apply(vals, ops.back())) return std::nullopt;
                ops.pop_back();
            }
            ops.push_back(c);
            ++i;
            expect_operand = true;
        } else {
            return std::nullopt;                        // unknown character
        }
    }

    while (!ops.empty()) {
        if (ops.back() == '(') return std::nullopt;      // unbalanced '('
        if (!apply(vals, ops.back())) return std::nullopt;
        ops.pop_back();
    }
    if (vals.size() != 1) return std::nullopt;
    return vals.front();
}

bool close(double a, double b) { return std::fabs(a - b) < 1e-9; }

} // namespace

int main() {
    assert(close(evaluate("2+2").value(), 4.0));
    assert(close(evaluate("3+4*2").value(), 11.0));            // precedence
    assert(close(evaluate("(3+4)*2").value(), 14.0));          // parens
    assert(close(evaluate("7 - 3 - 2").value(), 2.0));         // left-assoc
    assert(close(evaluate("2 * (3 + (4 - 1)) / 2").value(), 6.0));
    assert(close(evaluate("-5 + 3").value(), -2.0));           // unary minus
    assert(close(evaluate("10.5 / 2").value(), 5.25));

    assert(evaluate("10/0")   == std::nullopt);                // divide by zero
    assert(evaluate("(1+2")   == std::nullopt);                // unbalanced
    assert(evaluate("1+2)")   == std::nullopt);
    assert(evaluate("")       == std::nullopt);
    assert(evaluate("1 + + ") == std::nullopt);
    assert(evaluate("3 $ 4")  == std::nullopt);                // bad char

    std::puts("01_calculator: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - Shunting-yard: one value stack, one operator stack. On a new operator,
//     pop-and-apply everything with >= precedence first (left-assoc).
//   - Errors are values (std::optional), not exceptions or crashes. /0 is
//     caught in apply(), unbalanced parens on both passes.
//   - v3 extension: '^' is right-assoc (pop only STRICTLY greater precedence).
//   - Concepts: switch (06), a manual stack (20/07), std::optional (23).
// ============================================================
