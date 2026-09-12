# 15 — Folder 08 Revision + Exercises

## Prerequisites
Is folder ke saare lessons (01–14) aur aath examples chalaye hue (`05_stack_overflow`
ko haath se — woh crash hota hai).

---

## PART A — Concept check

### Functions / declaration
1. Function use karne ke 4 fayde?
2. Declaration aur definition mein fark? Definition bhi declaration hai?
3. ODR — declaration aur definition pe alag kaise?
4. `undefined reference` — kis stage ka error, 3 wajah?
5. Header aur source mein kya-kya? Alag kyun?

### Parameters / return
6. Pass by value / reference / pointer — kab kaunsa?
7. `const T&` parameter ka faayda bade objects pe?
8. `std::string_view` vs `const std::string&` parameter — kab farq?
9. RVO / copy elision — C++17 ne kya guarantee kiya?
10. `return std::move(localVector)` kyun galat?
11. `[[nodiscard]]` kya karta hai?

### Call stack
12. Stack frame mein kya-kya (4 cheezein)?
13. Prologue / epilogue / `call` / `ret` — kya karte hain?
14. Stack kis direction badhta hai? Return address kahan?
15. Calling convention — Windows x64 vs System V, pehla arg kahan?
16. Stack overflow — 3 wajah, crash kaisa (exception?)?
17. Local ka address return karna kyun UB?
18. Stack vs heap — speed, lifetime, size?

### Scope / lifetime
19. Scope aur lifetime mein fark — ek example scope chhota par lifetime bada?
20. `static` local — kab init, thread-safe?
21. Dangling `string_view` kaise banta hai?

### Default args / overloading
22. Default argument kin params pe? Kab evaluate hota hai?
23. Default kahan likhna — declaration ya definition?
24. Return type se overload kyun nahi?
25. Overload resolution ke 3 phases + conversion ranks?
26. `f(0)` vs `f(nullptr)` — resolution mein fark?
27. Name mangling — `extern "C"` se connection?

### Recursion
28. Recursive function ke 2 zaroori hisse?
29. Naive Fibonacci exponential kyun? Fix?
30. Tail call / TCO — C++ mein guaranteed?
31. Deep recursion — kya hota hai, iterative alternative?

### inline / constexpr / attributes
32. `inline` keyword ka asli matlab (ODR)?
33. `inline` keyword vs actual inlining — kaun decide karta hai?
34. `constexpr` function — compile-time aur runtime dono kaise?
35. `const` / `constexpr` / `consteval` — fark?
36. `if` vs `if constexpr` — templates mein?
37. `noexcept` ke 2 concrete effects (documentation ke alawa)?
38. `[[likely]]`/`[[unlikely]]` — codegen asar aur risk?

### main
39. `main` ke valid signatures? `argc` min value?
40. `argv[argc]` kya hai? `argv` strings ka lifetime?

### Namespaces
41. Namespace ki runtime cost? Symbol naam pe kya asar?
42. `using nse::f;` vs `using namespace nse;` — fark, aur header mein kya kabhi nahi?
43. Anonymous namespace kya karta hai? `nm` mein kaise pehchanoge?
44. ADL kya hai? `std::cout << s` mein ADL kahan hai?

---

## PART B — Output prediction

### B1
```cpp
int f(int x) { x += 10; return x; }
int a = 5;
int b = f(a);
std::cout << a << " " << b;
```
<details><summary>Answer</summary>`5 15` — pass by value; `a` unchanged.</details>

### B2
```cpp
void g(int& x) { x *= 2; }
int a = 5;
g(a);
std::cout << a;
```
<details><summary>Answer</summary>`10` — reference; original modified.</details>

### B3
```cpp
int ticket() { static int n = 0; return ++n; }
std::cout << ticket() << ticket() << ticket();
```
<details><summary>Answer</summary>`123` (evaluation order of `<<` operands is left-to-right for `operator<<`; each call advances the static). Note: chained `<<` args are sequenced, so `1` `2` `3`.</details>

### B4
```cpp
std::uint64_t fact(int n) { return n <= 1 ? 1 : n * fact(n - 1); }
std::cout << fact(4);
```
<details><summary>Answer</summary>`24` — `4*3*2*1`.</details>

### B5
```cpp
void h(int x = 1, int y = 2) { std::cout << x << y << " "; }
h();  h(5);  h(5, 6);
```
<details><summary>Answer</summary>`12 52 56 `</details>

