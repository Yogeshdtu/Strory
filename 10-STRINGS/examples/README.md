# Examples — Folder 10 (Strings)

| File | Lesson | Kya dikhata hai |
|---|---|---|
| `01_c_strings.cpp` | 01 | `char[]` + `'\0'`, `strlen` scan, `strcpy` overflow ka khatra, `strcmp` vs `==`, gayab terminator |
| `02_std_string.cpp` | 02, 03 | Construction, size/capacity, `.at()` throw, badalna (apne aap badhta hai), iterate, `c_str()` |
| `03_sso_demo.cpp` | 04, 07 | ⚠️ **`-O0` pe chalao** — `operator new` counter **SSO threshold** dhoondhta hai (libstdc++ pe 15) + geometric capacity growth |
| `04_string_view.cpp` | 05, 09 | Ek function saare sources pe, zero-copy slice/split, **3 dangling patterns + null-termination trap** |
| `05_fast_parsing.cpp` | 06 | `from_chars` vs `stoi` vs `atoi` vs `stringstream` — **naapa ~8x / ~30x** + strictness demo |
| `06_csv_parser.cpp` | 03, 05, 07 | Chhota **zero-copy** CSV → trade-record parser (`string_view` fields, `from_chars`, guard clauses) |

## Compile / run

```bash
./build.ps1 10-STRINGS/examples/02_std_string.cpp        # Windows
make FILE=10-STRINGS/examples/02_std_string.cpp          # Linux/Mac/Git-Bash
```

**`03_sso_demo.cpp` — `-O0` ZAROORI:**
```bash
g++ -std=c++20 -O0 -g 10-STRINGS/examples/03_sso_demo.cpp -o sso && ./sso
```
`-O2` pe GCC `std::string` ki chhoti zindagi wali local allocation ko **elide** kar deta hai (C++14
new-expression elision, folder 33) → demo "0 allocations" dikhata hai (GCC 16.2 pe bhi dekha). SSO
threshold dekhne ke liye `-O0`.

**`05_fast_parsing.cpp` — `-O2`:**
```bash
./build.ps1 fast 10-STRINGS/examples/05_fast_parsing.cpp
```

## Naape hue results (aapki machine pe alag honge)

x86-64, Windows (MinGW):

| Benchmark | GCC 15.1 | GCC 16.2 (3 runs) |
|---|---|---|
| `03` — SSO threshold (libstdc++) | **15 chars** (len ≤15 → 0 allocs; 16 → heap) | wahi — 15 |
| `03` — `sizeof(std::string)` | 32 bytes | 32 bytes |
| `03` — capacity growth | geometric **×2** (15 → 30 → 60 → 120 → …) | wahi ×2 |
| `05` — `from_chars` (100k strings) | ~0.86 ms/pass (1.0x) | 1.05–1.07 ms (1.0x) |
| `05` — `atoi` | ~2.9 ms (~3.4x) | 3.28–4.42 ms (3.1–4.1x) |
| `05` — `stoi` | ~7.1 ms (~8.3x — exceptions + `std::string` + locale) | 8.37–8.56 ms (7.9–8.1x) |
| `05` — `stringstream` | ~26 ms (~30x — allocations + locale) | 30.1–37.4 ms (28–35x) |

Ratios dono compilers pe lagbhag same rahe — absolute ms machine ki haalat ke saath hilte hain.

## Notes

- Saare 6 examples clean compile hote hain (koi `broken_on_purpose` nahi, koi jaan-boojh kar warning nahi).
- `04_string_view.cpp` — dangling patterns **comment** kiye hue code + samjhane wale prints ki tarah hain
  (chalane pe UB hota); pattern (c) chalta hai par dangling read comment kiya hua hai. GCC 16.2
  `-Wall -Wextra` in patterns pe koi warning nahi deta (lesson 09).
- Sanitizers: MinGW-w64 mein ASan/UBSan nahi (folder 09 examples README dekho). `04_string_view`
  ka dangling Linux/Clang pe `-fsanitize=address` se dekho.
