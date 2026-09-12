# 12 — Folder 14 Revision + Exercises

## Prerequisites
Lessons 01–11 aur saare 7 examples chalaye hue.

---

## PART A — Concept check

1. Ek process ki 5 memory regions — content, read/write, lifetime?
2. `.data` vs `.bss` — farq, executable file size pe asar?
3. `int* p = new int(5);` — `p` kahan, `*p` kahan, dono ki lifetime?
4. Stack "allocation" kitni mehngi, kyun? (instruction level)
5. Stack kis direction grow karta, buffer overflow pe iska matlab?
6. Stack size limit — typical (Linux/Windows), RAM se relation?
7. Stack overflow kaise dikhta (signal/exception)?
8. `malloc` ke andar fast path vs slow path — kya hota?
9. `malloc` syscall hai? Kab banta hai?
10. `new T` vs `malloc` — steps ka farq?
11. `delete` vs `delete[]` — internally, mismatch pe kya?
12. `new` failure — default kya, `nothrow` kya?
13. Memory leak ki definition — "still reachable" leak hai?
14. Use-after-free "kabhi chalta hai" kyun?
15. Double-free allocator ko kaise todta?
16. `delete p;` ke baad `p` ka kya? Best practice?
17. Static initialization order fiasco — kya, fix kya?
18. `thread_local` — storage, cost, use case? Shared across threads?
19. Allocation tail (p99.9/max) ke 4 sources?
20. Hot path zero-allocation ke 4 techniques?
21. Internal vs external fragmentation — example har ek?
22. C++ heap compact kyun nahi ho sakti?
23. Placement new kya karta, cleanup kaise?
24. Arena vs fixed-size pool — individual free? kab kaunsa?
25. ASan kya pakadta kya nahi? MinGW pe alternative?

---

## PART B — Output / behaviour prediction

### B1
```cpp
int  g;              // global
int  h = 7;          // global
int main() {
    int a;           // local
    std::cout << (g == 0) << " " << (h == 7);
    // 'a' ki value?
}
```
<details><summary>Answer</summary>`1 1` — `g` `.bss` (zero-init guaranteed). `h` `.data`. `a` local: **indeterminate** (garbage) — zero-init sirf static storage ke liye.</details>

### B2
```cpp
int* p = new int[5]{1, 2, 3};
std::cout << p[0] << p[3] << p[4];
delete p;            // note: no []
```
<details><summary>Answer</summary>`100` print (1, 0, 0 → `1` `0` `0`). Phir `delete p` (bina `[]`) = **UB** — should be `delete[] p`. Aksar crash / heap corruption.</details>

### B3
```cpp
int nextId() { static int n = 0; return ++n; }
std::cout << nextId() << nextId() << nextId();
```
<details><summary>Answer</summary>`123` — function-local `static` sirf pehli call pe init (0), phir persist.</details>

### B4
```cpp
int* p = new int(42);
int* q = p;
delete p;
std::cout << *q;
delete q;
```
<details><summary>Answer</summary>`*q` → UAF read (aksar `42` ya garbage). `delete q` → **double-free** (`q == p`, already freed) → allocator corruption / abort.</details>

### B5
```cpp
char buf[sizeof(std::string)];
auto* s = new (buf) std::string("a fairly long string value here");
std::cout << s->size();
// (no cleanup)
```
<details><summary>Answer</summary>`31` print, par: (a) `buf` `alignas(std::string)` nahi → misaligned → UB; (b) `~string()` miss → heap buffer leak. Sahi: `alignas(std::string) unsigned char buf[...]` + `s->~basic_string();`.</details>

### B6
```cpp
std::vector<int> v;
for (int i = 0; i < 1000; ++i) v.push_back(i);
// approx kitni heap allocations hui?
```
<details><summary>Answer</summary>~10 (2x growth: 1,2,4,...,1024 → ~10 reallocs, har ek `operator new` + copy + `delete`). `v.reserve(1000)` → 1 allocation, 0 copies.</details>

