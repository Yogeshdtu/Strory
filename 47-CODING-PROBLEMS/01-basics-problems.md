# 01 — Basics problems

## Prerequisites
- `01-PROGRAMMING-BASICS/` … `08-FUNCTIONS/`
- Bit tricks: `05-OPERATORS/05-bitwise-operators.md`
- Overflow: `03-VARIABLES-DATA-TYPES/05-int-deep-dive.md`

## Yeh file kya hai
30 problems — variables, loops, functions, integer math, bit tricks. Easy → hard.
Har problem: **statement + pattern hint + `<details>` mein approach + complexity.**

**Pehle khud likho** — ek scratch file banao aur `./build.ps1 scratch.cpp` se
chalao. Phir `<details>` kholo. Jahan poora worked code chahiye →
[`11-solutions/01-basics-solutions.md`](11-solutions/01-basics-solutions.md).

Compile hamesha: `g++ -std=c++20 -Wall -Wextra -Wshadow -g file.cpp -o t && ./t`

---

## Part A — Easy (~10 min each)

### A1. FizzBuzz
`1..n` print karo, par 3 ke multiple pe `Fizz`, 5 ke pe `Buzz`, dono ke pe `FizzBuzz`.
`Pattern:` modulo + branch order.
<details><summary>Approach</summary>

`i % 15 == 0` pehle check karo (ya `i%3==0` ka result ek bool mein rakho aur
concatenate karo). `O(n)`. Trap: agar `i%3` aur `i%5` alag-alag `if`-`else if`
likhe to `FizzBuzz` case chhoot jaata hai.
</details>

### A2. Sum of digits
Ek non-negative `long n` do, uske digits ka sum lauta do (e.g. `1234 → 10`).
`Pattern:` `% 10` / `/ 10` loop.
<details><summary>Approach</summary>

`while (n) { s += n % 10; n /= 10; }`. `O(log₁₀ n)`. `n == 0` handle: loop chalega
hi nahi, `s = 0` — sahi.
</details>

### A3. Reverse an integer
`int n` ke digits ulte karo. Overflow pe `0` lauta do (LeetCode 7).
`Pattern:` build digit-by-digit, check **before** the multiply.
<details><summary>Approach</summary>

`rev = rev*10 + n%10` har step. Overflow guard: `if (rev > INT_MAX/10 || rev <
INT_MIN/10) return 0;` **multiply se pehle**. Pehle multiply karke check karna
signed-overflow UB hai — compiler check delete kar sakta hai. `O(log n)`.
</details>

### A4. Is prime
`isPrime(unsigned long n)` — `n < 2` → false.
`Pattern:` trial division up to `√n`, skip evens.
<details><summary>Approach</summary>

2 aur 3 alag; phir `i` ko `5, 7, 11, 13, …` (`6k±1`) pe chalao jab tak `i*i <= n`.
`O(√n)`. `i*i` overflow se bachne ke liye `i <= n/i` likho.
</details>

### A5. GCD and LCM
`gcd(a,b)` Euclid se; phir `lcm(a,b)`.
`Pattern:` `gcd(a,b) = gcd(b, a%b)`.
<details><summary>Approach</summary>

`while (b) { a %= b; std::swap(a,b); }` → `a`. `lcm = a / gcd(a,b) * b`
(pehle divide, warna `a*b` overflow). `O(log min(a,b))`. C++17: `std::gcd` /
`std::lcm` `<numeric>` mein.
</details>

### A6. Nth Fibonacci
`fib(n)` — `n` up to ~90 (fits in `uint64_t`).
`Pattern:` two running variables, O(1) space.
<details><summary>Approach</summary>

`a=0, b=1; repeat n times: (a,b) = (b, a+b);` → `a`. `O(n)` time, `O(1)` space.
Recursion `fib(n-1)+fib(n-2)` = `O(φⁿ)` — mat likho. Matrix-power / fast-doubling =
`O(log n)` (solutions file).
</details>

### A7. Factorial with overflow detection
`factorial(unsigned n)` `uint64_t` mein; jahan overflow ho wahan `-1` (ya
`std::optional`) lauta do.
`Pattern:` check `result > MAX / i` before multiplying.
<details><summary>Approach</summary>

Loop `i = 2..n`: agar `res > UINT64_MAX / i` → overflow, return sentinel; warna
`res *= i`. `20!` fits, `21!` nahi. `O(n)`.
</details>

