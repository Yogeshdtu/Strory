# 06 — Scope aur lifetime

## Prerequisites
- [`05-the-call-stack.md`](05-the-call-stack.md)
- `03-VARIABLES-DATA-TYPES/02-what-is-a-variable.md`
- `02-CPP-FIRST-STEPS/06-braces-blocks-and-scope.md`

## Yeh topic abhi kyun
**Scope** = ek naam kahan-kahan "dikhta" hai (compile-time). **Lifetime** = ek
object kab tak zinda rehta hai (runtime). Yeh do alag cheezein hain, aur inhe
confuse karna dangling references/pointers, static-init bugs, aur "kabhi-kabhi
kaam karta hai" wale UB ka source hai.

---

## Scope — naam ki visibility

```cpp
int g = 10;                    // GLOBAL / file scope -- poori file mein

void f(int p) {                // p -- function parameter scope
    int a = 1;                 // FUNCTION-LOCAL (block) scope
    {
        int b = 2;             // INNER BLOCK scope
        std::cout << a << b << g;   // teenon dikhte hain
    }
    // b yahan NAHI dikhta
    for (int i = 0; i < 3; ++i) { }  // i -- loop scope
    // i yahan NAHI dikhta
}
// a, b, p, i -- yahan koi nahi
```

Rule: **`{ }` ke andar declared naam, `}` par khatam.** Andar wala scope bahar wale
ko dekh sakta hai, ulta nahi.

### Shadowing
```cpp
int x = 1;
{
    int x = 2;                 // ⚠️ bahar wale x ko "chhupata" hai
    std::cout << x;            // 2
}
std::cout << x;                // 1
```
`-Wshadow` isse warn karta hai — bug ka source. Alag naam do.

---

## Lifetime — object kab zinda

### 1. Automatic (local variables) — sabse common

```cpp
void f() {
    int a = 1;                 // yahan CONSTRUCT
    std::string s = "hi";
    // ...
}                              // yahan DESTRUCT (s ka destructor chalega) -- ulte order mein
```

- Declaration pe banta, enclosing `{ }` ke end pe khatam
- **Stack frame** pe rehta hai (lesson 05)
- Destructors **reverse order** mein (jo baad mein bana, pehle marega) — RAII
  (folder 17)
- Har function call pe **naya** object

### 2. Static (function ke andar) — ek hi baar

```cpp
int nextId() {
    static int counter = 0;    // pehli call pe hi initialize; program end tak zinda
    return ++counter;
}
nextId();  // 1
nextId();  // 2  -- counter apni value yaad rakhta hai
```

- **Ek hi baar** initialize (pehli baar jab control us line se guzarta hai)
- Program ke end tak zinda — value calls ke beech survive karti hai
- **Storage** static/data segment mein (stack pe nahi)
- C++11 se: static local ki initialization **thread-safe** (compiler guard lagata hai)

Use: call count, lazy singleton, cache. ⚠️ Multi-threaded code mein shared
mutable static = data race risk (folder 26).

### 3. Static / global (file scope) — program lifetime

```cpp
int g_count = 0;               // program start pe init (zero-init phase + dynamic-init phase)
```

- `main()` se pehle initialize, `main()` ke baad destruct
- **Static Initialization Order Fiasco**: alag TU ke globals ka init order
  undefined — ek global doosre TU ke global pe depend kare to bug (folder 25).
  Fix: "construct on first use" (function-static).

### 4. Dynamic (heap) — aap decide

```cpp
int* p = new int(42);          // ab banaya
// ...
delete p;                      // ab khatam (ya smart pointer khud kare)
```

Folder 14, 17.

---

## 🔑 Scope ≠ Lifetime

```cpp
static int& counter() {
    static int c = 0;          // SCOPE: sirf is function ; LIFETIME: program bhar
    return c;
}
```

`c` ka **scope** chhota (sirf `counter()` ke andar naam se access), par
**lifetime** poora program. Reference return karna yahan **safe** hai (`c` zinda
rehta hai).

Ulta:
```cpp
int& bad() {
    int local = 5;             // SCOPE: function ; LIFETIME: function
    return local;              // ⚠️ lifetime khatam -> dangling reference -> UB
}
```

---

## ⚠️ Dangling — lifetime khatam, par naam/pointer/reference abhi bhi hai

### Return reference/pointer to local
```cpp
int*  f1() { int x = 1; return &x; }        // ⚠️ dangling pointer
int&  f2() { int x = 1; return x;  }        // ⚠️ dangling reference
std::string_view f3() { std::string s = "hi"; return s; }   // ⚠️ view into dead string
```
Pointer/reference return karne ko GCC `-Wreturn-local-addr` pakadta hai — par upar wale `string_view` case
pe GCC 16.2 `-Wall -Wextra` ne **koi warning nahi di** (chala ke dekha). Clang ke `-Wdangling` /
`-Wreturn-stack-address` kuch aur pakadte hain. Warning pe bharosa mat karo. Fix: **by value return** (RVO se free).

### Reference member to temporary
```cpp
struct Holder { const std::string& ref; };
Holder h{ std::string("temp") };            // ⚠️ temp destroyed after ;, h.ref dangling
```

### `string_view` / `span` outliving source
```cpp
std::string_view sv;
{
    std::string s = "hello";
    sv = s;                                  // sv points into s
}                                            // s destroyed
std::cout << sv;                             // ⚠️ UB -- sv dangling
```

### Iterator/reference invalidation (folder 07, 19)
```cpp
std::vector<int> v = {1, 2, 3};
int& r = v[0];
v.push_back(4);                               // ⚠️ reallocation -> r dangling
```

