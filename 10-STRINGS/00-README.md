# 10 — STRINGS (PHASE 4)

## Prerequisites
`09-ARRAYS`, `07-char-and-ascii.md` (folder 03)

## Yeh folder kyun
Text handling. Aur `std::string` ke andar **SSO (Small String Optimization)** hai —
jo HFT mein allocation avoidance ka classic example hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-c-strings.md` | `char[]`, null terminator, `<cstring>` functions, buffer overflow risks |
| 02 | `02-std-string-basics.md` | `std::string` — construction, size, capacity, indexing |
| 03 | `03-string-operations.md` | Concatenation, `substr`, `find`, `replace`, `compare`, iteration |
| 04 | `04-string-internals-sso.md` | **SSO deep dive** — chhoti strings heap pe nahi jaatin, capacity growth |
| 05 | `05-string-view.md` | **`std::string_view`** — zero-copy views, dangling risks, kab use karein |
| 06 | `06-string-conversions.md` | `stoi`/`stod`, `to_string`, **`from_chars`/`to_chars`** (fast, no-alloc) |
| 07 | `07-string-performance.md` | Allocation cost, `reserve()`, copies avoid karna, **HFT: parsing without allocation** |
| 08 | `08-unicode-and-encoding.md` | UTF-8, `.size()` bytes deta hai, `char8_t`, multi-byte characters |
| 09 | `09-string-bugs.md` | Dangling `string_view`, iterator invalidation, `c_str()` lifetime, `+` chains |
| 10 | `10-exercises.md` | Practice + parsing problems |

## Examples

| File | Kya |
|---|---|
| `examples/01_c_strings.cpp` | C-strings aur unke dangers |
| `examples/02_std_string.cpp` | `std::string` ka poora API |
| `examples/03_sso_demo.cpp` | SSO — kahan se heap allocation shuru hoti hai |
| `examples/04_string_view.cpp` | `string_view` + dangling trap |
| `examples/05_fast_parsing.cpp` | `from_chars` vs `stoi` benchmark |
| `examples/06_csv_parser.cpp` | Ek chhota CSV parser |

## Time
1 hafta

## Status
✅ **COMPLETE** (Batch 4). 9 lessons + exercises + 6 compile-verified examples.
Highlights: C-strings → `std::string` → `std::string_view`, **SSO threshold
naapa (15 chars, `03_sso_demo.cpp` `-O0` pe)**, `from_chars` vs `stoi`
**naapa ~8x** (`05_fast_parsing.cpp`, GCC 15.1 aur 16.2 dono pe), zero-copy CSV parser, UTF-8 bytes ≠ chars.

**Hinglish pass (gap-fix part 2):** saare lessons + READMEs + comments Hinglish mein; saath mein GCC 16.2 pe
chala ke accuracy fixes — `-Wdangling` GCC ka flag hi nahi (aur GCC dangling `string_view` pe warn nahi karta),
`char* = "lit"` pe GCC sirf warning deta hai, `string_view` Windows x64 pe pointer se jaata hai, reallocation
counts theek kiye (1e6 appends = 17, 100k = 13), `std::string(nullptr)` pe libstdc++ exception phenkta hai.

## Next
→ [`../11-STRUCTS/00-README.md`](../11-STRUCTS/00-README.md)
