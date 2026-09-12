# 06 — Constant folding, propagation, dead code elimination

## Prerequisites
- `03-inlining.md` (inlining enables cross-function const propagation)
- `08-FUNCTIONS/11-constexpr-functions.md`

## Yeh topic abhi kyun
Yeh teen passes — **constant folding**, **constant propagation**, **dead code
elimination (DCE)** — compiler ki "book-keeping" hain jo har jagah chalti
rehti hain. Inhe samajhna do wajah se zaroori: (1) yeh benchmark ko delete
kar deti hain (lesson 14 — isliye `DoNotOptimize`), (2) inhe *jaan-boojh ke*
enable karke aap compile-time pe kaam shift kar sakte ho (`constexpr`,
`if constexpr`, template params).

---

## Constant folding

Compile-time pe known-constant expressions ko unke result se replace karo:
```cpp
int x = 3 * 8 + 2;          // -> int x = 26;
double k = 3.14159 / 2;     // -> folded (IEEE round-to-nearest at compile time)
int n = sizeof(int) * 4;    // -> 16
constexpr int m = fib(10);  // -> 55, computed by the compiler
```
Har `-O` level pe (yes, `-O0` bhi trivial folding karta). Float folding
compiler ke apne arithmetic se hota — usually matches runtime, par
`-ffast-math` ke bina strict IEEE, `-frounding-math` edge cases.

---

## Constant propagation

Agar ek variable ki value ek point pe known-constant hai, use aage substitute
karo:
```cpp
int a = 5;
int b = a * 2;      // a is 5 here -> b = 10
foo(b);             // -> foo(10)
```
**Inter-procedural** (`-fipa-cp`): agar ek function hamesha same constant arg
se call hota, compiler ek specialized clone banata:
```cpp
void scale(int* v, int n, int k) { for (i) v[i] *= k; }
scale(a, 100, 4);   // only call -> clone scale.constprop.0 with k=4 baked in
                    // -> v[i] <<= 2, and n=100 lets it fully unroll/vectorize
```
`-fipa-cp-clone` (`-O3`) isko aggressive karta.

Inlining (lesson 03) is ka biggest enabler — inline hone ke baad caller ke
constants callee ke body mein propagate hote.

---

## Dead code elimination (DCE)

Jo code ka result kabhi use nahi hota, ya jo unreachable hai — hata do:
```cpp
int f() {
    int x = expensive();     // x never used -> expensive() call DELETED (if pure)
    int y = 10;
    if (y > 100) rare();     // y is 10 -> branch is dead -> rare() call DELETED
    return 0;
}
```
Sub-forms:
- **Dead store elimination** — `x = 1; x = 2;` → first store gone.
- **Unreachable code** — after `return`/`throw`/`__builtin_unreachable()`.
- **Dead branch** — condition folds to constant → one side deleted.
- **Pure/const function calls with unused results** — `strlen(s)` with the
  result ignored → gone (`-fdelete-null-pointer-checks`, pure/const attrs).

⚠️ DCE **only deletes side-effect-free code**. A call that writes memory /
does I/O / is `volatile` / could throw / isn't provably pure — stays.

---

## The combined effect: whole functions vanish

```cpp
constexpr int table_size(int levels) { return levels * 8 + 1; }

int main() {
    constexpr int N = table_size(16);   // fold: N = 129
    std::array<int, N> buf{};           // size baked in
    for (int i = 0; i < N; ++i) buf[i] = i * i;   // N known -> unroll/vectorize
    return buf[42];                     // propagate: return 1764
}
// -O2: main() compiles to essentially `mov eax, 1764 ; ret`
```
Har pass ne dusre ko feed kiya: fold → propagate → the loop's bound is
constant → it's fully evaluated → DCE removes everything → `return 1764`.

This is why benchmarks need barriers (lesson 14): the compiler is *very* good
at proving your loop's output is a compile-time constant and deleting it.

---

## Using it deliberately

