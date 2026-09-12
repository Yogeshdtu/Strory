# 01 — Memory layout revisited (ab code ke saath)

## Prerequisites
- Folder 12 (pointers), Folder 13 (references)
- `01-PROGRAMMING-BASICS/11-memory-basics.md` (pehla parichay)
- Folder 08 file 05 (call stack)

## Yeh topic abhi kyun
Ab tak "memory" ek dhundhla idea tha. Ab aap khud allocate/free karoge — to
pehle **exactly kahan kya rehta hai** yeh pin karna zaroori: har variable kis
region mein, kaun read-only, kaun grow karta hai, kiski lifetime kya. Yeh poore
folder ka naksha hai.

---

## Ek running process ki memory — 5 regions

```
  low address
  ┌───────────────────┐
  │  .text            │  machine code (functions). READ-ONLY, executable.
  ├───────────────────┤
  │  .rodata          │  string literals, const globals. READ-ONLY.
  ├───────────────────┤
  │  .data            │  initialized globals/statics (non-zero). read-write.
  ├───────────────────┤
  │  .bss             │  zero-initialized globals/statics. read-write.
  │                   │  (executable file mein 0 bytes — OS load pe zero karta hai)
  ├───────────────────┤
  │  heap        ↓↓↓  │  new / malloc. Program ke request pe UP grow karta hai.
  │      ...          │
  │   (huge gap)      │
  │      ...          │
  │  stack       ↑↑↑  │  locals, call frames, return addresses. DOWN grow karta hai.
  └───────────────────┘
  high address
```

⚠️ Yeh **classic Linux** picture hai. Windows (PE) pe segments ka aapsi order
thoda alag ho sakta hai, aur heap/stack ka relative position bhi
([`examples/01_memory_layout.cpp`](examples/01_memory_layout.cpp) run karke
apni machine pe dekho). **Concept** har jagah sach hai:

1. Code + constants → ek **read-only** region (likhne ki koshish = crash).
2. Globals/statics → ek **fixed-size read-write** region (program ke shuru se
   end tak zinda).
3. Heap → ek region jo aapke `new`/`malloc` pe **badhta** hai.
4. Stack → ek region jo function call/return se **ghatta-badhta** hai.

---

## Har variable kahan jaata hai

```cpp
#include <string>

int    g_count   = 5;        // .data   (non-zero initializer)
int    g_total   = 0;        // .bss    (zero -> file mein jagah nahi)
const  double PI  = 3.14159;  // .rodata (read-only)
static int s_hits = 100;      // .data   (internal linkage, wahi segment)

const char* g_name = "engine";  // pointer 'g_name': .data ;  "engine" bytes: .rodata

void f(int arg) {               // 'arg': stack (is call ka frame)
    int local = 10;             // stack
    static int calls = 0;       // .data  (function-local static -- static storage duration!)
    int* p = new int(7);        // 'p': stack ;  jis int ko p point karta hai: HEAP
    std::string s = "hi";       // 's' object: stack ;  agar string > SSO to uske chars: HEAP
    delete p;
}

int main() { f(1); }           // 'main' ka frame: stack ;  code: .text
```

| Cheez | Region | Lifetime |
|---|---|---|
| `g_count`, `s_hits`, `calls` (non-zero init) | `.data` | poora program |
| `g_total` (zero init) | `.bss` | poora program |
| `PI`, `"engine"`, `"hi"` literals | `.rodata` | poora program (read-only) |
| `f`, `main` code | `.text` | poora program (read-only) |
| `arg`, `local`, `p` (the pointer), `s` (the object) | stack | jab tak `f` ka frame zinda |
| `new int(7)` ka int, `std::string` ke heap chars | heap | jab tak aap `delete` / string destroy na karein |

**Key insight:** `int* p = new int(7);` — `p` **stack** pe hai (8 bytes), jis
int ko woh point karta hai woh **heap** pe. Do alag regions, do alag lifetimes.

