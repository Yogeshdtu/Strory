# 09 — Loop performance — cache locality, unrolling, vectorization

## Prerequisites
- [`03-for-loop.md`](03-for-loop.md), [`05-nested-loops.md`](05-nested-loops.md)
- `06-CONDITIONS/08-branch-prediction-intro.md` (pipeline, "measure karo")
- `01-PROGRAMMING-BASICS/11-memory-basics.md` (RAM, addresses)
- `05-OPERATORS/05-bitwise-operators.md` mein power-of-2 / alignment chhua tha

## Yeh topic abhi kyun
Loop woh jagah hai jahan program apna 99% time bitaata hai. Aur ek loop kitna
tez chalta hai woh **sirf** iterations ki ginti pe nahi — **memory ko kis order
mein chhoota hai** us pe bhi depend karta hai. Same code, same data, alag access
order → **8x** farq (hum abhi maapenge).

Yeh poore performance track (folders 31–36) ka foundation hai. Yahan hum
**dekhenge aur maapenge**; mechanism folder 32 (cache) aur 33 (compiler opt).

---

## Memory pyramid — sab RAM barabar nahi

CPU ke liye data laane ki cost (approx, modern x86):

```
   register        : 0 cycles       (CPU ke andar)
   L1 cache        : ~4 cycles      ~32-48 KB   per core
   L2 cache        : ~12 cycles     ~256KB-2MB  per core
   L3 cache        : ~40 cycles     ~8-32 MB    shared
   RAM (DRAM)      : ~200-300 cycles            GBs
```

RAM se ek value laana ≈ **~100 additions** ke barabar. Isliye trick yeh hai:
**data ko cache mein rakho, aur jab laao to poora use karo.**

### Cache line — memory 64-byte blocks mein aati hai

Jab aap `a[i]` maangte ho aur woh cache mein nahi (miss), CPU `a[i]` ke aas-paas
ka poora **64-byte block** (= 16 `int`s) laata hai. Agli 15 `int`s ab "free" hain
(already cache mein).

**→ Sequential access = 1 miss har 16 elements. Random/strided access = 1 miss
har element.**

---

## Demo 1 — Row-major vs column-major (MEASURED)

`examples/05_cache_locality.cpp`. Ek `4096 x 4096` `int32` matrix (64 MiB, L3 se
kai guna bada), row-major stored: element `(i, j)` → index `i*N + j`.

```cpp
// ROW-MAJOR traversal: inner loop j  -> consecutive addresses
for (i = 0; i < N; ++i)
    for (j = 0; j < N; ++j)
        sum += m[i*N + j];

// COLUMN-MAJOR traversal: inner loop i  -> har step N*4 = 16384 bytes ki chhalang
for (j = 0; j < N; ++j)
    for (i = 0; i < N; ++i)
        sum += m[i*N + j];
```

### Measured (GCC 15.1, `-O2`, x86-64)

```
  row-major   (inner j, sequential)      :    8.3 ms
  column-major (inner i, stride 16384 B) :   69.0 ms
                                            --------
                                            8.4x slower
```

**Data values same. Total same. Sirf traversal ORDER badla.**

### Kyun

| | Row-major | Column-major |
|---|---|---|
| Ek cache line (16 ints) se use | poori (16/16) | sirf 1, phir line evict |
| Cache misses | ~N²/16 | ~N² (16x zyada) |
| Hardware prefetcher | pattern pakadta hai, aage ka data pehle se laata hai | stride bada, kam madad |
| TLB (page lookups) | ek page se kai rows | har row alag 4 KB page → TLB misses |

Column-major loop **memory bandwidth pe ruk jaata hai** — CPU idle baithi rehti
hai data ka wait karte hue.

### Fix
- **Loops ko memory order mein chalao** — inner loop wahi index jo memory mein
  fastest badalta hai (row-major storage → inner loop = last index).
- Agar columns chahiye **hi** (algorithm demands) → data ko **column-major store
  karo**, ya ek baar **transpose** karlo, phir har scan fast.
- Reuse-heavy kaam (matrix multiply) → **tiling/blocking** (folder 32). Single-pass
  scan ke liye tiling faayda nahi deta — bas memory order kaafi hai.

> **HFT relevance:** Yeh ek soch order book, market-data buffers, aur analytics
> layout ko drive karti hai: **"jo saath process hota hai, woh memory mein saath
> rakho."** AoS (`struct Order { price; qty; id; ... }` ka array) vs SoA (alag
> `prices[]`, `qtys[]`, `ids[]`) — agif aap sirf `price` pe loop karte ho, SoA
> har cache line 16 prices deta hai, AoS shayad 4. Folder 32, aur order book
> folders 39–40.

---

## Demo 2 — Loop unrolling: manual vs compiler (SURPRISING)

**Sawaal:** naive sum-loop ko haath se 4x "unroll" karne se tez hota hai?

