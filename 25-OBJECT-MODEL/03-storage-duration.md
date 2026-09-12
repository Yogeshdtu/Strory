# 03 — Storage duration: automatic, static, thread, dynamic

## Prerequisites
- `02-object-lifetime.md`, `24-COMPILATION-LINKING` file 06 (storage specifiers)
- `14-MEMORY` (stack/heap/bss/data)

## Yeh topic abhi kyun
Storage duration = ek object ki storage **kab allocate** hoti aur **kab release**.
Yeh lifetime se alag hai (file 02) par usse bandhi hai. Chaar categories hain, aur
har ek ka apna init timing, cost, aur thread-safety story hai — jo hot-path design
directly affect karta.

---

## Chaar storage durations

| Duration | Allocate | Release | Kaun (default) | Storage |
|---|---|---|---|---|
| **Automatic** | scope enter (frame) | scope exit | local variables, function params, temporaries | stack frame |
| **Static** | program start (ya first-use) | program end | namespace-scope vars, `static` locals, class `static` members | `.data` / `.bss` |
| **Thread** (`thread_local`) | thread start (ya first-use) | thread end | `thread_local` vars | TLS block per thread |
| **Dynamic** | `new` / `operator new` / `malloc` | `delete` / `operator delete` / `free` | heap objects | heap |

`static` keyword ka context-dependent meaning aur `thread_local` cost — folder 24
file 06 mein detail. Yeh file object-model angle se.

---

## Automatic

```cpp
void f() {
    int x = 5;              // storage: frame allocate hone pe; lifetime: yahin se
    Widget w;               // ctor yahan; dtor scope-end pe (reverse order)
    {
        Buffer b(4096);
    }                        // b ka storage + lifetime yahan khatam
}                            // w, x ka storage + lifetime yahan khatam
```

- **Cost:** effectively zero — frame ek `sub rsp, N` se allocate hota, sab locals
  ke liye ek saath. Trivial objects ke liye koi init instruction bhi nahi.
- **Reverse destruction order** — jo baad mein bana, pehle destruct (stack discipline).
- **Temporaries** bhi automatic — full-expression ke end pe destruct (file 05).
- Recursion → har call ka apna frame → apni copy of locals.

## Static

```cpp
int g_count;                       // .bss (zero-init), static duration
const char* g_name = "engine";     // .data (const-init) — pointer + string in .rodata
static int g_id = compute_id();    // .bss slot + a dynamic initializer run before main

int next() { static int n = 0; return ++n; }   // function-local static
```

- **Constant initialization** → value binary mein baked (`.data`/`.rodata`), ya
  zero → `.bss` (file mein 0 bytes). No runtime cost.
- **Dynamic initialization** (initializer koi constant expr nahi) → compiler ek
  init function emit karta, `.init_array` mein register → `main` se pehle chalta.
  Order: ek TU mein source order; **cross-TU unspecified** (fiasco — file 04).
- **Function-local static** → first call pe init, **thread-safe guard**
  (`__cxa_guard_acquire` — ek atomic + a byte). Uske baad ek cheap guard-byte
  check. Destruction: program exit pe (`__cxa_atexit`), reverse order.

## Thread (`thread_local`)

```cpp
thread_local std::mt19937 t_rng{seed()};      // har thread ka apna
thread_local int t_errno = 0;
```

- Har thread apna instance. Thread start pe construct (ya first use), thread exit
  pe destruct.
- **Access cost:** ek TLS lookup — x86-64 Linux pe `%fs:offset` (initial-exec) ya
  `__tls_get_addr` call (general-dynamic, `.so`s mein). Local variable jitna free
  nahi. **Hot loop mein reference ko ek local mein cache karo.**
- `constinit thread_local` → per-access "initialized yet?" guard skip.

## Dynamic

```cpp
Widget* w = new Widget(...);       // operator new (heap) + ctor
delete w;                           // dtor + operator delete
auto up = std::make_unique<Widget>(...);   // same, RAII-managed
```

