# 15 — CLASSES (PHASE 6)

## Prerequisites
`11-STRUCTS`, `14-MEMORY`

## Yeh folder kyun
Struct ne data ko bandha. Class us data ke saath **behaviour** bhi bandhti hai —
aur usko **protect** karti hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-what-is-a-class.md` | Class kya hai, blueprint vs object, encapsulation ka pehla idea |
| 02 | `02-members-and-methods.md` | Data members, member functions, `this` pointer |
| 03 | `03-access-specifiers.md` | `public`/`private`/`protected`, `struct` vs `class` default |
| 04 | `04-constructors.md` | Default, parameterized, delegating, `= default`, `= delete` |
| 05 | `05-member-initializer-lists.md` | **Initializer list vs assignment** — kyun matter karta hai, order rules |
| 06 | `06-destructors.md` | Destructor, kab chalta hai, cleanup, `virtual` destructor preview |
| 07 | `07-const-member-functions.md` | `const` methods, `mutable`, const-correctness |
| 08 | `08-static-members.md` | Static data members, static methods, initialization |
| 09 | `09-operator-overloading.md` | Operators overload karna, rules, `<<` for classes, spaceship `<=>` |
| 10 | `10-friend-functions.md` | `friend`, kab zaroori hai, encapsulation pe asar |
| 11 | `11-explicit-constructors.md` | `explicit`, implicit conversion se bachna |
| 12 | `12-nested-and-local-classes.md` | Nested classes, local classes |
| 13 | `13-class-layout.md` | Memory layout, padding, `sizeof(class)`, empty base optimization intro |
| 14 | `14-exercises.md` | Practice + design problems |

## Examples

| File | Kya |
|---|---|
| `examples/01_first_class.cpp` | Pehli class |
| `examples/02_constructors.cpp` | Saare constructor types |
| `examples/03_initializer_list.cpp` | Init list vs assignment — order trap |
| `examples/04_const_methods.cpp` | const-correctness |
| `examples/05_static_members.cpp` | Static members |
| `examples/06_operator_overload.cpp` | Money class with operators |
| `examples/07_class_layout.cpp` | Memory layout dekhna |
| `examples/08_order_class.cpp` | HFT-style Order class |

## Time
1–2 hafte

## Status
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../16-OOP/00-README.md`](../16-OOP/00-README.md)