---

## `.bss` ka trick — zero "free" hai

```cpp
int big[1'000'000];   // 4 MB of zeros
```

Yeh executable file ko 4 MB bada **nahi** karta. `.bss` mein sirf ek entry hoti
hai: "load pe yahan 4 MB zero-filled memory chahiye." OS demand-zero pages deta
hai. Isliye zero-initialized global arrays "muft" jaise lagte hain (jab tak aap
unhe touch na karo — tab page fault + physical page).

Non-zero initializer wala array (`int big[1'000'000] = {1, ...};`) `.data` mein
jaata hai aur file mein **poore 4 MB** leta hai.

---

## Stack vs heap — ek line ka farq

| | Stack | Heap |
|---|---|---|
| Kaun manage karta | compiler (automatic) | **aap** (`new`/`delete`) ya library (`vector`) |
| "Allocate" cost | ~0 (ek register move) | allocator ka kaam (file 08 — ~50-100+ ns, tail bahut bada) |
| Size | chhota (~1-8 MB, `ulimit`) | bada (GBs, RAM tak) |
| Lifetime | scope-bound (automatic) | aap decide karo (manual = leak/UAF ka risk) |
| Fragmentation | nahi | haan (file 09) |
| Access speed | fast (hot, cache mein) | pointer chase — cache miss ho sakta |

Rule of thumb: **jitna stack pe rakh sako, rakho.** Heap tab jab (a) size runtime
pe pata chale aur bada ho, (b) lifetime scope se lambi ho, (c) object bahut bada
ho (stack overflow risk — file 02).

---

## Andar kya hota hai

- **Segments ELF/PE headers se aate hain.** Loader file ko `mmap` karta hai:
  `.text` → read+exec page, `.rodata` → read-only, `.data` → private read-write
  (file se copy), `.bss` → demand-zero.
- **`.rodata` likhna = SIGSEGV.** `char* p = (char*)"hi"; p[0] = 'H';` → crash
  (page read-only hai). Isiliye `const char*` (aur `-Wwrite-strings`).
- **Heap `brk`/`mmap` se badhta hai.** Chhoti allocations ek bade `brk`-region
  se katti hain; badi allocations (>128 KB glibc) seedha `mmap` (file 03, 08).
- **Stack pages lazily map hoti hain.** Frame `rsp` ko ghata ke banta hai; naya
  page pehli baar touch pe fault → map. Guard page cross karo → stack overflow
  → SIGSEGV (file 02).
- **ASLR** har region ka base randomize karta hai → addresses har run pe alag.

> **HFT relevance:** memory-region awareness latency ka foundation hai. Hot data
> ko aise rakha jaata hai ki woh **ek known, pre-faulted, cache-resident** region
> mein ho — pre-allocated arrays / pools (heap se ek baar, phir reuse), ya bade
> `static`/`.bss` buffers jo startup pe touch (fault) kar diye jaate hain taaki
> hot path pe koi page fault na aaye. `new` ko hot path se hataya jaata hai
> kyunki uska region (general heap) shared, locked, aur fault-prone hai.

---

## Hands-on

```bash
./build.ps1 14-MEMORY/examples/01_memory_layout.cpp
```

Har segment ka address print hota hai. Do baar chalao — kaunse addresses same
(segments, ASLR ke bawajood is run mein fixed), kaunse har run alag? Linux pe:
`cat /proc/$(pgrep 01_memory_layout)/maps`.

---

## ⚠️ Traps

### Trap 1 — string literal ko modify
```cpp
char* p = (char*)"hello";
p[0] = 'H';                 // ⚠️ SIGSEGV -- .rodata read-only. `const char*` use karo
```

### Trap 2 — `new` ke pointer ko heap object samajhna
```cpp
int* p = new int(5);   // p STACK pe (8 bytes); *p HEAP pe (4 bytes)
```

