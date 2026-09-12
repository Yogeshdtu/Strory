# 12 — The four casts, deep. `dynamic_cast` internals, RTTI layout

## Prerequisites
- `16-OOP` (inheritance, vtable, `dynamic_cast` basics), `11-type-punning.md`
- `24-COMPILATION-LINKING` file 07 (mangling, RTTI in EH)

## Yeh topic abhi kyun
Folder 16 mein casts ka intro tha. Yahan **exact semantics**, kaunsa cast kya
**runtime cost** deta hai, aur `dynamic_cast` andar kaise kaam karta hai (vtable →
`type_info` → hierarchy walk). HFT: `dynamic_cast` hot path pe nahi (ns-level cost
+ `-fno-rtti`), toh alternatives (tag dispatch, `static_cast` down a known
hierarchy, `std::variant`) — sab is samajh pe.

---

## `static_cast<T>(x)` — compile-time, "related types"

Kya karta:
- **Numeric conversions** — `int`↔`double`, narrowing (with truncation),
  `enum`↔integral.
- **Up/down cast in a hierarchy** — `Derived*` → `Base*` (implicit anyway),
  `Base*` → `Derived*` (**you assert** it's really a `Derived` — **no runtime
  check**).
- `void*` → `T*`.
- Explicitly invoke a conversion (`static_cast<std::string>(cstr)`).
- `static_cast<T&&>(x)` = `std::move` (folder 18).

Cost: **usually zero** (a numeric conversion instruction, or a pointer offset
adjustment for multiple inheritance). No check — a wrong downcast is **UB**.

```cpp
Base* b = get();                 // actually a Circle
Circle* c = static_cast<Circle*>(b);   // no check — if b isn't a Circle: UB
```

## `dynamic_cast<T>(x)` — runtime-checked, polymorphic only

Kya karta:
- **Downcast / crosscast with a runtime check.** `Base*` → `Derived*`: returns
  `nullptr` if `x` isn't actually a `Derived` (or derived-from). `Base&` →
  `Derived&`: throws `std::bad_cast` on failure.
- **`dynamic_cast<void*>(p)`** — returns a pointer to the **most-derived object**
  (top of the complete object).
- Requires the source type to be **polymorphic** (has ≥ 1 virtual function).
- Needs **RTTI** enabled (`-fno-rtti` breaks it).

Cost: **not free** — a call into `__dynamic_cast` (libstdc++), which reads the
object's `type_info` (via the vtable) and walks the inheritance graph. Tens of ns
for a simple hierarchy, more for deep/multiple/virtual inheritance.

```cpp
if (auto* c = dynamic_cast<Circle*>(shape)) c->radius();   // safe, checked
Circle& cr = dynamic_cast<Circle&>(*shape);                // throws bad_cast if wrong
Shape* s = dynamic_cast<Shape*>(printable);                // cross-cast (Shape & Printable siblings)
```

## `const_cast<T>(x)` — add/remove cv-qualifiers only

Kya karta: `const`/`volatile` add ya remove. **Kuch aur nahi.**

```cpp
void legacy_api(char* s);                 // C API, should take const
const std::string msg = "hi";
legacy_api(const_cast<char*>(msg.data()));   // OK IF legacy_api doesn't write
```

- **Removing `const` and then writing** to an object that is **truly `const`** =
  **UB**. Removing `const` from a pointer to a non-const object (that just came
  through a `const` interface) — fine.
- Cost: zero (compile-time only).
- Code smell — usually means an interface should have been `const`-correct.

## `reinterpret_cast<T>(x)` — "reinterpret the bit pattern"

Kya karta:
- **Pointer ↔ integer** (`std::uintptr_t`) — round-trips.
- **Pointer ↔ unrelated pointer** — but **dereferencing as the new type is UB**
  unless it's `char`/`unsigned char`/`std::byte` (file 10, 11).
- Function pointer ↔ function pointer.

Cost: zero (it's just a relabel). **Danger:** almost every "deref the result"
usage is UB. Use `std::bit_cast`/`memcpy` for punning.

## C-style cast `(T)x` — don't

Tries, in order: `const_cast`, `static_cast`, `static_cast` + `const_cast`,
`reinterpret_cast`, `reinterpret_cast` + `const_cast`. **Whichever compiles wins**
— so `(T)x` can silently be a `reinterpret_cast` when you meant a `static_cast`.
Use the named casts; they're greppable and intent-revealing.

---

## `dynamic_cast` internals (Itanium ABI)

```
obj ──► [ vptr ] ──► vtable
                       ├── [-2] offset-to-top   (this - complete object)
                       ├── [-1] &type_info      ◄── RTTI for the object's dynamic type
                       ├── [ 0] ~T() ...
                       └── [ 1] virtual fns ...
```

`dynamic_cast<Derived*>(base_ptr)` roughly:
1. Load `base_ptr`'s vptr → vtable.
2. Load `vtable[-1]` → `&type_info` for the **dynamic** (most-derived) type.
3. Call `__dynamic_cast(base_ptr, &Base_type_info, &Derived_type_info, offset_hint)`.
4. `__dynamic_cast` walks the class's RTTI structure (`__class_type_info`,
   `__si_class_type_info` for single inheritance, `__vmi_class_type_info` for
   multiple/virtual) comparing `type_info` objects, computing the pointer
   adjustment for the target subobject.
