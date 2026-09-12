# 03 — Layer 2: pointers, references, memory, RAII

## Prerequisites
Folders `12-POINTERS`, `13-REFERENCES`, `14-MEMORY`, `17-RAII`.
Yeh Layer HFT phone screens ka bread-and-butter hai.

---

## A — Pointers vs references

### A1. Pointer aur reference mein fark — 4 concrete points.
<details><summary>Answer</summary>
(1) Reference ko bind hote hi **rebind nahi** kar sakte; pointer ko
reassign kar sakte. (2) Reference **null nahi** ho sakti (well-formed
code mein); pointer null ho sakta. (3) Reference ko apna storage nahi
lagta (usually) — ek alias; pointer ek object hai jismein address hai.
(4) Reference ke through `&` lena referent ka address deta; pointer ka
`&` pointer ka address. Reference = "guaranteed-valid alias", pointer =
"maybe-valid, rebindable address". (`13-REFERENCES/01`.)
</details>

### A2. `int* const p` vs `const int* p` vs `const int* const p`?
<details><summary>Answer</summary>
`const int* p` — pointer to const int: `*p` change nahi, `p` change kar
sakte. `int* const p` — const pointer to int: `p` change nahi (bind at
init), `*p` change kar sakte. `const int* const p` — dono const. Padhne
ka trick: `const` ke baayein jo hai woh const, agar `const` sabse left ho
to uske right wala. (`12-POINTERS/06`.)
</details>

### A3. `T&` vs `T&&` (rvalue reference) parameter — overload resolution?
<details><summary>Answer</summary>
`f(T&)` lvalue leta, `f(const T&)` const lvalue + (as fallback)
temporaries, `f(T&&)` non-const rvalues (temporaries, `std::move`d
values) leta. Rvalue-ref overload "yeh object ka guts main chura sakta
hoon" signal karta — move ke liye. `f(T&&)` template context mein
**forwarding reference** ban jaata (alag cheez — `07`). (`18-COPY-MOVE`.)
</details>

### A4. "Reference member" wali class — kya problem?
<details><summary>Answer</summary>
Reference member ko constructor initializer list mein bind karna padta
(no default), class ka copy-assignment implicitly deleted ho jaata
(reference rebind nahi hoti), aur lifetime bug ka risk — reference kisi
aise object ko point kar sakti jo class se pehle mar jaaye. Usually
pointer (rebindable, nullable) ya `std::reference_wrapper` behtar. (`15`,
`13/09`.)
</details>

---

## B — Memory model

### B1. Stack vs heap — 4 differences.
<details><summary>Answer</summary>
(1) **Allocation cost:** stack = pointer bump (~1 ns); heap = allocator
work (~20–100 ns, tail worse). (2) **Lifetime:** stack = automatic
(scope); heap = manual / smart pointer. (3) **Size:** stack bounded
(~1–8 MB, `ulimit -s`); heap large. (4) **Locality:** stack hot in cache
(reused); heap scattered. HFT: allocate on stack / from a pool, never
`new` in the hot path (`14/08`, `36/04–07`).
</details>

### B2. `new` / `delete` internally kya karte?
<details><summary>Answer</summary>
`new T` → `operator new(sizeof(T))` (usually `malloc`-like: free-list /
arena, may `mmap`/`brk`) → phir constructor. `delete p` → destructor →
`operator delete(p)`. `new T[n]` / `delete[]` alag pair (array cookie for
count). Mixing `new`/`delete[]` ya `malloc`/`delete` = UB. (`14-MEMORY/03`.)
</details>

### B3. Memory leak kaise detect karte, aur kaise prevent?
<details><summary>Answer</summary>
**Detect:** ASan/LSan (`detected memory leaks` + alloc stack), valgrind
`--leak-check=full` (`definitely lost`), massif for reachable-but-growing
(`45/06`, `45/07`). **Prevent:** RAII — har resource ek owning object
mein (`std::vector`, `std::unique_ptr`, file wrapper); no raw `new` in
user code; `shared_ptr` cycles ko `weak_ptr` se todo; caches ko bound
karo. (`14/05`, `17-RAII`.)
</details>

### B4. Use-after-free "kabhi kaam karta hai" — kyun, aur khatarnaak kyun?
<details><summary>Answer</summary>
Freed memory turant overwrite nahi hoti — allocator use quarantine /
free-list mein rakhta, contents kuch der intact reh sakte. To read
"purani sahi value" de sakta — is baar. Baad mein woh block reuse ho
jaata → silent corruption / crash far away. Khatarnaak: non-deterministic,
timing/load dependent, test mein pass. ASan quarantine se turant catch
(`14/06`, `45/12` A3).
</details>

