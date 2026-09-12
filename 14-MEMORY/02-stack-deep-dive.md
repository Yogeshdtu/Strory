# 02 — Stack deep dive

## Prerequisites
- [`01-memory-layout-revisited.md`](01-memory-layout-revisited.md)
- Folder 08 file 05 (the call stack — prologue/epilogue, frame addresses)
- Folder 04 file 05 (recursion) aur folder 08 file 05 example (stack overflow crash)

## Yeh topic abhi kyun
Stack C++ ki **default, fastest** memory hai — har local, har call frame yahin.
Iske rules (grows down, scope-bound, chhota, fixed limit) samajhna zaroori taaki
aap jaan sako kab stack kaafi hai aur kab heap chahiye — aur stack overflow ko
pehchan sako.

---

## Stack frame — recap + detail

Har function call ek **frame** push karta hai (folder 08):

```
   higher address
   ┌────────────────────────┐
   │ caller ka frame        │
   ├────────────────────────┤
   │ arguments (extra)      │   \
   │ return address         │    |  ek frame
   │ saved rbp              │    |  (callee ka)
   │ local variables        │    |
   │ saved registers        │    |
   │ (alignment padding)    │   /
   ├────────────────────────┤  <- rsp (stack pointer) yahan
   │ ... free stack ...     │
   ▼ grows DOWN (lower addr)
```

- **Call** → return address push, `call` jump.
- **Prologue** → `push rbp; mov rbp, rsp; sub rsp, <frame size>` (locals ke liye
  jagah).
- **Epilogue** → `mov rsp, rbp; pop rbp; ret` (frame instantly gaya).

**"Allocation" = `sub rsp, N`** — ek instruction, N compile-time constant. Isiliye
stack itna sasta hai ([`examples/02_stack_vs_heap.cpp`](examples/02_stack_vs_heap.cpp):
stack per-iter ~0.85 ns vs heap ~71 ns → **~83x**).

**"Deallocation" = `add rsp, N`** (ya `mov rsp, rbp`) — bhi ek instruction. Frame
ke saare locals ek saath "gaye" — koi per-object cleanup nahi (destructors alag
baat hain, woh compiler epilogue se pehle daalta hai).

---

## Grows down — aur kyun matter karta hai

x86/ARM pe stack **high address se low address** ki taraf badhta hai. Naya frame
= chhota address.

```cpp
void deep(int n) {
    int marker = n;
    std::cout << n << " : &marker = " << &marker << "\n";
    if (n > 0) deep(n - 1);
}
deep(5);
// har call ka &marker PICHHLE se KAM hota hai (~frame size ka gap)
```

Consequence: ek local array overflow karo, to aap **niche wale** (agle) frame
ko nahi, balki apne hi frame ke doosre locals / saved rbp / return address ko
corrupt karte ho (buffer address se upar likhte ho jab index positive badhta —
depends on layout). Classic stack-smashing.

---

## Stack ki size limit — fixed hai

| OS | Default main-thread stack | Badalne ka tareeka |
|---|---|---|
| Linux | 8 MB (`ulimit -s`) | `ulimit -s`, `setrlimit`, linker `-z stacksize` |
| macOS | 8 MB (main), 512 KB (threads) | `pthread_attr_setstacksize` |
| Windows | 1 MB (default) | linker `/STACK`, `CreateThread` param |

Threads ka stack alag aur aksar chhota (Linux default 8 MB bhi, par settable;
Windows 1 MB). **Stack RAM ke hisaab se nahi badhta** — yeh ek fixed reservation
hai. Cross karo → crash.

---

## Stack overflow — kab, kaise dikhta

```cpp
void infinite(int n) { int buf[256]; buf[0] = n; infinite(n + 1); }  // no base case
infinite(0);
// ~ (8 MB) / (256*4 + frame overhead) ≈ 7000-8000 calls -> SIGSEGV
```

Ya ek hi bade local se:

```cpp
void f() { int big[3'000'000]; big[0] = 1; }   // ~12 MB > 8 MB -> crash on first touch
```