### A8. Temperature table
`c` = `0, 10, 20, … 100` ke liye Celsius→Fahrenheit table print karo, columns
aligned.
`Pattern:` `F = C*9/5 + 32`; `std::setw`.
<details><summary>Approach</summary>

Integer math rakho jab tak decimals na chahiye. `<iomanip>` `std::setw(6)`.
Loop `O(1)` (fixed range). Trap: `9/5` integer = `1` — `C*9/5` likho, `C*(9/5)`
nahi.
</details>

### A9. Count set bits (popcount)
`int popcount(uint32_t x)` — teen tareeke: naive, Kernighan, builtin.
`Pattern:` `x & (x-1)` lowest set bit clear karta hai.
<details><summary>Approach</summary>

Naive: 32 iterations `x >> i & 1`. Kernighan: `while (x) { x &= x-1; ++c; }` —
`O(set bits)`. Builtin: `std::popcount(x)` (C++20) — ek `POPCNT` instruction.
Sabko cross-check karo.
</details>

### A10. Fast exponentiation
`ipow(int64_t base, unsigned exp)` — `O(log exp)`.
`Pattern:` square-and-multiply on the bits of `exp`.
<details><summary>Approach</summary>

`res = 1; while (exp) { if (exp & 1) res *= base; base *= base; exp >>= 1; }`.
`O(log exp)` multiplies. Modular version (`% m` har step) = A-level HFT/crypto
tool.
</details>

### A11. Decimal to binary string
`std::string toBinary(uint32_t n)` — no leading zeros, `n==0 → "0"`.
`Pattern:` collect remainders, reverse; or walk bits MSB→LSB.
<details><summary>Approach</summary>

MSB-first: pehla set bit dhundo (`31 - std::countl_zero(n)`), wahan se `n >> i & 1`
append. `n==0` special-case. `O(32)`.
</details>

### A12. Leap year
`bool isLeap(int y)`.
`Pattern:` divisible by 4, except centuries not divisible by 400.
<details><summary>Approach</summary>

`(y % 4 == 0 && y % 100 != 0) || y % 400 == 0`. `O(1)`. 1900 → no, 2000 → yes,
2024 → yes.
</details>

---

## Part B — Medium (~20 min each)

### B1. Roman ↔ Integer
Dono direction: `int fromRoman(std::string)`, `std::string toRoman(int)` (`1..3999`).
`Pattern:` subtractive pairs (IV, IX, XL, …).
<details><summary>Approach</summary>

**fromRoman:** har symbol ki value lo; agar current < next → subtract, warna add.
`O(n)`. **toRoman:** greedy — `{1000:"M", 900:"CM", 500:"D", 400:"CD", …}` table pe
ghar bana ke ghatate jao. `O(1)` (fixed table).
</details>

### B2. Collatz
`n > 0` do; kitne steps mein `1` aata hai, aur raaste mein sabse bada value kya
aaya.
`Pattern:` `n` even → `n/2`, odd → `3n+1`.
<details><summary>Approach</summary>

Loop jab tak `n != 1`. `3n+1` overflow se bachne ke liye `uint64_t` (aur bade
inputs pe `__int128` / detect). Steps unbounded theoretically, practically chhote.
Track `maxSeen`.
</details>

### B3. Sieve of Eratosthenes
`n` tak ke saare primes. `n` up to `10⁷`.
`Pattern:` mark multiples starting at `i*i`.
<details><summary>Approach</summary>

`std::vector<bool>` (ya `std::vector<char>` — tez, kam bit-twiddle) `is_prime`,
sab true; `i` `2..√n`: agar `is_prime[i]`, `j = i*i; j += i` karke mark false.
`O(n log log n)` time, `O(n)` space. `i*i` `size_t` mein.
</details>

### B4. Digital root
`dr(n)` — digits ka sum lo, single digit tak repeat karo. O(1) chahiye.
`Pattern:` `1 + (n-1) % 9` for `n > 0`.
<details><summary>Approach</summary>

Naive: nested loop, `O(log n · iterations)`. Closed form: `n == 0 → 0`, warna
`1 + (n - 1) % 9`. `O(1)`. (Kyunki `10 ≡ 1 (mod 9)`.)
</details>