```cpp
// naive: ek accumulator
int64_t s = 0;
for (size_t i = 0; i < n; ++i) s += a[i];

// manual 4x unroll: 4 ALAG accumulators
int64_t s0=0, s1=0, s2=0, s3=0;
for (size_t i = 0; i + 3 < n; i += 4) {
    s0 += a[i+0]; s1 += a[i+1]; s2 += a[i+2]; s3 += a[i+3];
}
// + tail loop, phir s0+s1+s2+s3
```

### Measured (`examples/06_loop_unroll.cpp`, GCC 15.1, ~4.2M int32, x86-64)

| flags | naive | manual 4x unroll | verdict |
|---|---|---|---|
| `-O2` | ~1570 ms | **~680 ms** | manual **2.3x TEZ** |
| `-O2 -funroll-loops` | ~1060 ms | ~740 ms | manual thoda tez |
| `-O2 -march=native` | ~545 ms | ~530 ms | **barabar** |
| `-O3` | **~600 ms** | ~725 ms | manual **SLOWER** |
| `-O3 -march=native` | ~550 ms | ~660 ms | manual **SLOWER** |

**Ek hi source file. Sirf flags badalne se manual unroll kabhi 2.3x faster,
kabhi 1.2x slower.**

### Kyun

- Naive loop mein `s += a[i]` ek **loop-carried dependency** hai: har add pichle
  add ke result ka wait karta hai. Yeh **latency-bound** hai (add latency ~1
  cycle, par serialised).
- Manual unroll ke **4 independent accumulators** us chain ko todte hain — 4 adds
  ek saath "in flight" → **throughput-bound** → tez.
- `-O2` akela GCC 15 is int32→int64 widening reduction ke liye yeh khud nahi
  karta. `-O3` **ya** `-march=native` pe compiler **khud** vectorize + multi-
  accumulate karta hai → naive ~2.5x tez ho jaata hai.
- Aur tab manual unroll **compiler ki apni vectorization ko constrain** karke
  **slow** kar deta hai.
- `std::accumulate` hamesha naive jaisa — koi jaadu nahi.

### Takeaway

```
Hand-unrolling LAST RESORT hai. Order:
  1. Naive + readable code likho
  2. Sahi flags: -O2 (kam se kam), aksar -O3 / -march=native
  3. Profiler ne bola tabhi is loop pe jao
  4. Tab bhi pehle `-S` / godbolt dekho compiler ne kya kiya
  5. Hand-unroll sirf jab measure se pakka faayda dikhe -- aur woh
     brittle hai (agli compiler/CPU pe ulta pad sakta hai)
```

---

## Vectorization (SIMD) — compiler ka asli hathiyaar

Ek **SIMD** instruction ek saath 4/8/16 values pe operate karti hai (SSE=128-bit,
AVX2=256-bit, AVX-512=512-bit). `-O2`/`-O3` pe compiler simple loops ko auto-
vectorize karta hai:

```cpp
for (size_t i = 0; i < n; ++i) c[i] = a[i] + b[i];
// -O3 -march=native pe: ~8 int32 additions per instruction
```

### Kya vectorization ko rokta hai
- **Loop-carried dependencies** (jaise reduction — par compiler split kar sakta hai)
- **Aliasing** — `a` aur `c` overlap kar sakte hain? `__restrict` / `std::assume_aliased` hint
- **Branches** loop body mein (data-dependent) — kabhi masked, kabhi nahi
- **Function calls** (jab tak inline na ho)
- **Non-contiguous access** (linked list, strided)
- **Complex index arithmetic**

### Dekho compiler ne vectorize kiya ki nahi
```bash
g++ -std=c++20 -O3 -march=native -fopt-info-vec-optimized file.cpp -c
# "loop vectorized using 32 byte vectors" jaise messages
```

`-march=native` zaroori hai — uske bina compiler sirf baseline SSE2 (128-bit)
maanta hai; AVX2/AVX-512 nahi.

---

## Loop hoisting / invariant code motion

Compiler loop-invariant kaam (jo har iteration same hai) loop ke bahar nikaal
deta hai `-O2` pe:

```cpp
for (size_t i = 0; i < v.size(); ++i)      // v.size() har iteration -- par inline + hoisted
    total += v[i] * factor + offset;         // factor, offset loop-invariant -> hoist

// compiler roughly:
const size_t n = v.size();
const int k = offset;                         // (simplified)
for (size_t i = 0; i < n; ++i) total += v[i] * factor + k;
```

Aap phir bhi **arbitrary function calls** condition/body mein daal ke isse tod
sakte ho (compiler ko pata nahi woh pure hai):

```cpp
for (size_t i = 0; i < v.size(); ++i)
    if (i < expensivePureButOpaque()) ...    // har iteration call -- hoist nahi hua
```

Loop-invariant ko khud `const` variable mein nikaal do jab doubt ho.

---

## Practical checklist — tez loop kaise likhein

1. **Contiguous data** — `std::vector` / `std::array`, `std::list` nahi (pointer
   chasing = har node cache miss)
2. **Memory order mein iterate** — inner loop = fastest-changing index
3. **`const auto&`** range-for mein — accidental copies nahi (file 04, ~50x)
4. **Body simple** — no allocation, no I/O, minimal branches, no virtual calls in
   the hottest inner loop
