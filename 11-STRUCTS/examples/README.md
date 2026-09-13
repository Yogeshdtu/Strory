# Examples — Folder 11 (Structs, Unions, Enums)

| File | Lesson | Kya dikhata hai |
|---|---|---|
| `01_struct_basics.cpp` | 01, 04 | Instances, dot access, `const&` params, return by value, `vector<Point>`, `sizeof` |
| `02_padding_demo.cpp` | 05 | `sizeof`/`alignof`/`offsetof`, **Bad(24) vs Good(16)** same members, padding samet raw bytes |
| `03_struct_optimization.cpp` | 05, 07 | Member reorder → size aadha; `LevelBad(40, 45% pad)` vs `LevelGood(24, 8%)`; cache-line density |
| `04_aos_vs_soa.cpp` | 07 | AoS vs SoA — `max(bid)` scan, `[[gnu::noipa]]` kernels, **`-O2` pe ~2.3×, `-O3 -march=native` pe 8–10×** |
| `05_unions.cpp` | 08 | Shared memory, type punning (+ safe `memcpy` tareeqa), tagged union |
| `06_variant.cpp` | 09 | `variant<Quote,Trade,Reject>`, `index()`, `get`/`get_if`, `std::visit`, event stream |
| `07_enums.cpp` | 10 | Plain vs `enum class`, underlying type, `switch` exhaustiveness, flags |
| `08_market_data_struct.cpp` | 06, 13 | **Packed 32-byte `QuoteMsg`**, `static_assert` layout locks, `memcpy` decode, byteswap |

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
# MinGW pe -mms-bitfields default hai -> -Wpadded sirf tail padding dikhata hai.
# Beech ki padding dekhne ke liye (sirf audit, production build nahi):
g++ -std=c++20 -Wpadded -mno-ms-bitfields 11-STRUCTS/examples/02_padding_demo.cpp -o /dev/null
```

## Notes

- Saare 8 examples clean compile hote hain (koi `broken_on_purpose` nahi, jaan-boojh ke warnings nahi —
  `07_enums.cpp` `-Wenum-compare` ka khatra text mein batata hai, trigger nahi karta).
- `04_aos_vs_soa.cpp` — **benchmark fix (GCC 16.2):** purana version reps ka loop seedha `main()` mein
  chalata tha aur sirf aakhri rep ka result use karta tha; GCC 16.2 `-O2` ne pehle 29 reps dead code maan ke
  hata diye (30 reps ~8 ms mein — 3.8 GB padhna physically namumkin). Kernels ab `[[gnu::noipa]]` functions
  mein hain. Lesson 07 mein poori kahani.
- `08_market_data_struct.cpp` — `-std=c++20` build ke liye `__builtin_bswap*` use karta hai (`std::byteswap` C++23
  hai). C++23 wala asli code `std::byteswap` use karega. Yeh `#pragma pack` use karta hai — dhyaan rahe, GCC
  `-Waddress-of-packed-member` sirf `[[gnu::packed]]` pe deta hai (lesson 06); file `&member` leti hi nahi, `memcpy`
  karti hai.
- `05_unions.cpp` — inactive-member reads technically UB hain (GCC isse allow karta hai); file sahi `memcpy` /
  `std::bit_cast` tareeqa bhi dikhati hai.

## Naape hue results (aapki machine pe alag)

GCC 16.2.0 (MinGW-w64), Windows x64, AMD Zen 2:

| Benchmark | Result |
|---|---|
| `02` — `Bad` vs `Good` (same members, reorder) | 24 B → 16 B (**33% chhota**) |
| `03` — `LevelBad` vs `LevelGood` | 40 B (45% padding) → 24 B (8%) — **1.67x ghana** |
| `04` — AoS vs SoA, `max(bid)`, `-O2` | AoS 213–219 ms, SoA 92–94 ms → **~2.3×** (dono loops scalar) |
| `04` — `-O2 -fvect-cost-model=cheap` | AoS ~214 ms, SoA ~30 ms → **~7×** (SoA vectorize hua) |
| `04` — `-O3 -march=native` | AoS 209–252 ms, SoA 25–26 ms → **8–10×** (AoS bhi vectorize, par bandwidth-bound) |
| (folder 09 `07`) AoS vs SoA, single-field RMW, `-O2` | SoA **5.4–5.8×** tez |
| `08` — packed `QuoteMsg` | exactly 32 bytes, `alignof` 1, saare `static_assert` pass |
