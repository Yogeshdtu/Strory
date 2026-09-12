# 07 — Layer 6: value categories, move semantics, forwarding, Rule of 5

## Prerequisites
Folder `18-COPY-MOVE`, `22-MODERN-CPP`, `25-OBJECT-MODEL/05`. This is a
very common "do you actually understand C++" filter.

---

## A — Value categories

### A1. lvalue, prvalue, xvalue — define, ek example har ek.
<details><summary>Answer</summary>
**lvalue** — has identity, not movable-from implicitly: a named variable
`x`, `*p`, `arr[i]`, a function returning `T&`. **prvalue** — pure
value, no identity: `42`, `a + b`, `T{}`, a function returning `T` (by
value). **xvalue** — has identity **and** movable: `std::move(x)`, a
function returning `T&&`, `arr[i]` where arr is an rvalue array.
"glvalue" = lvalue ∪ xvalue (has identity). "rvalue" = prvalue ∪ xvalue
(movable). (`25/05`.)
</details>

### A2. `std::move` sach mein kya karta hai?
<details><summary>Answer</summary>
**Kuch nahi move karta.** Ek `static_cast<T&&>` hai — apne argument ko
xvalue banata taaki move constructor/assignment (jo `T&&` leta) resolve
ho. Actual "stealing" move ctor/assignment ke andar hota. `std::move` on
a `const` object → `const T&&` → move ctor `T&&` match nahi karta → copy
ctor chalta (silent). (`18`, `06`.)
</details>

### A3. Move ke baad object ka state?
<details><summary>Answer</summary>
**Valid but unspecified.** Tum ise safely destroy ya reassign kar sakte
ho; koi aur assumption nahi. Standard library types usually "empty-ish"
ho jaate (`vector` → empty, `unique_ptr` → null, `string` →
unspecified, often empty). Moved-from object ko **read** karna
(`45/12` E3) = bug (well-defined but wrong data).
</details>

### A4. `int&& r = 5;` — legal? `r` ka value category?
<details><summary>Answer</summary>
Legal — rvalue reference ek temporary ko bind kar sakti aur uski lifetime
extend karti (scope tak). But `r` **as an expression is an lvalue** (it
has a name). Isliye `f(r)` lvalue overload ko match karta — agar aage
move karna hai to `f(std::move(r))`. (`25/05`, `18`.)
</details>

---

## B — Rule of 5, noexcept, moves

### B1. Move constructor `noexcept` kyun important hai — concrete impact?
<details><summary>Answer</summary>
`std::vector` reallocation pe: agar element ka move ctor `noexcept` hai →
`vector` **move** karta elements ko (fast). Agar move ctor **throw kar
sakta** → strong exception guarantee ke liye `vector` **copy** karta
(slow) — kyunki ek move ke beech throw hua to purana state recover nahi
ho paata. `std::move_if_noexcept`. Isliye move ops ko hamesha `noexcept`
banao. (`18`, `23`.)
</details>

### B2. Rule of 5 — poore 5 members list karo.
<details><summary>Answer</summary>
Destructor, copy constructor, copy assignment, move constructor, move
assignment. Agar ek bhi custom chahiye (raw resource ownership) → paanchon
ko explicitly manage karo (`= default` / `= delete` / custom). Better:
Rule of Zero — resource ko ek RAII member (`unique_ptr`, `vector`) mein
wrap karo, phir kuch bhi likhna nahi padta. (`17`, `18`.)
</details>

### B3. `T obj = std::move(other);` — copy ctor call ho sakta hai?
<details><summary>Answer</summary>
Haan, agar: (a) `T` ka move ctor nahi hai (ya deleted / not generated
because a dtor/copy is user-declared) → copy ctor `const T&` xvalue ko
bind kar leta. (b) `other` `const` hai → `const T&&` → move ctor match
nahi → copy. Silent performance bug. (`18`, `06`.)
</details>

### B4. Self-move-assignment (`x = std::move(x)`) — safe hona chahiye?
<details><summary>Answer</summary>
Standard says moved-from must be valid; self-move ideally leaves the
object valid (maybe unspecified). Naive move-assign (`delete ptr_; ptr_ =
other.ptr_; other.ptr_ = nullptr;`) self-move pe apne ko `delete` karke
UB. Guard karo (`if (this != &other)`) ya copy-and-swap style. Rare in
practice but interviewers test the awareness. (`18`.)
</details>