5. **`-O2` minimum**, benchmarks pe `-O3 -march=native` try karo
6. **Measure**, `-S`/godbolt dekho — assume mat karo
7. Hand-optimize (unroll, prefetch, SIMD intrinsics) **sabse aakhri mein**, profiler
   ke bolne pe, aur re-benchmark karke

---

## Hands-on

```bash
# cache locality -- -O2 zaroori
./build.ps1 fast 07-LOOPS/examples/05_cache_locality.cpp

# loop unroll -- SAARE flags try karo
g++ -std=c++20 -O2               07-LOOPS/examples/06_loop_unroll.cpp -o lu   && ./lu
g++ -std=c++20 -O3               07-LOOPS/examples/06_loop_unroll.cpp -o lu3  && ./lu3
g++ -std=c++20 -O2 -march=native 07-LOOPS/examples/06_loop_unroll.cpp -o lun  && ./lun
```

⚠️ **`-O0` pe yeh dono benchmarks jhoothe hain** — optimizer off, sab slow aur
comparison meaningless.

---

## ⚠️ Traps

### Trap 1 — `-O0` pe benchmark
Optimizer off → 5-20x slower everywhere, ratios meaningless. Hamesha `-O2`+.

### Trap 2 — column-major loop "chhota code" isliye theek
Chhota code ≠ fast. 8x slower memory-bound.

### Trap 3 — hand-unroll / SIMD intrinsics bina profile ke
Brittle, unreadable, aur aksar compiler se slow. Flags pehle.

### Trap 4 — `std::list` ko "modern vector jaisa" samajhna
Har `next` pointer chase = cache miss. Hot loops mein `vector` / flat structures.

### Trap 5 — micro-benchmark jo compiler optimize kar deta hai
`sum` ko use na karo to compiler poora loop hata deta hai. Result ko `volatile`
sink / `asm volatile` / print karke "use" karo (jaise examples mein).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Loop speed = iteration count" | Memory access order se 8x tak farq |
| "Manual unroll hamesha tez" | Flags pe depend — kabhi 2x tez, kabhi slower |
| "`-O2` sab kuch optimize kar deta hai" | `-O3` / `-march=native` aur vectorization khol dete hain |
| "Column-major bas thoda slow" | ~8x (cache misses + TLB + no prefetch) |
| "SIMD ke liye intrinsics likhne padte hain" | `-O3 -march=native` auto-vectorize karta hai simple loops |

---

## Exercises

1. **Cache measure:** `examples/05_cache_locality.cpp` chalao. Apni machine pe
   row/column ratio likho. `N` ko 1024 (4 MiB) karo — ratio kam hua? (Data ab L3
   mein fit ho sakta hai.)

2. **AoS vs SoA:** `struct P { double x, y, z; };` ka `vector<P>` (10M) — sabhi
   `x` ka sum. Phir teen alag `vector<double> xs, ys, zs` — `xs` ka sum. `-O2` pe
   time compare. Kyun farq?

3. **Unroll across flags:** `examples/06_loop_unroll.cpp` ko `-O2`, `-O3`,
   `-O2 -march=native`, `-O3 -march=native` pe chalao. Table banao. Manual unroll
   kab jeeta, kab haara?

4. **Vectorization report:** ek simple `c[i] = a[i] + b[i]` loop —
   `g++ -O3 -march=native -fopt-info-vec-optimized` se compile. Kya bola? Ab body
   mein `if (a[i] > 0)` daalo — abhi bhi vectorized?

5. **`std::list` penalty:** 5M `int`s ka sum — `std::vector<int>` se aur
   `std::list<int>` se. `-O2`. Ratio? Kyun?

6. **Loop hoisting:** ek loop jisme condition `i < v.size()` hai vs ek jisme
   `i < someOpaqueFn()` — `-O2 -S` se dekho `size()` hoist hua, function nahi.

7. **Kill the optimizer:** ek sum-benchmark likho jisme `sum` kahin use na ho.
   `-O2 -S` dekho — loop bacha? Ab `sum` ko print karo — ab bacha?

8. **Transpose fix:** column-major access chahiye — data ko ek baar transpose
   karke store karo, phir scan. Transpose ki cost + scan vs direct column-major
   scan — kitne scans ke baad transpose "paisa vasool"?

---

## Interview questions

1. Cache line kya hai? Sequential vs strided access — cache misses mein farq?
2. Row-major array ko column-major traverse karne pe kitna slow, aur kyun (3 reasons)?
3. Loop-carried dependency kya hai? Reduction loop use se kyun affected?
4. Manual loop unrolling ka faayda kahaan se aata hai — "kam iterations" nahi to kya?
5. Auto-vectorization ko kya cheezein rokti hain?
6. `-march=native` kya karta hai? Uske bina compiler kaunsa SIMD maanta hai?
7. `-O0` pe benchmark kyun invalid?
8. AoS vs SoA — kab kaunsa, cache ke hisaab se?

---

## Next
→ [`10-exercises.md`](10-exercises.md)
