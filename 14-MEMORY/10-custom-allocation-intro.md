# 10 — Custom allocation intro (placement new, arena, pool)

## Prerequisites
- [`04-new-and-delete.md`](04-new-and-delete.md), [`08-allocation-cost.md`](08-allocation-cost.md), [`09-fragmentation.md`](09-fragmentation.md)
- Folder 12 file 10 (`void*`), file 13 (bug catalog)

## Yeh topic abhi kyun
File 08/09 ne dikhaya: default `new` hot path ke liye slow + fragmenting hai.
Solution — **apni memory strategy**. Iska building block **placement new**
(memory aur construction ko alag karna) hai, aur us par **arena** aur **pool**
allocators bante hain. Yeh HFT memory design ka core hai (poori depth folder 22
`std::pmr` + folder 37+ HFT track).

---

## Placement new — construction bina allocation

Normal `new` = allocate + construct. **Placement new** = *aap* memory do, `new`
sirf construct kare:

```cpp
#include <new>

alignas(Widget) unsigned char buf[sizeof(Widget)];   // raw storage (stack/pool/arena)

Widget* w = new (buf) Widget(1, 2);   // buf pe Widget construct -- koi allocation nahi
// ... use w ...
w->~Widget();                          // destructor MANUALLY (delete nahi -- memory hamari hai)
```

- `new (ptr) T(args)` → `ptr` pe `T` construct, `ptr` hi return.
- **Koi matching `delete` nahi** — `delete w` yahan galat (woh `buf` ko free
  karne ki koshish karega). Sirf `w->~T()` explicitly.
- Storage ki **alignment** `T` ke liye sahi honi chahiye (`alignas(T)` / allocator-returned memory).
  (`std::aligned_storage` purane code mein milega — C++23 mein deprecated; `alignas(T) std::byte buf[sizeof(T)]`
  likho.)

Yeh woh mechanism hai jisse `std::vector` apni `reserve()` ki hui (but unused)
memory pe elements ko demand pe construct karta hai — capacity ≠ constructed
count.

---

## Arena / bump allocator — sabse simple

Ek bada block; ek pointer jo aage badhta; free individually **nahi** hota — poora
arena ek saath reset:

```cpp
class Arena {
    std::byte* base_;
    std::size_t cap_, off_ = 0;
public:
    explicit Arena(std::size_t n) : base_(static_cast<std::byte*>(::operator new(n))), cap_(n) {}
    ~Arena() { ::operator delete(base_); }

    void* alloc(std::size_t n, std::size_t align = alignof(std::max_align_t)) {
        std::size_t p = (off_ + align - 1) & ~(align - 1);     // align up
        if (p + n > cap_) return nullptr;                       // arena full
        off_ = p + n;
        return base_ + p;
    }
    void reset() { off_ = 0; }        // "free everything" -- O(1)
};
```

- `alloc()` = ek add + ek compare → **~1-2 ns, zero syscall, zero fragmentation**.
- `reset()` = `off_ = 0` → saara "freed" ek instruction mein.
- Perfect for **per-event / per-frame / per-request** scratch: event process
  karo (arena se allocate), event khatam → `reset()`.
- ⚠️ Individual free nahi. Non-trivial destructors khud track karke chalane
  padte (ya sirf trivially-destructible types rakho).

---

## Fixed-size pool — individual free ke saath

Ek block, N equal slots, free-list slots ke andar embedded
([`examples/07_simple_pool.cpp`](examples/07_simple_pool.cpp)):

```cpp
void* allocate()      { void* p = free_; free_ = *(void**)p; return p; }   // O(1)
void  deallocate(void* p) { *(void**)p = free_; free_ = p; }               // O(1)
```

- `allocate`/`deallocate` = do-teen pointer loads/stores → GCC 16.2 `-O2`, 64 orders ka burst (allocate +
  placement new → read + free): **1.70–1.85 ns/pair** vs `new`/`delete` **38–39 ns/pair** → **~21x tez**, aur
  **flat tail** (slow path hai hi nahi).
- ⚠️ Pehle yahan "~0.4 ns, ~190x" likha tha — woh benchmark har baar *wahi* slot lekar turant wapas deta
  tha, aur GCC ne poora pop+push ek `mov` bana diya tha (file 08 mein kahani). Pool fast hai, par 190x nahi.
- External fragmentation **impossible** (sab slots ek size).
- Ek size ke liye (order objects, event structs). Mixed sizes → alag pools.
- Combine with placement new: `void* m = pool.allocate(); T* t = new (m) T(...);
  ...; t->~T(); pool.deallocate(m);`.

---

## `std::pmr` — standard containers + custom memory (folder 22 preview)