### B5. Palindrome number
`bool isPalindrome(int n)` — **string mein convert kiye bina**. Negatives → false.
`Pattern:` half reverse karo, compare.
<details><summary>Approach</summary>

Pehli half consume karke `rev` banao jab tak `rev >= n_remaining`. Odd digits pe
`rev/10 == n`. Poora reverse karke compare karna bhi chalta hai par overflow risk;
half-reverse safe. `O(log n)`.
</details>

### B6. Arbitrary base conversion
`std::string convert(long value, int base)` — `base` `2..36`, negatives handle.
`Pattern:` repeated `% base`, digit map `0-9a-z`.
<details><summary>Approach</summary>

Sign nikaalo, `unsigned` pe kaam karo (warna `INT_MIN` negate = UB). `digits[r]`
push, phir reverse. `base` invalid → throw / empty. `O(log_base value)`.
</details>

### B7. Integer square root
`uint64_t isqrt(uint64_t n)` — largest `r` with `r*r <= n`. No `<cmath>`.
`Pattern:` binary search, or Newton.
<details><summary>Approach</summary>

Binary search `r` in `[0, 2³²]`: `mid*mid <= n` (overflow: `mid <= n/mid`).
`O(log n)`. Newton: `x = (x + n/x) / 2` converge — tez par edge cases (`n=0`,
last step overshoot). `std::sqrt` pe `double` precision `2⁵³` ke baad jhooth
bolta hai — isqrt exact hai.
</details>

### B8. nCr without overflow
`uint64_t nCr(int n, int r)` — result fits in `uint64_t` maan lo, par
intermediate na overflow ho.
`Pattern:` multiply and divide alternately.
<details><summary>Approach</summary>

`r = min(r, n-r)`; `res = 1; for i in 1..r: res = res * (n - r + i) / i;` — har
step `res * (…)` `i` se exactly divisible hota hai (running product of `i`
consecutive integers), isliye order safe. `O(r)`.
</details>

### B9. Trailing zeros in n!
`int trailingZeros(int n)` — `n!` ke end mein kitne `0`.
`Pattern:` count factors of 5.
<details><summary>Approach</summary>

`n/5 + n/25 + n/125 + …` jab tak term `> 0`. (Factors of 2 hamesha zyada hote
hain.) `O(log₅ n)`. `100! → 24`.
</details>

### B10. Power-of-two bit tricks
Teen functions: `bool isPow2(uint64_t)`, `uint64_t nextPow2(uint64_t)`,
`uint64_t lowestSetBit(uint64_t)`.
`Pattern:` `x & (x-1)`, `x & -x`.
<details><summary>Approach</summary>

`isPow2`: `x && !(x & (x-1))`. `lowestSetBit`: `x & (~x + 1)` = `x & -x` (unsigned
`-x` well-defined). `nextPow2`: `std::bit_ceil(x)` (C++20), ya bit-smear
(`x--; x|=x>>1; … x|=x>>32; x++;`). Sab `O(1)`.
</details>

### B11. Overflow-safe midpoint
`mid(lo, hi)` for a binary search. `lo + hi` overflow kar sakta hai.
`Pattern:` `lo + (hi - lo) / 2`.
<details><summary>Approach</summary>

`lo + (hi - lo) / 2` — never overflows jab `lo <= hi` aur dono same sign. Signed
`lo, hi` dono negative bade ho to bhi safe. `std::midpoint(lo, hi)` (C++20) ye
sahi karta hai aur rounding-toward-`lo` guarantee deta hai. Naive `(lo+hi)/2` ka
bug 2006 mein `binarySearch` JDK mein mila tha.
</details>

### B12. Modular exponentiation
`uint64_t powmod(uint64_t a, uint64_t e, uint64_t m)` — `a^e mod m`.
`Pattern:` square-and-multiply, reduce every step.
<details><summary>Approach</summary>

A10 jaisa, par har multiply ke baad `% m`. `a*a` overflow: `m < 2³²` ho to
`uint64_t` kaafi; warna `__int128` ya mulmod. `O(log e)`. Fermat / Miller–Rabin
ka core.
</details>

---

## Part C — Hard / discussion

### C1. Segmented sieve
`[L, R]` range ke primes, `R` up to `10¹²`, par `R - L <= 10⁶`. Full sieve fit
nahi hoga.
<details><summary>Approach</summary>