### B5. `return std::move(local);` — good or bad?
<details><summary>Answer</summary>
**Bad** (usually) — `return local;` already does NRVO (elides the
copy/move entirely). `std::move` par NRVO ko **disable** kar deta (ab ek
xvalue return hai, not a named object) → forced move instead of zero
copies. `-Wpessimizing-move` warn karta. Exception: returning a member or
a by-value parameter — there `std::move` genuinely helps. (`18`, `25/05`.)
</details>

---

## C — Perfect forwarding

### C1. Perfect forwarding problem — kya hai?
<details><summary>Answer</summary>
Ek wrapper `template<class... A> auto make(A&&... a)` ko args ko
underlying ctor tak **exactly** as received pahunchana hai — lvalue as
lvalue, rvalue as rvalue, const preserved. Bina forwarding: sab named
args lvalues ban jaate andar → moves kho jaate. `std::forward<A>(a)...`
category preserve karta. (`21`, `07`.)
</details>

### C2. Forwarding reference vs rvalue reference — syntax same, kaise
distinguish?
<details><summary>Answer</summary>
`T&&` **forwarding reference** hai sirf jab `T` us function template ke
liye **deduce** hota (`template<class T> void f(T&& x)`). `void f(int&&
x)` (concrete), `void f(std::vector<T>&& x)` (not the bare param),
`Foo&&` in a non-deduced context → **plain rvalue reference**. Also
`auto&& x = ...;` is a forwarding reference. (`07`, `21`.)
</details>

### C3. `emplace_back` perfect forwarding ka faayda ek example se.
<details><summary>Answer</summary>
`std::vector<std::string> v; v.emplace_back(10, 'x');` → `std::string`
ke `(size_t, char)` ctor ko args in-place forward karta — ek `string`
banta directly in the vector's storage. `push_back` mein `string(10,'x')`
temporary banti phir move hoti. Args ki value category preserved. (`19`,
`21`.)
</details>

---

## D — HFT-flavoured

### D1. HFT mein move semantics kyun matter — ek concrete case.
<details><summary>Answer</summary>
Ek `std::vector<Trade>` ya `std::string` ko function boundaries ke aar-
paar bhejna: move → pointer swap (O(1)), copy → allocation + memcpy (O(n)
+ heap, jitter). Return-by-value of large results (order book snapshot,
parsed message batch) free ho jaata NRVO/move se. Par HFT hot path
mein ideally **koi ownership transfer hi nahi** — pre-allocated buffers,
indices into pools; move is the fallback when you must hand off. (`36`,
`43`.)
</details>

### D2. "Move is always faster than copy" — true?
<details><summary>Answer</summary>
Nahi. Move of a type with no heap resource (`int`, `std::array<int, N>`,
a small SSO string) = same as copy (nothing to steal, still memcpy N
bytes). Move helps when copy = allocation + deep copy. Also a move that
isn't `noexcept` can force `vector` to copy anyway (`B1`). (`18`.)
</details>

### D3. `std::string` move — `noexcept`? SSO ke saath?
<details><summary>Answer</summary>
`std::string`'s move ctor **is** `noexcept` in the standard. But for a
**small** string (SSO), "move" still copies the inline bytes (there's no
heap buffer to steal) — so a moved-from small string may be unchanged,
and the move isn't free. Large string → pointer steal, moved-from becomes
empty. (`10-STRINGS`, `18`.)
</details>

### D4. `noexcept` move + `std::vector` growth — measure kya dikhata?
<details><summary>Answer</summary>
A type with `noexcept` move → `vector` grow relocates via moves (fast,
O(1)-ish per element for a heap-owning type). Remove the `noexcept` (or
give it a throwing move) → `vector` relocates via **copies** → for a
million heap-owning elements that's a million allocations + deep copies
on every growth. This is a favourite "why does adding `noexcept` speed
this up 10×" question. (`18`, `19`, `43`.)
</details>

---

## Interview tips for Layer 6

- "`std::move` doesn't move — it's a cast to rvalue" — say this exactly.
- Moved-from = "valid but unspecified; only destroy or reassign it."
- `noexcept` on moves is not cosmetic — it changes `vector` reallocation
  from moves to copies. This is the #1 practical move-semantics gotcha.
- `return std::move(local)` is a **pessimization** (kills NRVO) — know
  the exceptions (member / by-value param).
- Forwarding reference = deduced `T&&`; know reference collapsing
  (`& &&` → `&`).

## Next
→ [`08-concurrency-questions.md`](08-concurrency-questions.md)
