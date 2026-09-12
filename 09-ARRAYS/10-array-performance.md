# 10 — Array performance — cache, prefetch, AoS vs SoA, stack vs heap

## Prerequisites
- [`08-std-array.md`](08-std-array.md), [`09-std-span.md`](09-std-span.md)
- `07-LOOPS/09-loop-performance.md` (cache locality ~8x measured, vectorization)
- `06-CONDITIONS/08-branch-prediction-intro.md` ("measure karo")

## Yeh topic abhi kyun
Arrays fast kyun hain — aur unhe fast rakhne ke rules. Contiguity + predictable
stride = cache aur prefetcher ke best friends. Aur ek bada design lever: same data,
do layouts — **AoS vs SoA** — jismein 4x tak farq (yahan measure karenge).

---

## Arrays fast kyun

1. **Contiguous** → ek cache line (64 B = 16 `int`) mein 16 elements. 1 miss →
   16 useful values.
2. **Predictable stride** → hardware prefetcher pattern pakadta hai, aage ka data
   pehle se laata hai → memory latency hidden.
3. **`arr[i]` = ek scaled load** (`base + i*size`) — CPU addressing mode, 1
   instruction.
4. **Auto-vectorization** → simple array loops `-O2`/`-O3` pe SIMD (4-16
   elements/instruction).
5. **Branch-predictable** loop bound.

`std::list` / pointer-chained data inme se **kisi ka** faayda nahi leta — har
`next` ek cache miss.

---

## Traversal order — recap (folder 07 file 09)

Row-major 2D array ko column-major traverse karna → **~8x slower** (measured, 4096²).
**Rule: inner loop = memory mein fastest-changing index (row-major → last index).**

---

## AoS vs SoA — layout decides speed

Ek "entity" mein 6 fields (`x, y, z, vx, vy, vz`). 40 lakh entities.

### AoS — Array of Structs
```cpp
struct Entity { int x, y, z, vx, vy, vz; };   // 24 bytes
std::vector<Entity> entities;
```
Memory: `[x0 y0 z0 vx0 vy0 vz0][x1 y1 z1 ...]...`

### SoA — Struct of Arrays
```cpp
struct { std::vector<int> x, y, z, vx, vy, vz; } entities;
```
Memory: `[x0 x1 x2 ...][y0 y1 y2 ...]...`

### Measured (`examples/07_aos_vs_soa.cpp`, GCC 15.1, `-O2`, N=4M)

```
TASK A -- touch ONE field  (x = x*3 + 1):
  AoS (stride 24 B)   : ~542 ms
  SoA (contiguous)    : ~124 ms      -> SoA ~4.4x faster

TASK B -- x += vx  (2 of 6 fields):
  AoS : ~502 ms
  SoA : ~183 ms      -> SoA ~2.7x faster
```

**Kyun:** TASK A sirf `x` chahiye. AoS mein har 64-byte cache line se sirf 4 bytes
(`x`) kaam ke — **5/6 line barbaad**, 6x memory traffic. SoA mein poori line
kaam ki, aur SIMD-vectorize aasan.

### Kab AoS behtar
Jab **poora entity ek saath** chahiye — "entity `i` ko move karo, uske saare
fields chahiye". AoS mein ek entity ke 6 fields ek (ya do) cache line mein; SoA
mein 6 alag arrays se 6 alag lines.

| Access pattern | Layout |
|---|---|
| Field-subset over many entities (analytics, physics, SIMD) | **SoA** |
| All fields of one entity at a time | **AoS** |
| Mixed | AoSoA (hybrid), ya profile karke decide |

⚠️ `-O3 -march=native` pe compiler AoS ko bhi aggressively vectorize (gather)
kar sakta hai — gap narrow hota hai, kbhi micro-tasks flip bhi ho jaate hain.
**Apne target hardware + flags pe measure karo.**

---

## Stack vs heap arrays

```cpp
void f() {
    int a[1000];                       // STACK -- ~free alloc (rsp move), cache-hot
    std::array<int, 1000> b;           // STACK -- same
    std::vector<int> c(1000);          // HEAP -- new (bookkeeping), possibly cold, freed at scope end
    static int d[1000];                // BSS  -- alloc-free, zero-init, program lifetime
}
```

| | Stack `int[N]` / `std::array` | Heap `std::vector` |
|---|---|---|
| Alloc cost | ~0 | `new` (lock, bookkeeping) |
| Size | compile-time; ~MB limit (overflow!) | runtime; GBs |
| Locality | excellent (frame is cache-hot) | depends (fresh pages may fault) |
| Freed | automatic (frame pop) | destructor / RAII |

**Small, fixed, hot → stack array / `std::array`.** Large or runtime-sized →
`std::vector` (with `reserve()` to avoid regrow). Huge fixed → `static` or heap.

⚠️ `int big[1'000'000]` on stack → **stack overflow** (folder 08 lesson 05).

---

