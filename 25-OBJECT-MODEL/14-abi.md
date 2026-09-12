# 14 — ABI: the Itanium C++ ABI, ABI breaks, versioning

## Prerequisites
- `24-COMPILATION-LINKING` file 07 (mangling, `extern "C"`, ABI intro)
- `13-vtable-layout.md`, `06-trivial-standard-layout-pod.md`

## Yeh topic abhi kyun
ABI = **binary-level contract** jo do separately-compiled pieces ko interoperate
karne deta hai. Folder 24 mein intro tha; yahan object-model angle se: **kya
change ABI todta hai**, kaise detect karein, aur stable APIs kaise design karein.
HFT: static linking iske bahut saare issues ko sidestep karta — par vendored
libraries, plugins, aur toolchain upgrades pe yeh alive hai.

---

## ABI kya-kya cover karta hai

| Element | Example variation |
|---|---|
| **Name mangling** | Itanium (`_ZN...`) vs MSVC (`?...@@`) |
| **Calling convention** | args in registers (SysV: rdi, rsi, rdx, rcx, r8, r9) vs stack (Win x64: rcx, rdx, r8, r9); who saves what; return in rax / xmm0 |
| **Fundamental type sizes** | `long` = 8 (Linux LP64) vs 4 (Windows LLP64); `long double` = 80-bit / 128-bit / 64-bit |
| **Struct/class layout** | member offsets, padding, alignment, tail-padding reuse |
| **vtable layout** | slot order, `offset-to-top`, `type_info` position (file 13) |
| **Base class order**, virtual base placement | |
| **Exception handling** | `.eh_frame` / `.gcc_except_table` format, `__cxa_*` personality |
| **RTTI** | `type_info` structure, name-based vs address-based comparison |
| **Standard library types** | libstdc++ `std::string` (SSO layout), `std::list` node, `std::hash` — differ from libc++ |
| **`std::string` dual ABI** | `_GLIBCXX_USE_CXX11_ABI=0` (COW) vs `=1` (SSO) — different mangled names |

**"The ABI" you build against** = compiler + version + standard library + a set of
flags (`-std`, `_GLIBCXX_USE_CXX11_ABI`, `_GLIBCXX_DEBUG`, `-m32/-m64`).

---

## ABI break — the catalog

An **ABI break** = a change after which old compiled code linking against / calling
new compiled code misbehaves (link error at best, silent corruption at worst).

### Layout breaks (offsets shift)
- **Add / remove / reorder a non-static data member.**
- **Change a member's type** to a different size/alignment (`int` → `long` on LP64).
- **Add / remove a base class**, or change base order.
- **Change `alignas`** on the type or a member.
- **`#ifdef`-conditional member** compiled inconsistently across TUs (this is also
  an ODR violation — folder 24 file 04).
- **Change `#pragma pack`.**

### vtable breaks (slot indices shift)
- **Add / remove / reorder a virtual function** in a class or its bases.
- **Make a non-virtual function virtual** (or vice versa).
- **Add a virtual base.**
- Change of the "key function" can move where the vtable is emitted (link issue).

### Interface breaks (calling code baked assumptions)
- **Change a function's signature** — parameter types, count, `const`, ref-ness
  → different mangled name → `undefined reference` (the "loud" break).