### B5. Dangling pointer ke 3 common sources.
<details><summary>Answer</summary>
(1) Local ka address return / store karna (`return &local;` — `45/12`
A4). (2) `delete`/`free` ke baad pointer use karna (A3). (3) Container
reallocation — `push_back` ke baad purane `&v[0]` / iterators invalid
(`std::vector`), ya `erase` ke baad (`45/12` A10). Bonus: `string_view` /
`span` ki jise woh point karta usse chhoti zindagi.
</details>

### B6. Stack overflow kaise hota, kaise pehchano?
<details><summary>Answer</summary>
Unbounded/deep recursion, ya ek function mein bahut bada local array
(`char buf[8<<20]`). Pehchano: SIGSEGV; `bt` mein hazaaron identical
frames; fault address `$sp` ke paas. Fix: recursion base case / depth
limit, recursion→loop+`std::stack`, bade buffers heap pe (`08/05`,
`14/02`, `45/04`).
</details>

---

## C — Smart pointers & RAII

### C1. RAII — ek line mein, aur ek example.
<details><summary>Answer</summary>
**Resource Acquisition Is Initialization** — resource ka lifetime ek
object ke lifetime se baandho: constructor acquire kare, destructor
release. Stack unwinding (normal return ya exception) pe destructor
guaranteed chalta → koi leak nahi. Example: `std::lock_guard` (ctor
locks, dtor unlocks), `std::unique_ptr`, `std::fstream`. (`17-RAII`.)
</details>

### C2. `unique_ptr` vs `shared_ptr` vs `weak_ptr` — kab kaunsa?
<details><summary>Answer</summary>
**`unique_ptr`:** single owner, zero overhead (just a pointer),
move-only. **Default choice.** **`shared_ptr`:** genuinely shared
ownership, atomic refcount (has cost), control block allocation. **`weak_ptr`:**
non-owning observer of a `shared_ptr`; `lock()` se temporary `shared_ptr`;
`shared_ptr` **cycles** todne ke liye. HFT hot path: usually `unique_ptr`
ya raw non-owning pointers into a pool; `shared_ptr` atomic refcount aksar
avoid. (`17-RAII`, `14`.)
</details>

### C3. `shared_ptr` ka refcount thread-safe hai — matlab `shared_ptr`
fully thread-safe hai?
<details><summary>Answer</summary>
**Nahi.** Control block ka refcount atomic hai → alag threads alag
`shared_ptr` **instances** ko freely copy/destroy kar sakte. Par **ek hi
`shared_ptr` instance** ko do threads se concurrently modify (`reset()` +
copy) = data race on the `shared_ptr` object (do pointers). Fix: har
thread apni copy le, ya `std::atomic<std::shared_ptr>` / mutex. (`26`,
`45/12` B8.)
</details>

### C4. `make_unique` / `make_shared` kyun `new` ke muqable behtar?
<details><summary>Answer</summary>
(1) Exception safety — `f(unique_ptr<A>(new A), g())` mein evaluation
order se leak ho sakta tha (pre-C++17); `make_unique` isse khatam karta.
(2) `make_shared` control block + object ko **ek** allocation mein rakhta
(cache-friendly, ek `new` not do). (3) `new` type do baar likhna avoid.
Caveat: `make_shared` + `weak_ptr` jo bahut der zinda rahe → object ki
memory bhi hold hoti (single block). (`17-RAII`.)
</details>

### C5. Custom deleter kab chahiye? Example.
<details><summary>Answer</summary>
Jab resource `delete` se release nahi hota: `unique_ptr<FILE, decltype(&fclose)>`
(fclose), `unique_ptr<T, PoolReturn>` (pool ko wapas), C API handles
(`SDL_Window`, sockets), `mmap`'d regions (`munmap`). `unique_ptr` mein
stateless deleter zero-size (EBO); `shared_ptr` deleter control block
mein (type-erased). (`17-RAII`, `29-LINUX`.)
</details>

### C6. `std::unique_ptr` ko function mein pass — by value ka kya matlab?
<details><summary>Answer</summary>
`void sink(std::unique_ptr<T> p)` = **ownership transfer**. Call site pe
`std::move` karna padta (`sink(std::move(up))`) kyunki `unique_ptr`
move-only. By value + move signal karta "main ab ismein malik hoon".
Agar function ko sirf **use** karna hai own nahi — `T*` ya `T&` lo
(non-owning), `unique_ptr<T>&` nahi. (`18-COPY-MOVE`, `17`.)
</details>