C++17 `<memory_resource>` — container ka type badle bina uski memory strategy
badlo:

```cpp
#include <memory_resource>

std::byte buffer[64 * 1024];
std::pmr::monotonic_buffer_resource arena{buffer, sizeof(buffer)};   // bump allocator
std::pmr::vector<int>    v{&arena};      // is vector ki saari memory arena se
std::pmr::vector<std::pmr::string> names{&arena};

// scope end -> arena destroy -> sab ek saath gaya, koi per-element free nahi
```

- `monotonic_buffer_resource` — bump/arena (no individual free).
- `unsynchronized_pool_resource` — size-bucketed pools (individual free, single
  thread).
- Aap apna `std::pmr::memory_resource` derive karke likh sakte ho.

Yeh "custom allocator" ka modern, composable roop — poori detail folder 22.

---

## Andar kya hota hai

- **Placement new** ka koi overhead nahi — literally constructor call at given
  address (`lea` + ctor code). Compiler ise inline karta.
- **Arena `alloc`** → `add` + `and` (align) + `cmp` + `mov` — 4 instructions,
  branch predictable (arena rarely full mid-batch). No cache miss (bump pointer
  hot).
- **Pool free-list** → freed slot ke pehle 8 bytes mein next-free pointer (union
  trick — slot ya to object hai ya free-list node). `allocate` = 2 dependent
  loads worst case, aksar L1 hit.
- **`reset()`** semantics: memory ke bytes wahi rehte — sirf `off_`/`free_`
  reset. Isliye stale pointers **poison nahi hote** (ASan-style) — reset ke baad
  purana pointer use karna UAF (design discipline chahiye).
- **Alignment:** arena/pool ko `T` ke liye aligned memory dena. `::operator new`
  `alignof(std::max_align_t)` (16 usually) deta; over-aligned (`alignas(64)`
  cache-line) types ke liye aligned-new (`::operator new(n, std::align_val_t{64})`).

> **HFT relevance:** yeh hot-path memory ka **default toolkit** —
> (1) startup pe bada arena/pool `::operator new` se + first-touch,
> (2) per-event scratch: monotonic arena, event ke end pe `reset()`,
> (3) long-lived same-size objects (orders, book nodes): fixed pool, O(1)
> alloc/free, flat tail,
> (4) `std::pmr` se standard containers ko in resources pe chalao bina rewrite.
> Result: hot path pe `malloc`/`mmap`/lock/fault kuch nahi → deterministic
> nanoseconds. Poora HFT track (folder 37+) isi par bana hai.

---

## Hands-on

```bash
./build.ps1 fast 14-MEMORY/examples/07_simple_pool.cpp    # pool vs new ~21x (burst), placement new
```

Aur khud: upar wala `Arena` likho, ek loop mein 1000 chhote structs `alloc`
karo, `reset()`, phir se — `-O2` pe per-alloc ns naapo (rdtsc, `06` jaisa).
`::operator new` se compare.

---

## ⚠️ Traps

### Trap 1 — placement new ke saath `delete`
```cpp
T* t = new (buf) T();  delete t;   // ⚠️ UB -- buf allocated-by-new nahi. Sirf t->~T()
```

### Trap 2 — destructor bhool jaana
```cpp
new (poolSlot) std::string("x");  pool.deallocate(poolSlot);   // ⚠️ string ka buffer leak -- ~string() miss
```

### Trap 3 — misaligned storage
```cpp
char buf[sizeof(T)];  new (buf) T();   // ⚠️ buf align 1; T ko shayad 8/16 chahiye -> UB.
alignas(T) char buf[sizeof(T)];        // ✅
```

### Trap 4 — arena reset ke baad purane pointers use
```cpp
auto* a = arena.alloc(64);  arena.reset();  a->field = 1;   // ⚠️ UAF (bytes wahi, par logically freed)
```