- **Change return type** — mangled name unchanged for free functions in Itanium
  (return type isn't mangled!), but the calling convention for the return value
  can change → silent break.
- **Change a default argument** — the value is baked at the **call site**; old
  callers use the old default.
- **Change an `inline` function's body** — old callers already inlined the old
  body; you now have two behaviours in one program (also an ODR issue).
- **Change an `enum`'s underlying type** or a constant's value that's baked in.
- **`constexpr` / template** changes — instantiated into callers.

### Standard library / flags
- **`_GLIBCXX_USE_CXX11_ABI` mismatch** — `std::string` in a signature mangles
  differently (`std::__cxx11::basic_string` vs `std::basic_string`).
- **`_GLIBCXX_DEBUG`** — changes `std::vector` / iterator layout entirely.
- **libstdc++ vs libc++** — incompatible `std::` type layouts.
- **Compiler major version** — occasionally bumps its own ABI.

---

## What's ABI-SAFE

- Add a **non-virtual** member function (not in the layout, not in the vtable).
- Add a **static** data member.
- Add a virtual function **at the very end** of the most-derived class in a
  hierarchy nobody else derives from (still risky — prefer not).
- Change a function body that is **not** `inline` and not a template (recompiling
  that TU is enough).
- Add an overload with a new name/signature.
- Widen access (`private` → `public`) — source change, not ABI.

---

## Detecting ABI breaks

| Tool | What |
|---|---|
| **`abi-compliance-checker`** / **`abidiff`** (libabigail) | compare two builds of a library, report ABI diffs |
| **`-Wodr` + `-flto`** | catches mismatched type definitions across TUs (folder 24 file 04) |
| **`static_assert(sizeof(T) == N)` + `offsetof` asserts** | in the shared header — a layout change fails to compile |
| **Symbol versioning** (`.symver`, version scripts) | glibc-style — old and new symbol versions coexist in one `.so` |
| **`nm -C` / `abidw`** | dump the exported symbol set to diff |
| **A "no new symbols" CI gate** on a stable library | |

---

## Stable API design (when you must ship a binary)

1. **C ABI at the boundary** — `extern "C"` functions, C-compatible types
   (integers, pointers, POD structs with explicit fixed-width fields), no
   exceptions across, no `std::` types in signatures. C mangling + C calling
   convention are stable across compilers.
2. **Opaque handles (pImpl at the ABI level)** —
   ```c
   typedef struct Engine Engine;
   Engine* engine_create(const EngineConfig*);
   int     engine_submit(Engine*, const Order*);
   void    engine_destroy(Engine*);
   ```
   The caller never sees `Engine`'s layout → you can change it freely.
3. **Version the interface** — an `abi_version` field / a `get_version()` call;
   add functions, never change existing signatures.
4. **Don't inline across the boundary** — hide the implementation in the `.so`,
   `-fvisibility=hidden` + explicit exports (folder 24 file 05).

---

## Andar kya hota hai

- The compiler bakes ABI decisions into every object file: mangled symbol names,
  member offsets in generated field accesses, vtable slot indices in virtual
  calls, calling-convention register usage.
- The linker matches by mangled name only — a layout/vtable break produces code
  that computes the wrong offset / calls the wrong slot with **no diagnostic**.
- `_GLIBCXX_USE_CXX11_ABI` is implemented via an inline namespace
  (`std::__cxx11`) so the two `std::string`s have distinct mangled names —
  mismatches surface as `undefined reference to foo(std::__cxx11::basic_string...)`.
- Symbol versioning (`GLIBC_2.34` etc.) lets a single `.so` export
  `memcpy@GLIBC_2.2.5` and `memcpy@GLIBC_2.14` — old binaries bind the old one.

---

## > **HFT relevance**
> - **Static-link everything** (folder 24 file 09) — one compiler, one stdlib, one
>   flag set, one immutable binary → most ABI issues simply can't occur. Rebuild
>   vendored libraries from source with **your** flags.
> - **`static_assert(sizeof) + offsetof`** on every wire / IPC / shared-memory /
>   ABI-facing struct — a layout drift fails the build, not production.
> - **One `_GLIBCXX_USE_CXX11_ABI`, one `-std`, never `_GLIBCXX_DEBUG` in a mixed
>   build** — mismatches are silent corruption (folder 24 file 04, 10).
> - **Plugin / strategy interfaces = C ABI + opaque handles** — `extern "C"`
>   entry points (`strategy_create` / `strategy_on_tick` / `strategy_destroy`),
>   POD structs only, no exceptions across (folder 24 file 07). The host can load
>   plugins built by a different (compatible) toolchain.
> - **Toolchain upgrades** — validate with `abidiff` on any `.so` you don't
>   rebuild, and re-run the full test suite; a GCC major bump can move layout.

---

## Hands-on

```bash
bash 24-COMPILATION-LINKING/examples/07_binary_inspection.sh    # nm / c++filt / symbols
```

- `nm -C --defined-only libfoo.so | sort > syms.txt` — the exported ABI surface;
  diff it across two builds.
- Add a data member to a struct in a header, recompile only *one* TU that uses it,
  link with an unchanged other TU → observe the layout mismatch (garbage / crash).
- `echo 'int f(std::string);' ` compiled with `-D_GLIBCXX_USE_CXX11_ABI=0` vs `=1`
  → `nm` the two `.o` → different mangled names for `f`.
- `abidiff` (if installed) on two `.so` builds.

---

## ⚠️ Traps

### Trap 1 — adding a data member to a shipped struct
Every offset after it shifts; every field access in already-compiled callers is
now wrong. ABI break.

### Trap 2 — `_GLIBCXX_USE_CXX11_ABI` mismatch
`undefined reference to foo(std::__cxx11::basic_string...)` — one side old ABI,
one new. Rebuild consistently.

### Trap 3 — `std::` types in a shipped binary interface
`std::string` / `std::vector` layout isn't stable across stdlib versions/vendors.
Use C types + opaque handles at the boundary.

### Trap 4 — changing a default argument value
Baked at the call site. Old callers keep the old default silently.

### Trap 5 — changing an `inline` function's body in a header
Old TUs inlined the old body. Two behaviours in one program (ODR + ABI).

### Trap 6 — adding a virtual method not at the end
vtable slot indices shift for later methods → wrong calls in old code.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "ABI = name mangling" | + calling convention + layout + vtable + EH + stdlib types |
| "source-compatible = ABI-compatible" | Adding a data member / virtual is source-compatible but ABI-breaking |
| "the linker will catch ABI breaks" | Only signature (mangled-name) changes; layout/vtable breaks are silent |
| "`std::string` layout is stable" | Dual ABI (`_GLIBCXX_USE_CXX11_ABI`), differs across stdlib vendors |
| "changing a return type is safe (same name)" | Itanium doesn't mangle return type, but the return-value convention can change |
| "static linking has ABI concerns too" | Far fewer — one toolchain, one build; that's why HFT prefers it |

---

## Exercises

1. **ABI break or not:** (a) add `void log() const;` (non-virtual), (b) add `int
   flags;` data member, (c) add `virtual void reset();` at the end, (d) change
   `void f(int)` to `void f(long)`, (e) add `static int count;`.

   <details><summary>Answer</summary>

   (a) safe. (b) break (layout). (c) break-ish (vtable grows; safe *only* if no one
   derives and nothing is compiled against the old vtable — treat as a break).
   (d) break — different mangled name → `undefined reference` (loud). (e) safe.
   </details>

2. **Diagnose:** `undefined reference to Engine::run(std::__cxx11::basic_string<...>)`.
   What single inconsistency?

   <details><summary>Answer</summary>

   `_GLIBCXX_USE_CXX11_ABI` mismatch — one TU/library built with `=1` (new
   `std::__cxx11::string`), the other with `=0` (old COW `std::string`). Rebuild
   everything with the same setting.
   </details>

3. **Design a stable API** for a matching engine you ship as a `.so` to another
   desk. Sketch the interface.

   <details><summary>Answer</summary>

   ```c
   extern "C" {
   typedef struct Engine Engine;
   typedef struct { int32_t px_ticks; uint32_t qty; uint8_t side; uint8_t _pad[3]; } WireOrder;
   Engine* engine_create(uint32_t abi_version, const char* config_json);
   int     engine_submit(Engine*, const WireOrder*, uint64_t* out_order_id);
   void    engine_destroy(Engine*);
   uint32_t engine_abi_version(void);
   }
   ```
   POD structs with explicit widths + padding, opaque `Engine*`, no exceptions, an
   ABI version, add-only evolution.
   </details>

4. **Guard a wire struct:** three `static_assert`s for `struct MdTick { uint64_t
   ts; int64_t px; uint32_t qty; uint8_t side; uint8_t _pad[3]; };`.

   <details><summary>Answer</summary>

   `static_assert(std::is_trivially_copyable_v<MdTick>);`
   `static_assert(std::is_standard_layout_v<MdTick>);`
   `static_assert(sizeof(MdTick) == 24);`
   `static_assert(offsetof(MdTick, px) == 8 && offsetof(MdTick, qty) == 16);`
   (plus `static_assert(std::endian::native == std::endian::little);` if relied
   on).
   </details>

5. **Toolchain upgrade:** you bump GCC from 12 to 15 and rebuild your engine, but
   link against a vendor `.so` still built with GCC 11. What do you check?

   <details><summary>Answer</summary>

   Whether the vendor `.so` exposes `std::` types in its interface (risky across
   stdlib versions) — ideally it's a C ABI. Run `abidiff` / compare symbol sets;
   check `_GLIBCXX_USE_CXX11_ABI` consistency; run the full test suite. Safest:
   get the vendor to ship a build matching your toolchain, or static-link a
   from-source build.
   </details>

---

## Interview questions

1. ABI kya-kya cover karta hai (mangling ke alawa 4-5 cheezein)?
2. Source-compatible vs ABI-compatible — ek change jo pehla hai par doosra nahi.
3. ABI break catalog — layout / vtable / interface — ek-ek example.
4. `_GLIBCXX_USE_CXX11_ABI` mismatch ka symptom.
5. Return type change "same mangled name" — phir bhi kyun break ho sakta?
6. Stable binary C++ API kaise design karein (C ABI + opaque handle)?
7. ABI break detect karne ke tools.
8. Static linking ABI concerns ko kaise kam karta?

---

## Next
→ [`15-undefined-behaviour-catalog.md`](15-undefined-behaviour-catalog.md)
