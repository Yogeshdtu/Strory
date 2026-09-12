# Examples — Folder 21 (Templates & Generic Programming)

| File | Lesson(s) | Kya dikhata hai |
|---|---|---|
| `01_function_templates.cpp` | 01, 02 | argument deduction (no implicit conversion), explicit `<T>`, trailing/deduced return, non-type param `sum<T,N>`, overload resolution (exact non-template beats template) |
| `02_class_templates.cpp` | 03, 04, 05 | `FixedStack<T, Cap>` (NTTP capacity → inline array), member function template `push_range`, CTAD (`std::pair p{...}`), full + **partial** specialization (`TypeName<T>` / `TypeName<T*>`) |
| `03_variadic.cpp` | 06 | parameter packs, recursion over a pack, **fold expressions** (`+`, `&&`, comma), `sizeof...`, perfect-forwarding a pack into a constructor (`makeUnique`) |
| `04_if_constexpr.cpp` | 07, 12, 13 | `if constexpr` per type category (`*v` compiles only for pointer `T`), trivial→`memcpy` vs custom→`.bytes()`, compile-time `factorial` (recursion terminated by `if constexpr`) |
| `05_sfinae.cpp` | 09, 08 | `enable_if` overload set, `enable_if` on the return type, the **detection idiom** (`has_size` via `void_t` + `declval`), expression SFINAE (`tryAdd` → `...` fallback) |
| `06_concepts.cpp` | 10 | `Numeric`/`Addable`/`Sized` concepts, four ways to constrain, **subsumption** (more-constrained overload wins), constrained algorithm, clean errors |
| `07_crtp_policy.cpp` | 11, 16 | CRTP static polymorphism (`Shape<Circle>`, `sizeof(Circle)` = 8, no vptr), **policy-based** `Buffer<T, Cap, FullPolicy, StatsPolicy>`, **measured** CRTP vs virtual (heterogeneous boundary) |
| `08_compile_time_dispatch.cpp` | 16, 11, 01 | **measured** virtual vs template vs `std::variant`+`std::visit` dispatch cost; ASM note (`call` in the virtual loop, none in the template loop) |

## Compile / run

```bash
./build.ps1 21-TEMPLATES/examples/01_function_templates.cpp     # Windows (debug -O0 + heavy warnings)
make FILE=21-TEMPLATES/examples/01_function_templates.cpp       # Linux/Mac/Git-Bash
```

Benchmarks at **`-O2`**:

```bash
./build.ps1 fast 21-TEMPLATES/examples/07_crtp_policy.cpp
./build.ps1 fast 21-TEMPLATES/examples/08_compile_time_dispatch.cpp
./build.ps1 asm  21-TEMPLATES/examples/08_compile_time_dispatch.cpp
```

## Measured (GCC 15.1.0, `-O2`, x86-64, this box) — sample runs

### `07_crtp_policy.cpp`
```
sizeof(Circle) = 8   (no vptr -- CRTP base is empty)
CRTP vs virtual dispatch:
  virtual (heterogeneous vector<unique_ptr<Base>>) : ~2.43 ns/call
  CRTP                                             : ~0.56 ns/call
  note: with the concrete type visible, GCC devirtualizes -> virtual == CRTP.
        the gap appears only at a real Base* boundary.
```

### `08_compile_time_dispatch.cpp` (200,000,000 calls)
```
virtual (vtable indirect call)      : ~2.51 ns/call    (not inlined, blocks vectorization)
template (concrete type, inlined)   : ~1.14 ns/call
std::variant + std::visit           : ~1.13 ns/call    (jump table, one predictable indirect call)
```

The shape: compile-time dispatch (template / CRTP / `variant`+`visit`) inlines
and costs ~0.5–1.1 ns; `virtual` through a real polymorphic boundary is ~2.5 ns
and prevents the compiler from inlining or vectorizing the surrounding loop.

## Notes

- **No `broken_on_purpose` file.** Several examples have commented-out lines
  (`// myMax(3, 2.5); // ERROR ...`) that demonstrate what *doesn't* compile.
- `07_crtp_policy.cpp` deliberately routes the virtual benchmark through a
  **heterogeneous** `std::vector<std::unique_ptr<VBase>>` so the compiler can't
  devirtualize — otherwise virtual == CRTP and there's nothing to show. The
  example prints that caveat.
- `06_concepts.cpp` needs `-std=c++20` (the repo default). `05_sfinae.cpp` shows
  the pre-C++20 way for contrast.
- All files compile clean under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion
  -Wsign-conversion -Wcast-align -Wunused -Wnull-dereference -Wdouble-promotion`
  (`./build.ps1 folder 21-TEMPLATES`).
