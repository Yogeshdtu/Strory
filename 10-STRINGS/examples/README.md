# Examples — Folder 10 (Strings)

| File | Lesson | Kya dikhata hai |
|---|---|---|
| `01_c_strings.cpp` | 01 | `char[]` + `'\0'`, `strlen` scan, `strcpy` overflow risk, `strcmp` vs `==`, missing terminator |
| `02_std_string.cpp` | 02, 03 | Construction, size/capacity, `.at()` throw, modify (grows), iterate, `c_str()` |
| `03_sso_demo.cpp` | 04, 07 | ⚠️ **run at `-O0`** — `operator new` counter finds the **SSO threshold (15 on libstdc++)** + geometric capacity growth |
| `04_string_view.cpp` | 05, 09 | One function all sources, zero-copy slice/split, 5 dangling patterns |
| `05_fast_parsing.cpp` | 06 | `from_chars` vs `stoi` vs `atoi` vs `stringstream` — **measured ~8x / ~30x** + strictness demo |
| `06_csv_parser.cpp` | 03, 05, 07 | Small **zero-copy** CSV → trade-record parser (`string_view` fields, `from_chars`, guard clauses) |

## Compile / run

```bash
./build.ps1 10-STRINGS/examples/02_std_string.cpp        # Windows
make FILE=10-STRINGS/examples/02_std_string.cpp          # Linux/Mac/Git-Bash
```

**`03_sso_demo.cpp` — `-O0` ZAROORI:**
```bash
g++ -std=c++20 -O0 -g 10-STRINGS/examples/03_sso_demo.cpp -o sso && ./sso
```
`-O2` pe GCC `std::string` ki short-lived local allocation ko **elide** kar deta hai
(C++14 new-expression elision, folder 33) → demo "0 allocations" dikhata hai. SSO
threshold dekhne ke liye `-O0`.

**`05_fast_parsing.cpp` — `-O2`:**
```bash
./build.ps1 fast 10-STRINGS/examples/05_fast_parsing.cpp
```

## Measured results (aapke machine pe alag)

GCC 15.1.0, x86-64:

| Benchmark | Result |
|---|---|
| `03` — SSO threshold (libstdc++) | **15 chars** (len ≤15 → 0 allocs; 16 → heap) |
| `03` — `sizeof(std::string)` | 32 bytes |
| `03` — capacity growth | geometric **×2** (15 → 30 → 60 → 120 → …) |
| `05` — `from_chars` (100k strings) | ~0.86 ms/pass (1.0x baseline) |
| `05` — `atoi` | ~2.9 ms (~3.4x) |
| `05` — `stoi` | ~7.1 ms (~8.3x — exceptions + `std::string` + locale) |
| `05` — `stringstream` | ~26 ms (~30x — allocations + locale) |

## Notes

- All 6 examples compile clean (no `broken_on_purpose`, no intentional warnings).
- `04_string_view.cpp` — the dangling patterns are shown as **commented** code +
  explanatory prints (running them would be UB); pattern (c) is executed but the
  dangling read is left commented.
- Sanitizers: MinGW-w64 has no ASan/UBSan (see folder 09 examples README).
  `04_string_view` dangling → observe on Linux/Clang with `-fsanitize=address`.