- **Cost:** `operator new` = `malloc`-jaisa — fast path ek freelist pop, slow path
  `mmap`/`brk` + lock (folder 14). **Deterministic latency ki dushman.**
- Aap decide karte kab allocate/free — flexibility + responsibility (leak, UAF,
  double-free).
- HFT: hot path pe **zero dynamic allocation** — pre-allocate at startup, pools.

---

## Storage duration ≠ lifetime (recap)

```cpp
alignas(Widget) unsigned char buf[sizeof(Widget)];   // AUTOMATIC storage (scope)
Widget* w = ::new (buf) Widget(...);                  // Widget lifetime: ctor..dtor
w->~Widget();                                          //   — shorter than buf's storage
```

`buf` ki storage duration automatic hai (scope). Usme jo Widget hum banate hain
uski lifetime hum control karte — buf ki storage duration se independent.

Dynamic storage bhi: `operator new` se raw storage, phir placement new se object,
phir explicit dtor, phir `operator delete` — yeh `std::vector` / allocators
internally karte.

---

## Andar kya hota hai

- **Automatic** → frame layout compile-time fixed. `-O2` pe unused locals gayab,
  live ranges overlap (ek stack slot do non-overlapping locals ke liye).
- **Static const-init** → linker `.data`/`.bss`/`.rodata` mein rakhta. **Dynamic-
  init** → `_GLOBAL__sub_I_<tu>` function `.init_array` mein.
- **Function-local static** → hidden guard variable (`_ZGV...`), pehli call:
  `if (guard acquired) { ctor; guard = 1; }` with an atomic acquire. Subsequent:
  `if (guard) return &obj;` — one load.
- **`thread_local`** → `.tdata`/`.tbss` template; har thread start pe copy/zero;
  access via TLS ABI.
- **Dynamic** → `operator new` (replaceable) → `malloc` → allocator (ptmalloc,
  tcmalloc, jemalloc). Alignment: `operator new` `__STDCPP_DEFAULT_NEW_ALIGNMENT__`
  (usually 16) tak guarantee; over-aligned → aligned `operator new` (file 08).

---

## > **HFT relevance**
> - **Automatic** — free, deterministic, cache-local (frame is hot). Prefer stack
>   objects; `alloca`/VLA-style for bounded scratch (careful with size).
> - **Static** — use `constinit` for config/const globals (compile-time init, no
>   guard, no fiasco — file 04). Avoid non-trivial global constructors (run before
>   `main`, unspecified order, can pull surprising work into startup).
> - **`thread_local`** — per-thread RNG, arena pointer, "current context" — but
>   cache the address in hot loops (TLS lookup tax).
> - **Dynamic** — **not on the hot path.** Pre-allocate everything at startup;
>   `mlock` + touch pages during warmup so first access doesn't page-fault; pools
>   / arenas for anything that "grows" (folder 36). A single `new` in the tick
>   loop is a P99 spike waiting to happen.

---

## Hands-on

```bash
./build.ps1 25-OBJECT-MODEL/examples/01_lifetime_demo.cpp        # storage vs lifetime
./build.ps1 24-COMPILATION-LINKING/examples/09_linkage_storage.cpp   # all four, -pthread
```

- `g++ -O2 -S file.cpp` — find the frame `sub rsp, N` (automatic), the
  `__cxa_guard_acquire` around a function-local `static`, the `_GLOBAL__sub_I_`
  init function.
- A `thread_local int` incremented in a loop vs a cached `int& r = t;` — time both
  at `-O2`.

---

## ⚠️ Traps

### Trap 1 — non-trivial global constructor
```cpp
std::map<std::string,int> g_table = load();   // ⚠️ runs before main, unspecified order, may allocate
```
Construct-on-first-use, `constinit` (if possible), or init explicitly in `main`.

### Trap 2 — `thread_local` in a hot loop
```cpp
for (...) t_arena.alloc(n);           // ⚠️ TLS lookup per iteration
auto& a = t_arena; for (...) a.alloc(n);   // ✅
```

