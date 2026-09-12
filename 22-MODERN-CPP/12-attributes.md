# 12 — Attributes

## Prerequisites
- Folder 08 (functions, `[[nodiscard]]` intro), folder 06 (branches, `[[likely]]`)
- Folder 33 preview (compiler optimization)

## Yeh topic abhi kyun
Attributes `[[...]]` compiler ko **hints aur intent** dete — standard syntax mein
(pehle har compiler ke apne `__attribute__` / `__declspec` the). Kuchh warnings
control karte (`[[nodiscard]]`, `[[maybe_unused]]`), kuchh optimizer ko
information dete (`[[likely]]`, `[[assume]]`). HFT mein `[[likely]]` /
`[[unlikely]]` / `[[gnu::hot]]` branch layout aur code placement ke liye kaam aate.

---

## Standard attributes

| Attribute | Since | What |
|---|---|---|
| **`[[nodiscard]]`** | C++17 | warn if the return value is ignored. `[[nodiscard("reason")]]` (C++20). Put it on functions/types where discarding the result is a bug (`empty()`, `try_lock()`, an `error_code`, a factory). |
| **`[[maybe_unused]]`** | C++17 | suppress "unused" warnings on a variable/param/function (e.g. used only in an `assert` or one `#ifdef`). |
| **`[[fallthrough]]`** | C++17 | in a `switch`, mark an intentional fall-through so `-Wimplicit-fallthrough` doesn't warn. |
| **`[[deprecated]]`** | C++14 | warn on use; `[[deprecated("use X instead")]]`. |
| **`[[likely]]` / `[[unlikely]]`** | C++20 | hint which branch/label is the common path → the compiler lays out code so the hot path is fall-through and straight-line. |
| **`[[noreturn]]`** | C++11 | this function never returns (`std::abort`, a fatal-error handler) → the compiler omits post-call code and warns on a missing `return` after it. |
| **`[[no_unique_address]]`** | C++20 | let an empty (or overlappable) member occupy **zero** bytes — EBO for members (used by allocators, `std::vector`, policy classes — folder 15). |
| **`[[assume(expr)]]`** | C++23 | promise `expr` is true at this point; **UB if it isn't**. The optimizer can use it (drop a bounds check, assume a pointer non-null, assume a range). |
| **`[[carries_dependency]]`, `[[indeterminate]]`** | niche | memory-order dependency tracking; uninitialized-by-design. |

Attributes appertain to different things depending on placement (a statement, a
declaration, a type, a function). `[[likely]]` goes on a **statement**:
```cpp
if (x) [[likely]] { hot(); } else [[unlikely]] { cold(); }
for (...) [[likely]] { ... }
switch (k) { case A: [[likely]] ...; }
```

---

## Vendor attributes (GCC/Clang) — `[[gnu::...]]`

Standard syntax, non-standard semantics. Useful in perf code:

| Attribute | What |
|---|---|
| `[[gnu::hot]]` / `[[gnu::cold]]` | mark a function as frequently / rarely called → the linker groups hot functions together (better i-cache) and cold ones out of the way |
| `[[gnu::always_inline]]` / `[[gnu::noinline]]` | force / forbid inlining (override the heuristic) |
| `[[gnu::flatten]]` | inline *everything* this function calls, transitively |
| `[[gnu::const]]` / `[[gnu::pure]]` | the function has no side effects and (for `const`) doesn't read global memory → the compiler can CSE / hoist / eliminate redundant calls |
| `[[gnu::aligned(64)]]` | alignment (also `alignas`) |
| `[[gnu::target("avx2")]]` | compile this function for a specific ISA (function multiversioning) |
| `[[gnu::returns_nonnull]]`, `[[gnu::nonnull]]`, `[[gnu::malloc]]` | pointer contracts the optimizer exploits |
| `[[gnu::warn_unused_result]]` | GCC's older `[[nodiscard]]` |

`[[clang::...]]` has its own set (`[[clang::lifetimebound]]`, `[[clang::musttail]]`
for guaranteed tail calls, etc.).

---

## `[[likely]]` / `[[unlikely]]` in practice

