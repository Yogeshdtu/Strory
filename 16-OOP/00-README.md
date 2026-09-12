# 16 — OOP (Inheritance & Polymorphism) (PHASE 6)

## Prerequisites
`15-CLASSES`

## Yeh folder kyun
Inheritance, virtual functions, aur **vtable** — jo HFT mein aksar **avoid** kiya
jaata hai, aur aapko pata hona chahiye kyun.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-inheritance-basics.md` | Base/derived, `public`/`protected`/`private` inheritance |
| 02 | `02-constructors-destructors-order.md` | Construction/destruction ka order, base pehle |
| 03 | `03-virtual-functions.md` | `virtual`, runtime polymorphism, dynamic dispatch |
| 04 | `04-vtable-deep-dive.md` | **vtable aur vptr** — memory layout, dispatch ka exact mechanism |
| 05 | `05-override-and-final.md` | `override`, `final`, common override bugs |
| 06 | `06-abstract-classes.md` | Pure virtual, abstract base, interfaces |
| 07 | `07-virtual-destructors.md` | **Virtual destructor kyun mandatory hai**, memory leak demo |
| 08 | `08-object-slicing.md` | Slicing kya hai, kaise bachein |
| 09 | `09-multiple-inheritance.md` | Multiple inheritance, ambiguity, diamond problem |
| 10 | `10-virtual-inheritance.md` | Virtual base classes, layout complexity, cost |
| 11 | `11-rtti-and-dynamic-cast.md` | RTTI, `typeid`, `dynamic_cast`, **cost aur HFT mein kyun avoid** |
| 12 | `12-virtual-dispatch-cost.md` | **Measured cost** — indirect call, branch misprediction, inlining block |
| 13 | `13-crtp.md` | **CRTP** — static polymorphism, zero-cost alternative |
| 14 | `14-composition-vs-inheritance.md` | Kab inheritance, kab composition |
| 15 | `15-solid-principles.md` | SOLID, practical examples |
| 16 | `16-exercises.md` | Practice + design problems |

## Examples

| File | Kya |
|---|---|
| `examples/01_inheritance.cpp` | Basic inheritance |
| `examples/02_virtual_functions.cpp` | Polymorphism |
| `examples/03_vtable_layout.cpp` | vptr aur vtable dekhna |
| `examples/04_virtual_destructor.cpp` | ⚠️ Missing virtual dtor = leak |
| `examples/05_slicing.cpp` | Object slicing demo |
| `examples/06_diamond.cpp` | Diamond problem + virtual inheritance |
| `examples/07_dispatch_benchmark.cpp` | Virtual vs direct vs CRTP — measured |
| `examples/08_crtp.cpp` | CRTP static polymorphism |

## Time
2 hafte

## Status
✅ **COMPLETE** (Batch 6 — PHASE 6). 15 lessons + exercises + 8 compile-verified
examples. Inheritance (access modes, layout, name hiding), ctor/dtor order (+
virtual-call-in-ctor trap), virtual functions, **vtable/vptr deep dive**,
`override`/`final`, abstract classes/interfaces, **virtual destructors** (leak
demo), object slicing, multiple inheritance, virtual inheritance/diamond,
RTTI/`dynamic_cast`, **virtual dispatch cost (measured ~10x)**, **CRTP** (zero-cost
static poly), composition vs inheritance, SOLID (+ HFT reconciliation). Measured:
virtual ~23 ns vs direct/CRTP ~2.2 ns vs `variant` ~16 ns per call.

## Next
→ [`../17-RAII/00-README.md`](../17-RAII/00-README.md)
