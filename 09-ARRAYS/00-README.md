# 09 — ARRAYS (PHASE 4)

## Prerequisites
`08-FUNCTIONS`

## Yeh folder kyun
Ek variable ek value rakhta hai. Ab hum **kai values ek saath** rakhenge.

Aur yahan **array decay** milega — jo pointers ka darwaza hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-what-is-an-array.md` | Array kya hai, memory mein contiguous layout, zero-based indexing kyun |
| 02 | `02-declaring-and-initializing.md` | Declaration, aggregate init, partial init, `{}` se zeroing |
| 03 | `03-accessing-elements.md` | `arr[i]` actually kya karta hai (`*(arr + i)`), **out-of-bounds = UB** |
| 04 | `04-arrays-and-loops.md` | Traversal patterns, range-based for, `std::size` (C++17) |
| 05 | `05-array-decay.md` | **Array → pointer decay**, `sizeof` ka trap, function parameters |
| 06 | `06-multidimensional-arrays.md` | 2D/3D arrays, **row-major layout**, memory picture |
| 07 | `07-arrays-as-parameters.md` | Size pass karna, reference-to-array, template size deduction |
| 08 | `08-std-array.md` | `std::array` — size yaad rakhta hai, `.size()`, `.at()`, no decay |
| 09 | `09-std-span.md` | **`std::span` (C++20)** — non-owning view, modern parameter type |
| 10 | `10-array-performance.md` | **Cache locality**, prefetching, AoS vs SoA intro, stack vs heap arrays |
| 11 | `11-common-array-bugs.md` | Off-by-one, OOB, VLA absence, decay confusion, `delete` vs `delete[]` preview |
| 12 | `12-exercises.md` | Practice + memory diagrams |

## Examples

| File | Kya |
|---|---|
| `examples/01_array_basics.cpp` | Declaration, init, traversal |
| `examples/02_array_decay.cpp` | Decay ka demo — sizeof trap |
| `examples/03_2d_arrays.cpp` | Row-major layout dekhna |
| `examples/04_std_array.cpp` | `std::array` vs C array |
| `examples/05_span_demo.cpp` | `std::span` (C++20) |
| `examples/06_oob_asan.cpp` | ⚠️ Out-of-bounds — ASan se pakdo |
| `examples/07_aos_vs_soa.cpp` | Array-of-structs vs struct-of-arrays benchmark |

## Time
1 hafta

## Status
✅ **COMPLETE** (Batch 4). 11 lessons + exercises + 7 compile-verified examples.
Highlights: array decay + `sizeof` trap, `std::array` / `std::span`, row-major 2D,
**AoS vs SoA naapa** (`07_aos_vs_soa.cpp`: GCC 15.1 pe ~4.4x, GCC 16.2 pe 5.4–5.8x), OOB bug
catalogue — `_GLIBCXX_ASSERTIONS` (GCC 16.2 pe sirf `-O0` pe default), Linux pe ASan.

**Hinglish pass (gap-fix part 2):** lessons 04, 06–12 aur READMEs Hinglish mein; saath mein
accuracy fixes — `std::span` Windows x64 ABI pe pointer se jaata hai (assembly se dekha),
`_GLIBCXX_ASSERTIONS` ka `-O2` pe off hona, AoS/SoA GCC 16.2 pe dobara naapa.

## Next
→ [`../10-STRINGS/00-README.md`](../10-STRINGS/00-README.md)
