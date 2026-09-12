# 25 — OBJECT MODEL & UNDEFINED BEHAVIOUR (PHASE 15)

## Prerequisites
`24-COMPILATION-LINKING`, `18-COPY-MOVE`

## Yeh folder kyun
Yeh folder **C++ ke rules ka asli sach** batata hai. Object kya hai (standard ke
hisaab se), lifetime kya hai, aur UB kahan-kahan chhupa hai.

Yeh senior-level knowledge hai — aur HFT interviews mein yahin se sabse mushkil
sawal aate hain.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-what-is-an-object.md` | Standard ki definition — storage, type, lifetime, value |
| 02 | `02-object-lifetime.md` | Lifetime start/end, storage vs lifetime, reuse |
| 03 | `03-storage-duration.md` | Automatic, static, dynamic, thread — chaaron |
| 04 | `04-initialization-order.md` | **Static initialization order fiasco**, `constinit`, function-local statics |
| 05 | `05-temporaries.md` | Temporary objects, **lifetime extension** rules, dangling traps |
| 06 | `06-trivial-standard-layout-pod.md` | **Trivial, standard-layout, POD** — kya matlab, kyun matter karta hai |
| 07 | `07-object-representation.md` | Value vs object representation, padding bytes, `memcmp` traps |
| 08 | `08-alignment-deep.md` | Alignment rules, `alignas`, over-aligned types, `std::align` |
| 09 | `09-placement-new.md` | **Placement new**, manual lifetime management, aligned storage |
| 10 | `10-strict-aliasing.md` | **Strict aliasing rule** — kya legal hai, `memcpy` idiom, `std::bit_cast` |
| 11 | `11-type-punning.md` | Type punning ke saare tareeke, kaunsa legal hai |
| 12 | `12-casts-deep.md` | Chaaron casts poora, `dynamic_cast` internals, RTTI layout |
| 13 | `13-vtable-layout.md` | vtable ka exact memory layout, multiple inheritance mein kya hota hai |
| 14 | `14-abi.md` | **ABI** — Itanium C++ ABI, ABI breaks, versioning |
| 15 | `15-undefined-behaviour-catalog.md` | **UB ka poora catalog** — 50+ cases, compiler kya assume karta hai |
| 16 | `16-compiler-assumptions.md` | UB se compiler kaise optimize karta hai — real examples |
| 17 | `17-exercises.md` | Practice + UB hunting |

## Examples

| File | Kya |
|---|---|
| `examples/01_lifetime_demo.cpp` | Lifetime start/end |
| `examples/02_static_init_fiasco/` | ⚠️ Order fiasco reproduce karo |
| `examples/03_temporaries.cpp` | Lifetime extension |
| `examples/04_type_properties.cpp` | `is_trivial`, `is_standard_layout` etc. |
| `examples/05_placement_new.cpp` | Manual construction/destruction |
| `examples/06_strict_aliasing.cpp` | ⚠️ Aliasing violation vs `bit_cast` |
| `examples/07_vtable_inspect.cpp` | vtable memory dekhna |
| `examples/08_ub_examples.cpp` | ⚠️ UB catalog — UBSan ke saath chalao |

## Time
2–3 hafte

## Status
✅ **COMPLETE (Batch 8 — PHASE 15).** 16 lessons (`01`–`16`) + `17-exercises.md`
+ 8 examples (7 `.cpp` + the two-link-order `02_static_init_fiasco/`). Sab `.cpp`
`-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wcast-align
-Wunused -Wnull-dereference -Wdouble-promotion` pe clean (`./build.ps1 folder
25-OBJECT-MODEL`).

- `02_static_init_fiasco/` — **reproduced**: two link orders → two outputs; order
  B prints `BADLOG[1/(null)]` (the `std::string` member's ctor hadn't run). The
  construct-on-first-use side is identical both ways.
- `06_strict_aliasing` (`aliasing_trap`): `-O0` → `delta` = the real change;
  **`-O2 -fstrict-aliasing` → `delta == 0`** (the load was reused — UB biting).
- `08_ub_examples`: `overflow_check(INT_MAX)` = `1` at every `-O` (GCC folds
  `x+1>x`); `0` only with `-fwrapv`.
- `04_type_properties`: the trait matrix + **`memcmp == -1` without `memset`
  first** (padding bytes).
- `07_vtable_inspect`: same type → same vtable; MI → two vptr, `Printable`
  subobject at offset 8, base-cast changes the pointer value.

**Coverage:** what an object *is* (standard's "region of storage", subobjects,
`sizeof` ≥ 1) · **object lifetime** (ctor-complete → dtor-start, storage vs
lifetime, reuse, `std::launder`, implicit-lifetime types) · four storage
durations · **static init order fiasco** + construct-on-first-use + `constinit` ·
**temporaries & lifetime extension** (the 4 non-extension / dangling cases) ·
**trivial / trivially-copyable / standard-layout / POD /
`has_unique_object_representations`** (which unlocks `memcpy` / `offsetof` /
`memcmp`) · **object vs value representation**, padding, `-Wpadded` · **alignment
deep** (`alignas`, over-aligned types, aligned `new` vs `malloc`, `std::align`) ·
**placement new** & manual lifetime management (pools, `FixedOptional`) ·
**strict aliasing** (`bit_cast` / `memcpy` / byte-pointer, the `-O2` divergence)
· **type punning** (every method, which is legal) · **the four casts deep**
(`dynamic_cast` internals: vtable → `type_info` → hierarchy walk; cost) ·
**vtable layout exact** (slots, `offset-to-top`, `type_info`, MI thunks, virtual
inheritance) · **ABI** (Itanium, the ABI-break catalog, `abidiff`, stable-API
design) · **the UB catalog** (50+ cases by category) · **how the compiler
exploits UB** (null-check removal, overflow-fold, load reuse — real examples).

## Next
→ [`../26-CONCURRENCY/00-README.md`](../26-CONCURRENCY/00-README.md)