5. Success → adjusted `Derived*`. Failure → `nullptr` (pointer) or `throw
   std::bad_cast` (reference).

**`type_info` comparison** — GCC compares `type_info` *addresses* (fast) when it
can guarantee a single copy (`-fvisibility` / one shared lib); across DSO
boundaries it falls back to `strcmp` on the mangled name string (slower, and a
source of `dynamic_cast`-across-`.so`-fails bugs — bind the `type_info` with
`-Bsymbolic` / default visibility).

`typeid(expr)` — for a polymorphic `expr`, also loads `vtable[-1]`; for a
non-polymorphic expr, it's resolved at compile time.

---

## Cost comparison (order of magnitude)

| Cast | When resolved | Runtime cost |
|---|---|---|
| `static_cast` (numeric / known hierarchy) | compile | ~0 (one instr or a pointer offset) |
| `const_cast` / `reinterpret_cast` | compile | 0 (relabel) |
| `dynamic_cast` (simple single inheritance) | runtime | ~few–tens of ns (`__dynamic_cast` call + walk) |
| `dynamic_cast` (multiple / virtual inheritance, deep) | runtime | more — graph walk |
| `typeid` on polymorphic | runtime | one vtable load + `type_info` compare |

Measured in folder 16 (`07_dispatch_benchmark`): virtual dispatch ~23 ns vs
direct ~2.2 ns; `dynamic_cast` is in the "call + branch + maybe walk" bucket.

---

## Hot-path alternatives to `dynamic_cast` (folder 16, 21)

- **You know the type** (a factory tagged it, an enum, a discriminator) →
  `static_cast` down + assert.
- **Closed set of types** → **`std::variant` + `std::visit`** (compile-time
  dispatch, no RTTI) or a **tag enum + `switch`**.
- **Visitor pattern** — a virtual `accept(Visitor&)` — dispatch through the vtable
  you already have, no cast.
- **`dynamic_cast` is fine** in cold code (plugin loading, tooling, tests, error
  paths).

---

## > **HFT relevance**
> - **`-fno-rtti` on the hot binary** (folder 23 file 09) → `dynamic_cast` and
>   `typeid` unavailable → the design *must* use tag dispatch / `std::variant` /
>   visitors / known-type `static_cast`. This is a deliberate constraint that
>   forces faster patterns.
> - **`static_cast` down a hierarchy you control** (e.g. an order type you tagged
>   at creation) — zero cost, but you own the correctness (a wrong cast is UB, so
>   `assert` the tag in debug).
> - **`reinterpret_cast` only for pointer↔int and byte views** — punning goes
>   through `bit_cast`/`memcpy` (file 11).
> - **C-style casts banned in review** — they hide `reinterpret_cast`. Named casts
>   are greppable (`grep -r reinterpret_cast` = "audit these").
> - `dynamic_cast`-across-`.so` failing silently (`type_info` name compare) is a
>   real deployment bug — another reason HFT static-links (folder 24 file 09).

---

## Hands-on

```bash
./build.ps1 16-OOP/examples/07_dispatch_benchmark.cpp        # dispatch costs (folder 16)
./build.ps1 25-OBJECT-MODEL/examples/07_vtable_inspect.cpp   # vptr -> vtable[-1] = type_info
```

- `objdump -d -C` a function with a `dynamic_cast` → find the `__dynamic_cast`
  call and the `type_info` loads.
- Compile a hierarchy with and without `-fno-rtti`; a file using `dynamic_cast`
  fails to compile with `-fno-rtti` — replace it with a tag `switch`.
- `typeid(*base_ptr).name()` for two derived types — see the mangled names.

---

## ⚠️ Traps

### Trap 1 — `static_cast` downcast without knowing the type
```cpp
Circle* c = static_cast<Circle*>(shape);   // ⚠️ UB if shape isn't a Circle
```
`dynamic_cast` (checked) if unsure; `static_cast` only when you *know*.

