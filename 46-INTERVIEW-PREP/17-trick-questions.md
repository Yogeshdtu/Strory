# 17 — Trick questions: classic C++ traps and the right answers

## Prerequisites
Folders `02`–`28`, `45/12` (bug catalog). These are the "gotcha" questions
that separate memorized C++ from understood C++. For each: the trap, the
**right answer**, and *why*.

---

## A — Undefined / unspecified behaviour

### A1. `int i = 0; i = i++ + ++i;` — what's `i`?
<details><summary>Answer</summary>
Pre-C++17: **undefined behaviour** (multiple unsequenced modifications of
`i`). C++17: still **unspecified** value. Right answer: "don't write
this; there's no defined result to give you." (`05-OPERATORS/10`.)
</details>

### A2. `a[i] = i++;` — defined?
<details><summary>Answer</summary>
Pre-C++17 UB (the read of `i` for the index and the modification are
unsequenced). C++17 sequences the RHS before the LHS assignment's side
effect but the index evaluation vs `i++` is still murky — treat as
"don't." Say: "unsequenced modification and use — avoid." (`05/10`.)
</details>

### A3. Signed integer overflow — what happens?
<details><summary>Answer</summary>
**UB.** Not "wraps to negative" — the compiler is allowed to assume it
*never happens* and optimize accordingly (e.g. `x + 1 > x` → `true`,
deleting a bounds check). Unsigned overflow *is* defined (wraps mod 2ⁿ).
Fix: check via subtraction, wider type, or `__builtin_add_overflow`.
(`23/13`, `45/12` D1.)
</details>

### A4. `int x = INT_MAX; x++;` vs `unsigned u = UINT_MAX; u++;`
<details><summary>Answer</summary>
`x++` → **UB** (signed overflow). `u++` → **defined**, wraps to `0`.
This asymmetry is a favorite. (`03-VARIABLES`, `45/12` D1/D2.)
</details>

### A5. Dereferencing a null pointer — always a crash?
<details><summary>Answer</summary>
It's **UB**, not "guaranteed SIGSEGV." Usually crashes because page 0 is
unmapped, but the optimizer may have already assumed the pointer is
non-null (because you dereferenced it) and deleted your later `if (p)`
check → different, worse behaviour. Never rely on "null deref = clean
crash." (`12-POINTERS`, `45/05`.)
</details>

---

## B — Object lifetime & references

### B1. `const std::string& s = getName();` where `getName()` returns
`std::string` by value — is `s` valid after this line?
<details><summary>Answer</summary>
**Yes** — binding a `const&` (or `&&`) directly to a temporary **extends
its lifetime** to the reference's scope. BUT if `getName()` returned a
`const std::string&` to an *internal* member of a temporary object, that
temporary dies and `s` dangles. And `std::string_view sv = getName();`
dangles regardless (view doesn't extend lifetime). (`25/05`, `13/09`,
`45/12` E2.)
</details>

