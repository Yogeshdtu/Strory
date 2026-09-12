# 02 — ODR violation: break karke dekho

**Lesson:** 04 (ODR deep). Yeh example **fail hone ke liye** hai — error message
padhna hi seekh hai.

## One Definition Rule — 30-second version

- Har **non-inline function / variable**: poore program mein **exactly ek**
  definition.
- Har **inline function, class, template, `constexpr`**: har TU mein **ek**
  definition, aur saari definitions **token-for-token identical** honi chahiye.
- Violation of the first kind → **linker error** ("multiple definition").
- Violation of the second kind (different bodies/layouts in different TUs) →
  **IFNDR** (ill-formed, no diagnostic required) → aksar **silent**, garbage at
  runtime. `-flto -Wodr` isse pakad sakta hai.

## Build

```bash
./build.sh          # dono demos chalata hai; DEMO 1 ka link FAIL hona expected
```

## DEMO 1 — non-inline function in a header (linker pakadta hai)

`shared.hpp` mein `const char* venue_name() { return "NYSE"; }` — ek **non-inline
definition**. `a.cxx` aur `b.cxx` dono isse include karte → `a.o` aur `b.o` dono
mein `venue_name` ka body →

```
ld: b.o: multiple definition of `venue_name()'; a.o: first defined here
```

**Fix (koi ek):**
1. `shared.hpp` → sirf `const char* venue_name();` (declaration). Body ek naye
   `shared.cxx` mein. Link line pe `shared.o` add karo.
2. Ya `inline const char* venue_name() { ... }` — `inline` = "multiple identical
   definitions OK, linker merge kar de" (vague / COMDAT linkage).
3. `constexpr` function / `constexpr`/`inline` variable — inpe `inline` implied.

## DEMO 2 — ek naam, do layouts (silent ODR, `-Wodr` pakadta hai)

`silent_a.cxx` aur `silent_b.cxx` dono `struct Config` define karti hain — **alag
fields, alag sizes** (`int` vs `long`, ek extra field). Yeh do alag types hain
jinka naam same hai → ODR violation. Par:

```
$ g++ silent_a.o silent_b.o silent_main.o -o silent_bad   # NO ERROR
$ ./silent_bad
A: sizeof(Config)=12 describe=105
B: sizeof(Config)=16 describe=105     <- do alag sizeof! aur galat describe
```

Linker chup hai. Har TU apne local `Config` layout ke hisaab se code generate
karti hai; `inline int describe(const Config&)` ki **do alag bodies** hain aur
linker unme se **ek arbitrarily rakhta hai** — dono call sites us ek ko use karte,
doosre TU ke object ko **galat layout** se padhte hue (out-of-bounds field reads,
UB — yahan "105" bas ek garbage-but-deterministic value hai).

`-flto -Wodr` ke saath compiler ke paas dono TUs ka IR hota hai → mismatch detect:

```
warning: type 'struct Config' violates the C++ One Definition Rule [-Wodr]
note: the first difference ... is field 'verbose' ... vs 'extra'
```

**Fix:** `Config` ki **ek** definition ek shared header mein, `#pragma once`,
dono TUs wahi include karein. Kabhi do headers/TUs mein "same" type ko alag-alag
mat likho.

## Real-world shakl

- Ek `.cpp` `-DFEATURE_X` se compile hua, doosra bina — aur `struct` ka size
  `#ifdef FEATURE_X` pe depend karta hai → do layouts, ek binary. Classic.
- Ek library `-DNDEBUG` se bani (assert-free struct), aapka code bina → member
  offsets shift. ABI break.
- `-D_GLIBCXX_DEBUG` sirf kuch TUs pe → `std::vector` ka layout badal jata →
  crashes.
- **Rule:** wire structs / ABI-facing types pe `static_assert(sizeof(T) == N)`
  aur `static_assert(offsetof(T, field) == K)` — aur poore project mein
  consistent flags.

## Try karo

- DEMO 1 ko fix karo (option 1 ya 2) → clean link + run.
- `silent_b.cxx` ke `Config` ko `silent_a.cxx` jaisa exact bana do → dono
  `sizeof` 12, `describe` consistent.
- `nm -C a.o | grep venue` aur `nm -C b.o | grep venue` → dono mein `T venue_name()`
  (defined) — isiliye clash.
