# 05 — OPERATORS (PHASE 2)

## Prerequisites
`03-VARIABLES-DATA-TYPES`, `04-INPUT-OUTPUT`

## Yeh folder kyun
Variables ban gaye, I/O aa gaya. Ab un values pe **kaam** karna seekhenge.

Bitwise operators is folder ka sabse important hissa hain — woh market data parsing,
flags, aur low-level optimization mein har jagah use hote hain.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-arithmetic-operators.md` | `+ - * / %`, integer division traps, `%` negative numbers ke saath |
| 02 | `02-increment-decrement.md` | `++i` vs `i++` deep, iterators ke saath performance fark |
| 03 | `03-comparison-operators.md` | `== != < > <= >=`, float comparison, `<=>` spaceship (C++20) intro |
| 04 | `04-logical-operators.md` | `&& || !`, **short-circuit evaluation** deep, safety patterns |
| 05 | `05-bitwise-operators.md` | **`& | ^ ~ << >>` deep dive** — bit manipulation ka poora toolkit |
| 06 | `06-bit-tricks.md` | Set/clear/toggle/test bit, masks, flags, `popcount`, power-of-2 checks, `<bit>` (C++20) |
| 07 | `07-compound-assignment.md` | `+= -= *= /= %= &= |= ^= <<= >>=` — aur woh sirf shortcut kyun nahi hain |
| 08 | `08-ternary-operator.md` | `?:`, kab use karein, nested ternary se kyun bachein |
| 09 | `09-precedence-associativity.md` | Poori precedence table, common precedence bugs, brackets ka rule |
| 10 | `10-evaluation-order.md` | **Sequence points / sequenced-before**, `i = i++ + ++i` kyun UB hai, C++17 ke changes |
| 11 | `11-other-operators.md` | `sizeof`, comma operator, `,` ke traps, `::`, member access preview |
| 12 | `12-exercises.md` | Practice + bit manipulation problems |

## Examples

| File | Kya |
|---|---|
| `examples/01_arithmetic.cpp` | Division/modulo traps |
| `examples/02_bitwise_basics.cpp` | Har bitwise operator ka demo |
| `examples/03_bit_manipulation.cpp` | Set/clear/toggle/test, masks, flags |
| `examples/04_short_circuit.cpp` | Short-circuit se safety |
| `examples/05_precedence_traps.cpp` | Precedence bugs |
| `examples/06_bit_flags.cpp` | Order flags — HFT style bitfield usage |

## Time
5–7 din

## Status
✅ **COMPLETE** (Batch 2, PHASE 2). 12 lessons + `12-exercises.md` +
6 compile-verified examples. Full operator set + precedence/associativity,
**evaluation order & sequencing rules**, **bitwise operators** (systems
depth), bit tricks, and HFT-style bit-flag structs.

## Next
→ [`../06-CONDITIONS/00-README.md`](../06-CONDITIONS/00-README.md)
