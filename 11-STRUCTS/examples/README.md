# Examples — Folder 11 (Structs, Unions, Enums)

| File | Lesson | Kya dikhata hai |
|---|---|---|
| `01_struct_basics.cpp` | 01, 04 | Instances, dot access, `const&` params, return by value, `vector<Point>`, `sizeof` |
| `02_padding_demo.cpp` | 05 | `sizeof`/`alignof`/`offsetof`, **Bad(24) vs Good(16)** same members, raw bytes with padding |
| `03_struct_optimization.cpp` | 05, 07 | Member reorder → size half; `LevelBad(40, 45% pad)` vs `LevelGood(24, 8%)`; cache-line density |
| `04_aos_vs_soa.cpp` | 07 | AoS vs SoA — `max(bid)` scan, **measured ~1.6x** (see folder 09's `07_aos_vs_soa.cpp` for the ~4x RMW case) |
| `05_unions.cpp` | 08 | Shared memory, type punning (+ safe `memcpy` way), tagged union |
| `06_variant.cpp` | 09 | `variant<Quote,Trade,Reject>`, `index()`, `get`/`get_if`, `std::visit`, event stream |
| `07_enums.cpp` | 10 | Plain vs `enum class`, underlying type, `switch` exhaustiveness, flags |
| `08_market_data_struct.cpp` | 06, 08 | **Packed 32-byte `QuoteMsg`** with `static_assert` layout locks, `memcpy` decode, byteswap |

## Compile / run

```bash
./build.ps1 11-STRUCTS/examples/01_struct_basics.cpp       # Windows
make FILE=11-STRUCTS/examples/01_struct_basics.cpp         # Linux/Mac/Git-Bash
```

**Benchmark (`04`) — `-O2`:**
```bash
./build.ps1 fast 11-STRUCTS/examples/04_aos_vs_soa.cpp
g++ -std=c++20 -O3 -march=native 11-STRUCTS/examples/04_aos_vs_soa.cpp -o aos3 && ./aos3
```

**Padding audit (`02`):**
```bash
g++ -std=c++20 -Wpadded 11-STRUCTS/examples/02_padding_demo.cpp -o /dev/null   # see where it pads
```

## Notes

- All 8 examples compile clean (no `broken_on_purpose`, no intentional warnings —
  `07_enums.cpp` describes the `-Wenum-compare` risk in text rather than
  triggering it).
- `08_market_data_struct.cpp` — uses `__builtin_bswap*` (not `std::byteswap`,
  which is C++23) for the `-std=c++20` build. Real code on C++23 uses
  `std::byteswap`.
- `05_unions.cpp` — the inactive-member reads are technically UB (documented GCC/
  Clang extension); the file also shows the correct `memcpy` / `std::bit_cast`
  way.

## Measured results (aapke machine pe alag)

GCC 15.1.0, x86-64:

| Benchmark | Result |
|---|---|
| `02` — `Bad` vs `Good` (same members, reordered) | 24 B → 16 B (**33% smaller**) |
| `03` — `LevelBad` vs `LevelGood` | 40 B (45% padding) → 24 B (8%) — **1.67x denser** |
| `04` — AoS vs SoA, `max(bid)` scan, `-O2` | SoA **~1.6x faster** (~441 vs ~283 ms) |
| (folder 09 `07`) AoS vs SoA, single-field RMW | SoA **~4x faster** |
| `08` — packed `QuoteMsg` | exactly 32 bytes, `alignof` 1, all `static_assert`s pass |
