# 11 — Copy elision (RVO, NRVO, guaranteed elision C++17)

## Prerequisites
- [`06-move-constructor.md`](06-move-constructor.md), [`08-std-move.md`](08-std-move.md)
- Folder 08 file 05 (return values, call stack)

## Yeh topic abhi kyun
Move semantics bade objects ko function se return karna sasta banati hai (O(1)
move). **Copy elision** usse bhi aage jaati hai — **zero** copy aur **zero**
move. Compiler `return` value ko seedha caller ki storage mein construct kar deta
hai. C++17 se prvalue returns ke liye yeh **guaranteed** hai. Yeh samajhna
zaroori — aur yeh samajhna ki `return std::move(x)` isse **rok** deta hai.

---

## RVO — Return Value Optimization (prvalue return)

```cpp
Widget makeWidget() {
    return Widget{1, 2, 3};       // returning a prvalue (an unnamed temporary)
}

Widget w = makeWidget();
```

Without elision: `Widget{1,2,3}` is constructed, then copied/moved into the
return slot, then copied/moved into `w` → up to 3 constructions.

**With RVO:** `Widget{1,2,3}` is constructed **directly in `w`'s storage**. One
constructor call. Zero copies, zero moves, zero extra destructors.

**C++17: this is guaranteed** for prvalues. `Widget w = makeWidget();` *must*
construct `w` in place — the copy/move ctor isn't even required to exist (it can
be `= delete`d). It's not an "optimization" anymore; it's the semantics.

```cpp
class NoCopyNoMove {
public:
    NoCopyNoMove(int);
    NoCopyNoMove(const NoCopyNoMove&) = delete;
    NoCopyNoMove(NoCopyNoMove&&)      = delete;
};
NoCopyNoMove make() { return NoCopyNoMove{5}; }   // ✅ C++17 -- compiles, guaranteed elision
NoCopyNoMove x = make();                           // ✅ constructed in place
```

---

## NRVO — Named Return Value Optimization (named local return)

```cpp
Widget makeWidget() {
    Widget w{1, 2, 3};
    w.tweak();
    return w;                     // returning a NAMED local
}
```

NRVO: `w` is constructed **directly in the caller's return slot** — no
copy/move on `return`.

**NRVO is allowed but NOT guaranteed** (even in C++17). GCC and Clang do it
reliably for simple cases. It can't apply when:
- Multiple `return` statements return **different** named locals (compiler can't
  pick one slot).
- The returned name is a function **parameter** (it lives in the parameter slot,
  not the return slot).
- `return which ? a : b;` — a conditional expression, not a plain name.

**When NRVO doesn't apply**, C++11 has a fallback: a named local in a `return`
is treated as an **rvalue** → the **move** ctor is used (not copy). So worst case
for `return localVar;` is one move, not a copy.

```cpp
Widget bad(bool which) {
    Widget a, b;
    return which ? a : b;         // NRVO impossible; and `which ? a : b` is not a name
                                 // -> full COPY ctor (not even implicit move)
}
```

`examples/06_copy_elision.cpp` demonstrates: `makeRVO()` / `makeNRVO()` print
**one** ctor line; `makeConditional()` prints a COPY ctor.

---

## `-fno-elide-constructors` — see what elision saved

```bash
g++ -std=c++20 -O2 06_copy_elision.cpp -o normal
g++ -std=c++20 -O2 -fno-elide-constructors 06_copy_elision.cpp -o noelide
```

`-fno-elide-constructors` disables the **optional** elisions (NRVO, and the
pre-C++17 prvalue copy). Running both:
- `makeRVO()` — **still one ctor** with `-fno-elide-constructors`! Because C++17
  guaranteed prvalue elision **cannot be disabled** — it's the semantics, not an
  optimization.
- `makeNRVO()` — now shows an **extra MOVE ctor + temporary dtor**. That's what
  NRVO was eliding.

This flag is a teaching / debugging tool, never for production.

---

## `return std::move(local)` — the anti-pattern

```cpp
Widget good() { Widget w; return w; }               // ✅ NRVO or implicit move
Widget bad () { Widget w; return std::move(w); }    // ⚠️ NRVO DISABLED -> forced move ctor
```

