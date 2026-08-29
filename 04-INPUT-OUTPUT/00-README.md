# 04 — INPUT / OUTPUT (PHASE 2)

## Prerequisites
`03-VARIABLES-DATA-TYPES` (poora)

## Yeh folder kyun
Ab tak aap sirf **fixed** text print kar rahe the. Ab program user se baat karega —
input lega, formatted output dega. Iske bina koi useful program nahi ban sakta.

Aur `std::endl` vs `\n` ka jo fark aapne folder 02 mein dekha tha, uska poora
mechanism (buffering, flushing, syscalls) yahan samjhenge — jo HFT logging mein
critical hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-cout-deep-dive.md` | `std::cout` andar se — `ostream` object, `operator<<` overloads, chaining ka mechanism |
| 02 | `02-cin-and-input.md` | `std::cin`, `>>` operator, whitespace behaviour, type mismatch pe kya hota hai |
| 03 | `03-buffering-and-flushing.md` | Buffer kya hai, `\n` vs `endl` vs `flush`, syscall cost, **HFT logging relevance** |
| 04 | `04-cerr-and-clog.md` | `cerr` (unbuffered), `clog`, stream redirection (`>`, `2>`, `2>&1`) |
| 05 | `05-getline-and-strings.md` | `std::getline`, `>>` ke saath mix karne ka classic bug, leftover newline |
| 06 | `06-input-validation.md` | Stream states: `good`/`fail`/`eof`/`bad`, `clear()`, `ignore()`, robust input loop |
| 07 | `07-manipulators.md` | `<iomanip>`: `setw`, `setprecision`, `fixed`, `scientific`, `setfill`, `hex`/`oct`/`dec`, `boolalpha` |
| 08 | `08-std-format.md` | **C++20 `std::format`** — type-safe formatting, format specs, `std::print` (C++23) |
| 09 | `09-file-streams.md` | `ifstream`/`ofstream`/`fstream`, modes, RAII se file band hona, error handling |
| 10 | `10-string-streams.md` | `stringstream`, `istringstream`, `ostringstream` — parsing aur building |
| 11 | `11-printf-family.md` | `printf`/`scanf` — kab dikhte hain, kyun type-unsafe hain, format string vulnerabilities |
| 12 | `12-io-performance.md` | `sync_with_stdio(false)`, `cin.tie(nullptr)`, buffered vs unbuffered, **HFT: async logging kyun** |
| 13 | `13-exercises.md` | Practice + self-assessment |

## Examples

| File | Kya |
|---|---|
| `examples/01_cout_basics.cpp` | Chaining, types, precedence |
| `examples/02_cin_input.cpp` | Input lena, validation |
| `examples/03_endl_benchmark.cpp` | `\n` vs `endl` ka timing — real numbers |
| `examples/04_manipulators.cpp` | Formatting ka poora demo |
| `examples/05_format_cpp20.cpp` | `std::format` examples |
| `examples/06_file_io.cpp` | File read/write with RAII |
| `examples/07_stringstream_parse.cpp` | CSV line parse karna |
| `examples/08_robust_input.cpp` | Bulletproof input loop |

## Time
4–6 din

## Status
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega. Tab tak pichle folders ke exercises karo,
kyunki yeh sab unhi pe khada hoga.

## Next
→ [`../05-OPERATORS/00-README.md`](../05-OPERATORS/00-README.md)
