# 01 — Multi-file project: header/source separation

**Lessons:** 01 (translation units), 03 (include guards), 05 (linkage)

## Files

```
mathx.hpp   mathx.cxx     gcd / lcm / is_prime + constexpr square + inline clamp + extern kVersion
stats.hpp   stats.cxx     mean / stddev / count_primes_upto  (stats.cxx -> mathx.hpp)
            main.cxx      sirf headers include karta; definitions link time pe
```

> `.cxx` extension is repo ke global `*.cpp` compile-check ko skip karne ke liye
> hai (yeh multi-TU project akele `g++ file.cpp` se link nahi hoga). Real projects
> `.cpp`/`.cc` use karte hain.

## Build

```bash
./build.sh          # Linux / macOS / Git Bash
.\build.ps1         # Windows PowerShell
```

## Kya seekhna hai

### Translation unit (TU)
Ek TU = ek `.cxx` file + uske saare `#include` (preprocessing ke baad). Compiler
**ek TU ko akele** dekhta hai, ek **object file** (`.o`) banata hai. 4 headers, 3
`.cxx` → 3 TUs → 3 `.o` → linker inhe ek `app` mein jodta hai.

### Header mein kya jata hai
| Header mein | Kyun |
|---|---|
| Function **declarations** (`int f(int);`) | "yeh exist karta hai" — har caller ko signature chahiye |
| `constexpr` function **bodies** | compile-time eval har TU mein chahiye |
| `inline` function/variable **bodies** | multiple TUs mein define ho sakta (vague linkage merge) |
| `class` / `struct` definitions, templates | har TU ko poora layout / template chahiye |
| `extern` variable **declarations** | value ek hi jagah (`.cxx`), declaration sab jagah |

Header mein **non-inline function ki body mat rakho** → jo bhi TU usse include kare
usme ek definition aa jaayega → **ODR violation** (multiple definition), link error
(example `02`).

### Separate compilation ka fayda
`stats.cxx` badla? Sirf `stats.o` rebuild karo, phir re-link. `mathx.cxx` untouched
→ `mathx.o` reuse. Bade projects mein yahi build time bachata hai (Make/CMake yeh
dependency tracking automate karte — examples `05`, `06`).

### Cross-TU symbol resolution
`stats.cxx` `mathx::is_prime` **call** karta hai par uski body nahi dekhta — compiler
`stats.o` mein ek **undefined reference** chhodta hai. Linker `mathx.o` mein us
symbol ki **definition** dhoondta hai aur jodta hai. Agar `mathx.o` link line pe na
ho → `undefined reference to mathx::is_prime`.

## Try karo

- `main.cxx` mein `mathx::is_prime` ka spelling galat karo → **compile** error
  (declaration nahi mila).
- Link line se `mathx.o` hatao (`g++ stats.o main.o -o app`) → **link** error
  (`undefined reference`).
- `mathx.hpp` mein `gcd` ki body likh do (`{ ... }`, `inline` ke bina) → `mathx.o`
  aur `stats.o`+`main.o` dono mein `gcd` → **multiple definition** link error.
- `nm mathx.o | grep gcd` → `T` (defined text symbol). `nm main.o | grep is_prime`
  → `U` (undefined — linker ko chahiye).