### Trap 5 — pool ko mixed sizes ke liye use
```cpp
FixedPool p(64, N);  p.allocate();  // ⚠️ 100-byte object? overflow. Ek pool = ek size
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Placement new bhi allocate karta" | Nahi — aap memory dete ho, woh sirf construct karta |
| "Placement new ke baad `delete`" | `t->~T()` manually; memory aap manage karte |
| "Arena se individual free ho sakta" | Nahi — `reset()` sab ek saath. Individual → pool |
| "Pool har size ke liye kaam karta" | Ek pool = ek fixed size. Mixed → multiple pools |
| "Custom allocator = advanced, avoid" | Hot path ke liye zaroori; `std::pmr` se aasan |
| "Pool `new` se ~190x tez" | Asli burst workload pe ~21x (is machine pe); 190x ek khaali loop tha |
| "`operator new` replace kiya to saari allocations gini jaayengi" | MinGW DLL build pe libstdc++ ke andar ki calls nahi — `-static` |

---

## Exercises

1. **Placement new lifecycle:** `alignas(std::string) unsigned char
   b[sizeof(std::string)];` — `std::string* s = new (b) std::string("hello
   world this is long");` construct karo, use karo, sahi destroy karo. `delete
   s` kyun galat?

   <details><summary>Answer</summary>

   Use ke baad `s->~basic_string();` (ya `s->~string()` via alias). `delete s`
   galat — `b` `::operator new` se nahi aaya, uspe `operator delete` UB. Aur
   destructor ke bina string ka heap buffer leak.
   </details>

2. **Arena write:** upar wala `Arena` implement karo. 10000 random-size (8-128
   B) allocations `reset()` se pehle. Total time vs 10000 `::operator new` (`-O2`,
   rdtsc). Ratio?

   <details><summary>Answer</summary>

   GCC 16.2 `-O2`, 10,000 random 8–128 B sizes × 500 rounds, har allocation ka pehla byte likha (3 runs):
   Arena **1.6–2.6 ns/alloc** (bump + align), `::operator new`+`delete` **43–70 ns/pair** (pehla run sabse slow —
   heap abhi grow ho raha tha) → **~17–45x**. Aur arena ka tail flat (no syscall/lock). `reset()` ~0.
   ⚠️ Kaam ko `[[gnu::noipa]]` functions mein rakho, warna compiler unused allocations elide kar sakta hai.
   </details>

3. **Pool + placement new:** `07_simple_pool.cpp` ke `FixedPool` se `Order`
   objects allocate karo (placement new), use karo, `~Order()` + `deallocate`.
   1e6 baar loop — per-op ns vs `new Order` / `delete`.

   <details><summary>Answer</summary>

   `07_simple_pool.cpp` ka burst version (GCC 16.2 `-O2`): pool+placement **1.70–1.85 ns/pair**, `new`/`delete`
   **38–39 ns/pair** → **~21x**. `Order` trivially destructible hai to `~Order()` no-op. ⚠️ Agar loop ek hi slot
   baar-baar le aur de, compiler use ek `mov` bana deta hai — assembly dekh ke confirm karo ki asli kaam naap
   rahe ho.
   </details>

4. **`std::pmr`:** `std::pmr::monotonic_buffer_resource` + `std::pmr::vector<int>`
   (stack buffer se). 100000 `push_back`. `operator new` count (global override)
   — kitne? Plain `std::vector<int>` se compare.

   <details><summary>Answer</summary>

   GCC 16.2 pe gin ke: pmr + bada buffer (2 MB) → **0** global `operator new` (sab buffer se). Plain vector →
   **18** `operator new`. Buffer chhota (64 KB) ho to resource upstream (default `new_delete_resource`) se
   maangta hai — woh **aligned** `operator new(size, std::align_val_t)` bulata hai: `-static` build mein **4**
   aligned calls gine.

   ⚠️ **MinGW trap:** normal (DLL) build mein wahi counter **0** dikhata hai! `new_delete_resource` ka code
   `libstdc++-6.dll` ke andar hai, aur Windows pe exe mein replace kiya `operator new` DLL ke andar ki calls ko
   nahi pakadta (Linux ki tarah symbol interposition nahi). Template code (jaise `std::vector`, `std::string`)
   exe mein instantiate hota hai, isliye woh gina jaata hai. Library ke andar ki allocations ginni hain to
   `-static` se link karo.
   </details>

5. **Reset UAF:** arena se ek pointer lo, `reset()`, phir us pointer se likho.
   `-O2` pe kya hota? ASan (Linux) kya kehta (ya nahi kehta)?

   <details><summary>Answer</summary>

   Bytes abhi valid mapped memory hain → write "kaam kar jaata hai" (silent) →
   agli arena allocation ka data corrupt. Plain ASan yeh **nahi** pakadta (memory
   allocator-tracked nahi) — custom arena ko ASan manual poisoning
   (`__asan_poison_memory_region`) chahiye. Isliye discipline > tools yahan.
   </details>

---

## Interview questions

1. Placement new kya karta, kya nahi? Cleanup kaise?
2. Arena/bump allocator — `alloc` aur `reset` ki cost, kis workload ke liye?
3. Fixed-size pool — free-list slots mein kaise embed hoti, cost?
4. Arena vs pool — individual free? kab kaunsa?
5. `std::pmr` — kya problem solve karta (container type vs memory strategy)?
6. Custom arena ke stale-pointer bugs tools se kyun nahi pakad'te — kya karein?

---

## Next
→ [`11-memory-tools.md`](11-memory-tools.md)
