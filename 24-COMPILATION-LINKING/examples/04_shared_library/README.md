# 04 — Shared / dynamic library (`.so` / `.dll`)

**Lesson:** 09 (static vs dynamic linking)

## Files
```
greet.hpp    interface + cross-platform export macro (GREET_API)
greet.cxx    implementation (hello + a stateful bump())
main.cxx     app jo library ke against link hota
```

## Build
```bash
./build.sh          # Linux -> libgreet.so ; macOS -> .dylib ; Windows/MinGW -> greet.dll
.\build.ps1         # Windows PowerShell
```

## Static (`.a`) se farq

| | Static `.a` (ex 03) | Shared `.so` / `.dll` (yahan) |
|---|---|---|
| Library code kahan | binary ke andar (copy) | **alag file**, run time pe load |
| Link time pe | code copy | sirf **reference** (symbol name) note hota |
| Run time pe | kuch nahi | **loader** library ko dhoondta, map karta, symbols bind |
| Ek fix, N apps | har app rebuild | library replace, apps waise hi |
| Disk / RAM | har app ki copy | ek copy, **saare processes share** (read-only pages) |
| External call | direct `call` | **PLT/GOT** indirection (neeche) |
| Deploy | ek file | app **+ library**, aur path resolve hona chahiye |

## Run time pe library kaise milti hai

| Platform | Search order (roughly) |
|---|---|
| **Linux** | `DT_RPATH` (deprecated) → `LD_LIBRARY_PATH` → `DT_RUNPATH` (e.g. `$ORIGIN`) → `/etc/ld.so.cache` → `/lib`, `/usr/lib` |
| **Windows** | app folder → system dirs → `PATH` |
| **macOS** | `@rpath` / `@loader_path` / `@executable_path`, `DYLD_LIBRARY_PATH` |

`build.sh` Linux pe `-Wl,-rpath,'$ORIGIN'` embed karta hai (= "library binary ke
paas hi hai"). Windows pe `greet.dll` bas same folder mein hai.

- `ldd app` (Linux) / `objdump -p app.exe` (Windows) → dependencies list.
- Library hata ke chalao → Linux: `error while loading shared libraries`;
  Windows: `The code execution cannot proceed because greet.dll was not found`.

## PLT / GOT — dynamic call ki cost

App ko link time pe `greet::hello` ka **address nahi pata** (woh `.so` mein hai,
kahan load hoga abhi decide nahi). Toh:

- **GOT** (Global Offset Table) — ek array of pointers, run time pe loader (ya
  pehli call pe, lazy binding) fill karta hai.
- **PLT** (Procedure Linkage Table) — har external function ke liye ek chhota stub
  jo GOT entry ke through jump karta hai.

`call greet::hello` → actually `call hello@plt` → `jmp *hello@got` → real function.
Ek **extra indirect jump + a load** per cross-library call, aur woh GOT pointer
cache/branch-predictor pe pressure daalta hai. `-fno-plt` isse thoda kam karta
(direct GOT-indirect call), par indirection rehti hai.

Position-independent code (`-fPIC`, `.so` ke liye zaroori) bhi register pressure
aur `%rip`-relative addressing add karta hai vs a fixed-address static binary.

## > HFT relevance

Hot path pe **static linking** — cross-library PLT/GOT indirection nahi chahiye,
aur LTO poore binary ke across inline/devirtualize kar sake. Shared libraries
theek hain: plugins jo runtime pe swap karne hain (strategy `.so`), ya bahut bade
shared frameworks jo memory sharing se fayda dein. Core engine: `-static` ya
static-linked, `-fno-plt` at minimum agar kuch shared rehna hi ho.

Stateful `bump()` note: library ke andar ka `static` state **per-process** hai
(shared code, private data pages — copy-on-write). Do processes same `.so` use
karein to unke `bump()` counters alag.

## Try karo

- Library file rename/hata ke `app` chalao → loader error.
- Linux: `LD_LIBRARY_PATH=/some/other ./app` se ek alag `libgreet.so` point karo
  (agar banao) — same app, alag behaviour (yeh dynamic linking ka fayda *aur*
  khatra).
- `nm -D libgreet.so` (Linux) / `objdump -p greet.dll` — exported (dynamic)
  symbols.
- `readelf -d app` (Linux) → `NEEDED libgreet.so`, `RUNPATH $ORIGIN`.
- Ek naya `hello()` body daal ke sirf **library rebuild** karo (app nahi) →
  `./app` naya output — app ka binary bilkul nahi chhua.
