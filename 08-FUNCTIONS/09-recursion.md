# 09 — Recursion

## Prerequisites
- [`05-the-call-stack.md`](05-the-call-stack.md) (stack frames, unwinding)
- [`02-declaration-vs-definition.md`](02-declaration-vs-definition.md) (forward declaration — mutual recursion)
- `07-LOOPS/` (iteration — comparison ke liye)

## Yeh topic abhi kyun
Recursion = ek function jo **khud ko call** karta hai. Kuch problems recursively
sochna natural hai: trees, divide-and-conquer, grammars, "chhoti problem + baaki".
Par har recursive call ek **stack frame** khaata hai (lesson 05) — deep recursion
= stack overflow. Aur galat likha to exponential blowup. Yeh sab yahan.

---

## Anatomy — 2 zaroori hisse

```cpp
std::uint64_t factorial(int n) {
    if (n <= 1) return 1;                    // 1. BASE CASE  -- recursion RUKTA hai yahan
    return static_cast<std::uint64_t>(n)     // 2. RECURSIVE CASE -- chhoti problem pe khud ko call,
         * factorial(n - 1);                 //    base ki taraf badhta hua (n -> n-1)
}
```

| Hissa | Kaam | Bina iske |
|---|---|---|
| **Base case** | recursion rokna | infinite recursion → stack overflow |
| **Recursive case** | problem ko chhota karna, base ki taraf | agar chhota na ho → infinite |

**Base case pehle likho. Phir poocho: "recursive call base ke KAREEB jaa raha hai?"**

---

## Trace — `factorial(4)`

```
factorial(4)  → 4 * factorial(3)
                    factorial(3) → 3 * factorial(2)
                                       factorial(2) → 2 * factorial(1)
                                                          factorial(1) → 1        ← BASE, unwinding shuru
                                       factorial(2) → 2 * 1 = 2
                    factorial(3) → 3 * 2 = 6
factorial(4)  → 4 * 6 = 24
```

Peak pe **4 frames** stack pe (lesson 05 ka diagram). Har `return` pe ek frame
pop. `factorial(4)` ka `n` aur `factorial(3)` ka `n` **alag memory** — isli ye
recursion kaam karta hai.

`examples/04_recursion.cpp` — factorial, fib, sumDigits, power, gcd, aur depth
counter.

---

## ⚠️ Recursion vs Algorithm — Fibonacci ka sabak

### NAIVE — har call DO calls → O(2ⁿ) → disaster

```cpp
std::uint64_t fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);      // ⚠️ same fib(k) bar-bar compute hota hai
}
```

`fib(5)` ka call tree:
```
                fib(5)
          fib(4)        fib(3)
       fib(3) fib(2)  fib(2) fib(1)
       ...     ...     ...
```
`fib(3)` **do baar** compute hua. `fib(2)` teen baar. Exponential.

### Measured (`examples/04_recursion.cpp`, GCC 15.1, `-O2`)

```
  fibNaive(30) =   832040     [~2.2 ms]
  fibNaive(35) =  9227465     [~23 ms]     ← n +5 → ~11x slower
  fibNaive(40) = 102334155    [~290 ms]    ← n +5 → ~11x again
  fibNaive(50)  → minutes.    fibNaive(90) → longer than the universe.

  fibMemo(90)   [~0.001 ms]                ← O(n)
  fibIter(90)   [instant]                  ← O(n), O(1) space
```

### MEMOIZED — har n ek hi baar → O(n)

```cpp
std::uint64_t fib(int n, std::vector<std::int64_t>& cache) {
    if (n < 2) return n;
    if (cache[n] >= 0) return cache[n];              // pehle compute kiya? wahi lauta do
    return cache[n] = fib(n-1, cache) + fib(n-2, cache);
}
```

### ITERATIVE — no recursion, O(n), O(1) space

```cpp
std::uint64_t fib(int n) {
    if (n < 2) return n;
    std::uint64_t a = 0, b = 1;
    for (int i = 2; i <= n; ++i) { std::uint64_t c = a + b; a = b; b = c; }
    return b;
}
```

> **Sabak:** slow fib recursion ki galti **recursion** nahi thi — **algorithm**
> (repeated work) thi. Memoized recursion bhi O(n) hai. Aur jahan iteration
> natural ho, woh recursion se kam stack, kam overhead deti hai.

