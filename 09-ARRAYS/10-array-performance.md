# 10 — Array performance — cache, prefetch, AoS vs SoA, stack vs heap

## Prerequisites
- [`08-std-array.md`](08-std-array.md), [`09-std-span.md`](09-std-span.md)
- `07-LOOPS/09-loop-performance.md` (cache locality ~8x measured, vectorization)
- `06-CONDITIONS/08-branch-prediction-intro.md` ("measure karo")

## Yeh topic abhi kyun
Arrays fast kyun hain — aur unhe fast rakhne ke rules. Contiguity + predictable
stride = cache aur prefetcher ke best friends. Aur ek bada design lever: same data,
do layouts — **AoS vs SoA** — jismein kai guna farq aata hai (yahan measure karenge).

---

## Arrays fast kyun

1. **Contiguous** → ek cache line (64 B = 16 `int`) mein 16 elements. Ek miss → 16 kaam ki
   values.
2. **Predictable stride** → hardware prefetcher pattern pakad leta hai aur aage ka data pehle
   se le aata hai → memory ka intezaar chhup jaata hai.
3. **`arr[i]` = ek scaled load** (`base + i*size`) — CPU ka addressing mode, ek instruction.
4. **Auto-vectorization** → simple array loops `-O2`/`-O3` pe SIMD (ek instruction mein 4-16
   elements).
5. **Branch-predictable** loop ki limit.

`std::list` / pointer se jude data ko inme se **ek bhi** faayda nahi milta — har `next` ek
cache miss.

---

## Traversal order — recap (folder 07 file 09)

Row-major 2D array ko column-major traverse karna → **~8x slow** (naapa, 4096²).
**Rule: inner loop = woh index jo memory mein sabse tez badalta hai (row-major → aakhri index).**

---

## AoS vs SoA — layout se speed tay hoti hai

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

Analogy: AoS ek almari hai jisme har student ki file mein uske saare kaagaz hain. "Sabke marks
jodo" bolo, to har file kholni padegi aur baaki kaagaz beech mein aayenge. SoA mein marks ki
ek alag register hai — seedha ek line mein padh lo.

### Naapa (`examples/07_aos_vs_soa.cpp`, `-O2`, N=4M)

| Task | GCC 15.1 (pehle naapa) | GCC 16.2 (3 runs) |
|---|---|---|
| **A** — sirf ek field (`x = x*3 + 1`) | AoS ~542 ms, SoA ~124 ms → **~4.4×** | AoS 488–566 ms, SoA 89–98 ms → **5.4–5.8×** |
| **B** — 6 mein se 2 fields (`x += vx`) | AoS ~502 ms, SoA ~183 ms → **~2.7×** | AoS 489–546 ms, SoA 106–111 ms → **4.6–4.9×** |

**Kyun:** TASK A ko sirf `x` chahiye. AoS mein har entity ke 24 bytes mein se sirf 4 bytes
(`x`) kaam ke — yaani har cache line ka 5/6 hissa bekaar ghoom ke aata hai. SoA mein poori line
kaam ki hai, aur contiguous same-type data SIMD vectorize ke liye aasaan hai.

**Compiler badla, farq bada:** wahi code, wahi machine — GCC 16.2 ne SoA loops ko pehle se
behtar vectorize kiya, isliye gap badh gaya (khaaskar TASK B: ~2.7× → ~4.7×). Sabak: performance
numbers **compiler version ke saath** likho, aur upgrade ke baad dobara naapo.

### AoS kab behtar hai
Jab **poora entity ek saath** chahiye — "entity `i` ko move karo, uske saare fields chahiye".
AoS mein ek entity ke 6 fields ek (ya do) cache line mein hain; SoA mein 6 alag arrays se 6
alag lines aayengi.

| Access pattern | Layout |
|---|---|
| Bahut saari entities ke kuch fields (analytics, physics, SIMD) | **SoA** |
| Ek entity ke saare fields ek saath | **AoS** |
| Mila-jula | AoSoA (hybrid), ya profile karke decide karo |