### B7
```cpp
// a.cpp
std::string pre = "X";
// b.cpp
extern std::string pre;
std::string full = pre + "Y";
std::cout << full;   // ?
```
<details><summary>Answer</summary>**Unspecified** — cross-TU dynamic init order. `full` `"XY"` ya `"Y"` (pre abhi empty) ya crash. Fix: `const std::string& pre()` first-use accessor.</details>

### B8
```cpp
int arr[3] = {1, 2, 3};
int* p = arr;
delete p;            // ?
```
<details><summary>Answer</summary>**UB** — `arr` stack pe hai, `new` se nahi aaya. `delete` on non-heap / non-`new` pointer = undefined (crash / corruption).</details>

---

## PART C — Find the bug

### C1
```cpp
std::string* makeName() {
    std::string s = "engine";
    return &s;
}
```
<details><summary>Answer</summary>`s` local — return pe destroyed, returned pointer dangling (`-Wreturn-local-addr`). Fix: `std::string makeName() { return "engine"; }` (value, RVO).</details>

### C2
```cpp
void process() {
    int* buf = new int[1024];
    if (!validate(buf)) return;      // leak
    transform(buf);
    delete[] buf;
}
```
<details><summary>Answer</summary>Early `return` pe `delete[]` miss → leak. Fix: `std::vector<int> buf(1024);` (RAII) — har path pe free.</details>

### C3
```cpp
Widget* w = new Widget();
cache[key] = w;
return w;
// caller: delete w;   ... aur cache destructor bhi delete karta hai
```
<details><summary>Answer</summary>Do owners → double-free. Fix: ownership decide — `std::shared_ptr<Widget>` (shared), ya cache owns + caller gets raw/`weak`, ya caller owns + cache gets non-owning.</details>

### C4
```cpp
void f() {
    char line[8];
    std::strcpy(line, "this is way too long");
}
```
<details><summary>Answer</summary>Stack buffer overflow — 21 bytes into `char[8]`, saved rbp / return address / adjacent locals corrupt. `-fstack-protector` → `__stack_chk_fail`. Fix: `std::string` / bounded copy (`snprintf`, `std::string_view`).</details>

### C5
```cpp
int compute() {
    int result;
    for (int i = 0; i < n; ++i) result += data[i];
    return result;
}
```
<details><summary>Answer</summary>`result` uninitialized (`+=` before any `=`). Garbage sum. `-Wmaybe-uninitialized` / Valgrind. Fix: `int result = 0;`.</details>

### C6
```cpp
struct Pool {
    std::byte* base = static_cast<std::byte*>(::operator new(N));
    std::size_t off = 0;
    void* get(std::size_t n) { auto* p = base + off; off += n; return p; }
};
```
<details><summary>Answer</summary>(a) `off + n > N` check nahi → overflow past arena → UB. (b) alignment ignore → misaligned returns. (c) `~Pool` mein `::operator delete(base)` nahi → leak. Fix: bounds check, align-up, destructor.</details>

### C7
```cpp
auto* a = arena.alloc(64);
fill(a, 64);
arena.reset();
send(a, 64);          // ?
```
<details><summary>Answer</summary>`reset()` ke baad `a` logically freed (arena `off=0`). `send` reads memory jo agli `alloc` overwrite karegi → data race / stale. Reset ke pehle `send`, ya `a` ki copy.</details>

### C8
```cpp
thread_local std::vector<char> scratch;
void handler(std::span<const char> in) {
    scratch.resize(in.size());
    std::copy(in.begin(), in.end(), scratch.begin());
    dispatch(scratch);
}
// 4 threads concurrently call handler
```
<details><summary>Answer</summary>Actually **safe** — `thread_local` → har thread ka apna `scratch`, no data race, aur capacity reuse across calls (good). Trap check: agar `scratch` plain global hota → race. `thread_local` yahan sahi choice.</details>

---

## PART D — Write it

### D1 — segment address printer
Har region (`.text` via `&func`, `.rodata` via literal, `.data` via init global,
`.bss` via zero global, heap via `new`, stack via local) ka address print karo,
sort karke dikhao apni machine pe order kya. `01_memory_layout.cpp` extend.

