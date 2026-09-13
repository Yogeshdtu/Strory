# Examples — Folder 12 (Pointers)

| File | Lesson | Kya dikhata hai |
|---|---|---|
| `01_basic_pointers.cpp` | 01, 02, 03 | `&`, `*`, pointer se value badalna, address ek number, `sizeof` (saare 8), re-pointing |
| `02_pointer_arithmetic.cpp` | 05, 06 | `p+n` `sizeof(*p)` se scale hota hai, `arr[i]==*(arr+i)`, pointer walk, subtraction, `char*` byte view |
| `03_const_pointers.cpp` | 07 | `const int*` vs `int* const` vs `const int* const`, declarations padhna, API const-correctness |
| `04_pointers_structs.cpp` | 08 | `->` vs `(*p).`, `Order*`/`const Order*` params, nullptr guards, linked-list walk |
| `05_function_pointers.cpp` | 11 | Declare, call, callbacks, dispatch table (`std::array` of fp), capture-less lambda → fp |
| `06_dangling_pointer.cpp` | 12, 13 | ⚠️ **Jaan-boojh kar dangling/UAF bugs** (3 intentional warnings) — use-after-return (`return &x` + out-parameter), use-after-free (`write` arg se free memory pe likhna), vector realloc, scope end + fixes |
| `07_pointer_diagrams.cpp` | 01, 03, 09 | Har step pe memory-state table (`p = &a`, `*p = ...`, re-point, `**ppx`) |
| `08_swap_via_pointers.cpp` | — (folder 13 ka pul) | by-value (toota) vs by-pointer vs by-reference vs `std::swap` |

## Compile / run

```bash
./build.ps1 12-POINTERS/examples/01_basic_pointers.cpp        # Windows
make FILE=12-POINTERS/examples/01_basic_pointers.cpp          # Linux/Mac/Git-Bash
```

## Jaan-boojh kar warnings

- **`06_dangling_pointer.cpp`** — **jaan-boojh kar** 3 warnings deta hai (GCC 16.2 `-Wall`):
  `-Wreturn-local-addr` (local ka `return &x`), `-Wdangling-pointer` (local ka address out-parameter mein
  store), aur `-Wdangling-pointer` (block khatam hone ke baad block-scoped local ka pointer use). Wahi lesson
  hai — compiler kuch dangling cases khud pakad leta hai. Baaki 7 files clean compile hoti hain.

## GCC 16.2 pe `06_dangling_pointer.cpp` asal mein kya karta hai (naapa)

| Run | Kya hua |
|---|---|
| `./dp` (`-O0`) | Saare BUGs chalte hain, exit 0. BUG 1: `p1 = 0` — GCC ne `return &x` ko `return nullptr` bana diya (assembly `mov eax, 0`). BUG 1b: `*p1b = 32758` (42 nahi). |
| `./dp write` (`-O0`) | BUG 2 free memory mein likhta hai → crash **BUG 3 ke vector allocation mein** (Segmentation fault, exit 139, 5/5 baar). Nuksaan der se, door kahin. |
| `./dp write` (`-O2`) | Koi crash nahi — GCC ne `delete` ke baad wala dead store hata diya. |

**Purana bug theek kiya:** is file ka pehla version `*danglingLocal()` padhta tha. GCC 16.2 pe woh nullptr deref
tha → program BUG 1 pe hi crash (exit 139) aur BUG 2–4 kabhi chalte nahi the. `checkall` sirf compile karta
hai, isliye "OK" dikhta raha.

## Sanitizers

⚠️ **MinGW-w64 (`C:\mingw64`) mein `libasan`/`libubsan` NAHI** — ASan/UBSan link fail karte hain.
`06_dangling_pointer.cpp` ka asli diagnosis (heap-use-after-free / stack-use-after-return, exact line ke saath)
**Linux / macOS / Clang / WSL** pe:

```bash
g++ -std=c++20 -fsanitize=address,undefined -g 06_dangling_pointer.cpp -o dp && ./dp write
```

`./build.ps1 san <file>` MinGW pe `-D_GLIBCXX_ASSERTIONS + -fstack-protector-all` pe fall back karta hai (STL
bounds + canaries; raw dangling nahi pakadta).

## Notes

- Koi `broken_on_purpose` file nahi. `06_dangling_pointer.cpp` runtime-UB demo hai (folder 09 ke
  `06_oob_asan.cpp` jaisa), compile failure nahi.
- `05_function_pointers.cpp` capture-less lambda → `int(*)(int,int)` conversion use karta hai (valid). Capture
  karne wala lambda convert NAHI hota — lesson 11 dekho.