## `reserve()` — vector regrowth

```cpp
std::vector<int> v;
for (int i = 0; i < 100000; ++i) v.push_back(i);   // ⚠️ ~17 reallocations + copies

std::vector<int> v;
v.reserve(100000);                                  // ✅ one allocation
for (int i = 0; i < 100000; ++i) v.push_back(i);
```

Har reallocation = new block + copy all + free old. `reserve(upperBound)` ise
khatam kar deta hai. (Folder 19.)

---

## Alignment (folder 05 file 05, folder 11)

```cpp
alignas(64) std::array<float, 16> row;     // cache-line aligned -- SIMD + no false sharing
```

`alignas` se array ko cache-line (64) ya SIMD-register (16/32/64) boundary pe
rakho — vectorized loads faster, false sharing (folder 28) avoid.

---

## Andar kya hota hai

- Sequential array loop → prefetcher streams data → loop is **memory-bandwidth
  bound**, running at ~tens of GB/s.
- Strided access (AoS single-field, column-major) → cache line ka fraction used →
  effective bandwidth divided by (line_size / element_used).
- `-O2` vectorizes contiguous same-type loops (SoA ideal); AoS single-field
  needs gather (slower, needs `-O3`/`-march=native`).
- Stack array: frame already in L1/L2 → first touch cheap. Fresh heap pages →
  page fault + zero-fill on first touch (folder 29).

> **HFT relevance:** Data layout is *the* biggest single-thread performance lever
> in HFT. Order books, market-data snapshots, analytics buffers are **SoA / flat
> arrays**, cache-line aligned, stack or preallocated. Hot loops touch only the
> fields they need, contiguously, and vectorize. `std::vector` is preallocated
> once (`reserve`) — never regrown on the hot path. `int**` / `list` / node
> graphs are banned from hot code. Folders 32, 36, 39.

---

## Hands-on

```bash
./build.ps1 fast 09-ARRAYS/examples/07_aos_vs_soa.cpp        # AoS vs SoA
# also -O3 -march=native -- gap change?
g++ -std=c++20 -O3 -march=native 09-ARRAYS/examples/07_aos_vs_soa.cpp -o aos3 && ./aos3
```

---

## ⚠️ Traps

### Trap 1 — AoS for field-subset analytics
```cpp
for (auto& e : entities) totalX += e.x;   // ⚠️ 5/6 of each line wasted
```

### Trap 2 — huge stack array
```cpp
void f() { double m[2000][2000]; }        // ⚠️ 32 MB -> stack overflow
```

### Trap 3 — vector without `reserve` in a hot fill loop
```cpp
for (...) v.push_back(x);                  // ⚠️ repeated reallocation
```

### Trap 4 — micro-optimizing layout without profiling
Profile first. For small data (fits in L1/L2), AoS vs SoA barely matters.

### Trap 5 — `-O0` benchmark
Meaningless. `-O2` minimum.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Array speed = element count" | Access pattern (stride, contiguity) — up to 8x |
| "AoS vs SoA doesn't matter" | ~4x for field-subset access (measured) |
| "SoA always faster" | AoS wins for whole-entity access |
| "Stack vs heap array — same speed" | Alloc cost + first-touch faults differ |
| "`std::vector` grows for free" | Each regrow = alloc + copy all + free |

---

## Exercises

1. **AoS vs SoA:** `examples/07_aos_vs_soa.cpp` run at `-O2` and `-O3
   -march=native`. Table: TASK A & B, both flag sets. Explain each.

2. **Whole-entity access:** add a TASK C to the example — "for each entity,
   `x = x + y + z + vx + vy + vz`" (all 6 fields). Now which layout wins?

3. **`reserve`:** fill a `std::vector<int>` with 1M elements, with and without
   `reserve(1'000'000)`. `-O2`, time. Reallocation count (instrument via a
   custom allocator or `capacity()` logging).

4. **Stack vs heap:** sum a 100000-int array — `int a[100000]` (stack) vs
   `std::vector<int>(100000)` (heap), including allocation. First-run vs
   warm-run difference?

5. **Alignment:** `std::array<float, 16>` vs `alignas(64) std::array<float, 16>`
   — SIMD add loop, `-O3 -march=native`, time. `-fopt-info-vec`.

6. **Column-major on flat:** `std::vector<int> m(N*N)` — row-major vs
   column-major sum. Same ~8x as folder 07 file 09?

---

## Interview questions

1. Arrays fast kyun (4 reasons)?
2. AoS vs SoA — kab kaunsa? Field-subset access pe kya hota hai?
3. Stack array vs heap `std::vector` — allocation, size, locality?
4. `std::vector` regrowth ki cost? `reserve` ka role?
5. `alignas` array pe — kyun (SIMD, false sharing)?
6. `-O3 -march=native` AoS/SoA gap ko kaise affect karta hai?

---

## Next
→ [`11-common-array-bugs.md`](11-common-array-bugs.md)