⚠️ `-O3 -march=native` pe compiler AoS loop ko bhi vectorize kar sakta hai — par vectorize hona
≠ tez hona. `11-STRUCTS/examples/04_aos_vs_soa.cpp` (max of one field, Zen 2, GCC 16.2) mein AoS
loop vectorize hua (gather nahi — har element ka alag `vmovd` load) aur phir bhi ~210 ms pe atka
raha, kyunki seema memory bandwidth thi; SoA 93 → 25 ms gaya aur gap 2.3× se **8–10×** ho gaya.
Chhote tasks mein gap ulta bhi ja sakta hai. **Apne target hardware + flags pe naapo.**

---

## Stack vs heap arrays

```cpp
void f() {
    int a[1000];                       // STACK -- allocation lagbhag free (rsp move), cache mein garam
    std::array<int, 1000> b;           // STACK -- wahi
    std::vector<int> c(1000);          // HEAP -- new (bookkeeping), shayad thanda, scope end pe free
    static int d[1000];                // BSS  -- allocation free, zero-init, poore program tak
}
```

| | Stack `int[N]` / `std::array` | Heap `std::vector` |
|---|---|---|
| Allocation cost | ~0 | `new` (lock, bookkeeping) |
| Size | compile-time; kuch MB ki limit (overflow!) | runtime; GBs |
| Locality | bahut achhi (frame cache mein garam) | depend karta hai (naye pages pe fault ho sakta hai) |
| Free | apne aap (frame pop) | destructor / RAII |

**Chhota, fixed, hot → stack array / `std::array`.** Bada ya runtime size → `std::vector`
(`reserve()` ke saath taaki baar-baar na badhe). Bahut bada fixed → `static` ya heap.

⚠️ Stack pe `int big[1'000'000]` → **stack overflow** (folder 08 lesson 05).

---

## `reserve()` — vector ka baar-baar badhna

```cpp
std::vector<int> v;
for (int i = 0; i < 100000; ++i) v.push_back(i);   // ⚠️ 18 allocations (pehla + 17 regrow) + copies

std::vector<int> v;
v.reserve(100000);                                  // ✅ ek allocation
for (int i = 0; i < 100000; ++i) v.push_back(i);
```

Har regrow = naya block + saare elements copy/move + purana free. `capacity()` badalne pe gin
ke dekha (GCC 16.2): 100000 `push_back` mein **18 allocations**, final capacity 131072 (har
baar double). `reserve(upperBound)` ise khatam kar deta hai. (Folder 19.)

---

## Alignment (folder 05 file 05, folder 11)

```cpp
alignas(64) std::array<float, 16> row;     // cache-line aligned -- SIMD + false sharing se bachav
```

`alignas` se array ko cache-line (64) ya SIMD register (16/32/64) ki boundary pe rakho —
vectorized loads aasaan, aur false sharing (folder 28) se bachav.

---

## Andar kya hota hai

- Sequential array loop → prefetcher data ki dhaara pehle se laata rehta hai → loop ki speed ki
  seema aksar **memory bandwidth** banti hai, ek-ek cache miss nahi.
- Koodte hue access (AoS ka single field, column-major) → har cache line ka chhota hissa hi kaam
  ka → utne hi data ke liye zyada lines laani padti hain.
- `-O2` seedhe-saade contiguous same-type loops vectorize karta hai (SoA ideal). AoS ke single
  field ke liye compiler ko har stride se alag-alag load karke vector banana padta hai — hota bhi
  hai (GCC 16.2 `-O3 -march=native` pe dekha), par bandwidth ki seema nahi hatti.
- Stack array: frame pehle se L1/L2 mein → pehla touch sasta. Heap ke naye pages → pehle touch pe
  page fault + zero-fill (folder 29).

