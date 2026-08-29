# 25 — OBJECT MODEL & UNDEFINED BEHAVIOUR (PHASE 15)

## Prerequisites
`24-COMPILATION-LINKING`, `18-COPY-MOVE`

## Yeh folder kyun
Yeh folder **C++ ke rules ka asli sach** batata hai. Object kya hai (standard ke
hisaab se), lifetime kya hai, aur UB kahan-kahan chhupa hai.

Yeh senior-level knowledge hai — aur HFT interviews mein yahin se sabse mushkil
sawal aate hain.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-what-is-an-object.md` | Standard ki definition — storage, type, lifetime, value |
| 02 | `02-object-lifetime.md` | Lifetime start/end, storage vs lifetime, reuse |
| 03 | `03-storage-duration.md` | Automatic, static, dynamic, thread — chaaron |
| 04 | `04-initialization-order.md` | **Static initialization order fiasco**, `constinit`, function-local statics |
| 05 | `05-temporaries.md` | Temporary objects, **lifetime extension** rules, dangling traps |
| 06 | `06-trivial-standard-layout-pod.md` | **Trivial, standard-layout, POD** — kya matlab, kyun matter karta hai |
| 07 | `07-object-representation.md` | Value vs object representation, padding bytes, `memcmp` traps |
| 08 | `08-alignment-deep.md` | Alignment rules, `alignas`, over-aligned types, `std::align` |
| 09 | `09-placement-new.md` | **Placement new**, manual lifetime management, aligned storage |
| 10 | `10-strict-aliasing.md` | **Strict aliasing rule** — kya legal hai, `memcpy` idiom, `std::bit_cast` |
| 11 | `11-type-punning.md` | Type punning ke saare tareeke, kaunsa legal hai |
| 12 | `12-casts-deep.md` | Chaaron casts poora, `dynamic_cast` internals, RTTI layout |
| 13 | `13-vtable-layout.md` | vtable ka exact memory layout, multiple inheritance mein kya hota hai |
| 14 | `14-abi.md` | **ABI** — Itanium C++ ABI, ABI breaks, versioning |
| 15 | `15-undefined-behaviour-catalog.md` | **UB ka poora catalog** — 50+ cases, compiler kya assume karta hai |
| 16 | `16-compiler-assumptions.md` | UB se compiler kaise optimize karta hai — real examples |
| 17 | `17-exercises.md` | Practice + UB hunting |

## Examples

| File | Kya |
|---|---|
| `examples/01_lifetime_demo.cpp` | Lifetime start/end |
| `examples/02_static_init_fiasco/` | ⚠️ Order fiasco reproduce karo |
| `examples/03_temporaries.cpp` | Lifetime extension |
| `examples/04_type_properties.cpp` | `is_trivial`, `is_standard_layout` etc. |
| `examples/05_placement_new.cpp` | Manual construction/destruction |
| `examples/06_strict_aliasing.cpp` | ⚠️ Aliasing violation vs `bit_cast` |
| `examples/07_vtable_inspect.cpp` | vtable memory dekhna |
| `examples/08_ub_examples.cpp` | ⚠️ UB catalog — UBSan ke saath chalao |

## Time
2–3 hafte

## Status
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../26-CONCURRENCY/00-README.md`](../26-CONCURRENCY/00-README.md)
