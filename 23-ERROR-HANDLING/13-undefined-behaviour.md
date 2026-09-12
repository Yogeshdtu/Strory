# 13 — Undefined behaviour ka catalog

## Prerequisites
- `12-assertions.md`
- `12-POINTERS`, `14-MEMORY`, `05-OPERATORS` (integer overflow, shifts)
- `03-VARIABLES-DATA-TYPES` file 05 (`int` range, `int32_t`)

## Yeh topic abhi kyun
UB "error" ka sabse khatarnak roop hai kyunki iska koi behaviour hai hi nahi —
crash bhi ho sakta, sahi answer bhi de sakta (aaj), aur `-O2` pe compiler aapki
poori function silently delete kar sakta. UB error handling ke folder mein isliye
hai ki **UB ko handle nahi kiya jaata — usse likha hi nahi jaata**, aur usse
pakadne ke tools (assert, sanitizers) hi aapki defence hain. HFT mein `-O2`/`-O3`
+ aggressive flags = UB ke consequences aur bhi nailhead.

---

## UB, unspecified, implementation-defined — teen alag cheezein

| Term | Matlab | Example |
|---|---|---|
| **Undefined behaviour (UB)** | Standard *koi* requirement nahi lagata. Kuch bhi ho sakta hai — including "compiler assumes it never happens". | signed overflow, OOB access, use-after-free, data race |
| **Unspecified** | Kai valid choices, compiler ek chunta hai, document karna zaroori nahi | function arguments ka evaluation order |
| **Implementation-defined** | Unspecified, par document karna padta hai | `sizeof(int)`, `char` signed hai ya nahi, right-shift of negative |

UB = "you promised this wouldn't happen; compiler builds on that promise."

---

## Compiler UB ke saath kya karta hai

Yeh key mental model: **compiler maan leta hai UB kabhi nahi hoga**, aur us
assumption pe optimize karta hai.

```cpp
int foo(int* p) {
    int x = *p;          // (A) compiler: "p valid hai" (warna UB) -> maan liya
    if (p == nullptr)    // (B) ...toh yeh check DEAD hai -> DELETE
        return -1;
    return x + 1;
}
```

`*p` ne "p non-null" imply kiya (null deref UB hai). Toh `-O2` pe `(B)` ka check
**gayab** ho jaata — kyunki agar `p` null hota toh `(A)` already UB tha, aur
"compiler ko UB ke baare mein sochna nahi". Result: aapka null-check chala hi nahi.

```cpp
for (int i = 0; i <= n; ++i)   // agar `i` overflow kar sakta (n == INT_MAX)...
    sum += a[i];               // ...compiler maan sakta loop terminate hoga
                               //    (signed overflow UB) -> vectorize, unroll aggressively
```

Signed overflow UB hone ki wajah se compiler `i` ko effectively "kabhi wrap nahi
karta" maanta hai — jo optimization ke liye achha, par agar aapka data `INT_MAX`
touch kare to loop misbehave.

---

## Common UB — catalog

### Memory
- **Out-of-bounds** array/`vector`/`string` access (read *ya* write). `v[v.size()]`,
  `p[-1]`.
- **Use-after-free / use-after-return** — freed pointer ya local ka address
  dereference.
- **Double free**, `free`/`delete` mismatch (`new[]` → `delete`, `malloc` → `delete`).
- **Dereferencing `nullptr`** ya uninitialized/dangling pointer.
- **Reading uninitialized** variable (indeterminate value; for most types UB to
  even read).
- **Misaligned access** through a pointer whose type demands more alignment.
- **Type punning via incompatible pointer cast** (strict aliasing) — `float f;
  int i = *(int*)&f;`. Use `std::bit_cast` / `memcpy`.
- **`std::launder` ke bina** placement-new ke baad purane pointer se access (edge).

### Integers / arithmetic
- **Signed integer overflow** — `INT_MAX + 1`. (Unsigned wraps — defined.)
- **Division by zero**, `INT_MIN / -1`, `INT_MIN % -1`.
- **Shift by ≥ width** (`x << 32` for 32-bit `x`) ya **negative shift count**.
- **Left-shift of negative** (`-1 << 1`) — UB pre-C++20, defined C++20+.
- **Conversion of out-of-range floating → integer** (`(int)1e20`).

