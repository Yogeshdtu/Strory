# 19 — STANDARD LIBRARY (PHASE 9)

## Prerequisites
`18-COPY-MOVE` (move semantics zaroori hai)

## Yeh folder kyun
Yeh course ka **sabse bada folder** hai. Spec ne kaha tha: "vector/string/map/
unordered_map pe mat rukna." Isliye hum poori standard library audit karenge.

Aur har container ke saath **performance characteristics** aur **HFT mein kaunsa kab**
bhi padhenge.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-stl-architecture.md` | Containers + Iterators + Algorithms — teenon ka rishta |
| 02 | `02-vector-deep.md` | **`std::vector`** — growth, `reserve`, `capacity`, invalidation, `shrink_to_fit` |
| 03 | `03-array-and-span.md` | `std::array`, `std::span` recap |
| 04 | `04-deque-list-forward-list.md` | `deque` ka internal layout, `list`, `forward_list`, kab kaunsa |
| 05 | `05-map-and-set.md` | `map`/`set`/`multimap`/`multiset` — red-black trees, ordered guarantees |
| 06 | `06-unordered-containers.md` | **Hash tables** — buckets, load factor, custom hash, collision handling |
| 07 | `07-container-adapters.md` | `stack`, `queue`, `priority_queue` |
| 08 | `08-iterators.md` | Iterator categories, invalidation rules, `<iterator>` utilities |
| 09 | `09-algorithms-part1.md` | Non-modifying: `find`, `count`, `all_of`, `search`, `equal`, `mismatch` |
| 10 | `10-algorithms-part2.md` | Modifying: `copy`, `transform`, `replace`, `remove`, **erase-remove idiom** |
| 11 | `11-algorithms-part3.md` | Sorting: `sort` (introsort), `stable_sort`, `partial_sort`, `nth_element` |
| 12 | `12-algorithms-part4.md` | Binary search, set operations, heap operations, permutations, min/max |
| 13 | `13-numeric-algorithms.md` | `<numeric>`: `accumulate`, `reduce`, `inner_product`, `iota`, scans |
| 14 | `14-ranges.md` | **`<ranges>` (C++20)** — views, pipelines, lazy evaluation |
| 15 | `15-utility-types.md` | `pair`, `tuple`, `optional`, `variant`, `any`, `expected` (C++23) |
| 16 | `16-functional.md` | `<functional>`: `std::function` (**aur uski cost**), `bind`, invocables, lambdas deep |
| 17 | `17-chrono.md` | `<chrono>` — durations, time points, clocks, **timing code correctly** |
| 18 | `18-random.md` | `<random>` — engines, distributions, seeding, `rand()` kyun bura hai |
| 19 | `19-filesystem.md` | `<filesystem>` — paths, directory iteration, file operations |
| 20 | `20-regex.md` | `<regex>` — basics aur **kyun woh slow hai** |
| 21 | `21-type-traits.md` | `<type_traits>` — introspection, SFINAE ka foundation |
| 22 | `22-bit-utilities.md` | `<bit>` (C++20): `bit_cast`, `popcount`, `countl_zero`, `byteswap` (C++23) |
| 23 | `23-allocators.md` | Allocator concept, custom allocators, `std::allocator` |
| 24 | `24-memory-resource.md` | **PMR (`<memory_resource>`)** — polymorphic allocators, monotonic buffer |
| 25 | `25-container-performance.md` | **Poora performance table**, cache behaviour, benchmarks |
| 26 | `26-stl-in-hft.md` | **HFT mein kaunsa container** — aur aksar 'koi nahi' kyun hota hai |
| 27 | `27-exercises.md` | Practice + STL problems |

## Examples

| File | Kya |
|---|---|
| `examples/01_vector_deep.cpp` | Growth, reserve, invalidation — measured |
| `examples/02_map_vs_unordered.cpp` | Lookup benchmark, cache behaviour |
| `examples/03_algorithms_tour.cpp` | 30+ algorithms ka demo |
| `examples/04_erase_remove.cpp` | Erase-remove idiom |
| `examples/05_ranges_demo.cpp` | C++20 ranges pipelines |
| `examples/06_optional_variant.cpp` | `optional`, `variant`, `expected` |
| `examples/07_chrono_timing.cpp` | Code timing karne ka sahi tareeka |
| `examples/08_std_function_cost.cpp` | `std::function` vs lambda vs function pointer — measured |
| `examples/09_custom_allocator.cpp` | Ek chhota custom allocator |
| `examples/10_pmr_demo.cpp` | PMR monotonic buffer — allocation-free container |
| `examples/11_container_benchmark.cpp` | Poora container comparison suite |

## Time
4–6 hafte

## Status
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../20-ALGORITHMS-DSA/00-README.md`](../20-ALGORITHMS-DSA/00-README.md)