### D2 — stack vs heap depth
Recursive `void r(int n)` jo har call `&local` print kare. `r(0)` se shuru,
`n % 500 == 0` pe print. Kis depth pe crash? Frame size estimate se match karo.

### D3 — leak-free rewrite
Ek diya-hua leaky snippet (`new` in loop, `new` before early return, `vector<T*>`)
ko `std::vector` / `std::unique_ptr` se rewrite karo. Counted-`new` override se
`outstanding == 0` verify.

### D4 — bump arena
`Arena` class: `alloc(n, align)` (bounds + align-up), `reset()`, destructor.
1e6 chhote allocations + `reset` loop. `-O2` rdtsc se per-alloc ns. `::operator
new` se ratio.

### D5 — fixed pool + placement new
`07_simple_pool.cpp` ke `FixedPool` pe `Order` objects: `allocate` → placement
`new` → use → `~Order()` → `deallocate`. 1e6 loop, per-op ns vs `new`/`delete`.
Pool-full pe graceful `nullptr`.

### D6 — allocation latency histogram
`06_allocation_benchmark.cpp` ko extend: raw `new` ke 500k samples ka ek ASCII
histogram (buckets: <32, 32-64, 64-128, 128-256, 256-1024, 1024-4096, >4096 ns).
Tail buckets ka count dikhao.

---

## PART E — HFT angle

1. **Wire-to-order budget:** ek order ka total budget ~500 ns hai. `06`
   benchmark se `new` ka p50 (~50 ns) aur p99.9 (~500 ns) — ek allocation is
   budget ka kitna % kha jaata (p50 case, p99.9 case)?

2. **Zero-alloc audit:** ek hot-path function jo `std::string`, `std::vector`,
   `std::map` use karta hai — har ek kaunsi hidden allocation la sakta? Kaise
   hataoge (reserve, `string_view`, `std::array`, `pmr`, pool)?

3. **Soak test:** trading process ko 6 ghante replay pe chalao. RSS har minute
   log. Flat plateau expected — agar linear upward, kya bug (leak vs frag), aur
   kaise distinguish?

4. **Pool sizing:** order pool `capacity = ?`. Peak concurrent resting orders
   1000; safety factor. Pool full ho gaya to kya (reject order? fallback
   `new`? assert)? HFT mein kaunsa acceptable?

5. **Arena reset timing:** per-event scratch arena — `reset()` kab? Event ke
   end pe. Agar ek async task arena-allocated data ko event ke baad tak hold
   kare — kya bug, kaise design (copy out / separate lifetime)?

---

## PART F — Challenge

**"Zero-allocation event pipeline + soak harness"**

Ek chhota market-data → order pipeline banao jo **steady state mein ek bhi
`malloc`/`new` na kare**, aur ek harness jo yeh **prove** kare:

```
Startup:
  - order pool  : FixedPool(sizeof(Order), MAX_ORDERS), first-touch
  - event arena : Arena(EVENT_SCRATCH_BYTES)
  - ring buffer : fixed-size, pre-allocated, for outbound messages
  - global operator new/delete override -> counter (jaisa 04/05 mein)

Hot loop (per incoming packet):
  - parse packet -> Message (string_view / span into the packet buffer, no copy)
  - arena.alloc() se per-event scratch (working set)
  - pool.allocate() + placement new se naya Order (resting)
  - matched order -> order->~Order() + pool.deallocate()
  - outbound -> ring buffer slot (no alloc)
  - arena.reset() at end of packet

Harness:
  - warm up 10k packets
  - assert: steady-state operator new count delta == 0 over 1M packets
  - rdtsc per-packet latency -> p50 / p99 / p99.9 / max
  - RSS before vs after 1M packets -> must be flat (± noise)
  - ek "bug mode" (#ifdef): hot path mein ek `std::string` copy daalo ->
    harness ko fail karna chahiye (new count > 0, RSS creep)
```

Deliverables: source + measured p50/p99/p99.9 per packet + "new count delta: 0"
+ RSS flat proof. Yeh folder 09/10/11/13/14 ko jodta hai aur HFT track (folder
36+) ka direct seed hai.

---

## Next
→ [`../15-CLASSES/00-README.md`](../15-CLASSES/00-README.md)