### Trap 2 — `const_cast` away `const` then write a truly-const object
```cpp
const int x = 5;
const_cast<int&>(x) = 10;   // ⚠️ UB
```

### Trap 3 — `reinterpret_cast` + deref as another type
File 10/11 — UB. `bit_cast`/`memcpy`.

### Trap 4 — C-style cast silently becoming `reinterpret_cast`
```cpp
Derived* d = (Derived*)someUnrelatedPtr;   // compiles as reinterpret_cast -> UB later
```
Named casts.

### Trap 5 — `dynamic_cast` in a hot loop
Tens of ns each. Cache the result, restructure to a `variant`/visitor, or tag the
type.

### Trap 6 — `dynamic_cast<void*>` expecting the `Base` subobject address
It gives the **most-derived** (complete) object's address — useful for identity
comparison, not for accessing `Base`.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`static_cast` downcast is checked" | No check — you assert; wrong = UB. `dynamic_cast` checks |
| "`dynamic_cast` is basically free" | A `__dynamic_cast` call + hierarchy walk — ns, not zero |
| "`const_cast` + write is fine" | Only if the object isn't *truly* const; else UB |
| "`reinterpret_cast` reinterprets values safely" | Only relabels; deref-as-other-type is UB |
| "C-style casts are just shorter" | They can silently pick `reinterpret_cast` — use named casts |
| "`dynamic_cast<void*>` gives the base part" | Gives the complete (most-derived) object address |

---

## Exercises

1. **Which cast:** (a) `int` → `double`, (b) `Base*` → `Derived*` when you *know*
   it's a `Derived`, (c) same but you *don't* know, (d) `void*` → `char*` to read
   bytes, (e) remove `const` to call a non-`const`-correct C API that won't write.

   <details><summary>Answer</summary>

   (a) `static_cast`. (b) `static_cast` (you own correctness). (c) `dynamic_cast`
   (checked). (d) `static_cast` (or `reinterpret_cast`) to `char*` — reading bytes
   is the legal exception. (e) `const_cast` (and pray the API keeps its promise).
   </details>

2. **UB or not:** `Shape* s = new Circle; Square* sq = static_cast<Square*>(s);
   double a = sq->area();`

   <details><summary>Answer</summary>

   UB — `s` points at a `Circle`, `static_cast<Square*>` doesn't check, and
   `sq->area()` reads/uses memory as a `Square`. `dynamic_cast<Square*>(s)` would
   return `nullptr`.
   </details>

3. **Explain the walk:** what does `dynamic_cast<Derived*>(basePtr)` do at runtime,
   step by step?

   <details><summary>Answer</summary>

   Load `basePtr`'s vptr → vtable; load `vtable[-1]` → `&type_info` for the
   dynamic type; call `__dynamic_cast(basePtr, &Base_ti, &Derived_ti, hint)`;
   `__dynamic_cast` walks the RTTI class graph comparing `type_info`s and
   computing the subobject offset; return the adjusted `Derived*` or `nullptr`.
   </details>

4. **Replace it:** a hot function does `if (auto* m = dynamic_cast<MarketOrder*>(o))
   ... else if (auto* l = dynamic_cast<LimitOrder*>(o)) ...`. Two faster designs.

   <details><summary>Answer</summary>

   (1) Tag: `enum class Kind` set at construction + `switch (o->kind)` + a checked
   `static_cast`. (2) `std::variant<MarketOrder, LimitOrder, ...>` + `std::visit`
   — compile-time dispatch, no RTTI, inlinable. (Also: a virtual method on `Order`
   that does the type-specific work, so no cast at all.)
   </details>

5. **`-fno-rtti`:** which of `static_cast`, `dynamic_cast`, `const_cast`,
   `reinterpret_cast`, `typeid` still work?

   <details><summary>Answer</summary>

   `static_cast`, `const_cast`, `reinterpret_cast` — fine. `dynamic_cast` (for
   downcasts/crosscasts) and `typeid` on a polymorphic expression — **unavailable**
   (`typeid` on a non-polymorphic type still works, it's compile-time).
   </details>

---

## Interview questions

1. Chaar casts — kaunsa kya karta, kaunsa runtime-checked?
2. `static_cast` downcast — checked? Wrong cast ka kya?
3. `dynamic_cast` internals — vtable → `type_info` → walk. Cost?
4. `dynamic_cast<void*>` kya deta?
5. `const_cast` + write — kab UB?
6. C-style cast ka problem (silently `reinterpret_cast`)?
7. `-fno-rtti` — kaunse casts affect hote? Hot-path alternatives?
8. `dynamic_cast` across a `.so` boundary kyun fail ho sakta (`type_info` compare)?

---

## Next
→ [`13-vtable-layout.md`](13-vtable-layout.md)