Pehle `√R` tak ke base primes sieve karo (`~10⁶` bool). Phir `[L, R]` ka ek
`R-L+1` size ka bool block: har base prime `p` ke liye `[L,R]` mein pehla multiple
`max(p*p, ((L + p - 1)/p) * p)` se mark karo. Memory `O(√R + (R-L))`, time
`O((R-L) log log R + √R)`.
</details>

### C2. The `int` loop counter trap
`for (int i = 0; i < n; ++i)` — `n` kab UB / infinite loop deta hai? Fix?
<details><summary>Answer</summary>

`n > INT_MAX` possible ho (e.g. `n` `size_t` hai, `> 2³¹-1`) → `i` `INT_MAX` pe
`++i` = signed overflow UB; practically wrap to `INT_MIN`, loop kabhi khatam
nahi. Fix: counter type ko range se match karo — `for (std::size_t i = 0; i <
n; ++i)`, ya `n` ko `int` mein tabhi lo jab pakka `<= INT_MAX`. `-fsanitize=
undefined` ye pakadta hai.
</details>

### C3. Recursion → iteration
`factorial`, `fib`, aur Ackermann `A(m,n)` — kaunse ko iteration mein badalna
trivial hai, kaunsa explicit stack maangta hai, aur kyun?
<details><summary>Answer</summary>

`factorial`/`fib`: single linear recursion / tail-recursive shape → ek loop +
O(1)-O(n) vars. `fib` ke double-recursion ko bhi bottom-up `dp` se O(n) loop.
Ackermann: non-primitive-recursive, nesting depth data-dependent aur unbounded →
`std::vector` ko manual call stack ki tarah use karna padta hai (frame = `(m,
n)`). Lesson: recursion = implicit stack; agar recursion depth input pe depend
karti hai aur badi ho sakti hai, stack overflow risk — heap pe explicit stack
lo.
</details>

### C4. Float sum order
Ek million `float` (e.g. `0.1f` repeated, plus kuch bade) ko forward, backward,
aur pairwise (tree) sum karo. Teeno alag jawab. "Sahi" kaunsa?
<details><summary>Answer</summary>

Float addition associative **nahi** hai (rounding). Chhote values ko bade running
sum mein add karne pe woh "swallow" ho jaate hain (`bigSum + tiny == bigSum`).
Sorted-ascending sum better; **pairwise / Kahan summation** best — error `O(1)`
vs naive `O(n)`. HFT/quant: accumulator ko `double` rakho even for `float` data,
ya Kahan. `-ffast-math` compiler ko reorder karne deta hai (tez, par
non-deterministic) — production risk pe socho.
</details>

### C5. Branchless min/max/abs/sign
Bina `if`/`?:` ke `min`, `max`, `abs`, `sign` likho. Phir `-O2 -S` dekho —
compiler ne pehle hi kar diya?
<details><summary>Answer</summary>

`abs`: `(x ^ (x >> 31)) - (x >> 31)` (arithmetic shift). `sign`: `(x > 0) - (x <
0)`. `min`: `y ^ ((x ^ y) & -(x < y))`. **Par** modern GCC/Clang `std::min`,
`std::abs`, aur simple ternary ko already `CMOV` / `SMIN` mein compile karte hain
`-O2` pe — hand-rolled bit-hack aksar **same ya slower** hota hai aur padhne mein
bura. Measure; default: clear code likho.
</details>

### C6. The loop that vanishes
```cpp
int sum = 0;
for (int i = 0; i < n; ++i) sum += arr[i];
// sum kabhi use nahi hota
```
`-O2` pe loop poora gayab. Kyun, aur kab ye "UB deletes my code" ban jaata hai?
<details><summary>Answer</summary>

`sum` observable nahi (no `volatile`, print, return) → dead-store elimination →
loop bhi dead. Benchmark likhte waqt result ko `volatile`, `benchmark::DoNotOptimize`,
ya external sink mein daalo. Alag khatra: agar loop body mein UB hai (signed
overflow, OOB read) compiler ye **maan leta hai ki UB hota hi nahi** aur us
assumption pe poora branch/loop kaat sakta hai — "correct but empty" binary.
`-fsanitize=undefined` + `-Wall` pehle.
</details>

---

## Next
→ [`02-arrays-strings-problems.md`](02-arrays-strings-problems.md)