Symptoms:
- **SIGSEGV** (Linux) / **stack overflow exception `0xC00000FD`** (Windows).
- Debugger mein: recursion ki hazaaron frames (agar recursion), ya crash `f` ke
  prologue mein (`sub rsp` ke turant baad pehla write).
- `-fsanitize=address` → `stack-overflow`. `-fstack-protector` deep locals ke
  overflow (smashing) pe `__stack_chk_fail` — par yeh **overflow-past-limit**
  se alag hai (woh guard page hit karti hai).

Folder 08 file 05 ka example (`05_stack_overflow.cpp`) yeh deliberately karta
hai (progress print karta hua).

---

## Kya stack pe rakhein, kya nahi

✅ **Stack pe rakho:**
- Chhote-medium locals, fixed size (`int`, structs, `std::array<T, small N>`).
- Function ke andar hi jeene wali cheezein.
- Jo cheez aap by-value return karoge (RVO — folder 08).

❌ **Stack pe mat rakho:**
- Bade buffers (>~100 KB ko already socho; MBs = definitely nahi).
- Runtime-size arrays jo bade ho sakte hain. (C++ mein VLA hai bhi nahi standard —
  `int a[n]` with runtime `n` GCC extension hai, avoid — `std::vector`.)
- Lifetime jo scope se lambi ho (return ke baad chahiye → heap / caller-owned).

Grey zone: recursion. Har frame chhota rakho, aur depth bound karo — warna
iterative + explicit stack/`std::stack` (folder 20).

---

## `alloca` — mat use karo (mostly)

```cpp
#include <alloca.h>
void f(size_t n) {
    int* p = (int*)alloca(n * sizeof(int));   // stack pe runtime-size block
    // ...  (function return pe automatic free)
}
```

- Fast (rsp adjust), koi free nahi.
- Par: **no bounds**, `n` bada → silent stack overflow; kuch platforms pe flaky;
  loop ke andar `alloca` = leak-until-return.
- Modern C++: `std::vector` (heap par safe), ya chhoti known upper bound ke liye
  `std::array` + "used count", ya libraries ka `small_vector`.

---

## Andar kya hota hai

- **Stack pages lazily map hoti hain.** OS ek region *reserve* karta hai, par
  physical pages pehli touch pe deti hai. Ek **guard page** region ke end pe —
  usse touch karo → fault → OS "stack overflow" signal (ya thoda grow, platform
  pe depend).
- **Red zone** (System V x86-64): leaf functions `rsp` ke neeche 128 bytes bina
  adjust kiye use kar sakte hain. Windows x64 mein red zone nahi.
- **Frame size compile time pata hai** (usually) → ek `sub rsp, N`. Dynamic
  parts (`alloca`, VLA) ise variable bana dete hain → extra bookkeeping,
  optimizer ko dikkat.
- **Cache:** stack top hamesha hot (har call touch karta) → L1 mein. Isiliye
  stack access practically free — data already cache mein.

> **HFT relevance:** hot path ka working set jitna zyada stack (aur chhote
> `std::array` members) pe ho, utna accha — woh L1-resident, zero-alloc, aur
> branch-free lifetime. Bade fixed buffers `static`/pool mein (stack limit +
> re-entrancy), par chhote per-event scratch structs stack pe. Deep call chains
> aur recursion hot path pe avoid — frame setup + I-cache pressure. `alloca`/VLA
> hot code mein nahi (unpredictable, optimizer-hostile).

---

## Hands-on

```bash
./build.ps1 fast 14-MEMORY/examples/02_stack_vs_heap.cpp          # ~83x measured
./build.ps1 08-FUNCTIONS/examples/05_stack_overflow.cpp           # deliberate crash (folder 08)
```

Aur khud: ek recursive function jo har call pe `&local` print kare — addresses
ghatte hue dekho, aur ~kitni depth pe crash hota (frame size se divide karke
estimate check karo).

---

## ⚠️ Traps

### Trap 1 — bada local array
```cpp
void f() { double m[500000]; }   // ⚠️ ~4 MB -- limit ke paas/paar. vector/static
```

