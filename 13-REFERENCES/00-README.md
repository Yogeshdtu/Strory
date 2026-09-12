# 13 — REFERENCES (PHASE 5)

## Prerequisites
`12-POINTERS`

## Yeh folder kyun
Pointers ka safer, cleaner cousin. Aur yeh **move semantics** (folder 18) ka
direct prerequisite hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-what-is-a-reference.md` | Reference = alias, ek hi object ke do naam, memory diagram |
| 02 | `02-reference-vs-pointer.md` | Poora comparison table, kab kaunsa use karein |
| 03 | `03-references-as-parameters.md` | Pass by reference, copies bachana, output parameters |
| 04 | `04-const-references.md` | `const T&` — bade objects ke liye standard parameter type |
| 05 | `05-returning-references.md` | Return by reference, **dangling reference ka khatra** |
| 06 | `06-references-in-loops.md` | `for (auto& x : v)` vs `const auto&` vs `auto` — copies ka fark |
| 07 | `07-reference-members.md` | Class members as references, initialization constraints |
| 08 | `08-rvalue-references-intro.md` | `T&&` ka pehla parichay — poora folder 18 mein |
| 09 | `09-reference-bugs.md` | Dangling refs, lifetime issues, reference to temporary |
| 10 | `10-exercises.md` | Practice + diagrams |

## Examples

| File | Kya |
|---|---|
| `examples/01_references_basics.cpp` | Reference vs copy vs pointer |
| `examples/02_pass_by_reference.cpp` | Function parameters |
| `examples/03_const_ref_performance.cpp` | Copy vs const-ref benchmark |
| `examples/04_dangling_reference.cpp` | ⚠️ Dangling reference demo |
| `examples/05_ref_vs_ptr.cpp` | Side-by-side comparison |

## Time
1 hafta

## Status
✅ **COMPLETE** (Batch 5 — PHASE 5). 9 lessons + exercises + 5 compile-verified
examples. Alias model, reference vs pointer (poora table), pass-by-reference,
`const T&` + lifetime-extension rules, returning references + dangling, range-`for`
copy-vs-alias, reference members (+ `reference_wrapper`), `T&&`/move ka pehla
parichay, aur reference-bug catalog. Benchmark: by-value vs `const&` `std::vector`
= **~190x** measured (`03_const_ref_performance.cpp`, `-O2`).

## Next
→ [`../14-MEMORY/00-README.md`](../14-MEMORY/00-README.md)
