# 07 — Devirtualization: kab compiler virtual calls resolve karta

## Prerequisites
- `16-OOP/` (vtable/vptr, virtual dispatch)
- `21-TEMPLATES/16-*` (compile-time dispatch alternative)
- `03-inlining.md`

## Yeh topic abhi kyun
`virtual` call = ek indirect call (`call [rax + offset]`) — target runtime
pe vtable se aata. Woh **inline nahi hota**, **branch-predict karna padta**
(BTB), aur optimizer uske paar nahi dekh sakta (folder 32 lesson 10). Par
compiler kabhi-kabhi prove kar leta ki call ka target ek hi ho sakta hai —
tab woh use **direct call** (aur phir inline) bana deta. Yeh "devirtualization"
kab hoti hai — aur kaise use encourage karein — HFT-relevant hai.

---

## Virtual call ki cost (recap, folder 16/32)

```cpp
shape->area();
```
→ (1) load `shape` (a pointer), (2) load `shape->vptr` (first 8 bytes), (3)
load `vptr[area_slot]` (the function pointer), (4) `call` that address.
= 2 dependent loads + an indirect call. Plus:
- **not inlined** → the optimizer can't fold constants / CSE across it, and
  `area()`'s body stays a separate function.
- **BTB prediction** → if `shape` points at mixed concrete types across
  iterations, the indirect branch mispredicts (folder 31 lesson 07), and the
  I-cache jumps between different `area` bodies.

Measured elsewhere in this repo: virtual ~23 ns vs CRTP/direct ~2.2 ns
(folder 16 `07`), virtual ~2.4-2.5 ns vs template ~1.1 ns (folder 21).

---

## Kab compiler devirtualize karta

### 1. Concrete type is visible ("exact type known")
```cpp
Circle c(2.0);
c.area();                  // c IS a Circle, not "a Shape*" -> DIRECT call, then inlined
Shape& s = c;
s.area();                  // still: s is bound to a known Circle in this scope
```
If the dynamic type is provable from the local flow, no vtable lookup.

### 2. `final` class or `final` method
```cpp
struct Circle final : Shape { double area() const override; };
Shape* p = get_circle();
p->area();                 // p could be null/other... but if the static type is Circle*
                           // and Circle is final -> no derived class can override -> DIRECT

struct Base { virtual void f() final; };   // f can't be overridden further
```
`final` tells the compiler "the hierarchy stops here" → for that static type,
the target is fixed.

### 3. Whole-program / LTO speculative devirt
With `-flto` (+ `-fdevirtualize-speculatively`, on at `-O2`), the compiler
sees *all* the derived classes in the program. If a `Shape*` can only ever be
one of two types, it emits:
```asm
    ; if (vptr == &Circle_vtable) Circle::area();   // inlined, direct
    ; else if (vptr == &Square_vtable) Square::area();
    ; else call [vptr+slot];                        // fallback
```
a "speculative devirtualization" — a type check + direct/inlined call, with
the indirect call as fallback. Fast when one type dominates.

### 4. `-fwhole-program` / anonymous-namespace classes
A class in an anonymous namespace can't be derived from in another TU → the
compiler knows the full set of overrides.