The compiler already uses PGO and heuristics ("error paths are cold", "loop
back-edges are hot"). `[[likely]]` helps when **you** know something it can't
infer:

```cpp
Result process(const Packet& p) {
    if (!validate(p)) [[unlikely]] {          // malformed packets are rare
        return Result::Reject;
    }
    // hot path -- laid out as straight-line fall-through, no taken branch
    apply(p);
    return Result::Ok;
}
```

Effect: the "likely" side is placed so the CPU's default not-taken prediction and
the fetch unit's fall-through favour it; the unlikely side (error handling) is
moved to the end of the function or a cold section. On a well-predicted branch
the win is small (a few %); it matters most for **code layout / i-cache** on a
branchy hot function, and it composes with `[[gnu::hot]]`/`cold`.

Don't sprinkle it everywhere — a wrong hint pessimizes. Use it on genuinely
skewed branches (validation failures, cache misses, rare special cases).

---

## `[[assume]]` — powerful and dangerous (C++23)

```cpp
void scale(std::span<float> data, int n) {
    [[assume(n >= 0 && n % 8 == 0)]];          // promise: n is non-negative and a multiple of 8
    for (int i = 0; i < n; ++i) data[i] *= 2;  // compiler can vectorize by 8 with no remainder loop, no i<n bounds fuss
}
```

The optimizer treats the expression as an axiom. If it's **ever false at
runtime**, the whole program is UB — worse than a failed assert, because there's
no diagnostic. Use only for invariants you can **prove** (enforced at a boundary,
guaranteed by the type system). Pre-C++23: `__builtin_assume` (Clang),
`__assume` (MSVC), `if (!x) __builtin_unreachable();` (GCC).

---

## Andar kya hota hai

- `[[nodiscard]]`, `[[maybe_unused]]`, `[[deprecated]]`, `[[fallthrough]]` are
  **diagnostic-only** — they change warnings, not codegen.
- `[[noreturn]]` lets the compiler drop the return sequence and any code after a
  call to the function, and treat paths through it as not needing a return value.
- `[[likely]]`/`[[unlikely]]` feed the basic-block placement pass: the likely
  successor becomes the fall-through edge, the unlikely one a forward jump to a
  cold block (often at function end). No instructions are added — it's layout.
- `[[gnu::hot]]`/`[[gnu::cold]]` put functions in `.text.hot` / `.text.unlikely`
  sections; the linker clusters them → the hot working set fits in fewer i-cache
  lines / pages.
- `[[assume(expr)]]` / `__builtin_assume`: the compiler adds `expr` to its set of
  known facts for that point, enabling range/alias/null analysis to drop checks
  and pick better vectorization — with no runtime check that `expr` holds.
- `[[gnu::const]]`/`pure`: a `const` function's calls with equal arguments are
  common-subexpression-eliminated and hoisted out of loops.

> **HFT relevance:** attributes are a low-effort layer of the optimization
> toolbox. `[[likely]]`/`[[unlikely]]` on the rare branches of a hot function
> (validation failure, cache-miss slow path, once-per-session special case) plus
> `[[gnu::hot]]`/`[[gnu::cold]]` on the functions themselves improve **code
> layout and i-cache residency** of the tick path — small per-branch but real on
> a branchy pipeline. `[[nodiscard]]` on `try_lock()`, `error_code`-returning
> APIs, and `[[nodiscard]]` factories catches "forgot to check" bugs at compile
> time. `[[no_unique_address]]` keeps policy/allocator members zero-size (folder
> 15). `[[assume]]` / `__builtin_assume` on **proven** invariants (n is a
> multiple of the SIMD width, a pointer from a pool is non-null, an index is in
> range) lets the vectorizer drop remainder loops and bounds fuss — but a false
> assumption is silent UB, so it's used sparingly and only where the invariant is
> guaranteed upstream. Measure; a wrong `[[likely]]` or an over-eager `[[assume]]`
> can regress or break.

---

## Hands-on

```bash
./build.ps1 asm 06-CONDITIONS/examples/04_branch_benchmark.cpp    # see branch layout
```
```cpp
// attr.cpp
[[nodiscard]] bool try_take();
[[noreturn]] void fatal(const char*);
int classify(int x) {
    if (x < 0) [[unlikely]] return -1;
    return x * 2;
}
```
Compile with `-O2 -S -masm=intel`, find where the `[[unlikely]]` block landed
(end of the function). Add `[[gnu::cold]]` to an error handler and check the
section (`objdump -h`).

---

## ⚠️ Traps

### Trap 1 — `[[assume(expr)]]` with `expr` that can be false
```cpp
[[assume(idx < size)]];   // ⚠️ if idx CAN reach size here -> UB, no diagnostic. Only for guaranteed invariants
```

### Trap 2 — `[[likely]]` on a well-predicted or 50/50 branch
```cpp
if (rare) [[likely]] ...   // ⚠️ wrong hint -> pessimizes layout. Use it only on genuinely skewed branches
```

### Trap 3 — ignoring an unknown attribute silently
```cpp
[[myvendor::fancy]] void f();   // an unknown attribute is IGNORED (with a warning) -- don't rely on a typo'd one
```

### Trap 4 — `[[nodiscard]]` and then casting to void everywhere
```cpp
(void)try_lock();   // ⚠️ defeats the point. If you truly don't care, comment why
```

### Trap 5 — expecting `[[gnu::always_inline]]` to always work
```cpp
// It's a strong hint; the compiler can still refuse (recursion, varargs, address taken across TUs). Verify in the asm
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`[[likely]]` speeds up the branch" | It changes basic-block *layout* (i-cache, fall-through) — small on a predicted branch |
| "`[[assume]]` is a checked assertion" | It's an unchecked axiom — false → UB, no message (worse than `assert`) |
| "All attributes affect codegen" | Many are diagnostic-only (`nodiscard`, `maybe_unused`, `deprecated`, `fallthrough`) |
| "An unknown attribute is an error" | It's ignored (usually with a warning) — a typo'd attribute does nothing |
| "`[[no_unique_address]]` guarantees zero size" | It *allows* zero size for an empty/overlappable member — the compiler may still not overlap in some layouts (esp. MSVC ABI historically) |

---

## Exercises

1. **nodiscard candidates:** name three standard-library functions where
   `[[nodiscard]]` catches a real bug.

   <details><summary>Answer</summary>

   `std::vector::empty()` (people mean `clear()`), `std::async` (discarding the
   `future` makes it block in the destructor — surprise serialization),
   `std::unique_ptr::release()` (discarding it leaks). Also `std::launder`,
   `std::allocator::allocate`, `try_lock`.
   </details>

2. **likely layout:** in `int f(int x){ if (x<0) [[unlikely]] return -1; return
   x*2; }`, what does the `[[unlikely]]` change in the generated code?

   <details><summary>Answer</summary>

   The `x < 0` branch's taken target (the `return -1`) is placed at the **end**
   of the function as a cold block; the common path (`return x*2`) is the
   straight-line fall-through. Same instructions, better layout for i-cache and
   the fetch unit's not-taken default.
   </details>

3. **assume vs assert:** you have `n % 16 == 0` guaranteed by the caller. Compare
   `assert(n % 16 == 0);` and `[[assume(n % 16 == 0)]];` for a release build.

   <details><summary>Answer</summary>

   `assert` is compiled out with `NDEBUG` → no effect on the optimizer in
   release. `[[assume]]` tells the optimizer `n` is a multiple of 16 → it can
   vectorize the loop by 16 with **no remainder/scalar tail** and skip
   `n`-alignment fuss. Cost: if the caller ever violates it, silent UB.
   </details>

4. **hot/cold:** how do `[[gnu::hot]]` and `[[gnu::cold]]` improve i-cache
   behaviour?

   <details><summary>Answer</summary>

   They place functions in `.text.hot` / `.text.unlikely` sections; the linker
   groups all hot functions contiguously (and cold ones elsewhere). The hot
   working set then spans fewer cache lines / pages → fewer i-cache and iTLB
   misses on the tick path; cold error/setup code doesn't pollute those lines.
   </details>

5. **no_unique_address:** why does `template <class T, class Alloc> class Vec {
   [[no_unique_address]] Alloc alloc_; T* data_; ... };` matter for a stateless
   allocator?

   <details><summary>Answer</summary>

   A stateless allocator is an empty class. Without `[[no_unique_address]]` it
   still takes ≥1 byte (+ padding) as a member → `sizeof(Vec)` grows. With it,
   the empty `alloc_` overlaps other members → `Vec` stays 3 words, matching a
   hand-rolled vector. (EBO for members.)
   </details>

---

## Interview questions

1. `[[nodiscard]]` / `[[maybe_unused]]` / `[[fallthrough]]` — diagnostic ya codegen?
2. `[[likely]]` / `[[unlikely]]` kya karte (branch layout), kab use?
3. `[[assume(expr)]]` vs `assert` — release build mein fark, risk?
4. `[[gnu::hot]]` / `[[gnu::cold]]` — i-cache pe kaise asar?
5. `[[no_unique_address]]` — empty allocator/policy member ke liye kyun?
6. Unknown attribute ka kya hota (ignored) — kya risk?

---

## Next
→ [`13-format-and-print.md`](13-format-and-print.md)
