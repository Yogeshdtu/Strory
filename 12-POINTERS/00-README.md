# 12 — POINTERS (PHASE 5)

## Prerequisites
`11-STRUCTS`, `11-memory-basics.md` (folder 01)

## Yeh folder kyun
**Yahan se asli C++ shuru hoti hai.**

Pointers wahi cheez hai jo C++ ko systems language banati hai — aur jo beginners ko
sabse zyada darati hai. Hum ise diagrams ke saath, dheere-dheere karenge.

Har prerequisite (variables → addresses → memory) pehle ho chuka hai. Ab aap taiyaar ho.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-what-is-a-pointer.md` | Pointer kya hai — 'address rakhne wala variable', memory diagram |
| 02 | `02-address-of-operator.md` | `&` operator, address kya hota hai, printing addresses |
| 03 | `03-dereferencing.md` | `*` operator, 'value at address', declaration vs dereference mein `*` ka fark |
| 04 | `04-nullptr.md` | `nullptr`, `NULL` aur `0` kyun nahi, null checks, null dereference = crash |
| 05 | `05-pointer-arithmetic.md` | `p+1` type ke size se badhta hai, array traversal, valid ranges |
| 06 | `06-pointers-and-arrays.md` | Array decay dobara, `arr[i]` == `*(arr+i)`, pointer vs array ka fark |
| 07 | `07-const-and-pointers.md` | `const int*` vs `int* const` vs `const int* const`, padhne ka rule |
| 08 | `08-pointers-to-structs.md` | `->` operator, `(*p).x` vs `p->x` |
| 09 | `09-double-pointers.md` | `int**`, kab chahiye, 2D arrays ke saath |
| 10 | `10-void-pointers.md` | `void*`, type erasure, C APIs, casting rules |
| 11 | `11-function-pointers.md` | Function pointers, syntax, callbacks, **virtual dispatch preview** |
| 12 | `12-dangling-pointers.md` | Dangling pointers, use-after-free, ASan se pakadna |
| 13 | `13-pointer-bugs-catalog.md` | Uninitialized, dangling, double-free, leaks, arithmetic errors — poora catalog |
| 14 | `14-exercises.md` | Practice + **bahut saare memory diagrams** |

## Examples

| File | Kya |
|---|---|
| `examples/01_basic_pointers.cpp` | `&`, `*`, addresses — step by step |
| `examples/02_pointer_arithmetic.cpp` | Arithmetic aur array traversal |
| `examples/03_const_pointers.cpp` | Teenon const variations |
| `examples/04_pointers_structs.cpp` | `->` operator |
| `examples/05_function_pointers.cpp` | Callbacks |
| `examples/06_dangling_pointer.cpp` | ⚠️ Dangling — ASan se pakdo |
| `examples/07_pointer_diagrams.cpp` | Har step pe memory state print karo |
| `examples/08_swap_via_pointers.cpp` | Classic pass-by-pointer |

## Time
2 hafte

## Status
✅ **COMPLETE** (Batch 5 — PHASE 5 shuru). 13 lessons + exercises + 8
compile-verified examples. `&`/`*`/`nullptr`, pointer arithmetic, arrays↔pointers,
`const` combinations, `->`, `int**`, `void*`, function pointers (→ virtual
dispatch), dangling/UAF, aur poora bug catalog. Memory diagrams ke saath.

## Next
→ [`../13-REFERENCES/00-README.md`](../13-REFERENCES/00-README.md)