### Trap 2 — unbounded recursion
```cpp
long fib(long n) { return fib(n-1) + fib(n-2); }   // ⚠️ no base case -> stack overflow
```

### Trap 3 — "stack RAM ke saath badhta hai"
Nahi. Fixed reservation (Linux 8 MB, Windows 1 MB). 16 GB RAM se koi fark nahi.

### Trap 4 — thread stack ko main jaisa maanna
```cpp
std::thread t([]{ char buf[2'000'000]; });   // ⚠️ thread stack chhota ho sakta (Win 1 MB) -> crash
```

### Trap 5 — `alloca` loop mein
```cpp
for (...) { void* p = alloca(1024); }   // ⚠️ har iteration stack grow, function-end tak free nahi
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Stack allocation ka bhi kuch cost hai" | ~1 instruction (`sub rsp, N`) — practically free |
| "Stack utna bada ho sakta jitni RAM" | Fixed limit (MBs). Cross = crash |
| "Overflow niche wale frame ko corrupt karta" | Apne frame ke locals / return addr (stack smashing) |
| "Har thread ka stack main jaisa 8 MB" | Platform/config pe depend — aksar chhota |
| "`alloca` / VLA safe convenient hain" | No bounds, overflow risk, optimizer-hostile — `vector` |

---

## Exercises

1. **Grows down proof:** recursive `void d(int n)` jo `&n` (ya `&local`) print
   kare, `d(5)`. Consecutive addresses ka difference — approx frame size? Ghat
   raha ya badh raha?

   <details><summary>Answer</summary>

   Addresses **ghat** rahe (down growth). Consecutive difference ≈ frame size
   (locals + saved regs + return addr + alignment).
   </details>

2. **Overflow depth:** `void r(int n){ int buf[256]; buf[0]=n; if(n%1000==0)
   print(n); r(n+1); }` — kis depth ke aas-paas crash? `8 MB / (256*4 + ~48)`
   se match?

   <details><summary>Answer</summary>

   Frame ≈ 1024 + overhead ≈ ~1080 bytes → 8 MB / 1080 ≈ ~7700 calls (Linux).
   Windows (1 MB) pe ~950. Machine pe thoda vary.
   </details>

3. **Big local:** `int a[N];` in a function, `a[0]=1; print(&a);` — `N` ko
   100000 se badha ke 5000000 tak. Kis N pe crash? Limit se relate karo.

   <details><summary>Answer</summary>

   Crash ~jab `N * 4` bytes stack limit ke paas (Linux ~8 MB → N ≈ 2M; Windows
   ~1 MB → N ≈ 250K). Crash pehle write (first touch) pe.
   </details>

4. **Thread stack:** `std::thread` mein ek 900 KB local buffer. Windows pe
   chalega? `pthread_attr_setstacksize` / `std::thread` ke platform default?

   <details><summary>Answer</summary>

   Windows default thread stack 1 MB → 900 KB borderline (frame overhead ke saath
   crash ho sakta). Linux default 8 MB → theek. Portable code stack pe bade
   buffers avoid kare.
   </details>

5. **`sub rsp` dekho:** ek function `void f(){ int a[100]; a[0]=1; escape(a); }`
   ka `-O2 -S` assembly — prologue mein `sub rsp, <?>` kitna? 100*4 = 400 se
   match (+ alignment)?

   <details><summary>Answer</summary>

   `sub rsp, 416` ya similar — 400 bytes array + 16-byte alignment. Ek hi
   instruction se poora array "allocate".
   </details>

---

## Interview questions

1. Stack frame mein kya-kya hota? Prologue/epilogue kya karte?
2. Stack "allocation" kitna mehnga — kyun?
3. Stack kis direction grow karta, aur iska buffer-overflow pe kya matlab?
4. Stack size limit — typical values, badalne ke tareeke, RAM se relation?
5. Stack overflow kaise dikhta (signal, debugger)? `-fstack-protector` vs guard page?
6. `alloca` / VLA kyun avoid — 3 reasons?

---

## Next
→ [`03-heap-deep-dive.md`](03-heap-deep-dive.md)