---

## Recursion ki cost

| | Recursion | Iteration |
|---|---|---|
| Stack | `n` frames (peak) — overflow risk | O(1) frame |
| Call overhead | har call: prologue/epilogue, `call`/`ret` (~2 ns, lesson 10) | loop branch (~free) |
| Readability | trees, divide-conquer pe better | linear/counting pe better |
| Compiler | TCO kabhi (neeche) | hamesha tight loop |

**Deep, linear recursion (`n = 10⁶`) → stack overflow.** `sumList(a, n) = a[0] +
sumList(a+1, n-1)` — 10⁶ frames → crash. Loop use karo.

---

## Tail recursion aur TCO

**Tail call** = recursive call function ka **bilkul aakhri** kaam ho (uske baad
kuch nahi):

```cpp
// NOT tail-recursive -- return ke baad `n *` bacha hai
std::uint64_t fact(int n) {
    return n <= 1 ? 1 : n * fact(n - 1);       // multiply AFTER the call
}

// Tail-recursive -- call ke baad kuch nahi
std::uint64_t factTail(int n, std::uint64_t acc = 1) {
    return n <= 1 ? acc : factTail(n - 1, acc * n);   // call = last thing
}
```

**Tail Call Optimization (TCO):** compiler tail-recursive call ko ek **loop**
(reuse the same frame) mein badal sakta hai → no stack growth. `-O2` pe GCC/Clang
aksar karte hain.

⚠️ **C++ TCO ki GUARANTEE nahi deta** (Scheme deta hai). `-O0` pe nahi hota,
`-O2` pe bhi kabhi nahi (destructors, debugging). Isli ye: **deep recursion ke
liye TCO pe bharosa mat karo** — iteration mein khud badlo.

`examples/05_stack_overflow.cpp` mein `frameHog` buffer + `-O0` isi liye — taaki
crash reliably dikhe (TCO na ho).

---

## Mutual recursion

```cpp
bool isEven(int n);
bool isOdd(int n);                          // forward declaration zaroori

bool isEven(int n) { return n == 0 ? true  : isOdd(n - 1); }
bool isOdd(int n)  { return n == 0 ? false : isEven(n - 1); }
```

`isEven` `isOdd` ko call karta hai jo abhi define nahi hua → **forward
declaration** chahiye (lesson 02). (Yeh ek toy example hai — `n % 2 == 0` behtar.)

---

## Kab recursion natural hai

- **Trees / graphs** — DFS, tree traversal (`node->left`, `node->right`)
- **Divide and conquer** — merge sort, quick sort, binary search
- **Backtracking** — permutations, N-queens, maze solving
- **Grammars / parsing** — recursive-descent parsers
- **Mathematical definitions** — jab problem khud recursively defined ho

...aur inme bhi depth `log n` ho to safe; depth `n` ho to iteration/explicit
stack.

---

## Recursion → iteration (explicit stack)

Jab depth bahut ho:

```cpp
// Recursive tree sum
long long sum(Node* n) {
    if (!n) return 0;
    return n->value + sum(n->left) + sum(n->right);
}

// Iterative -- apna stack heap pe (overflow nahi)
long long sum(Node* root) {
    long long total = 0;
    std::vector<Node*> stack;
    if (root) stack.push_back(root);
    while (!stack.empty()) {
        Node* n = stack.back(); stack.pop_back();
        total += n->value;
        if (n->left)  stack.push_back(n->left);
        if (n->right) stack.push_back(n->right);
    }
    return total;
}
```

Heap-allocated `std::vector` GBs tak ja sakta hai; call stack sirf MBs.

> **HFT relevance:** Hot path mein recursion **avoid** — unpredictable depth =
> unpredictable stack use + unpredictable latency (har frame ki setup cost, aur
> deep recursion I-cache/D-cache ko thrash kar sakta hai). Order book trees,
> parsers — iteration + preallocated explicit stacks/arenas se. Jahan recursion
> use ho (offline tools, config), depth bound aur TCO-friendly likho. Interview:
> "yeh recursive function ko iterative banao" common hai.

---

## Hands-on