### Trap 3 — bada array stack pe
```cpp
int buf[2'000'000];    // ⚠️ ~8 MB local -> stack overflow (file 02). heap/static/vector
```

### Trap 4 — `.bss` array "free" samajh ke touch karna
```cpp
static int huge[10'000'000];
for (auto& x : huge) x = 1;   // ab 40 MB physical pages fault ho gaye
```

### Trap 5 — local ka address return (region confusion)
```cpp
int* f() { int x = 1; return &x; }   // ⚠️ x stack pe, frame return pe gaya (folder 12/13)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Global variables heap pe hote hain" | `.data`/`.bss` — alag region, poori program lifetime |
| "`new int` sab kuch heap pe daalta hai" | Pointer stack pe; pointee heap pe |
| "`.bss` array file ko bada karta hai" | Sirf ek "N zero bytes chahiye" note — demand-zero |
| "String literal `char[]` hai, modifiable" | `.rodata`, read-only — likhna UB/crash |
| "Stack aur heap same cheez hain, bas naam alag" | Alag regions, alag manager, alag cost/lifetime/limits |

---

## Exercises

1. **Region quiz:** har ek — kaunsa region?
   `int g = 3;` (global), `static int s;` (global), `"abc"`, `int local;` (in
   `main`), `new double`, `std::vector<int> v(10)` ka data, `main` ka code.

   <details><summary>Answer</summary>

   `g` → `.data`. `s` → `.bss` (zero). `"abc"` → `.rodata`. `local` → stack.
   `new double` → heap (pointer stack pe). `v` ka data → heap. `main` code →
   `.text`.
   </details>

2. **Print & compare:** [`examples/01_memory_layout.cpp`](examples/01_memory_layout.cpp)
   do baar chalao. Kaunse addresses run-to-run same, kaunse alag? Kyun?

   <details><summary>Answer</summary>

   Ek run ke andar: segments fixed. Do runs ke beech: ASLR sab base randomize
   karta hai → sab addresses badalte (par relative order aksar same).
   </details>

3. **`.bss` vs `.data` size:** do programs — `int a[1000000];` vs
   `int a[1000000] = {1};`. `ls -l` / file size compare (Linux) ya `size a.out`.
   Farq?

   <details><summary>Answer</summary>

   Pehla `.bss` → binary ~4 MB chhota. Doosra `.data` → binary mein poore 4 MB
   (non-zero initializer ke kaaran har element file mein).
   </details>

4. **Write to `.rodata`:** `char* p = (char*)"x"; p[0] = 'y';` compile karo
   (`-Wno-write-strings`), chalao. Kya hota? `const` kyun bachaata?

   <details><summary>Answer</summary>

   Runtime SIGSEGV — `.rodata` page read-only. `const char*` ise compile-time
   error bana deta (`p[0] = ...` invalid), crash se pehle.
   </details>

5. **Pointer vs pointee region:** `int* p = new int(9);` — `&p` aur `p` (== jahan
   `*p` hai) print karo. Dono alag regions mein — kaise pehchana? (heap addr vs
   stack addr ka rough range.)

   <details><summary>Answer</summary>

   `&p` stack range mein (bahut high ya OS-specific), `p` heap range mein (heap
   base ke paas). `01_memory_layout.cpp` ke output se ranges compare karo.
   </details>

---

## Interview questions

1. Ek process ki 5 memory regions — naam, content, read/write, lifetime?
2. `.data` vs `.bss` — farq, aur executable file size pe asar?
3. `int* p = new int;` — `p` kahan, `*p` kahan? Lifetimes?
4. String literal likhne pe kya hota, kyun? `const char*` ka role?
5. Stack vs heap — allocate cost, size limit, lifetime, fragmentation?
6. ASLR kya karta? Debugging pe kya asar?

---

## Next
→ [`02-stack-deep-dive.md`](02-stack-deep-dive.md)
