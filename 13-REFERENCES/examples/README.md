# Examples — Folder 13 (References)

| File | Lesson | Kya dikhata hai |
|---|---|---|
| `01_references_basics.cpp` | 01, 02 | Reference = alias (`&r == &x`), init mandatory, rebind nahi hoti, copy vs reference vs pointer |
| `02_pass_by_reference.cpp` | 03, 04 | by value vs by pointer vs by reference, output parameters, `const&` (zero copy), swap |
| `03_const_ref_performance.cpp` | 04 | **Benchmark** — `std::vector` by value vs `const&`: measured **~190x** (524 ns/call vs 2.7 ns/call) |
| `04_dangling_reference.cpp` | 05, 09 | ⚠️ **Deliberate dangling refs** (2 intentional warnings) — return-local, vector-realloc invalidation, lifetime-extension-doesn't-propagate, + fixes |
| `05_ref_vs_ptr.cpp` | 02 | Full side-by-side table: init / rebind / null / arithmetic / syntax / containers / sizeof |

## Compile / run

```bash
./build.ps1 13-REFERENCES/examples/01_references_basics.cpp        # Windows
make FILE=13-REFERENCES/examples/01_references_basics.cpp          # Linux/Mac/Git-Bash
```

Benchmark ko **`-O2`** pe chalao (warna numbers bekaar):

```bash
./build.ps1 fast 13-REFERENCES/examples/03_const_ref_performance.cpp
```

## Measured — `03_const_ref_performance.cpp` (`-O2`, GCC 15.1, x86-64)

```
vector size    : 4096 ints (16384 bytes)
calls per test : 300000

by value  (copy)  : 157.2 ms   (524.0 ns/call)
by const& (alias) :   0.81 ms  (  2.7 ns/call)

speedup (value / ref) : ~190x
```

Dono functions `__attribute__((noinline))` hain — taaki compiler copy ko "optimize
away" na kar de. Yeh us real-world case ko model karta hai jahan function doosri
translation unit mein hota hai. Har by-value call = `operator new(16384)` + memcpy
+ `free`. Function body trivial (2 elements) — to poora farq copy ka hai.

> Number reproduce hoga par exact value machine/allocator pe depend karta hai.
> Point: **bade objects `const T&` se pass karo.**

## Jaan-boojh kar warnings

- **`04_dangling_reference.cpp`** — **jaan-boojh kar** 2 warnings deta hai:
  - `-Wreturn-local-addr` — BUG 1: `int& f() { int x; return x; }`
  - `-Wdangling-reference` (GCC 13+) — BUG 3: `const int& r = passThrough(42);`

  BUG 1 ko file runtime pe **call nahi karti** — is toolchain pe reference-to-dead-local
  read karna seedha SIGSEGV hai. BUG 2 (vector realloc) aur BUG 3 chalte hain:
  BUG 2 deterministically dikhata hai ki reference ka address ab `&v[0]` se alag
  hai (stale → freed memory). BUG 3 `-O0` pe `42` (luck), `-O1` pe `0` deta hai —
  dono UB.

## Sanitizers

⚠️ **MinGW-w64 (`C:\mingw64`) mein `libasan`/`libubsan` NAHI** — ASan/UBSan link
fail karte hain. `04_dangling_reference.cpp` ka asli diagnosis
(stack-use-after-return, heap-use-after-free, exact line ke saath)
**Linux / macOS / Clang / WSL** pe:

```bash
g++ -std=c++20 -fsanitize=address,undefined -g 04_dangling_reference.cpp -o dr && ./dr
```

`./build.ps1 san <file>` MinGW pe `-D_GLIBCXX_ASSERTIONS + -fstack-protector-all`
pe fall back karta hai (STL bounds + stack canaries; raw dangling nahi pakadta).

## Notes

- No `broken_on_purpose` file. `04_dangling_reference.cpp` runtime-UB demo hai
  (folder 12 ke `06_dangling_pointer.cpp` jaisa), compile-failure nahi.
- `05_ref_vs_ptr.cpp` mein `vector<int&>` commented-out hai — woh compile nahi hota
  (references assignable/default-constructible nahi, container ki requirement fail).
