# 15 — The UB catalog (50+ cases)

## Prerequisites
- `23-ERROR-HANDLING` file 13 (UB intro — read that first if you haven't)
- Everything in this folder so far
- [`examples/08_ub_examples.cpp`](examples/08_ub_examples.cpp)

## Yeh topic abhi kyun
Folder 23 file 13 ne UB ka concept diya. Yeh file ek **reference catalog** hai —
har category, har common case, ek line mein. Iska maqsad: aap ise scan karein aur
apne code mein "yeh toh UB hai" pehchan sakein. File 16 batata hai compiler in
sabpe kaise optimize karta (aur galat answer deta).

**UB = standard koi requirement nahi lagata.** Compiler maan leta hai UB kabhi
nahi hoti, aur us par optimize karta — dead-code elim, range narrowing, load
reuse. "-O0 pe kaam kar gaya" proof nahi hai.

---

## Memory & pointers

| # | UB | Fix |
|---|---|---|
| M1 | Out-of-bounds array/`vector`/`string` **read** (`a[n]`, `v[v.size()]`, `p[-1]`) | bounds check; `.at()` off hot path; `std::span` |
| M2 | Out-of-bounds **write** | same |
| M3 | **Use-after-free** — deref a `delete`d / `free`d pointer | RAII, smart pointers, ownership discipline |
| M4 | **Use-after-return** — deref a pointer/ref to a destroyed automatic | don't return `&local`; `-Wreturn-stack-address` |
| M5 | **Double free** (`delete p; delete p;`) | null after delete, or RAII |
| M6 | `delete` / `free` mismatch (`new[]`→`delete`, `malloc`→`delete`, `new`→`free`) | pair them; `unique_ptr` |
| M7 | Deref **`nullptr`** | check first (before the deref, not after — file 16) |
| M8 | Deref an **uninitialized / dangling / wild** pointer | initialize (`T* p = nullptr;`) |
| M9 | **Read an uninitialized** non-`unsigned char` variable | `int x{};`, `T obj{};` |
| M10 | **Misaligned access** through a pointer needing stricter alignment | `alignas`, `std::align`, `memcpy` |
| M11 | Pointer arithmetic **outside** `[array, array+n]` (except one-past-end, which you can't deref) | keep indices in range |
| M12 | Comparing / subtracting pointers into **different** arrays/objects (`<`, `-`) | only within one array |
| M13 | Accessing a member/element through a pointer to storage where **no object of that type lives** (`(T*)malloc` for non-implicit-lifetime `T`) | placement new (file 09) |
| M14 | Using a pointer to an object whose **lifetime ended** (storage reused) without `std::launder` | fresh pointer / `std::launder` (file 02, 09) |
| M15 | `memcpy` with **overlapping** src/dst | `memmove` |
| M16 | `memcpy`/`memset` of a **non-trivially-copyable** type | member-wise; only trivially-copyable |
| M17 | Passing `nullptr` where the function requires non-null (`std::string(nullptr)`, `memcpy(nullptr, ...)` with n>0) | pass valid pointers |
| M18 | Modifying a **string literal** (`char* s = "x"; s[0] = 'y';`) | `const char*`; a local `char[]` if you need to mutate |

## Integers & arithmetic

| # | UB | Fix |
|---|---|---|
| I1 | **Signed integer overflow** (`INT_MAX + 1`, `INT_MIN` negation, `abs(INT_MIN)`) | check first; `__builtin_add_overflow`; unsigned; `-fwrapv` (defines it) |
| I2 | **Division by zero** (`a / 0`, `a % 0`) — integer | check divisor |
| I3 | `INT_MIN / -1`, `INT_MIN % -1` (overflows the result) | guard |
| I4 | **Shift count `>=` width** (`x << 32` for 32-bit `x`) or **negative** | mask the count / use a wider type |
| I5 | Left-shift of a negative value (UB **pre-C++20**; defined C++20+) | use unsigned; or C++20 |
| I6 | Left-shift such that the result **overflows the signed type** (`1 << 31` for `int`) | `1u << 31` |
| I7 | Converting an **out-of-range floating value to an integer** (`(int)1e20`, `(int)NAN`) | range-check; `std::lround` etc. |
| I8 | Converting a pointer to a too-small integer type and back | `std::uintptr_t` |

## Lifetime & objects

| # | UB | Fix |
|---|---|---|
| L1 | Accessing an object **before** its lifetime starts / **after** it ends | file 02 |
| L2 | Constructor / destructor **before completion / after start** touching the whole object | careful ordering |
| L3 | **Modifying a truly-`const` object** (via `const_cast` + write, or a mutable alias) | don't; design `const`-correctly |
| L4 | Calling a **pure virtual** from a ctor/dtor | don't; a virtual call there resolves to the current class |
| L5 | Returning a **reference/pointer to a local** | return by value |
| L6 | Using a **dangling reference** (temporary destroyed, file 05) | lifetime-extend or copy |
| L7 | **`this`** used after the object is destroyed (e.g. a callback fires post-destruction) | unregister in the dtor |
| L8 | Reading a **non-active `union` member** (C++) | `std::bit_cast` / tagged struct (file 11) |
| L9 | `delete` through a base pointer with a **non-virtual** destructor | virtual dtor |
| L10 | Placement-new **array form** overflowing the buffer (cookie bytes) | loop of scalar placement news |

## Sequencing & expressions

| # | UB | Fix |
|---|---|---|
| S1 | Multiple **unsequenced modifications** of the same scalar (`i = i++ + ++i;`, `a[i] = i++;`) | split into statements |
| S2 | Modify and separately read the same scalar unsequenced (`f(i, i++)` pre-C++17 in some cases) | one modification per full-expression |
| S3 | **Infinite loop with no side effects / no I/O / no volatile / no atomic** (compiler may assume forward progress) | do something observable, or it's a real bug |
| S4 | Reaching the **end of a value-returning function** without `return` (not `main`) | always return; `-Wreturn-type` |
| S5 | `std::unreachable()` **reached**; `[[assume(expr)]]` with `expr` **false** | only where truly impossible; `assert(false)` first (folder 23 file 12) |
| S6 | Recursion / large `alloca` **overflowing the stack** | bound depth; heap for big buffers |

## Types & aliasing

| # | UB | Fix |
|---|---|---|
| T1 | **Strict-aliasing violation** — access an object through an incompatible-type glvalue (`*(int*)&floatvar`) | `std::bit_cast` / `memcpy` (file 10) |
| T2 | `reinterpret_cast<Struct*>(byte_buffer)` + deref (no `Struct` object there) | `memcpy` into a real `Struct` |
| T3 | Calling a function through an **incompatible function pointer type** | match the type exactly |
| T4 | `bool` holding a value other than 0/1 (from a bad `memcpy`/union) | don't `memcpy` into a `bool` |
| T5 | Enum holding a value **outside its range** (for a fixed-underlying-type enum it's fine; for an unscoped enum without fixed type, out-of-range is UB) | use `enum class : T` |

## Library preconditions

| # | UB | Fix |
|---|---|---|
| P1 | `v.front()` / `v.back()` / `v[0]` on an **empty** container | check `!empty()` |
| P2 | Dereferencing an **empty `std::optional`** (`*opt`) / `std::get` wrong `variant` alternative (that one throws) | `has_value()` / `opt.value()` |
| P3 | Using an **invalidated iterator** (after `push_back` realloc, `erase`, etc.) | re-acquire; index; `reserve` |
| P4 | `std::string::operator[](size())` **write** (read of the null terminator is OK) | don't write there |
| P5 | Passing an invalid range to an algorithm (`first > last`, iterators from different containers) | correct ranges |
| P6 | Comparator that isn't a **strict weak order** (`<` that returns true for equal, or inconsistent) → `std::sort` UB | correct comparator |
| P7 | Calling a **moved-from** object's method that has a precondition (moved-from is *valid but unspecified*) | reassign before use |
| P8 | `std::vector<bool>` treated as a real `bool` array (it's a bitfield proxy) — not UB but a common trap | `std::vector<char>` / `std::bitset` |

## Concurrency (folder 26/27)

| # | UB | Fix |
|---|---|---|
| C1 | **Data race** — two threads, one non-atomic object, ≥1 write, no synchronization | `std::atomic`, mutex, message passing |
| C2 | Using a `std::mutex` / `std::condition_variable` after it's destroyed | lifetime discipline |
| C3 | `notify` / `wait` on a CV without holding the associated lock for the predicate check | predicate form of `wait` under the lock |

## The "compiler assumes it can't happen" ones

| # | UB | What the optimizer does |
|---|---|---|
| U1 | `int x = *p; if (!p) ...` | assumes `p != nullptr` → deletes the check (file 16) |
| U2 | `for (int i = 0; i <= n; ++i)` where `i` could overflow | assumes the loop terminates → vectorizes / unrolls |
| U3 | `x + 1 > x` (signed) | assumes true → folds to `1` |
| U4 | `p + n < p` (overflow check on a pointer) | assumes false → folds to `0` |
| U5 | A branch that would be UB if taken | assumes not taken → prunes it |

---

## > **HFT relevance**
> HFT builds `-O2`/`-O3 -march=native -flto`, often `-ffast-math`, sometimes
> `-fno-exceptions -fno-rtti`. This is the **most UB-aggressive** environment: a
> bug that's a harmless garbage value at `-O0` can mis-vectorize a hot loop,
> corrupt an order, or delete a safety check at `-O2 -flto`. Practice:
> - **CI runs the full suite under `-fsanitize=address,undefined` and (separately)
>   `-fsanitize=thread`.** Zero tolerance — every hit is a bug.
> - **`static_assert` layout/size/alignment** on every wire/ABI struct.
> - **`std::bit_cast` / `memcpy`** for all punning; **fixed-width integer types**
>   with explicit overflow checks on accumulators (position, notional).
> - **`assert` hot-path invariants** (debug/CI); **`std::unreachable()` only after
>   an `assert(false)`** (folder 23 file 12).
> - **RAII / smart pointers** — kills the whole M3/M4/M5/M6/L9 family.
> - **`-fwrapv` is a discussion**, not a default — it defines signed overflow
>   (removing that UB) at the cost of some loop optimizations; most shops instead
>   write overflow-safe code and keep the optimizations.

---

## Hands-on

```bash
./build.ps1 25-OBJECT-MODEL/examples/08_ub_examples.cpp        # the catalog, mostly #if 0
# Linux:
g++ -std=c++20 -O1 -g -fsanitize=address,undefined examples/08_ub_examples.cpp -o ub && ./ub
```

Un-`#if 0` one case at a time and run under ASan/UBSan (Linux) — each produces a
precise diagnostic. Then compile the same at `-O2` with **no** sanitizer and see
some "work", some crash, some give a surprising answer (file 16).

---

## ⚠️ Traps (meta)

### Trap 1 — "it works at `-O0`" as a correctness test
`-O0` hides most UB. Test at `-O2`+ and under sanitizers.

### Trap 2 — assuming `unsigned` overflow is UB
Unsigned **wraps** (defined, mod 2^n). Only **signed** overflow is UB.

### Trap 3 — "reading uninitialized memory just gives garbage"
For most types it's UB (the compiler can assume it never happens → propagate
based on a nonsense value).

### Trap 4 — `if (ptr + n < ptr)` as an overflow check
Pointer overflow is UB → the compiler folds this to `false`. Compare the sizes as
integers instead.

### Trap 5 — a comparator that isn't a strict weak order
`std::sort` with `[](auto a, auto b){ return a <= b; }` → UB (can crash / loop /
corrupt). Use `<`.

### Trap 6 — moved-from object assumed empty/zero
It's "valid but unspecified" — you may destroy it or assign to it, but don't
assume its state.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "UB means the program crashes" | Anything — crash, garbage, deleted code, "works today" |
| "unsigned overflow is UB" | Unsigned wraps; signed overflow is UB |
| "`-O0` proves it's not UB" | `-O0` hides it; `-O2`/`-flto` + sanitizers expose it |
| "reading uninitialized = garbage value" | UB for most types; the compiler can build on the nonsense |
| "one-past-end pointer is invalid" | Forming it is legal; *dereferencing* it is UB |
| "`std::sort` with `<=` is fine" | Not a strict weak order → UB |

---

## Exercises

1. **UB or defined:** (a) `unsigned u = 0; --u;` (b) `int i = INT_MAX; ++i;`
   (c) `int x; return x;` (d) `1u << 31` (e) `-1 << 1` in C++20 (f) `int a[3];
   int* p = a + 3;`

   <details><summary>Answer</summary>

   (a) defined — wraps to `UINT_MAX`. (b) UB — signed overflow. (c) UB — read of
   uninitialized `int`. (d) defined — `2147483648u`. (e) defined in C++20 (was UB
   pre-C++20). (f) defined — `a + 3` is the valid one-past-end pointer; only
   *dereferencing* `*p` would be UB.
   </details>

2. **Spot 3 UBs:**
   ```cpp
   int a[4];
   int s = 0;
   for (int i = 1; i <= 4; ++i) s += a[i];
   int* q;
   return s + *q;
   ```

   <details><summary>Answer</summary>

   (1) `a` is uninitialized → reading `a[i]` is UB. (2) `i <= 4` reads `a[4]` —
   out of bounds. (3) `q` is uninitialized → `*q` is a wild dereference. (Also `s`
   depends on all of the above.)
   </details>

3. **Fix the overflow check:** `bool would_overflow(int a, int b) { return a + b <
   a; }` — make it correct.

   <details><summary>Answer</summary>

   `a + b` is signed overflow UB, so the compiler folds `a + b < a` unreliably.
   Use `__builtin_add_overflow(a, b, &out)` (returns true on overflow), or check
   `b > 0 ? a > INT_MAX - b : a < INT_MIN - b`.
   </details>

4. **Comparator:** why does `std::sort(v.begin(), v.end(), [](const T& a, const T&
   b){ return a.score >= b.score; })` risk a crash?

   <details><summary>Answer</summary>

   `>=` is reflexive (`a >= a` is true) → it's not a *strict* weak order. `std::sort`
   requires strict weak ordering; violating it is UB — it can read out of bounds
   / infinite-loop / corrupt. Use `>` (or `<`).
   </details>

5. **Classify by category:** use-after-free; `INT_MIN / -1`; data race;
   `i = i++;`; `*(float*)&intvar`.

   <details><summary>Answer</summary>

   use-after-free → memory (M3). `INT_MIN / -1` → integer (I3). data race →
   concurrency (C1). `i = i++;` → sequencing (S1). `*(float*)&intvar` → aliasing
   (T1).
   </details>

---

## Interview questions

1. UB ki 5 categories — ek-ek example.
2. Signed vs unsigned overflow — kaunsa UB, kaunsa defined?
3. Uninitialized read UB kyun (garbage value se aage)?
4. One-past-end pointer — form karna legal, deref UB — samjhao.
5. `std::sort` comparator ka requirement, violate karne ka natija.
6. "Compiler assumes UB can't happen" — 3 concrete cases (null-check, overflow
   loop, overflow comparison).
7. Moved-from object ka state — kya guarantee?
8. Sanitizers CI mein kyun, production mein kyun nahi?

---

## Next
→ [`16-compiler-assumptions.md`](16-compiler-assumptions.md)