| Tool | Effect |
|---|---|
| `constexpr` variable / function | force compile-time evaluation (or error) |
| `consteval` function | *must* run at compile time (C++20) |
| `constinit` | guarantee static init is a constant (no dynamic-init order fiasco) |
| `if constexpr` | dead-branch elimination *before* instantiation (templates) |
| template non-type param `<int K>` | `K` is a constant in the body → full specialization |
| `constexpr` lookup tables | build the table at compile time, zero runtime cost |
| `[[assume(cond)]]` / `__builtin_assume` | tell the optimizer a fact it can propagate (unchecked) |

Example: a CRC table, a sine table, a permutation — compute it in a
`constexpr` function, store as `constexpr std::array`. Runtime just indexes.

---

## `if constexpr` vs runtime `if`

```cpp
template <bool Checked>
int get(const std::vector<int>& v, size_t i) {
    if constexpr (Checked) { if (i >= v.size()) throw ...; }   // <- gone entirely when Checked=false
    return v[i];
}
```
`if constexpr (false)` — the branch is **not even compiled** (not just
DCE'd) → no code, and the contents needn't be valid for the other type. A
runtime `if (Checked)` with a `constexpr bool` gets DCE'd too, but must still
compile.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — benchmark loop const-folded away
`for (i) s += i*i; ` with `s` unused → `-O2` computes the closed form or
deletes it → 0 ns (lesson 14, example 08 case 1). Use `DoNotOptimize`.

### Trap 2 — assuming `const` means "compile-time constant"
`const int n = read_config();` — runtime value, just immutable. Only
`constexpr` (with a constant initializer) is a compile-time constant.
`const` can still enable propagation *if* the initializer is constant.

### Trap 3 — `constexpr` function that silently runs at runtime
`constexpr` = "*can* run at compile time *if* called in a constant context".
`int n = f(x);` with runtime `x` → runs at runtime. Use `constexpr int n =
f(5);` or `consteval` to force it.

### Trap 4 — float constant folding ≠ runtime float result (rare)
Compile-time folding uses the compiler's arbitrary-precision or host FP;
with `-frounding-math` / non-default rounding modes / `long double` width
differences, a folded constant can differ from the runtime computation by an
ULP. Almost never matters; know it exists.

### Trap 5 — `[[assume]]` / `__builtin_assume` with a false condition
The compiler propagates it as fact → if it's actually false at runtime, UB
(it may delete "impossible" code paths). Only assert-verified invariants.

### Trap 6 — dead code you *wanted* (a self-test, a canary)
`volatile` it, or `DoNotOptimize` it, or print it — otherwise DCE removes
your runtime check.

---

## > **HFT relevance**

> - **Bake constants at compile time.** Tick sizes, lot sizes, level counts,
>   protocol field offsets, feed IDs → `constexpr` → the hot path has no
>   config loads, and `price / tick_size` becomes a shift/reciprocal-multiply.
> - **`constexpr` lookup tables** — decimal-parse tables, CRC tables, symbol
>   hash seeds, precomputed masks — built by the compiler, indexed at runtime.
> - **`if constexpr` for build variants** — a `Debug`/`Prod` template
>   parameter that compiles out all the validation/logging in the prod
>   instantiation, zero runtime `if`.
> - **`[[assume]]` for verified invariants** — "book depth ≤ MAX", "seq is
>   monotonic" — lets the optimizer drop bounds handling, *after* you've
>   asserted it in debug builds.
> - **Watch DCE in your self-tests** — a startup invariant check whose result
>   you don't observe gets deleted; assign it to a `volatile` or `abort()` on
>   failure.

---

## Hands-on

```bash
# whole-function fold:
echo 'constexpr int ts(int l){return l*8+1;} int main(){constexpr int n=ts(16);
      int s=0; for(int i=0;i<n;++i) s+=i*i; return s;}' \
  | g++ -O2 -S -masm=intel -std=c++20 -xc++ - -o - | grep -A4 'main:'
#   -> essentially `mov eax, <constant> ; ret`

# see the IPA constant-propagation clones:
g++ -O2 -fdump-ipa-cp=/dev/stdout -c file.cpp | grep -i "constprop\|cloning"

# dead code report:
g++ -O2 -fdump-tree-dce=/dev/stdout -c file.cpp | head -40

# benchmark WITHOUT a barrier -> watch it vanish:
./build.ps1 fast 33-COMPILER-OPTIMIZATION/examples/08_benchmark_barriers.cpp   # case 1 = 0.00 ms
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`const` = compile-time constant" | `const` = immutable; `constexpr` = compile-time |
| "`constexpr` function always runs at compile time" | only in a constant context; else runtime |
| "DCE deletes anything unused" | only side-effect-free code; I/O / volatile / writes stay |
| "the compiler won't delete my benchmark" | it will — closed-form / DCE; use `DoNotOptimize` |
| "`if constexpr` == runtime `if` with constant" | `if constexpr(false)` isn't compiled at all |
| "folded float == runtime float" | ~always; ULP edge cases with non-default rounding |

---

## Exercises

1. `int f(int x) { const int k = 10; int y = k * k; if (y == x) return 1;
   return 0; }` — `-O2` pe kya bachta?

   <details><summary>Answer</summary>

   `k` folds to 10, `y = k*k` folds to 100. `if (y == x)` → `if (100 == x)`
   → `if (x == 100)`. So `f` becomes: `return x == 100;` — one `cmp eax,
   100` + `sete al` (or `xor`/`cmp`/`setz`), ~3 instructions, no `imul`, no
   local `k`/`y`. The `const` on `k` wasn't even needed — the compiler
   propagates the literal `10` regardless; `const` just documents intent.
   </details>

2. `bool g(int* p) { int a = *p; log("read"); int b = *p; return a == b; }` —
   compiler `b = *p` ko `a` se replace kar sakta (common subexpression)? Aur
   agar `log` hata dein?

   <details><summary>Answer</summary>

   **With `log("read")` in between**: `log` is an opaque call (I/O, could do
   anything, including — as far as the compiler knows — write through some
   pointer that aliases `p`). So the compiler must **reload** `*p` for `b`;
   it can't assume `a == b`. `g` returns a real comparison of two loads.
   **Remove `log`**: now nothing between the two `*p` reads can change memory
   → CSE folds `b = a` → `return a == a` → `return true` → `g` compiles to
   `mov eax, 1 ; ret` and never even dereferences `p` (careful: if `p`
   could be null, `-fdelete-null-pointer-checks` still assumes it's valid
   because you dereferenced it). This is why "pure" annotations and
   `-flto` matter — an opaque call is an optimization barrier.
   </details>

3. Tumhe ek 256-entry CRC table chahiye jo har startup compute hoti hai
   (~microseconds). Compile-time pe kaise, aur kya milega?

   <details><summary>Answer</summary>

   Write a `constexpr` function that fills a `std::array<uint32_t, 256>` with
   the CRC polynomial reduction, and store the result in a `constexpr`
   (or `constinit`) array:
   ```cpp
   consteval std::array<uint32_t,256> make_crc_table() {
       std::array<uint32_t,256> t{};
       for (uint32_t i = 0; i < 256; ++i) {
           uint32_t c = i;
           for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
           t[i] = c;
       }
       return t;
   }
   constexpr auto CRC = make_crc_table();
   ```
   The compiler evaluates the whole thing during compilation → the array is
   baked into `.rodata` → **zero startup cost, zero dynamic init, no
   init-order fiasco**, and the table is `const` so it's shareable/read-only.
   `consteval` guarantees it can't accidentally slip to runtime.
   </details>

---

## Interview questions

1. Constant folding vs constant propagation — ek line each + example.
2. Inter-procedural constant propagation (`-fipa-cp`) — kya clone banata.
3. DCE kya delete karta aur kya NAHI (side effects).
4. Inlining const-propagation ko kaise enable karta.
5. `const` vs `constexpr` vs `consteval` vs `constinit`.
6. `if constexpr` vs runtime `if` with a `constexpr bool` — codegen + compile difference.
7. Ek benchmark loop const-fold ho gaya — kaise pata chala, kaise roka.

---

## Next
→ [`07-devirtualization.md`](07-devirtualization.md)
