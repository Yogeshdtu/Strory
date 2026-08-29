# 18 — COPY & MOVE SEMANTICS (PHASE 8)

## Prerequisites
`17-RAII`, `13-REFERENCES`

## Yeh folder kyun
**Yeh modern C++ ka dil hai.**

Move semantics ke bina C++11+ samajh nahi aayega. Aur HFT mein zero-copy thinking
yahin se aati hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-copy-constructor.md` | Copy constructor, kab chalta hai, deep vs shallow copy |
| 02 | `02-copy-assignment.md` | Copy assignment operator, self-assignment check |
| 03 | `03-rule-of-three.md` | **Rule of 3** — dtor, copy ctor, copy assign saath mein |
| 04 | `04-value-categories.md` | **lvalue, prvalue, xvalue, glvalue, rvalue** — poora taxonomy |
| 05 | `05-rvalue-references.md` | `T&&`, binding rules, kya bind hota hai |
| 06 | `06-move-constructor.md` | Move constructor, resource stealing, moved-from state |
| 07 | `07-move-assignment.md` | Move assignment, self-move |
| 08 | `08-std-move.md` | **`std::move` sirf ek CAST hai** — woh khud kuch move nahi karta |
| 09 | `09-rule-of-five.md` | **Rule of 5**, aur `= default` / `= delete` |
| 10 | `10-rule-of-zero.md` | **Rule of 0** — best practice, RAII members use karo |
| 11 | `11-copy-elision.md` | RVO, NRVO, **guaranteed copy elision (C++17)** |
| 12 | `12-perfect-forwarding.md` | Forwarding references, `std::forward`, reference collapsing |
| 13 | `13-noexcept-move.md` | **`noexcept` move kyun zaroori hai** — `vector` growth ka behaviour |
| 14 | `14-move-in-practice.md` | Kab move actually hota hai, kab nahi, common mistakes |
| 15 | `15-exercises.md` | Practice + apni move-enabled class likhna |

## Examples

| File | Kya |
|---|---|
| `examples/01_copy_semantics.cpp` | Copy ctor/assign trace |
| `examples/02_rule_of_three.cpp` | Deep copy class |
| `examples/03_value_categories.cpp` | lvalue/rvalue identify karna |
| `examples/04_move_semantics.cpp` | Move ctor/assign trace |
| `examples/05_std_move_demo.cpp` | `std::move` kya karta hai aur kya nahi |
| `examples/06_copy_elision.cpp` | RVO dekhna (`-fno-elide-constructors` se compare) |
| `examples/07_perfect_forwarding.cpp` | Forwarding references |
| `examples/08_noexcept_vector.cpp` | `noexcept` move ka vector growth pe asar — measured |
| `examples/09_copy_vs_move_bench.cpp` | Copy vs move cost |

## Time
2 hafte

## Status
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../19-STL/00-README.md`](../19-STL/00-README.md)