### B6
```cpp
void p(int)    { std::cout << "int "; }
void p(double) { std::cout << "double "; }
p('a');  p(3);  p(3.0);  p(3.0f);
```
<details><summary>Answer</summary>`int int double double ` — `'a'`→int (promo), `3`→int, `3.0`→double, `3.0f`→double (float→double promo).</details>

### B7
```cpp
constexpr int sq(int x) { return x * x; }
int arr[sq(3)];
std::cout << sizeof(arr) / sizeof(arr[0]);
```
<details><summary>Answer</summary>`9` — `sq(3)` compile-time → array of 9.</details>

### B8
```cpp
int& bad() { int x = 42; return x; }
int y = bad();
std::cout << "reached";
```
<details><summary>Answer</summary>`reached` printed — par `bad()` returns dangling reference; `int y = bad()` is UB (may print garbage / crash / work). `-Wreturn-local-addr` warns.</details>

### B9
```cpp
int n = 5;
namespace cfg { int n = 7; }
int main() {
    int n = 9;
    {
        using cfg::n;
        std::cout << n;
    }
    std::cout << n << ::n;
}
```
<details><summary>Answer</summary>`795` — andar ke block mein `using cfg::n;` ne `cfg::n` (7) ko laaya aur bahar wale local `n` ko chhupa diya. Block khatam hote hi local `n` (9) wapas, aur `::n` global (5). (GCC 16.2 pe chala ke check kiya.)</details>

---

## PART C — Find the bug

### C1
```cpp
int biggest(int a, int b) { if (a > b) return a; }
```
<details><summary>Answer</summary>`b >= a` pe koi return → UB. `-Wreturn-type`. Add `return b;`.</details>

### C2
```cpp
int* makeCounter() { int c = 0; return &c; }
```
<details><summary>Answer</summary>Local ka address return → dangling pointer (frame gayab). Return by value, ya heap/static.</details>

### C3
```cpp
// header.hpp
int helper(int x) { return x + 1; }
```
<details><summary>Answer</summary>Non-`inline` function header mein defined → 2+ TUs include → ODR violation → linker error. `inline` lagao ya `.cpp` mein.</details>

### C4
```cpp
void log(std::string_view m, int level = 1);
void log(std::string_view m, int level = 1) { /*...*/ }
```
<details><summary>Answer</summary>Default argument declaration aur definition dono mein → "redefinition of default argument". Ek jagah rakho.</details>

### C5
```cpp
constexpr int getConfig() { return std::atoi(std::getenv("N")); }
constexpr int n = getConfig();
```
<details><summary>Answer</summary>`getenv`/`atoi` `constexpr` context mein allowed nahi → `constexpr int n` compile error. Use `const int n = getConfig();` (runtime).</details>

### C6
```cpp
long long sumArr(const int* a, int n) {
    return n == 0 ? 0 : a[0] + sumArr(a + 1, n - 1);
}
sumArr(data, 2'000'000);
```
<details><summary>Answer</summary>2M-deep linear recursion → stack overflow. Loop version.</details>

### C7
```cpp
int parse(std::string_view s) noexcept {
    return std::stoi(std::string(s));
}
```
<details><summary>Answer</summary>`std::stoi` throws on bad input; `noexcept` function throwing → `std::terminate()`. Use `from_chars`, ya `noexcept` hatao.</details>

### C8
```cpp
int main(int argc, char* argv[]) {
    std::string_view out = argv[2];   // "--out FILE"  ke liye
    // ...
}
```
<details><summary>Answer</summary>`argc` check nahi — agar sirf `./prog` diya to `argv[2]` OOB (past `nullptr`). Pehle `if (argc < 3) { usage; return 2; }`.</details>

### C9
```cpp
namespace md  { int parse(const char*) { return 1; } }
namespace oms { int parse(const char*) { return 2; } }
using namespace md;
using namespace oms;
int main() { return parse("x"); }
```
<details><summary>Answer</summary>Compile error: `call of overloaded 'parse(const char [2])' is ambiguous` — dono using-directives ne dono `parse` dikha diye. Fix: `md::parse("x")` likho, ya global scope ki using-directives hatao (file 14, Trap 1).</details>

---

## PART D — Practical tasks

### D1. Math library (header/source)
`mathx.hpp` + `mathx.cpp`: `gcd`, `lcm`, `isPrime`, `nextPrime`, `factorial`
(`std::uint64_t`), `power` (fast exponentiation). Sab `mathx.hpp` mein declare,
`mathx.cpp` mein define. `constexpr` jahan possible. `main.cpp` se use +
`static_assert` tests. Separately compile + link.

