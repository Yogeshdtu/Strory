# Mock transcript — C++ deep-dive round (~40 min)

> Read the **interviewer** lines aloud (a partner, or yourself), answer
> from memory, then compare with the **candidate** track. `[note]` lines
> are the grader's-eye view.

---

**Interviewer:** Let's start easy. What's the difference between a pointer
and a reference?

**Candidate:** A reference is an alias for an existing object — it must be
bound at initialization, can't be rebound, and can't be null in
well-formed code, so it doesn't need its own storage conceptually. A
pointer is itself an object that holds an address; it can be null, can be
reassigned, and you can do arithmetic on it. So a reference says
"guaranteed-valid alias", a pointer says "maybe-valid, rebindable
address."

`[note: crisp, hits rebind / null / storage. Good.]`

**Interviewer:** When would you pass by `const&` versus by value?

**Candidate:** By value for small cheap-to-copy types — `int`, `double`,
a small struct — or when the function needs its own copy anyway. By
`const&` for large read-only objects like `const std::string&` or
`const std::vector<T>&` to avoid the copy. If the function needs to
modify the caller's object, non-const `&`; though I'd often prefer
returning the result instead of an out-parameter.

**Interviewer:** Here's a class. `struct Buf { char* p; ~Buf(){ delete[]
p; } };` with a constructor that `new[]`s. What's wrong?

**Candidate:** Rule of Three. It owns a raw resource and has a
destructor, but the compiler-generated copy constructor and copy
assignment do a shallow pointer copy — so two `Buf` objects end up owning
the same buffer, and you get a double free when both destruct, or a
use-after-free if one is destroyed and the other is still used. I'd fix
it by Rule of Zero — make `p` a `std::vector<char>` or `std::string` and
delete the destructor — or, if I must keep the raw pointer, implement a
deep-copy copy constructor and copy assignment via copy-and-swap, plus
`noexcept` move constructor and move assignment. Or `= delete` the copies
if it shouldn't be copyable.

`[note: names the rule, the exact bug (double free / UAF), AND three
valid fixes with Rule of Zero first. Strong.]`

**Interviewer:** You mentioned `noexcept` moves. Why do they matter for
`std::vector`?

**Candidate:** When a `vector` grows and reallocates, it needs the strong
exception guarantee. If the element's move constructor is `noexcept`, it
moves the elements to the new buffer — cheap for a heap-owning type. If
the move constructor can throw, a throw partway through relocation would
leave the vector in a state it can't roll back, so `vector` **copies**
instead, via `move_if_noexcept`. For a million heap-owning elements
that's a million allocations plus deep copies on every growth. So adding
`noexcept` to your move operations can flip reallocation from O(1)-ish
per element to O(n) deep copy.

**Interviewer:** What does `std::move` actually do?

**Candidate:** Nothing at runtime — it's a `static_cast` to an rvalue
reference. It just makes the argument an xvalue so that a move
constructor or move assignment, which take `T&&`, get selected during
overload resolution. The actual stealing happens inside those functions.
And a gotcha: `std::move` on a `const` object gives you `const T&&`,
which a move constructor taking `T&&` won't bind to, so you silently get
a copy.

`[note: "nothing at runtime, it's a cast" — the answer they want. Plus
the const gotcha unprompted.]`

**Interviewer:** `return std::move(local);` — good or bad?

**Candidate:** Bad, usually. `return local;` already does NRVO — the
copy or move is elided entirely, zero operations. Wrapping it in
`std::move` turns it into an xvalue return, which disables NRVO and
forces an actual move. `-Wpessimizing-move` warns about it. The exception
is returning a data member or a by-value parameter — those can't be
NRVO'd, so `std::move` there is a genuine improvement.

**Interviewer:** Last one. Virtual destructor — why, and what exactly
goes wrong without it?

**Candidate:** If you `delete` a derived object through a base-class
pointer and `~Base` isn't virtual, only `~Base` runs — `~Derived` is
skipped, so any resources the derived part owns leak, and formally it's
undefined behaviour. With a virtual destructor, `delete` consults the
vtable and runs `~Derived` then `~Base`. The rule: any class that gets
deleted polymorphically needs a virtual destructor. A value type that's
never used as a base doesn't need one, and adding it would cost a vptr.

`[note: doesn't just cite the rule — explains the mechanism and the
consequence, and knows the cost of adding it unnecessarily.]`

---

## Rubric

| Dimension | Score 1–5 | Notes |
|---|---|---|
| Correctness | | Any wrong statement of fact? |
| Depth / mechanism | | "rule hai" vs "here's what the compiler does and why" |
| Modern C++ hygiene | | Rule of Zero first, `noexcept` moves, `nullptr`, NRVO awareness |
| Communication | | Structured, concise, offers the fix + trade-off |
| Handling follow-ups | | Does the answer survive "why?" |

**Bar for a strong hire:** mechanism-level answers on move semantics and
the object model, Rule of Zero instinct, and the `noexcept`-move-vs-copy
relocation point without prompting.
