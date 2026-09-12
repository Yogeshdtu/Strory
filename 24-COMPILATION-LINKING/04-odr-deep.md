# 04 — One Definition Rule (ODR) deep

## Prerequisites
- `01-translation-units.md`, `03-include-guards-and-pragma.md`
- [`examples/02_odr_violation/`](examples/02_odr_violation/)

## Yeh topic abhi kyun
ODR C++ ka woh niyam hai jo "kitni baar cheezein define ho sakti hain" govern
karta hai. Todne ke do zaike hain: ek jo **linker turant pakadta hai** ("multiple
definition"), aur ek jo **silent** hai (IFNDR) aur runtime pe garbage/crash deta.
Doosra wala HFT mein bahut real hai — mixed compile flags, vendored headers,
`#ifdef`-dependent layouts. Isse samajhna aur `static_assert`/`-Wodr` se pakadna
zaroori hai.

---

## ODR — teen niyam

**1. Har non-inline function / non-inline variable: poore program mein exactly ek
definition.**
```cpp
int add(int, int);              // declaration — kitni baar bhi
int add(int a, int b){return a+b;}   // definition — EXACTLY ONE (kisi ek .cpp mein)
```
Zero → `undefined reference` (file 10). Do+ → `multiple definition` (link error).

**2. Har class, enum, inline function, inline variable, template: har TU mein ek
definition, aur agar multiple TUs mein hai to sab byte-for-byte identical (same
tokens, same lookups).**
```cpp
// header (guarded) -> har TU jo include kare, usme ek identical definition -> OK
inline int clamp(int x){ return x<0?0:x; }
struct Point { double x, y; };
```

**3. Ek entity "used" (odr-used) hui to uski definition honi chahiye** — bhale
implicit (e.g. odr-used `static` constexpr member ko C++17 se pehle out-of-class
definition chahiye tha).

---

## Kism 1 — linker pakadta hai: "multiple definition"

```cpp
// shared.hpp  — ⚠️ non-inline function definition in a header
const char* venue() { return "NYSE"; }
```
`a.cxx` aur `b.cxx` dono include karein → `a.o` aur `b.o` dono mein `venue` ka body:

```
ld: b.o: multiple definition of `venue()'; a.o: first defined here
```

**Fixes:**
| Fix | Kaise |
|---|---|
| `inline` | `inline const char* venue() { ... }` — "multiple identical defs OK, merge" |
| Declaration + one def | header: `const char* venue();` ; `shared.cxx`: body |
| `static` (internal linkage) | `static const char* venue() {...}` — har TU ki apni copy (usually not what you want for functions) |
| anonymous namespace | same as `static`, modern form |

`constexpr` / `consteval` functions aur `constexpr`/`inline` variables — inpe
`inline` **implied** hai, so header mein safe.

### Templates aur `inline` — already fine

Template definitions header mein hoti hain aur ODR-safe hain (compiler har
instantiation ko **COMDAT / vague linkage** deta hai — linker duplicates merge kar
deta). Isliye template-heavy header libraries kaam karti hain.

---

## Kism 2 — silent (IFNDR): ek naam, do definitions

Yeh khatarnak wala. `struct Config` do TUs mein **alag** define:

```cpp
// tu_a.cxx
struct Config { int rate; int depth; bool verbose; };       // 12 bytes

// tu_b.cxx
struct Config { long rate; long depth; long extra; bool verbose; };   // 32 bytes
```

- Dono TUs compile — har ek apne local `Config` ke hisaab se code generate karta.
- Link **succeeds** — koi symbol clash nahi (struct koi symbol emit nahi karta;
  member functions agar hote to COMDAT merge ek arbitrary version rakh leta).
- Runtime: `sizeof(Config)` do jagah alag (12 vs 16 in the example). Ek TU ka code
  doosre TU ke `Config` object ko **galat offsets** se padhta → garbage / crash.

Standard isse **IFNDR** kehta hai — *ill-formed, no diagnostic required*. Compiler/
linker ko batane ki zaroorat nahi. **Aapki zimmedari.**

### Kaise pakdein

| Tool | Kya |
|---|---|
| **`-flto -Wodr`** | LTO ke paas dono TUs ka IR — mismatched types/functions detect karta hai (example `02` demo 2) |
| **`static_assert(sizeof(T) == N)`** / `offsetof` checks | ek shared header mein — har TU verify kare (folder 23 file 12) |
| **Consistent build flags** | `-DNDEBUG`, `-D_GLIBCXX_DEBUG`, `-DFEATURE_X` — sab TUs pe same warna layouts drift |
| **`gold` / `lld` `--detect-odr-violations`** | debug-info-based heuristic ODR check at link |
| **ASan** | ODR violation for globals (`__asan_odr_indicator`) — kabhi-kabhi |

---

## ODR violation ke real-world sources

1. **`#ifdef`-dependent layout, inconsistent flags:**
   ```cpp
   struct Order {
       int id; double px;
   #ifdef WITH_TELEMETRY
       std::uint64_t recv_ts;      // sirf kuch TUs mein
   #endif
   };
   ```
   Ek `.cpp` `-DWITH_TELEMETRY` se, doosra bina → do layouts, ek binary.

2. **Library built with different flags than your code** — `-DNDEBUG` (assert-free
   members), `-D_GLIBCXX_DEBUG` (`std::vector` debug layout — bilkul alag!),
   `-fno-rtti`, different `-std`.

3. **Two headers, "same" type copy-pasted** with a drift.

4. **Anonymous namespace type in a header** — har TU ko apna distinct type deta
   hai; agar woh type ek inline function ke signature mein use ho → ODR mess.

5. **Different `struct` packing** (`#pragma pack`) in different TUs.

6. **`inline` function with a definition that depends on a macro** set differently
   per TU.

---

## Andar kya hota hai

- **Non-inline symbol** → object file mein `T` (strong, defined). Do strong defs →
  linker `multiple definition`.
- **`inline` / template symbol** → **weak / COMDAT** (`W` in `nm`, `.section
  .text._Z...,"axG"`). Linker duplicates ko merge karta hai, **assuming they're
  identical** (ODR rule 2). Agar identical nahi → linker **ek arbitrarily** rakh
  leta, baaki discard — silent wrong behaviour.
- **Class/struct** koi symbol emit nahi karta (pure compile-time). Iska ODR
  violation isliye compile/link pe invisible; sirf generated code ke offsets se
  manifest hota. Debug info (`.debug_info`) mein har TU ka apna `Config` DIE hota —
  `-Wodr`/gold isi ko compare karte.

---

## > **HFT relevance**
> Silent ODR violations HFT mein **top-tier debugging nightmares** hain:
> intermittent wrong prices, corrupted book state, crashes jo sirf release build
> mein aate. Common trigger: ek vendored feed-handler library ek `-std`/`-D` set
> se bani, aapka engine doosre se; ya ek `.cpp` galti se `-D_GLIBCXX_DEBUG` ke
> saath (STL container layouts totally change).
>
> Defence:
> - **Ek build system, ek flag set** poore project + dependencies ke liye. Vendored
>   deps ko apne flags se rebuild karo (ya unke ABI ko pin karo).
> - **`static_assert(sizeof(WireMsg) == N)` + `offsetof` asserts** har
>   ABI/wire/shared struct pe, ek canonical header mein (folder 23 file 12).
> - **`-flto -Wodr`** at least CI builds mein — ye demo-2 kind ko pakadta hai.
> - **`#ifdef`-dependent struct members se bacho.** Optional fields → separate
>   struct / always-present + a flag.
> - **No anonymous-namespace types in headers.**

---

## Hands-on

```bash
cd 24-COMPILATION-LINKING/examples/02_odr_violation && ./build.sh
```

DEMO 1: linker "multiple definition of venue_name()". DEMO 2: plain link OK with
two `sizeof(Config)`; `-flto -Wodr` catches it. Phir DEMO 1 ko `inline` se fix
karo, DEMO 2 ke `Config` ko match karke silent bug hatao.

---

## ⚠️ Traps

### Trap 1 — non-inline definition in header
```cpp
// util.hpp
int g_seq = 0;               // ⚠️ multiple definition
inline int g_seq = 0;        // ✅ (C++17 inline var)  OR  extern + one .cpp def
```

### Trap 2 — `inline` function ki body do headers mein alag
```cpp
// fast.hpp:   inline int score(int x){ return x*2; }
// slow.hpp:   inline int score(int x){ return x*3; }   // ⚠️ IFNDR — linker ek rakhega
```

### Trap 3 — inconsistent macro affecting a type/inline fn
```cpp
#ifdef PROD
inline constexpr int kDepth = 32;
#else
inline constexpr int kDepth = 8;
#endif
// agar different TUs different PROD -> different kDepth -> ODR (if odr-used in inline ctx)
```

### Trap 4 — `-D_GLIBCXX_DEBUG` sirf kuch TUs pe
`std::vector`/`std::string` ka layout badal jata → any TU boundary passing STL
containers → crashes.

### Trap 5 — anonymous namespace type in a header
```cpp
// cfg.hpp
namespace { struct Tag {}; }         // ⚠️ har TU ko distinct Tag
inline void reg(Tag);                 // ODR: inline fn signature har TU mein alag type
```

### Trap 6 — "link succeeds → no ODR problem"
Kism 2 silent hai. `static_assert`/`-Wodr`/consistent flags chahiye.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "ODR = linker error" | Kism 1 haan; Kism 2 silent (IFNDR) — runtime garbage |
| "template/inline in header = ODR violation" | Nahi — COMDAT/vague linkage; *identical* copies merge fine |
| "class define karna definition nahi" | Class definition **hai** — rule 2 (identical across TUs) lagta |
| "link ho gaya to types match karte hain" | Struct layout mismatch link pe invisible; `-Wodr`/`static_assert` chahiye |
| "`#ifdef` in a struct is fine if guarded" | Guard double-inclusion rokta; different *flags* per TU still → ODR |
| "constexpr variable header mein daalna risky" | `constexpr`/`inline` var ODR-safe (inline implied) |

---

## Exercises

1. **Classify the fix:** `graph.hpp` mein `std::vector<int> topo_order(const
   Graph&) { ... }` (full body). Three TUs include it. Error, and the 3 possible
   fixes.

   <details><summary>Answer</summary>

   Link error: `multiple definition of topo_order(Graph const&)`. Fixes: (a) mark
   it `inline`; (b) leave only `std::vector<int> topo_order(const Graph&);` in the
   header, put the body in `graph.cpp`; (c) if it's genuinely a helper meant to be
   TU-local, `static` / anonymous namespace (but then each TU has its own copy —
   usually you want (a) or (b)).
   </details>

2. **Silent or loud:** for each, does the toolchain diagnose it? (a) `struct X`
   with different members in two TUs, (b) non-inline `int f(){...}` in a header
   included twice, (c) `inline int f(){ return 1; }` vs `inline int f(){ return
   2; }` in two headers, (d) a class with a member function defined differently
   in two TUs.

   <details><summary>Answer</summary>

   (a) silent (IFNDR) — unless `-flto -Wodr`. (b) loud — `multiple definition`.
   (c) silent — weak symbol, linker keeps one. (d) silent — member function is
   COMDAT, one kept arbitrarily.
   </details>

3. **`_GLIBCXX_DEBUG` trap:** `libfeed.a` built with `-D_GLIBCXX_DEBUG`, your app
   without. Both pass `std::vector<Tick>` across the boundary. Symptom + root
   cause + fix.

   <details><summary>Answer</summary>

   Symptom: crashes / corruption when the app touches a vector created by the
   library (or vice versa). Root cause: `_GLIBCXX_DEBUG` changes `std::vector`'s
   layout and iterator types → two incompatible `std::vector<Tick>` → ODR
   violation across the boundary. Fix: build everything (incl. the library) with
   the *same* STL flags; never mix `_GLIBCXX_DEBUG`.
   </details>

4. **Guard yourself:** a `struct MdPacket` must match a 48-byte wire layout and be
   identical in every TU. Three lines to add.

   <details><summary>Answer</summary>

   `static_assert(std::is_standard_layout_v<MdPacket>);`
   `static_assert(sizeof(MdPacket) == 48);`
   `static_assert(offsetof(MdPacket, price) == 16);` (and similar for each field).
   Put them right below the struct, in the one shared header.
   </details>

5. **Why does templates-in-headers work but functions-in-headers doesn't?**

   <details><summary>Answer</summary>

   A plain function definition emits a *strong* symbol → two of them collide.
   Template instantiations (and `inline` functions) emit *weak / COMDAT* symbols;
   the linker is told "there may be many identical copies, keep one, drop the
   rest." That's allowed precisely because ODR rule 2 promises they're identical.
   </details>

---

## Interview questions

1. ODR ke teen niyam — apne shabdon mein.
2. "multiple definition" vs silent ODR (IFNDR) — kaunsా kab, kaise pakdein?
3. Templates/`inline` functions headers mein kyun ODR-safe (COMDAT / vague linkage)?
4. `-D_GLIBCXX_DEBUG` sirf kuch TUs pe kyun disaster?
5. `-Wodr` kya karta hai, kaise (`-flto` kyun chahiye)?
6. Ek `struct` mein `#ifdef` member + inconsistent flags = ? Kaise bachein?
7. Anonymous-namespace type header mein — kya problem?

---

## Next
→ [`05-linkage.md`](05-linkage.md)