### Lifetime / objects
- **Accessing object outside its lifetime** (before ctor done / after dtor).
- **Calling virtual function from ctor/dtor** that resolves to a derived override
  (not UB, but a common surprise) — actual UB: calling a pure virtual during
  ctor/dtor.
- **Modifying a `const` object** (one that's *actually* const, e.g. `const int
  x = 5;` then casting away).
- **Returning reference/pointer to a local**.

### Control / sequencing
- **Reaching end of a value-returning function** without `return` (not `main`).
- **Multiple unsequenced modifications** — `i = i++ + ++i;`, `a[i] = i++;`.
- **Infinite loop with no side effects** (compiler may assume forward progress).
- **`std::unreachable()` reached**, `[[assume(x)]]` with `x` false.

### Concurrency
- **Data race** — do threads, ek hi non-atomic object, kam se kam ek write, no
  synchronization. (Folder `26`/`27`.)

### Library
- **Precondition violation** — `std::vector::front()` on empty, `std::optional`
  `operator*` on empty, `std::string::operator[](size())` write, invalid iterator
  use, `std::memcpy` with overlapping ranges (use `memmove`), passing `nullptr`
  to a `std::string(const char*)` ctor.

---

## UB pakadne ke tools

| Tool | Kya pakadta | Kab chalao |
|---|---|---|
| **`-fsanitize=undefined`** (UBSan) | signed overflow, OOB (with bounds info), shift, misalignment, null deref, unreachable | CI, dev |
| **`-fsanitize=address`** (ASan) | heap/stack/global OOB, use-after-free, use-after-return, double free, leaks | CI, dev |
| **`-fsanitize=thread`** (TSan) | data races | CI (concurrency tests) |
| **`assert` + `_GLIBCXX_ASSERTIONS`** | precondition/invariant violations, STL bounds | debug, CI |
| **`-Wall -Wextra -Wconversion -Wshadow` ...** | many *potential* UB sources at compile time | always |
| **Valgrind (memcheck)** | uninit reads, OOB, UAF (slower, no recompile) | Linux, occasional |
| **`-ftrapv` / `-fwrapv`** | trap on / define signed overflow | narrow use |
| **`-D_FORTIFY_SOURCE=2 -O2`** | some buffer overflows in libc calls | release-hardening |

**Sanitizers production build pe nahi** (2–20x slowdown, memory) — CI mein poora
test suite unke saath chalao. Har UBSan/ASan hit = fix, suppress nahi.

> Is repo mein: `./build.ps1 san file.cpp` — ASan+UBSan try karta hai; MinGW pe
> libasan aksar missing hai to fallback `-D_GLIBCXX_ASSERTIONS -fstack-protector-all`
> (STL bounds + canaries; raw C-array OOB nahi pakadta — uske liye Linux/Clang).

---

## Defensive coding — UB likhne se bachna

- **`int32_t`/`int64_t`** jab range matter kare; overflow-prone spots pe pehle
  check (`if (a > INT_MAX - b) ...`), ya `-fwrapv` / builtins
  (`__builtin_add_overflow`).
- **`.at()` / bounds check** hot-path ke bahar; hot path pe `assert(i < n)` +
  invariant se guarantee.
- **RAII / smart pointers** — dangling/UAF/double-free ka poora class khatam.
- **`std::bit_cast` / `memcpy`** for type punning, never pointer cast.
- **Initialize everything** — `int x{};`, `T obj{};`. `-Wuninitialized`.
- **`std::span` / `std::string_view`** — pointer+length ko saath rakho, OOB kam.
- **`enum class` + exhaustive switch + `assert(false)`/`std::unreachable()`** —
  "impossible" case explicit.
- **`-fsanitize` in CI**, `-Werror` on the warning set.

---

## Andar kya hota hai

- UBSan compile pe har risky op ke aage ek check inject karta hai (`__ubsan_handle_*`).
  Overflow → check `a`, `b` before `a+b`; OOB → compare index vs known bound (jab
  bound static ya `_GLIBCXX_ASSERTIONS` se pata ho).
- ASan: har allocation ke around **redzones** + ek **shadow memory** (1 byte
  shadow per 8 bytes app memory) — har load/store se pehle shadow check → OOB/UAF
  turant. `-fsanitize=address` ~2x slow, ~3x memory.
- "Compiler deleted my code": UB-based optimization aksar dead-code elimination +
  range analysis ke through — koi warning nahi (mostly). `-Wnull-dereference`,
  `-Warray-bounds`, `-Waggressive-loop-optimizations` kuch pakadte hain.

---

## > **HFT relevance**
> HFT builds `-O2`/`-O3` + `-march=native` + `-fno-exceptions -fno-rtti`, aur
> aksar `-ffast-math` (jo khud kuch FP guarantees chhodta hai — alag topic). Is
> environment mein UB ke consequences maximally aggressive hain: ek OOB write jo
> `-O0` pe bas ek galat number deta, `-O3` pe pura loop mis-vectorize kar sakta ya
> ek security-relevant corruption. Ek signed-overflow-based loop assumption ek
> exchange message burst pe misfire kar sakta.
>
> Practice:
> - **CI pe ASan + UBSan + TSan** — poora unit/integration suite. Zero tolerance.
> - **`static_assert` layout/size/alignment** har wire struct pe (file `12`).
> - **`assert` hot-path invariants**, `std::unreachable()` after exhaustive
>   dispatch (measured wins), par hamesha `assert(false)` ke saath.
> - **No pointer-cast type punning** — `std::bit_cast<std::uint64_t>(double_px)`.
> - **Integer discipline** — fixed-width types, explicit overflow checks on
>   accumulators (position, notional), `__builtin_*_overflow` where needed.
> - **`-fsanitize` production pe kabhi nahi** — cost. CI ka kaam.
>
> "Kaunse operations UB hain aur compiler unpe kya assume karta hai" — yeh HFT
> C++ interview ka pakka topic hai, kyunki galat samajh yahan directly wrong
> trades / crashes deti hai.

---

## Hands-on

```bash
./build.ps1 san 23-ERROR-HANDLING/examples/04_exception_cost.cpp   # ASan/UBSan (ya fallback)
```

Ek chhoti file likho jisme jaan-boojh ke UB ho:
```cpp
int a[4] = {};
int i = 5;
return a[i];             // OOB read
```
`-O0 -fsanitize=undefined,address` se — diagnostic. Phir `-O2` bina sanitizer —
shayad "kaam kar gaya" (garbage return, no crash) — yahi UB ka dhokha hai.

`int f(int x){ return x+1 > x; }` ko `-O2 -S` se dekho — `return 1;` (signed
overflow UB → compiler `x+1 > x` ko hamesha true maanta hai). `-fwrapv` se
recompile — ab actual comparison.

---

## ⚠️ Traps

### Trap 1 — null-check `*p` ke *baad*
```cpp
int v = *p;
if (!p) return err;    // ⚠️ *p ne "p non-null" imply kiya -> yeh check -O2 pe DELETE
```
Check pehle: `if (!p) return err; int v = *p;`.

### Trap 2 — signed overflow "wrap ho jaayega"
```cpp
if (x + 1 < x) overflow();   // ⚠️ signed: UB -> compiler maan leta kabhi nahi hoga -> dead
if (x == INT_MAX) overflow();          // ✅
if (__builtin_add_overflow(x, 1, &y))  // ✅
```

### Trap 3 — shift by width
```cpp
uint32_t m = 1u << shift;     // ⚠️ shift >= 32 -> UB (aur 32 common off-by-one)
uint64_t m = shift < 32 ? (1u << shift) : 0;   // ya 64-bit type
```

### Trap 4 — type punning via cast
```cpp
float f = 1.5f;
uint32_t bits = *reinterpret_cast<uint32_t*>(&f);   // ⚠️ strict-aliasing UB
uint32_t bits = std::bit_cast<uint32_t>(f);         // ✅
```

### Trap 5 — dangling reference from a function
```cpp
const std::string& name() { return std::string("tmp"); }   // ⚠️ returns dangling ref
```

### Trap 6 — `std::vector` iterator invalidation
```cpp
for (auto it = v.begin(); it != v.end(); ++it)
    if (*it == x) v.push_back(y);   // ⚠️ push_back realloc -> `it`, `end()` dangling -> UB
```

### Trap 7 — `memcpy` overlapping
```cpp
std::memcpy(buf+1, buf, n);   // ⚠️ overlap -> UB. std::memmove.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "UB matlab crash" | Kuch bhi — crash, galat answer, code deleted, "works today" |
| "`-O0` pe UB safe hai" | `-O0` pe symptoms chhup sakte; UB abhi bhi UB, `-O2` pe bite karega |
| "unsigned overflow bhi UB" | Unsigned **wraps** (defined mod 2^n). Signed overflow UB |
| "sanitizer se slow, isliye skip" | Production pe skip, **CI pe must** — har hit fix |
| "mera null-check compiler nahi hatega" | Agar pehle deref hai to hata deta hai (UB implied non-null) |
| "type pun via `union`/cast theek hai" | C++ mein `std::bit_cast`/`memcpy`. Cast = strict-aliasing UB |
| "reading uninitialized = garbage value, bas" | Kai types ke liye UB, aur compiler garbage-based branch delete kar sakta |

---

## Exercises

1. **Predict `-O2`:**
   ```cpp
   bool check(int* p) { return p && *p == 0; }
   int use(int* p) { int v = *p; if (!p) return -1; return v; }
   ```
   `use(nullptr)` ka behaviour?

   <details><summary>Answer</summary>

   `check` theek hai (`&&` short-circuits, `p` check pehle). `use`: `*p` pehle →
   UB for `nullptr` → compiler assumes `p != nullptr` → `if (!p)` **removed** →
   `use(nullptr)` derefs null → crash (or worse). Move the null-check above `*p`.
   </details>

2. **Overflow loop:**
   ```cpp
   int sum(const int* a, int n) { int s = 0; for (int i = 0; i <= n; ++i) s += a[i]; return s; }
   ```
   Do bugs (ek UB, ek plain).

   <details><summary>Answer</summary>

   Plain: `i <= n` reads `a[n]` — one past the intended `[0,n)` (OOB if `n` is the
   size). UB compounding: if `n == INT_MAX`, `i <= n` + `++i` overflows `i` →
   signed overflow UB → compiler may assume the loop always terminates / vectorize
   in a way that breaks near the boundary. Use `i < n`, and `std::size_t`/`int64`
   for the counter if `n` can be huge.
   </details>

3. **Type pun:** `uint64_t as_bits(double d)` — do implementations, ek UB ek sahi.

   <details><summary>Answer</summary>

   UB: `return *reinterpret_cast<uint64_t*>(&d);` (strict aliasing). Sahi:
   `return std::bit_cast<uint64_t>(d);` (C++20) or `uint64_t x; std::memcpy(&x,
   &d, 8); return x;`.
   </details>

4. **Which are UB:** (a) `unsigned u = -1;` (b) `int i = INT_MAX + 1;`
   (c) `1 << 31` (int) (d) `x = x++;` (e) `std::vector<int> v; v[0];`
   (f) `(char)300` (g) `int a[3]; a[3] = 0;`

   <details><summary>Answer</summary>

   UB: (b) signed overflow, (c) left-shift into/over sign bit of `int` (UB
   pre-C++20; `1<<31` still UB as it overflows `int`), (d) unsequenced
   modification, (e) OOB on empty vector, (g) OOB write. **Not UB:** (a) defined
   wrap to `UINT_MAX`, (f) implementation-defined (value of narrowing to `char`).
   </details>

5. **Sanitizer plan:** ek 200k-line engine ke CI ke liye — kaunse sanitizers,
   kaunse builds, kya production mein?

   <details><summary>Answer</summary>

   CI: one build+test pass with `-fsanitize=address,undefined` (they compose),
   a separate pass with `-fsanitize=thread` for the concurrency tests (TSan
   can't combine with ASan). `-D_GLIBCXX_ASSERTIONS` in debug/CI. Warnings as
   errors (`-Wall -Wextra -Wconversion -Wshadow ...`). Production: none of the
   sanitizers (perf) — maybe `-D_FORTIFY_SOURCE=2` and stack protector on
   non-hot code. Every sanitizer finding is a blocker.
   </details>

---

## Interview questions

1. UB vs unspecified vs implementation-defined — ek-ek example.
2. "Compiler UB ke saath optimize karta hai" — ek concrete example (null-check
   removal ya overflow loop).
3. Signed vs unsigned overflow — kaunsa UB, kaunsa defined?
4. Strict aliasing kya hai, type punning safely kaise?
5. ASan aur UBSan kya-kya pakadte hain, kaise (redzones / shadow / inline checks)?
6. Sanitizers production pe kyun nahi, CI pe kyun must?
7. `std::unreachable()` / `[[assume]]` — UB se kya rishta?
8. 5 sabse common UB jo aap defensively likhke bacha sakte ho.

---

## Next
→ [`14-exercises.md`](14-exercises.md)