> **HFT relevance:** Single thread pe data layout sabse bada performance lever hai. Order books,
> market-data snapshots, analytics buffers **SoA / flat arrays** hote hain — cache-line aligned,
> stack pe ya pehle se allocate kiye hue. Hot loops sirf zaroori fields chhoote hain, contiguous
> tareeke se, aur vectorize hote hain. `std::vector` ek baar `reserve` hota hai — hot path pe kabhi
> nahi badhta. `int**` / `list` / node graphs hot code mein ban hain. Aur jab compiler upgrade ho,
> layout benchmarks dobara chalao — yahan GCC 16.2 ne gap badal diya. Folders 32, 36, 39.

---

## Hands-on

```bash
./build.ps1 fast 09-ARRAYS/examples/07_aos_vs_soa.cpp        # AoS vs SoA
# -O3 -march=native pe bhi -- gap badla?
g++ -std=c++20 -O3 -march=native 09-ARRAYS/examples/07_aos_vs_soa.cpp -o aos3 && ./aos3
```

---

## ⚠️ Traps

### Trap 1 — field-subset analytics ke liye AoS
```cpp
for (auto& e : entities) totalX += e.x;   // ⚠️ har line ka 5/6 bekaar
```

### Trap 2 — bahut bada stack array
```cpp
void f() { double m[2000][2000]; }        // ⚠️ 32 MB -> stack overflow
```

### Trap 3 — hot fill loop mein bina `reserve` ke vector
```cpp
for (...) v.push_back(x);                  // ⚠️ baar-baar reallocation
```

### Trap 4 — bina profile kiye layout ka micro-optimization
Pehle profile karo. Chhota data (L1/L2 mein aa jaaye) ho to AoS vs SoA se lagbhag fark nahi padta.

### Trap 5 — `-O0` benchmark
Bekaar hai. Kam se kam `-O2`.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Array ki speed = elements ki ginti" | Access pattern (stride, contiguity) — 8x tak |
| "AoS vs SoA se fark nahi padta" | Field-subset access pe kai guna (naapa: 16.2 pe 5.4–5.8×) |
| "SoA hamesha tez" | Poore entity ke access pe AoS jeetta hai |
| "Stack vs heap array — same speed" | Allocation cost + pehle touch ke faults alag |
| "`std::vector` free mein badhta hai" | Har regrow = allocation + saara copy + free |
| "Benchmark ka result compiler badalne pe bhi wahi rahega" | Yahan 15.1 → 16.2 pe TASK B ~2.7× → ~4.7× hua |

---

## Exercises

1. **AoS vs SoA:** `examples/07_aos_vs_soa.cpp` ko `-O2` aur `-O3 -march=native` pe chalao.
   Table: TASK A & B, dono flag sets. Har ek ko samjhao.

2. **Poore entity ka access:** example mein TASK C jodo — "har entity ke liye
   `x = x + y + z + vx + vy + vz`" (saare 6 fields). Ab kaunsa layout jeeta?

3. **`reserve`:** 1M elements se `std::vector<int>` bharo, `reserve(1'000'000)` ke saath aur
   bina. `-O2`, time lo. Reallocations gino (`capacity()` badalne pe count karo).
   <details><summary>Answer (count wala hissa)</summary>

   Is lesson ka 100000 wala case GCC 16.2 pe **18 allocations** deta hai (capacity 1, 2, 4, …,
   131072). 1M ke liye bhi doubling hi hai — khud gino. Time ka farq apni machine pe naapo.
   </details>

4. **Stack vs heap:** 100000-int array ka sum — `int a[100000]` (stack) vs
   `std::vector<int>(100000)` (heap), allocation samet. Pehle run aur garam run mein farq?

5. **Alignment:** `std::array<float, 16>` vs `alignas(64) std::array<float, 16>` — SIMD add loop,
   `-O3 -march=native`, time lo. `-fopt-info-vec` bhi dekho.

6. **Flat array pe column-major:** `std::vector<int> m(N*N)` — row-major vs column-major sum.
   Folder 07 file 09 jaisa ~8x aaya?

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