### D2. Call-stack visualizer
`examples/02_call_stack_trace.cpp` extend: ek function jo apni recursion depth,
frame address, aur consecutive-frame size print kare. Alag-alag local sizes
(`char buf[N]` with N = 0, 256, 4096) pe frame-diff dekho. Table banao.

### D3. Overload resolution lab
10 overloads of `describe(...)`: `int`, `unsigned`, `long`, `double`, `char`,
`bool`, `const char*`, `std::string`, `std::string_view`, `void*`. 20 calls with
various literals/variables. Predict → run → explain mismatches. `nm -C` se
mangled names.

### D4. Recursion → iteration
In sabko recursive **aur** iterative likho, dono ka result match:
`factorial`, `fibonacci`, `sumDigits`, `reverse a string`, `binary search`,
`tree height`, `power`. `n = 10^6` pe kaunse recursive versions crash karte hain?

### D5. `constexpr` protocol
Ek "wire format" ke field offsets/sizes `constexpr` functions se compute karo:
```cpp
constexpr std::size_t kHeaderSize = 8;
constexpr std::size_t fieldOffset(int fieldIndex);   // constexpr
static_assert(fieldOffset(3) == 20);
```
Ek `constexpr` checksum table (CRC-8 ya XOR) generate karo. `-S` se verify —
table rodata mein, koi runtime build code nahi.

### D6. Inline benchmark suite
`examples/06_inline_asm_check.cpp` extend: `-O0`, `-O2`, `-O3`, `-O2 -flto`
(2-file setup) pe `inline` vs `noinline` vs `std::function` (folder 22 preview) ka
call overhead. Table (ns/call). `-S` se `call` count.

### D7. CLI tool
`stats [--json] [--precision N] FILE` — file se numbers padho, mean/median/stddev/
min/max print karo. `--json` pe JSON output. Return codes: 0 ok, 1 file error,
2 usage. `std::from_chars` for parsing. `std::span<char*>` for argv.

### D8. Stack-safe deep tree
Ek skewed binary tree (linked list jaisa, 10^6 nodes) banao. `height()` recursive
(crash?) aur explicit-stack iterative (works?). `sum()` bhi dono. Measure.

---

## PART E — Self-assessment

```
[ ] Mujhe declaration vs definition, ODR, aur header/source split samajh aata hai
[ ] Mujhe undefined-reference linker error ki wajah pata hai
[ ] Main pass-by value/reference/pointer sahi choose kar sakta hoon
[ ] Mujhe const& parameters aur string_view ka faayda pata hai
[ ] Mujhe RVO / copy elision pata hai (aur return std::move mat karo)
[ ] Mujhe [[nodiscard]] ka use pata hai
[ ] Mujhe stack frame ka layout pata hai (return addr, saved rbp, locals)
[ ] Mujhe prologue/epilogue aur call/ret ka kaam pata hai
[ ] Mujhe pata hai stack neeche badhta hai, aur calling convention kya hai
[ ] Maine -O0 aur -O2 pe function ka assembly dekha hai (frame gayab -O2 pe)
[ ] Mujhe stack overflow ki wajah aur signature (SIGSEGV, not exception) pata hai
[ ] Maine 05_stack_overflow.cpp chalaya aur crash dekha
[ ] Mujhe local ka address return karna kyun UB, pata hai
[ ] Mujhe scope vs lifetime ka fark aur dangling string_view pata hai
[ ] Mujhe static local ka behaviour aur thread-safety pata hai
[ ] Mujhe default arguments ke rules (trailing, ek jagah, per-call eval) pata hain
[ ] Mujhe overload resolution ke phases aur conversion ranks aate hain
[ ] Mujhe f(0) vs f(nullptr) aur name mangling pata hai
[ ] Mujhe recursion ke base case, cost, aur tail-call/TCO pata hai
[ ] Maine naive fib ka exponential blowup measure kiya
[ ] Main deep recursion ko iteration mein badal sakta hoon
[ ] Mujhe inline keyword ka asli matlab (ODR) pata hai
[ ] Maine inline vs noinline call overhead measure kiya (~2 ns/call)
[ ] Mujhe constexpr functions (dual use), consteval, if constexpr pata hain
[ ] Mujhe noexcept, [[nodiscard]], [[likely]], [[gnu::const]] ke effects pata hain
[ ] Main main(argc, argv) se CLI args parse kar sakta hoon (from_chars, span)
[ ] Main apne namespaces bana sakta hoon (nesting, alias, anonymous, inline) aur using-declaration vs directive ka fark jaanta hoon
[ ] Mujhe ADL samajh aata hai (getline bina std::, operator<<)
[ ] Maine aath examples chalaye hain
```

