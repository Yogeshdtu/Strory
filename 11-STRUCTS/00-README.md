# 11 — STRUCTS, UNIONS, ENUMS (PHASE 4)

## Prerequisites
`09-ARRAYS`, `10-STRINGS`

## Yeh folder kyun
Ab tak har variable akela tha. Ab hum **related data ko ek saath** bandhenge.

Aur yahan **padding aur alignment** poora samjhenge — jo HFT mein cache efficiency
ka sabse bada lever hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-what-is-a-struct.md` | Struct kya hai, members, dot operator, kyun chahiye |
| 02 | `02-struct-initialization.md` | Aggregate init, designated initializers (C++20), default member initializers |
| 03 | `03-nested-structs.md` | Structs ke andar structs, arrays of structs |
| 04 | `04-structs-and-functions.md` | Pass by value vs reference, return karna, copies ki cost |
| 05 | `05-padding-and-alignment.md` | **Padding deep dive**, `alignof`, `alignas`, member ordering se size optimization |
| 06 | `06-packed-structs.md` | `#pragma pack`, `__attribute__((packed))`, unaligned access ka trade-off |
| 07 | `07-aos-vs-soa.md` | **Array-of-Structs vs Struct-of-Arrays** — data-oriented design, HFT critical |
| 08 | `08-unions.md` | `union` kya hai, memory sharing, type punning, active member rules |
| 09 | `09-std-variant.md` | **`std::variant`** — type-safe union, `std::visit`, kab prefer karein |
| 10 | `10-enums.md` | `enum` vs **`enum class`**, underlying type, scoping, conversions |
| 11 | `11-bitfields.md` | Bitfields, packing, portability issues, alternatives |
| 12 | `12-struct-vs-class.md` | Sirf default access ka fark, convention |
| 13 | `13-exercises.md` | Practice + layout optimization problems |

## Examples

| File | Kya |
|---|---|
| `examples/01_struct_basics.cpp` | Struct banana aur use karna |
| `examples/02_padding_demo.cpp` | Padding dekhna, `-Wpadded` |
| `examples/03_struct_optimization.cpp` | Member reorder se size half |
| `examples/04_aos_vs_soa.cpp` | AoS vs SoA — measured benchmark |
| `examples/05_unions.cpp` | Union aur type punning |
| `examples/06_variant.cpp` | `std::variant` + `visit` |
| `examples/07_enums.cpp` | `enum class` best practices |
| `examples/08_market_data_struct.cpp` | HFT-style message struct with static_asserts |

## Time
1–2 hafte

## Status
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../12-POINTERS/00-README.md`](../12-POINTERS/00-README.md)
