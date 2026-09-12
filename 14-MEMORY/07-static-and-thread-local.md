# 07 — Static storage & `thread_local`

## Prerequisites
- [`01-memory-layout-revisited.md`](01-memory-layout-revisited.md)
- Folder 08 (functions) — function-local `static`

## Yeh topic abhi kyun
Kuch cheezein poore program tak zinda rehni chahiye — ek config, ek logger, ek
lookup table. Unke liye **static storage duration** hai (`.data`/`.bss`). Aur
per-thread state ke liye `thread_local`. Inke initialization rules mein ek
classic bug hai (**static init order fiasco**) jo samajhna zaroori.

---

## 4 storage durations — recap

| Duration | Kaise banta | Kab | Region |
|---|---|---|---|
| **automatic** | local variable | scope enter → exit | stack |
| **dynamic** | `new` | `new` → `delete` | heap |
| **static** | global, `static` local, `static` member | program start → end | `.data`/`.bss` |
| **thread** | `thread_local` | thread start → thread end | per-thread block |

Yeh lesson pichhle do (stack, heap) ke baad **static** aur **thread** cover
karta hai.

---

## Static storage duration — teen roop

```cpp
int g_config = 10;                 // 1. namespace-scope global -> static duration

struct Widget {
    static int s_count;            // 2. static data member -> static duration
};                                 //    (definition: int Widget::s_count = 0;  ek .cpp mein)

int nextId() {
    static int counter = 0;        // 3. function-local static -> static duration,
    return ++counter;              //    par visibility sirf function mein
}
```