### B2. Returning a reference to a local — `int& f() { int x = 5; return
x; }`
<details><summary>Answer</summary>
**UB** — `x` dies at the return; the caller holds a dangling reference.
`-Wreturn-local-addr` catches the direct form. Return by value instead.
(`13/09`, `45/12` A4.)
</details>

### B3. `std::vector<int> v{1,2,3}; int& r = v[0]; v.push_back(4); r = 9;`
<details><summary>Answer</summary>
**UB** if the `push_back` reallocated (size 3 likely == capacity 3) — `r`
now dangles into freed memory. Iterator/reference invalidation on
reallocation. `v.reserve(4)` first would make it safe. (`19`, `45/12`
A10.)
</details>

### B4. `auto&& x = std::vector<int>{1,2,3}; ` then use `x` — ok?
<details><summary>Answer</summary>
Ok — `auto&&` is a forwarding reference; binding to a prvalue extends the
temporary's lifetime to `x`'s scope. `x` is a valid `std::vector<int>&&`
you can use (as an lvalue). The trap is elsewhere: `auto&& r =
container.front();` then modifying the container. (`25/05`, `07`.)
</details>

---

## C — Classes & inheritance

### C1. `Widget w();` inside a function — what did you declare?
<details><summary>Answer</summary>
A **function** named `w` returning `Widget` (most vexing parse), not a
default-constructed object. Use `Widget w;` or `Widget w{};`. Similarly
`Widget w(Foo());` declares a function taking a function pointer. (`15`,
`02`.)
</details>

### C2. Calling a virtual function from a constructor.
<details><summary>Answer</summary>
You get the **current class's** version, not the derived override — the
derived part isn't constructed yet, the vptr still points at the base
vtable. Same in destructors (reversed). Don't rely on virtual dispatch in
ctors/dtors. (`25/02`, `16`, `45/12` E5-adjacent.)
</details>

### C3. `Base* p = new Derived(); delete p;` with a non-virtual `~Base`.
<details><summary>Answer</summary>
**UB** — only `~Base` runs; `~Derived` is skipped → Derived's resources
leak / worse. Fix: `virtual ~Base()`. Rule: any class deleted through a
base pointer needs a virtual destructor. (`16`, `45/12` E4.)
</details>

### C4. `std::vector<Animal> zoo; zoo.push_back(Dog{});` then
`zoo[0].speak()` (virtual) — barks?
<details><summary>Answer</summary>
No — **object slicing**. `vector<Animal>` stores `Animal` values; the
`Dog` was copied into an `Animal` subobject, vptr is `Animal`'s. Use
`std::vector<std::unique_ptr<Animal>>`. (`16`, `45/12` E7.)
</details>

### C5. Empty class — `sizeof(struct {})`?
<details><summary>Answer</summary>
**1** (not 0) — every object needs a distinct address. As a base
subobject it can be 0 bytes (EBO / `[[no_unique_address]]`). (`25/08`,
`04`.)
</details>

---

## D — STL & strings

### D1. `std::vector<bool> v(8); bool* p = &v[0];` — compiles?
<details><summary>Answer</summary>
No — `vector<bool>` is a bit-packed specialization; `operator[]` returns a
**proxy**, not `bool&`, so `&v[0]` isn't `bool*`. `auto x = v[0]` gives
the proxy, not a `bool`. Use `vector<char>` or `bitset`. (`19`, `05`.)
</details>

### D2. `std::string s = "hi"; const char* p = s.c_str(); s += " there";
puts(p);`
<details><summary>Answer</summary>
**UB (likely)** — `s += ...` may reallocate, invalidating `p`. Even
without realloc, the standard invalidates `c_str()`'s pointer on any
non-const operation. Re-call `c_str()` after mutation. (`10-STRINGS`.)
</details>

### D3. `for (auto x : getVector())` — any issue?
<details><summary>Answer</summary>
Fine — the range expression's temporary lives for the whole loop
(guaranteed). The trap is `for (auto x : getObject().itemsRef())` where
`itemsRef()` returns a reference into a temporary that *isn't* the range
object → the temporary dies, you iterate a dangling range. C++23
tightened this. (`25/05`.)
</details>

### D4. `std::map<K,V> m; V& r = m[key];` for a missing key — what
happens?
<details><summary>Answer</summary>
`operator[]` **inserts** a default-constructed `V` for a missing key and
returns a reference to it. Not a lookup-only op. Use `.at()` (throws),
`.find()`, or `.contains()` for pure lookup. Also `m[key]` on a `const
map` doesn't compile. (`19`.)
</details>

---

## E — Numeric & misc

### E1. `float f = 0.1f; if (f == 0.1) ...` — taken?
<details><summary>Answer</summary>
**Not taken** (usually) — `0.1f` (float-rounded) is promoted to `double`
for the comparison and doesn't equal `0.1` (double-rounded). Never `==`
on floats; compare with a tolerance, or use integers/fixed-point.
(`03-VARIABLES`, `45/12` C7.)
</details>

### E2. `char buf[8]; strcpy(buf, "12345678");` — fits?
<details><summary>Answer</summary>
No — `"12345678"` is 8 chars **+ a NUL terminator** = 9 bytes → 1-byte
overflow (stack smash). `strlen` counts 8, `sizeof` needs 9. Use
`snprintf`, `std::string`, or a bounded copy. (`10`, `45/12` A2.)
</details>

### E3. `sizeof(arr) / sizeof(arr[0])` inside a function taking `int
arr[]`.
<details><summary>Answer</summary>
Wrong — `arr` decayed to `int*`, so `sizeof(arr)` is 8 (pointer size),
and the "count" is `8/4 = 2` regardless of the real array size. Pass the
size explicitly, or take `std::span` / `std::array<int,N>&`. (`09`, `12`.)
</details>

### E4. `printf("%d\n", 3.0);` — prints 3?
<details><summary>Answer</summary>
**UB** — `%d` expects `int`, you passed a `double` (passed as 8 bytes in
a float register / differently than an int). Garbage, or crash. `printf`
format/type mismatch is UB. Use `%f`, or `std::print`/`std::format`
(type-checked), or iostreams. `-Wformat` catches it. (`04-INPUT-OUTPUT`.)
</details>

### E5. `#define MAX(a,b) a > b ? a : b` then `int x = MAX(1,2) + 3;`
<details><summary>Answer</summary>
Expands to `1 > 2 ? 1 : 2 + 3` = `1 > 2 ? 1 : 5` = **5**, not `2 + 3 = 5`
by luck here — try `MAX(3,2) + 3` → `3 > 2 ? 3 : 2 + 3` = **3**, not 6.
Macro precedence trap. Parenthesize everything, or use a `constexpr`
function / `std::max`. (`22`, `45/12` C3.)
</details>