`return std::move(w)` makes the return expression an **xvalue**, not a plain
name → NRVO can't apply (it only elides named locals returned as-is). You've
*guaranteed* a move ctor call where you might have had zero.

`-Wpessimizing-move` (in `-Wall`) warns. `examples/05_std_move_demo.cpp` shows
`makeGood()` (silent — elided) vs `makeBad()` (prints "MOVE").

**Legit `return std::move(...)`:**
- `return std::move(member_);` — a data member isn't a local; no NRVO applies,
  and you *want* the move.
- `return std::move(param_);` where `param_` is a by-value parameter and the
  function returns a **different** (base) type — the implicit-move rule may not
  kick in for the type mismatch.

---

## Elision and side effects

Elision can remove constructor/destructor calls **even if they have side
effects** (printing, counting, logging). This is explicitly allowed — your code
must not depend on the number of copies/moves.

```cpp
struct Tracer {
    Tracer()             { count++; }
    Tracer(const Tracer&) { count++; }
    static int count;
};
Tracer make() { return Tracer{}; }
Tracer t = make();
// Tracer::count is 1 (elided), not 2 or 3. Don't write code that assumes otherwise.
```

---

## Andar kya hota hai

- The caller allocates the return object's storage (in its own stack frame) and
  passes a **hidden pointer** to it as an implicit argument (the "return slot" /
  RVO pointer — like an out-parameter). The callee constructs the return value
  *through that pointer*.
- Guaranteed prvalue elision (C++17): `return T{...}` constructs `T` directly via
  that pointer. There's never a temporary. `Widget w = makeWidget();` — `w` *is*
  the return slot.
- NRVO: the compiler proves the named local `w` can *be* the return slot →
  constructs `w` there from the start. If it can't prove it, `w` is a normal
  local and `return w;` does a move (or copy if no move op) into the slot.
- `return std::move(w)` → the expression is `Widget&&`, not the name `w` → the
  NRVO analysis (which pattern-matches `return <name>;`) fails → a move ctor call
  into the slot.

> **HFT relevance:** guaranteed elision + move semantics means **value-returning
> APIs are free** — a function can `return std::vector<Order>`, a `Book`
> snapshot, a parsed message struct by value with zero copy and (for prvalues)
> zero move. This lets HFT code use clean value semantics in setup/parsing/
> aggregation without the "return a pointer / out-param for performance" C-style
> contortions. The one rule everyone internalizes: **never `return
> std::move(local)`** — it silently turns a zero-cost return into a move (and
> for a non-movable type, a compile error). `-Werror=pessimizing-move` is common.

---

## Hands-on

```bash
./build.ps1 18-COPY-MOVE/examples/06_copy_elision.cpp
g++ -std=c++20 -O2 -fno-elide-constructors 18-COPY-MOVE/examples/06_copy_elision.cpp -o ce_off && ./ce_off
```

Compare: normal build (RVO/NRVO — one ctor each), `-fno-elide-constructors`
(NRVO gone — extra MOVE + dtor; but guaranteed prvalue elision for `makeRVO()`
still holds).

---

## ⚠️ Traps

### Trap 1 — `return std::move(local)` (kills NRVO)
```cpp
Big f() { Big b; return std::move(b); }   // ⚠️ -Wpessimizing-move. `return b;`
```

### Trap 2 — code that counts copies/moves
```cpp
assert(Tracer::copyCount == 2);   // ⚠️ elision may make it 0 or 1. Don't assert on copy counts
```

### Trap 3 — expecting NRVO for conditional / parameter returns
```cpp
T f(bool c) { T a, b; return c ? a : b; }   // ⚠️ no NRVO, and not even implicit move -> COPY
T g(T x) { return x; }                        // NRVO doesn't apply to params, but x IS implicitly moved
```

### Trap 4 — thinking `-O0` disables guaranteed elision
Guaranteed prvalue elision (C++17) holds at every optimization level — it's
semantics.

