# 03 — VARIABLES & DATA TYPES (PHASE 2)

## Prerequisites
- Folder `01-PROGRAMMING-BASICS` (khaas kar lesson 10: bits/binary, lesson 11: memory)
- Folder `02-CPP-FIRST-STEPS` (poora)

## Yeh folder kyun

Ab tak aapne sirf **fixed text** print kiya hai. Ab hum **data** store karenge.

Yeh folder deliberately **bahut slow** hai. Hum `int age = 20;` ko itna tod ke padhenge
ki aapko har cheez clear ho jaye:
- `age` kya hai?
- `int` kya hai?
- `=` exactly kya karta hai?
- `20` kahan gaya?
- Memory mein kya hua?

Kyunki yehi cheezein aage pointers (12), memory (14), aur object model (25) mein
foundation banti hain. Aur HFT interviews mein `int` ki exact size, range, aur overflow
behaviour poocha jaata hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-what-is-data.md` | Data kya hai, type kyun chahiye |
| 02 | `02-what-is-a-variable.md` | **Labelled box** analogy, memory mein kya banta hai |
| 03 | `03-declaration-definition-initialization.md` | Teenon ka exact fark |
| 04 | `04-the-assignment-operator.md` | `=` kya karta hai (aur `==` se fark) |
| 05 | `05-int-deep-dive.md` | Size, signed/unsigned, range, **overflow = UB** |
| 06 | `06-floating-point.md` | `float`/`double`, IEEE-754, precision traps |
| 07 | `07-char-and-ascii.md` | `char`, ASCII, signedness gotcha |
| 08 | `08-bool.md` | `bool`, conversions, size |
| 09 | `09-fixed-width-types.md` | `<cstdint>`, `int32_t`, **HFT ke liye mandatory** |
| 10 | `10-initialization-forms.md` | Copy/direct/brace init, narrowing, `{}` kyun best |
| 11 | `11-const-and-constexpr.md` | Immutability, compile-time constants |
| 12 | `12-auto-and-type-deduction.md` | `auto` kab use karein, kab nahi |
| 13 | `13-type-conversions.md` | Implicit conversions, integer promotion, casts |
| 14 | `14-sizeof-and-limits.md` | `sizeof`, `<limits>`, portability |
| 15 | `15-naming-and-style.md` | Naming conventions, readable code |
| 16 | `16-exercises.md` | Practice + self-assessment |

## Examples

| File | Kya |
|---|---|
| `examples/01_first_variable.cpp` | Pehla variable, step by step |
| `examples/02_all_types.cpp` | Saare fundamental types, sizes, ranges |
| `examples/03_integer_overflow.cpp` | Overflow ka demo (signed vs unsigned) |
| `examples/04_float_traps.cpp` | `0.1 + 0.2 != 0.3` aur baaki traps |
| `examples/05_initialization.cpp` | Sabhi initialization forms |
| `examples/06_conversions.cpp` | Implicit conversion ke bugs |
| `examples/07_fixed_width.cpp` | `<cstdint>` types, HFT-style struct |
| `examples/08_const_constexpr.cpp` | `const` vs `constexpr` |
| `examples/09_char_demo.cpp` | `char` as number, signedness gotcha |
| `examples/10_auto_demo.cpp` | `auto` ke fayde aur traps |

## Time
1–2 hafte. Jaldi mat karo — yeh foundation hai.

## Next
→ [`../04-INPUT-OUTPUT/00-README.md`](../04-INPUT-OUTPUT/00-README.md)