**Scoring:**
- **25–29** → Excellent. Batch 3 poora! Folder 09 (Arrays) pe jao. 🎯
- **18–24** → Achha. Miss hue lessons dobara — khaas kar 05 (call stack).
- **11–17** → Files 05, 08, 09, 10, 14 dobara. Examples + gdb try karo.
- **< 11** → Poora folder dobara. Call stack lesson 05 sabse zaroori hai.

---

## PART F — Challenge

### Challenge 1: "Recursive-descent calculator"

Ek expression evaluator jo `+ - * / ( )` aur precedence handle kare:
```
"2 + 3 * 4"       -> 14
"(2 + 3) * 4"     -> 20
"10 / 2 / 5"      -> 1
"2 * 2 * 2 * 2"   -> 16
```

Structure (mutual recursion — forward declarations!):
```cpp
double parseExpression(Tokenizer&);   // handles + -
double parseTerm(Tokenizer&);         // handles * /
double parseFactor(Tokenizer&);       // handles numbers, ( expr )
```

Requirements: har function ek kaam, guard clauses, error handling (divide by
zero, unbalanced parens, bad token → clear message), `-Wall -Wextra -Wshadow
-Werror` clean. `main` se expression as CLI arg ya stdin.

Extension: variables (`x = 5; x * 2`), `constexpr` version for literal expressions.

### Challenge 2: "Call stack tracer library"

Ek chhoti header-only library `trace.hpp`:
```cpp
struct ScopeTracer {
    ScopeTracer(const char* fn);   // ctor: depth++, print "  ...enter fn"
    ~ScopeTracer();                // dtor: print "  ...exit fn", depth--
};
#define TRACE ScopeTracer _tracer_(__func__)
```

Kisi recursive function mein `TRACE;` daalo — call tree indent ke saath print
hoga (RAII se automatic exit). `fibNaive(6)` pe pura call tree dekho. Yeh
folder 05 (call stack), 17 (RAII), aur debugging (45) ko jodta hai.

### Challenge 3: "Micro-benchmark harness"

Ek `bench.hpp`:
```cpp
template <typename F>
BenchResult benchmark(std::string_view name, F&& fn, int iterations);
// warmup, multiple runs, min/median/mean/stddev, "kill the optimizer" (asm volatile sink)
```

Isse use karke folder 06–08 ke saare benchmarks re-run karo ek consistent
harness se: branch prediction, cache locality, loop unroll, inline overhead,
fib recursion. Ek report table (ns/op ya ms) generate karo. `-O2` aur
`-O3 -march=native` dono.

Yeh folder 35 (benchmarking) ka preview hai — aur aapke saare pichle experiments
ko ek jagah lata hai.

---

## 🎉 Folder 08 complete — aur Batch 3 (PHASE 3) complete!

Aapne seekha:
- Functions — declaration/definition, ODR, header/source
- Parameters — value/reference/pointer, `const&`, `string_view`, copy cost
- Return values — multiple returns, RVO, `[[nodiscard]]`, dangling
- **Call stack deep dive** — frames, prologue/epilogue, return address, calling
  convention, stack growth, real assembly, stack overflow (crashed it)
- Scope vs lifetime — static locals, dangling references/views
- Default arguments — trailing rule, per-call eval, vs overloading
- Overloading — resolution phases, conversion ranks, ambiguity, name mangling
- Recursion — base case, exponential fib (measured), tail-call/TCO, → iteration
- `inline` — ODR meaning (not "fast"), compiler inlining, ~2 ns/call measured
- `constexpr` functions — compile-time eval, `consteval`, `if constexpr`, tables
- Attributes — `[[nodiscard]]`, `noexcept`, `[[likely]]`, `[[gnu::const]]`, ...
- `main(argc, argv)` — CLI parsing, `from_chars`, `std::span`, return codes

**PHASE 3 done:** program ab **faisla leta hai** (06), **repeat karta hai** (07),
aur **reusable + composable** hai (08) — call stack ke saath.

Agla (PHASE 4): **arrays** — kai values ek saath, aur **array decay** jo pointers
ka darwaza hai.

---

## Next
→ [`../09-ARRAYS/00-README.md`](../09-ARRAYS/00-README.md)
