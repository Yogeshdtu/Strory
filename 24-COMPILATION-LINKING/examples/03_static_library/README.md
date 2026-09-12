# 03 — Static library (`.a`)

**Lesson:** 09 (static vs dynamic linking)

## Files
```
calc.hpp                interface
add.cxx  mul.cxx        library ke implementations
unused.cxx              ek function jo main use NAHI karta (member-selection demo)
main.cxx                app
```

## Build
```bash
./build.sh          # Linux / macOS / Git Bash  (archive + link + inspection)
.\build.ps1         # Windows PowerShell
```

## `.a` kya hai

Ek **`.a` (archive)** = `.o` files ka ek zip-jaisa bundle + ek symbol index.
`ar rcs libcalc.a add.o mul.o unused.o` banata hai. Yeh **library** nahi "load"
hoti — link time pe iske members me se **sirf zaroori `.o`** aapke binary mein
**copy** ho jaate hain.

```
add.cxx ─┐
mul.cxx ─┼─► g++ -c ─► add.o mul.o unused.o ─► ar rcs ─► libcalc.a
unused ──┘                                                   │
                                     main.o ──► g++ main.o -L. -lcalc ─► app (add/mul/dot ANDAR)
```

## Member selection (observe karo)

`main` sirf `add`, `mul`, `dot`, `build_id` use karta hai. `nm -C app` mein:

```
T calc::add(...)      <- present
T calc::mul(...)      <- present  (dot ne ise call kiya)
T calc::dot(...)      <- present
T calc::build_id()    <- present
                      <- calc::huge_unused ABSENT
```

Linker ne `unused.o` ko archive se **kheencha hi nahi** — kyunki koi bhi
translation unit uske symbol ko reference nahi karta. (Yeh archive-level member
granularity hai. Ek `.o` ke ANDAR ke unused functions ke liye
`-ffunction-sections -Wl,--gc-sections` chahiye.)

## Static linking ke properties

| | Static (`.a`) |
|---|---|
| Library code kahan | **binary ke andar** — ek self-contained file |
| Runtime dependency | koi nahi — `ldd app` mein `libcalc` nahi |
| Deploy | ek file copy karo, bas |
| Update library | poora app rebuild+redeploy |
| Disk/RAM (many apps) | har app ki apni copy (no sharing) |
| Cross-TU optimization | `-flto` se library code bhi inline ho sakta app mein |
| Symbol resolution | **link time** — resolved, fixed addresses |
| Call overhead | direct `call` (no PLT indirection) |

## > HFT relevance

Low-latency shops **static linking prefer karte hain**, aur aksar `-static` se
libc/libstdc++ bhi andar. Kyun:

- **Koi PLT/GOT indirection** — har external call ek direct `call rel32`, ek
  extra load / indirect jump nahi (dynamic ka overhead — example `04`).
- **LTO poore binary pe** — library boundaries cross karke inline, devirtualize.
- **Deterministic** — koi `LD_LIBRARY_PATH` surprise, koi "prod pe alag libc
  minor version", koi lazy-binding page fault on first call.
- **Deploy simplicity** — ek binary, `scp`, done. Immutable artifact.

Cost: bade binaries, aur ek security fix ke liye rebuild. HFT ke liye acceptable
trade.

## Try karo

- `main.cxx` mein `calc::huge_unused(5)` call karo, rebuild → `nm` mein ab woh
  bhi dikhega (ab reference ho gaya).
- `ar d libcalc.a mul.o` (delete member) phir re-link → `undefined reference to
  calc::mul` (aur `dot` bhi, jo `mul` call karta).
- `g++ main.o libcalc.a -o app` — `-L`/`-l` ke bina, archive ko seedha naam se.
- Link **order** matters: `g++ -lcalc main.o` (library pehle) → GNU ld pe
  `undefined reference` — library ko un TUs ke **baad** likho jo use karti hain.