- Sab `.data` (non-zero init) ya `.bss` (zero init) mein.
- **Ek hi copy** poore program mein.
- Lifetime: `main` se pehle (ya pehli use se — #3) → `main` ke baad destroy.

### `inline` variables (C++17) — header mein define

```cpp
// config.hpp
inline int g_shared = 42;          // har TU dekhe, par ek hi object (ODR-safe)
struct S { static inline int x = 0; };   // static member, header mein hi define
```

C++17 se pehle `static` member ko ek `.cpp` mein define karna padta tha. `inline`
ne yeh header-only bana diya.

---

## Initialization — 2 phases

### Phase 1: static initialization (compile/load time)

- **Zero-initialization:** sab static objects pehle 0 (yeh `.bss`).
- **Constant initialization:** jinke initializer compile-time constant hain
  (`constexpr`, literals) → binary mein hi value baithi hai. Zero cost at runtime.

```cpp
int a = 5;                 // constant-init -> .data mein 5, ready
int b[1000] = {};          // zero-init -> .bss
constexpr int c = f(3);    // f constexpr -> compile time
```

### Phase 2: dynamic initialization (runtime, `main` se pehle)

Jinke initializer runtime pe hi compute ho sakte:

```cpp
int d = std::time(nullptr);          // dynamic -- runtime call
std::string e = "hello";             // dynamic -- constructor
const std::vector<int> table = build();   // dynamic
```

Yeh `main` shuru hone se pehle chalti hai (ek hi TU ke andar **top-to-bottom
order**). **Alag TUs ke beech order UNSPECIFIED** — yahi fiasco.

---

## ⚠️ Static initialization order fiasco

```cpp
// a.cpp
std::string prefix = "LOG: ";

// b.cpp
extern std::string prefix;
std::string banner = prefix + "started";   // ⚠️ prefix abhi construct hua ya nahi? UNSPECIFIED
```

Agar `b.cpp` ka dynamic init `a.cpp` ke pehle chala → `prefix` abhi empty/garbage
(zero-initialized, constructor nahi chala) → `banner` galat, ya crash.

### Fix: "construct on first use" (function-local static)

```cpp
// a.cpp
const std::string& prefix() {
    static const std::string p = "LOG: ";   // pehli call pe construct, phir cached
    return p;
}

// b.cpp
const std::string& banner() {
    static const std::string b = prefix() + "started";   // prefix() guaranteed constructed
    return b;
}
```

Function-local `static` **pehli baar function call hone pe** initialize hota hai
(C++11 se **thread-safe** — compiler ek guard variable + lock daalta). Order
ab call order se determined, unspecified nahi.

⚠️ **Destruction order** bhi: static objects reverse-order-of-construction mein
destroy hote (`main` ke baad). Ek static ka destructor doosre (already-destroyed)
static ko touch kare → **static destruction order fiasco**. Local-static +
"leak on purpose" (`static T& x = *new T;` — never destroyed) kuch cases mein
use hota.

---

## `thread_local`

Har thread ki **apni copy**:

```cpp
thread_local int t_requestCount = 0;      // har thread ka alag counter
thread_local std::string t_scratch;       // har thread ka alag buffer

void handle() {
    ++t_requestCount;                     // sirf is thread ka
}
```

- Main thread ki copy `.tdata`/`.tbss` se; naye threads ko start pe apna block
  milta (aur destructors thread end pe).
- **Dynamic-init `thread_local`** har thread mein pehli use pe init hota (guard
  check har access pe — ya pehle access pe; chhota overhead).
- Use cases: per-thread scratch buffers (koi lock nahi), per-thread RNG, error
  state (`errno` thread_local hai), per-thread pools/arenas.

```cpp
// per-thread reusable buffer -- no allocation in hot loop, no sharing
thread_local std::vector<char> t_buf;
void process(std::span<const char> in) {
    t_buf.clear();
    t_buf.insert(t_buf.end(), in.begin(), in.end());   // reuses capacity across calls
    // ...
}
```

---

## Andar kya hota hai

- **Constant-init statics** → binary ki `.data`/`.rodata` mein value; load pe
  `mmap`, zero runtime init.
- **Zero-init** → `.bss`, demand-zero pages.
- **Dynamic-init** → compiler har TU ke liye ek `__static_initialization...`
  function banata; unke pointers ek `.init_array` section mein; CRT startup
  (`__libc_csu_init`) `main` se pehle sab call karta.
- **Function-local static** → hidden `guard` byte (`__cxa_guard_acquire/release`).
  Pehli call: lock, init, guard set. Baad ki calls: ek atomic load check (branch,
  predictable) → practically free. Init me exception → guard reset, agli call
  phir try.
- **`thread_local`** → access ke liye ek indirection (TLS block base +
  offset; `%fs`/`%gs` segment register x86 pe). Thoda mehnga plain global se,
  par lock-free per-thread.

> **HFT relevance:** `thread_local` per-thread scratch/pool/RNG ke liye ideal —
> zero contention, zero per-call allocation (buffer capacity persist karti).
> Bade lookup tables / config → `static const` (constant-init jahan possible →
> zero startup cost, `.rodata` shared, cache-friendly). Static init order
> fiasco ko "construct on first use" se avoid; kuch shops startup pe hi
> explicitly warm/initialize order-controlled tareeke se karte (no reliance on
> cross-TU order). Hot path pe `thread_local` dynamic-init guard ka chhota cost
> bhi dhyaan mein — POD `thread_local` (guard-free) prefer.

---

## Hands-on

```cpp
// order.cpp -- fiasco + fix
#include <iostream>
#include <string>

std::string A = "A";                       // TU-order dynamic init
std::string B = A + "B";                    // depends on A (same TU -> ok here)

const std::string& C() { static std::string c = "C"; return c; }   // first-use

int main() { std::cout << B << " " << C() << "\n"; }
```

```bash
g++ -std=c++20 -Wall -Wextra order.cpp -o order && ./order   # "AB C"
```

Aur: `thread_local int t;` ko 3 threads se `++t` karwao, har thread apna value
print kare — sab independent.

---

## ⚠️ Traps

### Trap 1 — cross-TU static dependency
```cpp
// x.cpp: Config cfg = load();
// y.cpp: int v = cfg.value;   // ⚠️ cfg construct hua ya nahi? unspecified
```

### Trap 2 — static destructor doosre static ko touch
```cpp
struct Logger { ~Logger() { registry().remove(this); } };   // ⚠️ registry() already destroyed?
```

### Trap 3 — `thread_local` ko "sab threads share karte" samajhna
Ulta — **har thread ki alag copy**. Sharing ke liye plain global + sync.

### Trap 4 — non-trivial `thread_local` hot loop mein
```cpp
thread_local std::map<K,V> cache;   // har access pe init-guard check + non-trivial
```

### Trap 5 — function-local static thread-safety pe C++03 assume
C++11+ mein thread-safe (guarded). C++03 / `-fno-threadsafe-statics` pe race.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Global variables heap pe" | `.data`/`.bss` — static storage |
| "Static init order source order follow karta" | Ek TU mein haan; TUs ke beech **unspecified** |
| "`static` local har call init hota" | Sirf pehli call; C++11 se thread-safe |
| "`thread_local` = shared across threads" | Per-thread copy — opposite of shared |
| "Constant-init aur dynamic-init same cost" | Constant: zero runtime. Dynamic: `main` se pehle chalta |

---

## Exercises

1. **Classify init:** har ek — constant-init, zero-init, ya dynamic-init?
   `int a = 5;`, `int b;` (global), `std::string s = "x";` (global),
   `constexpr int c = 2*3;`, `int d = rand();` (global), `int e[100] = {};`

   <details><summary>Answer</summary>

   `a` constant. `b` zero (.bss). `s` dynamic (ctor). `c` constant. `d` dynamic
   (runtime call). `e` zero.
   </details>

2. **Fiasco reproduce:** do `.cpp` files — ek mein `std::string P = "p";`, doosre
   mein `std::string Q = P + "q";` (with `extern`). Link, run kai baar. Kabhi
   galat? Fix "first use" se.

   <details><summary>Answer</summary>

   Link order / TU order pe depend — `Q` ka init `P` se pehle chal sakta →
   `Q == "q"` (P empty) ya crash. `const std::string& P()` first-use accessor se
   deterministic.
   </details>

3. **`thread_local` independence:** `thread_local int t = 0;` — 4 threads, har
   ek `t += tid; print(t);`. Values? Main thread ka `t`?

   <details><summary>Answer</summary>

   Har thread apna `t` (0 se shuru) → prints `tid`. Main thread ka `t` alag (0
   ya jo main ne kiya). Koi race nahi — separate storage.
   </details>

4. **Guard cost:** `int f(){ static int x = expensive(); return x; }` — pehli
   call vs 1e7 subsequent calls ka per-call cost (`-O2`). Guard overhead
   measurable?

   <details><summary>Answer</summary>

   Pehli call: `expensive()` + guard set. Baad ki: ek atomic/relaxed load +
   predictable branch (~sub-ns amortized). `-O2` pe often hoisted out of loops.
   </details>

5. **Destruction order:** 2 global objects `A`, `B` (`A` pehle defined). `~B`
   `A` ko use karta hai — safe? Ab reverse (`~A` uses `B`)?

   <details><summary>Answer</summary>

   Destruction = reverse of construction. `A` first constructed → last destroyed.
   `~B` uses `A`: `B` destroyed first, `A` still alive → safe. `~A` uses `B`: `B`
   already destroyed → UB (static destruction fiasco).
   </details>

---

## Interview questions

1. 4 storage durations — kaise banti, kahan, lifetime?
2. Static init ke 2 phases (static vs dynamic init)?
3. Static initialization order fiasco — kya, kyun, "construct on first use" fix?
4. Function-local `static` kab initialize hota, thread-safe kab se?
5. `thread_local` — storage, cost vs plain global, use cases?
6. Static **destruction** order fiasco — kaise banta?

---

## Next
→ [`08-allocation-cost.md`](08-allocation-cost.md)