### 5. PGO (lesson 11)
The profile records which concrete type hit each call site. PGO then
speculatively devirtualizes the hot type (like #3 but driven by real data).

---

## Kya devirt ko rok deta

- The dynamic type genuinely varies and isn't `final` → real vtable dispatch.
- No LTO and the derived classes live in other TUs → the compiler can't
  enumerate them.
- The pointer comes from a factory / `dynamic_cast` / a container of
  `Base*` populated elsewhere.
- `-fno-devirtualize` (rare, debugging).

---

## Encourage karne ke tareeke

| Technique | Effect |
|---|---|
| `final` on classes that won't be derived, and on leaf overrides | biggest single win — makes the target fixed |
| `-flto` | lets speculative devirt see all types |
| Keep hierarchies small + in headers | compiler enumerates overrides |
| Use the concrete type where you have it (don't up-cast to `Base&` needlessly) | exact-type devirt |
| PGO | speculative devirt on the hot type |
| **Or: don't use `virtual` on the hot path at all** → CRTP / `std::variant` + `visit` / tag-`switch` / function pointers / templates (folder 21, folder 36) |

---

## The honest answer for hot paths

Devirtualization is a **best-effort** optimization. It fires often in simple
cases (`final`, exact type, LTO + one dominant type) and *not* when you have
a real heterogeneous `std::vector<Base*>` iterated in a hot loop — which is
exactly the DOD anti-pattern (folder 32 lesson 10).

For HFT hot paths, **don't rely on devirt** — architect the dispatch away:
- **`std::variant<A, B, C>` + `std::visit`** — closed set, the compiler
  generates a jump table, often inlines each arm. Measured ~1.1 ns (folder 21).
- **CRTP** — static polymorphism, zero dispatch cost, inlines fully.
- **tag `switch` on a small `enum`** — one predictable indirect (jump table),
  each case inlined.
- **function pointer + `void* ctx`** — one indirect, but no vtable chase and
  you control layout.

`virtual` is fine on the **control plane** (session setup, config, admin) —
clarity there beats nanoseconds.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — assuming `virtual` will be devirtualized "because the compiler is smart"
It often isn't, for the case that matters (heterogeneous container, cross-TU
types, no LTO). Measure; don't assume.

### Trap 2 — forgetting `final`
A leaf class / leaf override with no `final` leaves the compiler unable to
prove the target even when *you* know the hierarchy stops. `final` is free
and enables exact devirt.

### Trap 3 — `final` on a class you later need to mock/derive for tests
`final` blocks test doubles. Use an interface + a `final` production impl, or
a template seam, or don't mock that class.

### Trap 4 — speculative devirt with a flat type distribution
If the call site sees 5 types evenly, the type-check chain (#3) is *slower*
than just doing the indirect call. PGO/heuristics usually get this right;
verify with `perf`.

### Trap 5 — expecting devirt without LTO across TUs
`Base` in `iface.hpp`, `Derived` in `impl.cpp`, call site in `main.cpp`, no
`-flto` → the compiler in `main.cpp` doesn't know `Derived` exists → no
speculative devirt.

### Trap 6 — `virtual` + `inline` and thinking it inlines
`inline` on a virtual is ODR only. It inlines only when devirtualized.

---

## > **HFT relevance**

> - **No `virtual` on the tick path.** Strategy/handler dispatch → `variant +
>   visit`, CRTP, or `switch(enum)`. Measured single-digit ns vs ~20+ for a
>   cold virtual (folder 16/21).
> - **`final` everywhere it applies** — leaf strategies, concrete order-book
>   types — even on control-plane code it's free and documents the design.
> - **`-flto` + PGO** for the code that *does* use `virtual` (adapters,
>   gateways) → speculative devirt on the dominant type.
> - **If you inherit a `virtual`-heavy framework**, wrap the hot call in a
>   type-`switch` or hoist the type check out of the loop (dispatch once,
>   then a monomorphic inner loop).
> - **Check `perf annotate`** on any remaining `call [reg]` in the hot loop —
>   that's a vtable/function-pointer dispatch; decide if it's worth killing.

---

## Hands-on

```bash
# exact-type devirt:
echo 'struct S{virtual int a()const;}; struct C final:S{int a()const override{return 7;}};
      int f(){C c; return c.a();}' | g++ -O2 -S -masm=intel -std=c++20 -xc++ - -o - | grep -A3 'f():'
#   -> `mov eax, 7 ; ret`  (no vtable, fully devirtualized + inlined)

# heterogeneous container -> real dispatch:
./build.ps1 asm 16-OOP/examples/07_dispatch_benchmark.cpp | grep -n "call"
#   `call [rax+...]` in the virtual loop; direct in the CRTP loop

# speculative devirt report:
g++ -O2 -flto -fdump-ipa-devirt=/dev/stdout -c file.cpp | grep -i devirt

# variant+visit alternative:
./build.ps1 fast 21-TEMPLATES/examples/08_compile_time_dispatch.cpp
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "compiler devirtualizes virtual calls" | best-effort; not for heterogeneous containers / cross-TU |
| "`final` is just documentation" | it lets the compiler fix the call target → exact devirt |
| "`virtual` + `inline` inlines" | `inline` = ODR; inlines only when devirtualized |
| "speculative devirt is always a win" | flat type distribution → the type-check chain is slower |
| "devirt works without LTO across TUs" | needs to enumerate all overrides — LTO or same TU |
| "just use `virtual`, it's fine" | on the hot path it's ~10× a direct/inlined call |

---

## Exercises

1. `std::vector<std::unique_ptr<Shape>> shapes;` populated from a factory with
   `Circle`, `Square`, `Triangle`. `for (auto& s : shapes) total += s->area();`
   — will `-O2 -flto` devirtualize this? What actually happens per iteration?

   <details><summary>Answer</summary>

   **Probably not usefully.** Even with LTO enumerating the 3 types, if all 3
   occur with similar frequency, speculative devirt (a chain of `if (vptr ==
   &Circle_vt) ... else if ...`) is *slower* than one indirect call, so the
   compiler/heuristics leave it as `call [vptr+slot]`. Per iteration: load
   `unique_ptr` → `Shape*` (contiguous, ok), deref → heap-scattered object
   (~1 cache miss), load vptr, load slot, indirect `call` (BTB predict — if
   types are interleaved, mispredicts + I-cache bounces between the 3 `area`
   bodies), and `area()` is not inlined so no cross-call folding. This is the
   folder-32 lesson-10 DOD anti-pattern. Fix: SoA by type + a loop per type,
   or `variant` + `visit`, or a tag `switch`.
   </details>

2. `struct Handler { virtual void on_msg(const Msg&) = 0; }; struct FeedA final
   : Handler { ... };` — one `FeedA` instance, used via `Handler*`. Devirt?

   <details><summary>Answer</summary>

   **Yes, likely fully devirtualized.** If the compiler can see (same TU or
   LTO) that the `Handler*` is bound to a `FeedA` — or even just that the
   static type at the call site is `FeedA*` — then because `FeedA` is
   `final`, no override can exist below it → the target of `on_msg` is fixed
   → direct call → inlined. Even through a `Handler*`, if `FeedA` is the only
   class deriving `Handler` in the program (LTO), speculative devirt collapses
   to a single direct/inlined call with no type check. `final` on the leaf +
   `-flto` is what makes this reliable.
   </details>

3. Tumhare paas ek framework hai jisme `virtual process()` har event pe
   chalta, aur tum framework ka source nahi badal sakte. Hot path pe uski cost
   kaise ghataoge?

   <details><summary>Answer</summary>

   Options: (1) **Hoist the dispatch out of the loop** — if the concrete
   handler is fixed for a batch of events, call a `static_cast<Concrete*>(h)
   ->Concrete::process(...)` once to get a monomorphic path, or grab the
   member function pointer once. (2) **Type-`switch` at your boundary** — if
   you know the small set of concrete types, `switch (h->type_tag())` and
   call each concretely (each a direct/inlined call). (3) **`-flto`** across
   your code + the framework's static lib → speculative devirt on the
   dominant type. (4) **Batch by type** — group events so each inner loop
   hits one concrete `process`, giving the BTB a stable target and letting
   the I-cache stay warm. (5) If truly stuck, accept it and make sure
   `process()`'s body is small and hot-cache-friendly so the ~10-20  cyc
   dispatch is the whole cost, not dispatch + a cache-miss-heavy body.
   </details>

---

## Interview questions

1. Virtual call ki full cost (loads + indirect + not-inlined + BTB).
2. 4 situations where the compiler devirtualizes.
3. `final` — how it enables devirtualization.
4. Speculative devirtualization — what code the compiler emits, when it's a loss.
5. Why devirt across TUs needs LTO.
6. `virtual` + `inline` — does it inline? (only via devirt)
7. 3 ways to architect virtual dispatch off the hot path.

---

## Next
→ [`08-branch-hints.md`](08-branch-hints.md)