---

## D — HFT-flavoured

### D1. HFT hot path mein `new`/`delete` kyun banned?
<details><summary>Answer</summary>
Allocator: lock (multi-thread), free-list walk, possible `mmap`/`brk`
syscall, possible **page fault** on first touch — cost non-deterministic,
p99.9 hundreds of ns to µs (`14/08` measured p99.9 ~2585 ns). HFT ko
**bounded** latency chahiye. Fix: pre-allocate at startup, object pools
(`FixedPool`, `ObjectPool` — `36/06-07`, `44`), arena/PMR (`36/05`),
`reserve()` everything, warm-up pass.
</details>

### D2. Object pool — construct-on-acquire vs recycle-pre-constructed?
<details><summary>Answer</summary>
**Construct-on-acquire:** slot free-list se lo, placement-new object
banao, release pe explicit dtor. Clean state, ctor cost har baar.
**Recycle:** objects already constructed rehte, acquire = pointer wapas,
release = free-list mein (no dtor). Fastest (`36/07`: recycle+reset ~20
ns vs construct ~30), par **stale fields** ka danger — `reset()` disciplined
hona chahiye. (`36/06-07`, `44` `mh_object_pool.hpp`.)
</details>

### D3. "Generation-checked handle" kya hai, kis bug ko rokta?
<details><summary>Answer</summary>
Pool slot ko raw pointer dene ki jagah `{index, generation}` handle do.
`get(handle)` slot ke current generation ko handle ke generation se
compare karta; release pe `++generation`. Ek stale handle (late async
ack/fill on a recycled order slot) ka `get()` **nullptr** deta, dangling
pointer nahi. Use-after-free guard without `shared_ptr` overhead. (`44`
`ObjectPool`, `45/12` A3 fix.)
</details>

### D4. `alignas(64)` kab aur kyun?
<details><summary>Answer</summary>
64 = typical cache-line size. Use: (1) **false sharing** rokna — do
threads ke hot variables ko alag lines pe rakho (`alignas(64)` on each,
+ trailing pad), warna line CPU-to-CPU ping-pong (`43/08`, `45/12` B9).
(2) SIMD loads/stores jinko alignment chahiye. (3) Ek struct jo exactly
ek line mein fit karna hai. `std::hardware_destructive_interference_size`
portable constant deta (par sab compilers pe stable nahi).
</details>

### D5. `placement new` kya hai, kab use hota?
<details><summary>Answer</summary>
`new (addr) T(args)` — **pehle se allocated** memory pe object construct
karta, allocation nahi. Pools/arenas mein: raw buffer se slot lo,
placement-new se object banao; release pe `p->~T()` explicitly call karo
(placement new ka `delete` nahi hota). `std::launder` kabhi lifetime
tracking ke liye. `std::vector` internally yahi karta (capacity vs size).
(`36/06`, `25-OBJECT-MODEL/02`.)
</details>

### D6. `mlockall` / prefaulting kyun matter karta HFT mein?
<details><summary>Answer</summary>
Pehli baar jab koi page touch hota → **minor page fault** (kernel maps a
frame, ~1–5 µs). Swapped page → **major fault** (disk, ~ms). Hot path pe
yeh unacceptable jitter. Fix: startup pe saari memory allocate karo, har
page ko touch karo (prefault), `mlockall(MCL_CURRENT|MCL_FUTURE)` se
swap-out rok do. Verify: `perf stat -e page-faults` steady-state ~0
(`29/13`, `45/11`).
</details>

---

## Interview tips for Layer 2

- Pointer questions pe hamesha **lifetime** aur **ownership** ki baat
  karo — "kaun malik hai, kab release hoga, kya isse pehle mar sakta hai".
- "Yeh leak / UAF kaise pakdoge" ka jawab tool + reasoning dono ho
  (ASan/valgrind + "shared mutable + no clear owner").
- HFT twist: har allocation question ka jawab "...aur hot path mein hum
  yeh nahi karte, pool/arena se aata hai" pe khatam karo.
- `unique_ptr` = default; `shared_ptr` sirf jab ownership genuinely
  shared — over-using `shared_ptr` ek junior signal hai.

## Next
→ [`04-oop-questions.md`](04-oop-questions.md)