```bash
./build.ps1 fast 08-FUNCTIONS/examples/04_recursion.cpp   # fib benchmark -O2
./build.ps1 08-FUNCTIONS/examples/02_call_stack_trace.cpp  # recursion frames

# TCO dekho:
g++ -O2 -S -masm=intel -o - <<'EOF' | c++filt | grep -A15 'factTail'
unsigned long factTail(int n, unsigned long acc) {
    return n <= 1 ? acc : factTail(n - 1, acc * n);
}
EOF
# -O2 pe koi `call factTail` nahi -> loop ban gaya (TCO)
```

---

## ⚠️ Traps

### Trap 1 — base case missing / galat
```cpp
int f(int n) { return f(n - 1); }           // ⚠️ n negative jaata rahega -> overflow
int f(int n) { if (n == 0) return 0; return f(n - 2); }   // ⚠️ odd n -> base skip
```

### Trap 2 — recursive call base ke kareeb nahi
```cpp
int f(int n) { if (n <= 0) return 0; return f(n); }       // ⚠️ n badalta hi nahi
```

### Trap 3 — deep linear recursion
```cpp
long long sum(const int* a, int n) {
    return n == 0 ? 0 : a[0] + sum(a + 1, n - 1);         // ⚠️ n = 10^6 -> stack overflow
}
```

### Trap 4 — exponential without memoization
```cpp
int paths(int i, int j) { return (i==0||j==0) ? 1 : paths(i-1,j) + paths(i,j-1); }  // ⚠️ O(2^(i+j))
```

### Trap 5 — TCO pe bharosa
```cpp
long long f(int n, long long acc) { return n == 0 ? acc : f(n-1, acc+n); }
f(10'000'000, 0);   // ⚠️ -O0 pe overflow; -O2 pe shayad theek -- GUARANTEE nahi
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Recursion hamesha slow" | Depends — memoized recursion O(n); overhead thoda zyada |
| "Naive fib slow kyunki recursion" | Nahi — repeated work (algorithm). Memoize → fast |
| "Deep recursion elegant" | `n` frames → overflow. `log n` depth OK |
| "C++ tail calls optimize hote hain" | Kabhi `-O2` pe — **GUARANTEE nahi**. Deep → iteration |
| "Recursive = kam code = better" | Readability sirf trees/divide-conquer pe; loop otherwise |

---

## Exercises

1. **Trace:** `power(2, 5)` recursively (`power(b,e) = e==0 ? 1 : b*power(b,e-1)`).
   Call tree aur frames likho.

2. **Base case bug:** `int f(int n) { if (n == 1) return 1; return n + f(n-1); }`
   — `f(5)`? `f(0)`? `f(-3)`? Fix base case.

3. **Fib 3 ways:** `examples/04_recursion.cpp` chalao. `fibNaive(30/35/40)` ka
   time apni machine pe. Ratio ~11x per +5? `fibMemo`/`fibIter` ka time?

4. **Recursion → iteration:** `int sumDigits(int n)` recursive → loop version.
   `factorial` recursive → loop.

5. **Deep recursion crash:** `long long countDown(long long n) { return n <= 0 ?
   0 : 1 + countDown(n - 1); }` — `countDown(100'000)`, `countDown(10'000'000)`.
   Kaunsa crash? `-O0` vs `-O2` pe alag?

6. **Explicit stack:** ek binary tree ka `int height(Node*)` recursive likho,
   phir iterative (explicit `std::stack`) — deep skewed tree pe dono test.

7. **Tail recursion + TCO:** `sumTo(int n, long long acc)` tail-recursive likho.
   `-O2 -S` se dekho — `call sumTo` hai ya loop ban gaya? `-O0` pe?

8. **Memoize:** `int coins(int amount)` (min coins for amount, denominations
   `{1, 3, 4}`) — naive recursive (exponential), phir memoized. Time compare.

---

## Interview questions

1. Recursive function ke 2 zaroori hisse?
2. Naive Fibonacci exponential kyun? Fix?
3. Recursion vs iteration — cost (stack, overhead), kab kaunsa?
4. Tail call kya hai? TCO? C++ mein guaranteed?
5. Deep recursion se kya hota hai? Iterative alternative kaise?
6. Mutual recursion — forward declaration kyun?
7. `factorial(5)` ke peak pe stack pe kitne frames? Unwinding kab shuru?

---

## Next
→ [`10-inline-functions.md`](10-inline-functions.md)