**Common thread:** kisi cheez ka lifetime khatam ho gaya, par uska "handle"
(pointer / reference / view / iterator) abhi bhi use ho raha hai. Sanitizers
(`-fsanitize=address`) runtime pe pakadte hain.

---

## Const-correctness aur scope

Chhota scope = kam bugs. Variable ko **jitni der zaroori ho utni der** zinda rakho:

```cpp
// ⚠️ Bada scope, mutable
int result;
// ... 20 lines ...
result = compute();
use(result);

// ✅ Chhota scope, const
// ... 20 lines ...
const int result = compute();
use(result);
```

`if`-with-initializer (folder 06 file 06) isi ke liye:
```cpp
if (const auto it = m.find(key); it != m.end()) { use(it->second); }
// it ka scope sirf yeh if
```

---

## Andar kya hota hai

- **Automatic**: stack frame ka ek offset. Prologue frame banata hai, constructor
  us jagah object banata hai, epilogue se pehle destructor chalta hai (scope end),
  epilogue frame gira deta hai.
- **Static local**: data segment mein jagah + ek hidden `bool __guard` flag.
  Pehli call: `if (!guard) { construct(); guard = true; }` (thread-safe lock ke
  saath).
- **Global**: data segment (init) ya BSS (zero-init). `main` se pehle ek
  init-routine list chalti hai.

> **HFT relevance:** Function-static ("construct on first use") config/lookup
> tables ke liye common — par pehli call pe ek branch + possible lock (`-O2` ise
> optimize karta hai par branch rehta hai). Ultra-hot path mein: `constexpr`
> table (lesson 11) ya explicitly-initialized global. Dangling `string_view` /
> `span` HFT parsers mein #1 bug class — zero-copy design mein view ka source
> buffer view se zyada zinda hona chahiye (folder 36, 38). ASan/UBSan CI mein
> mandatory.

---

## Hands-on

```bash
./build.ps1 08-FUNCTIONS/examples/02_call_stack_trace.cpp
```

Ek program: `int& danglingRef()` (local return) — `-Wall` warning dekho, phir
`-fsanitize=address` se chalao. Aur ek `static int` counter function — calls ke
beech value survive karti hai verify karo.

---

## ⚠️ Traps

### Trap 1 — return reference/pointer/view to local (upar)

### Trap 2 — shadowing
```cpp
int count = getCount();
if (cond) { int count = 0; /* ⚠️ bahar wala count nahi badla */ }
```

### Trap 3 — static local in multi-threaded code
```cpp
int& sharedCounter() { static int c = 0; return c; }
// 2 threads ++sharedCounter() -> DATA RACE (init thread-safe hai, mutation nahi)
```

### Trap 4 — static init order (cross-TU global depends on another)
```cpp
// a.cpp:  extern Config cfg;  int x = cfg.value;   // ⚠️ cfg abhi init nahi hua ho sakta
```

### Trap 5 — loop variable ka scope loop ke baad chahiye
```cpp
for (int i = 0; i < n; ++i) if (found(i)) break;
use(i);   // ❌ i scope se bahar -- for se pehle declare karo
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Scope aur lifetime same" | Scope = naam ki visibility (compile); lifetime = object zinda (runtime) |
| "`static` local har call pe reset" | Ek hi baar init; value survive karti hai |
| "Local return by reference se copy bachti" | Frame gayab → dangling → UB |
| "`string_view` return safe hai" | Sirf jab underlying data outlive kare |
| "Shadowing bas style issue" | Aksar bug — `-Wshadow` on |
| "Globals ka init order predictable" | Same TU haan; cross-TU **undefined** |

---

## Exercises

1. **Scope quiz:** har naam ka scope batao —
   ```cpp
   int g = 1;
   void f(int p) {
       int a = 2;
       for (int i = 0; i < 3; ++i) { int b = i; g += b; }
       { int c = 5; a += c; }
   }
   ```

2. **`static` counter:** `int ticket()` jo `1, 2, 3, ...` deta hai (function-static).
   5 baar call karo.

3. **Dangling:** teenon likho, `-Wall` warnings + `-fsanitize=address` output —
   ```cpp
   int* p() { int x = 1; return &x; }
   int& r() { int x = 1; return x; }
   std::string_view s() { std::string t = "hi"; return t; }
   ```

4. **Shadowing bug:** yeh code likho, `-Wshadow` se compile —
   ```cpp
   int total = 100;
   for (int i = 0; i < 3; ++i) { int total = i * 10; }
   std::cout << total;
   ```
   Output? Warning?

5. **string_view lifetime:** ek function `std::string_view firstToken(const std::string&)`
   — safe hai? (Haan, agar caller ki string zinda rahe.) Ab
   `std::string_view firstToken(std::string s)` (by value param) — ab?

6. **Construct on first use:** ek `const std::vector<int>& primes()` jo pehli call
   pe compute karke function-static mein rakhe, baad ki calls turant. Verify.

7. **Lifetime trace:** ek `struct Loud { Loud(){cout<<"ctor ";} ~Loud(){cout<<"dtor ";} };`
   — ek function mein 3 local `Loud` banao, alag blocks mein. ctor/dtor order note karo.

---

## Interview questions

1. Scope aur lifetime mein fark? Ek example jahan scope chhota par lifetime bada.
2. `static` local variable — kab init, kab tak zinda, thread-safe?
3. Return reference/pointer to local — kya hota hai? Compiler warning?
4. Dangling `string_view` kaise banta hai? Kaise bache?
5. Static initialization order fiasco kya hai? Fix?
6. Shadowing kya hai, kyun problem, kaunsa warning?
7. Local objects ke destructors kis order mein chalte hain?

---

## Next
→ [`07-default-arguments.md`](07-default-arguments.md)