### Trap 5 — relying on NRVO for a non-movable, multi-return type
```cpp
NonMovable f(bool c) { if (c) return NonMovable{1}; return NonMovable{2}; }
// ✅ each `return NonMovable{...}` is a prvalue -> guaranteed elision -> OK
// but `NonMovable a; return a;` in a multi-return function -> needs a move -> compile error
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "RVO is a compiler optimization you can't rely on" | Prvalue elision is **guaranteed** (C++17); NRVO is best-effort but reliable on GCC/Clang |
| "`return std::move(x)` helps performance" | It disables NRVO — a pessimization |
| "`-fno-elide-constructors` disables all elision" | Only the optional kind; guaranteed prvalue elision stays |
| "Elision won't skip constructors with side effects" | It can and does — don't depend on copy counts |
| "You need a move ctor to return by value" | For prvalues (C++17), not even that — guaranteed elision |

---

## Exercises

1. **Count ctors:** `struct T { T(); T(const T&); T(T&&) noexcept; };  T
   makeA() { return T{}; }  T makeB() { T x; return x; }  T a = makeA(); T b =
   makeB();` — how many ctor calls total (normal build)?

   <details><summary>Answer</summary>

   `makeA()` → 1 (`T{}` constructed in `a`'s slot — guaranteed elision). `makeB()`
   → 1 (`x` is `b`'s slot — NRVO). Total **2** ctor calls, 0 copies, 0 moves.
   With `-fno-elide-constructors`: `makeB` adds a move ctor + a temp dtor.
   </details>

2. **Fix the pessimization:** `std::string upper(std::string s) { for (auto& c :
   s) c = toupper(c); return std::move(s); }` — what's wrong, and does removing
   `std::move` here lose the move?

   <details><summary>Answer</summary>

   `-Wpessimizing-move`. Remove it: `return s;` — `s` is a by-value parameter
   named in a `return` → treated as an rvalue → **implicitly moved** into the
   return slot (NRVO doesn't apply to params, but the implicit move does). So
   `return s;` is still a move, not a copy — and it doesn't block any elision
   that was possible.
   </details>

3. **Non-movable return:** `struct NM { NM(int); NM(const NM&) = delete;
   NM(NM&&) = delete; };  NM f() { return NM{5}; }  NM x = f();` — compiles in
   C++17? In C++14?

   <details><summary>Answer</summary>

   C++17: **yes** — guaranteed prvalue elision, `NM{5}` constructed directly in
   `x`, no copy/move needed. C++14: **no** — the return technically needs a
   move/copy ctor (even if elided), and both are deleted.
   </details>

4. **NRVO blocked:** `T pick(bool b) { T x, y; if (b) return x; return y; }` —
   does NRVO apply? What happens on each `return`?

   <details><summary>Answer</summary>

   NRVO can't apply (two different named locals, one return slot). Each `return
   x;` / `return y;` → `x`/`y` are locals named in a `return` → **implicitly
   moved** into the return slot (move ctor). One move per call, not a copy.
   </details>

5. **-fno-elide effect:** for `makeRVO()` returning `Tracer{1}` (a prvalue), what
   does `-fno-elide-constructors` change in the output? For `makeNRVO()`
   returning a named `x`?

   <details><summary>Answer</summary>

   `makeRVO()` — **no change**, still one ctor (C++17 guaranteed prvalue elision
   is unaffected by the flag). `makeNRVO()` — now prints an extra **MOVE ctor**
   and a temporary **destructor** (the flag disabled NRVO, so the local is moved
   into the return slot).
   </details>

---

## Interview questions

1. RVO vs NRVO — kaunsa guaranteed (C++17), kaunsa best-effort?
2. Guaranteed prvalue elision — kya matlab (copy/move ctor ki zaroorat)?
3. NRVO kab **nahi** apply hota (3 cases)? Fallback kya?
4. `return std::move(local)` — kya karta, kyun bura?
5. `-fno-elide-constructors` kya disable karta, kya nahi?
6. Elision side-effect-wali ctors ko skip kar sakta — iska implication?

---

## Next
→ [`12-perfect-forwarding.md`](12-perfect-forwarding.md)