### Trap 3 — returning a pointer/ref to an automatic
```cpp
int* bad() { int x = 0; return &x; }   // ⚠️ x's storage gone at return
```

### Trap 4 — assuming static local destruction order
Reverse of construction, at exit. A static local that depends on another during
its destructor can hit a destruction-order fiasco (file 04).

### Trap 5 — dynamic allocation for something bounded
```cpp
auto* buf = new char[64];   // ⚠️ 64 bytes — just use `char buf[64];`
```

### Trap 6 — `thread_local` object with a heavy constructor
Constructed per thread on first use — a pool of 64 threads each paying that cost.
Keep `thread_local` objects cheap or `constinit`.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "static duration = initialized at program start" | Const-init: yes. Dynamic-init: before `main`, unspecified cross-TU order |
| "function-local static has no cost after init" | One guard-byte check per call (cheap, but non-zero) |
| "`thread_local` access ≈ local variable" | TLS lookup — cache the address in hot code |
| "dynamic storage is fine if you `delete` it" | Latency non-determinism + fragmentation; keep off the hot path |
| "storage duration = lifetime" | `char buf[]` + placement new: storage automatic, lifetime yours |
| "globals init in source order" | Within a TU yes; across TUs unspecified |

---

## Exercises

1. **Classify:** `static int a;` (file scope), `int b;` (in a function), `static
   int c;` (in a function), `thread_local int d;`, `int* e = new int;`.

   <details><summary>Answer</summary>

   `a` — static duration (internal linkage). `b` — automatic. `c` — static
   duration (one instance, first-use init). `d` — thread duration. `*e` — dynamic
   duration (the pointee); `e` itself is automatic.
   </details>

2. **Init cost:** which of these run code before `main`? `int g1 = 5;`, `int g2 =
   rand();`, `const char* g3 = "x";`, `std::string g4 = "x";`

   <details><summary>Answer</summary>

   `g1`, `g3` — constant initialization, baked into `.data`/`.rodata`, no code.
   `g2` — dynamic init (call `rand()`), runs before `main`. `g4` — dynamic init
   (`std::string` ctor, may allocate), runs before `main`.
   </details>

3. **Guard:** where does `__cxa_guard_acquire` appear, and how do you avoid it for
   a hot-path accessor?

   <details><summary>Answer</summary>

   Around the first-time initialization of a function-local `static` with a
   non-constant initializer. Avoid: make it `constinit`/const-initializable, or
   don't use a function-local static — set a `constinit` pointer during explicit
   init and read that, or pass the dependency in.
   </details>

4. **TLS tax:** a loop of 1e8 iterations does `t_counter += arr[i];` where
   `t_counter` is `thread_local`. Rough fix and why.

   <details><summary>Answer</summary>

   `long local = t_counter; for (...) local += arr[i]; t_counter = local;` — hoist
   the TLS lookup out of the loop. Each iteration's `thread_local` access is a TLS
   address computation the compiler can't always fold away; a plain local lives in
   a register.
   </details>

5. **Design:** a per-connection scratch buffer, 4 KB, used every message on a
   16-thread server. Automatic, `thread_local`, or dynamic?

   <details><summary>Answer</summary>

   `thread_local std::array<std::byte, 4096>` (or a `thread_local` arena) — one
   per thread, reused across messages, no per-message allocation, no contention.
   Automatic would re-create it per call (fine if cheap, but a `thread_local`
   avoids even that); dynamic adds allocation latency and a free path.
   </details>

---

## Interview questions

1. Chaar storage durations — allocate/release timing.
2. Automatic storage ki cost (≈ zero) — kyun?
3. Static: const-init vs dynamic-init — kya farq, order guarantee?
4. Function-local static: init timing, thread-safety, per-call cost.
5. `thread_local` access cost — hot loop mein kya karo?
6. Dynamic storage HFT hot path pe kyun avoid?
7. Storage duration vs lifetime — ek concrete example.

---

## Next
→ [`04-initialization-order.md`](04-initialization-order.md)