### E6. `int arr[5] = {0}; int* p = arr + 10;` — legal?
<details><summary>Answer</summary>
Forming a pointer more than one-past-the-end is **UB** (even without
dereferencing). `arr + 5` (one past) is legal to form but not deref;
`arr + 6` or `arr + 10` is UB. (`12-POINTERS`, `25/15`.)
</details>

---

## F — "Why does this compile / run?"

### F1. `int main() {}` with no `return` — exit code?
<details><summary>Answer</summary>
`main` (only `main`) implicitly `return 0;` if it falls off the end.
Exit code **0**. Any other function with a non-void return that falls off
the end is UB. (`08-FUNCTIONS`.)
</details>

### F2. `std::vector<int> v; v.reserve(10); v[5] = 3;` — ok?
<details><summary>Answer</summary>
**UB** — `reserve` changes capacity, **not size**; `v.size()` is still 0,
so `v[5]` is out of bounds. Use `resize(10)` (constructs elements) or
`push_back`/`emplace_back`. (`19`, `05`.)
</details>

### F3. `T&& x = get();` where `get()` returns `T&&` (an rvalue ref) —
lifetime extended?
<details><summary>Answer</summary>
**No** — lifetime extension applies when binding to a *temporary*
(prvalue), not when binding to an xvalue that's a reference return. If
`get()` returns `std::move(local_temp)` you get a dangling reference.
Only `T x = get();` (a copy/move) is safe there. (`25/05`, `07`.)
</details>

---

## How to handle these in the room

- If you spot UB: **name it** ("that's undefined behaviour because...")
  and give the fix. Don't guess a value.
- If unsure: reason from the model (lifetime, sequencing, decay,
  slicing) out loud — the interviewer wants the reasoning.
- Many of these have a one-word tag: *most vexing parse*, *slicing*,
  *iterator invalidation*, *dangling*, *unsequenced*, *decay*. Knowing
  the tag signals you've seen it before.

## Next
→ [`18-behavioural.md`](18-behavioural.md)
