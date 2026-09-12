# 02 — Cache lines: the 64-byte unit of transfer

## Prerequisites
- `01-memory-hierarchy.md`
- `31-CPU-ARCHITECTURE/02-registers.md` (alignment ka intro)

## Yeh topic abhi kyun
Hierarchy ne bataya "DRAM dheema hai." Ab **granularity**: cache DRAM se ek
byte nahi laata, na ek word — **poori 64 bytes** ("cache line") ek saath.
Yeh ek number — 64 — cache optimization ka aadha syllabus explain kar deta:
spatial locality kyun kaam karti, false sharing kyun hota, struct layout kyun
matter karta, `alignas(64)` kyun likhte hain.

---

## Line kya hai

Cache ek flat byte-array nahi. Woh **lines** (ya "blocks") mein bata hua hai,
har line **64 bytes** (current x86: Intel, AMD; kai ARM bhi 64, Apple M-series
128). Address ke lowest 6 bits (2⁶ = 64) "line ke andar offset" batate hain;
baaki bits line ko identify karte (lesson 03).

Jab bhi koi byte cache mein nahi milta ("miss"), CPU us byte ki **poori
64-byte aligned line** upar ki layer se laata:

```
address 0x...1234 chahiye  ->  line = 0x...1200 .. 0x...123F  (64 B) poori aati
                               (0x1200 = 0x1234 & ~63)
```

Aap ek `char` maango — 64 bytes transfer hote. Aap ek `double` (8 B) maango
jo line boundary pe hai — abhi bhi 64 (ya do lines to 128, neeche).

---

## Kyun 64? (design bet)

1. **DRAM burst.** DRAM ek row activate karke usme se burst mein data deta —
   64 B ~ ek natural burst size (8 transfers × 8 B). Chhota maangna utna hi
   mehnga.
2. **Spatial locality bet.** Programs jab address X chhute hain to X+1, X+8,
   X+16 bhi jaldi chhute hain (agla array element, agla struct field). Ek line
   laake CPU shart lagata hai ki "aas-paas ka bhi chahiye hoga" — aur mostly
   jeet jaata.
3. **Tag overhead.** Har line ke saath metadata (tag, valid, dirty, coherence
   state). Chhoti lines → zyada lines → zyada tag RAM (waste). Badi lines →
   kam tags, par ek line mein zyada "shayad na chahiye" data (over-fetch).
   64 B sweet spot nikla.

Trade-off: badi line = zyada spatial locality capture, par zyada bandwidth
waste jab aap sirf 4 bytes use karo (example `02` — random 64-B line, 4 B
use = 16x waste).

---

## Empirically dekho — example `01` aur `02`

`01_cache_line_size.cpp`: ek 64 MiB buffer, badhte stride se scan. Har stride
pe same total distinct-lines, par accesses/pass = N/stride. Per-access cost:

```
 stride(B)   ns/access (min of 9, is box)
    1          0.32     \
    2          0.34      |  stride < line: ek line ke andar kai hits,
    4          0.37      |  miss ki cost amortize -> saste
    8          0.50     /
   16          1.0      \
   32          1.6       |  stride line size ke paas: kam hits/line ->
   64          3.4       |  cost tezi se chadhta
  128          5.4      /
  256          3.9      (noise + TLB)
```

Ramp ~64 B ke aas-paas flatten hona chahiye (= line size). **Is box pe woh
128 tak chadhta** — kyunki sequential access pe HW prefetcher (lesson 06)
kaam karta aur knee "smear" ho jata. ⚠️ CLAUDE.md Rule 2: textbook ka clean
knee real prefetched hardware pe fuzzy hota. **Sharp 64-B cliff dekhna hai
to prefetcher ko haraao** — example `02` PART 2 (random line order): sequential
4.6 ns/line vs random 32 ns/line, ~7x, kyunki ab har line ek asli miss hai.

---

## `hardware_*_interference_size` (C++17)

```cpp
#include <new>
std::hardware_destructive_interference_size   // >= isse door rakho => no false sharing
std::hardware_constructive_interference_size  // <= isme rakho => ek hi line, saath aate
```

Is box pe dono **64**. Practically: false-sharing padding ke liye 64 (ya 128
for cross-line-pair prefetch safety — Intel L2 spatial prefetcher lines ko
jodo mein laata). MinGW pe guard chahiye:

```cpp
#ifdef __cpp_lib_hardware_interference_size
  constexpr size_t kLine = std::hardware_destructive_interference_size;
#else
  constexpr size_t kLine = 64;
#endif
```

---

## Struct layout aur line straddle

Ek 48-byte struct 64-B boundary pe start ho to poora ek line mein. Par agar
woh offset 40 pe start ho (kisi array ke andar bad padding se), to bytes
40..63 line A mein, 64..87 line B mein → **ek object access = 2 cache misses**.

```
line A: [........................ obj.a obj.b obj.c ]   <- pehle 24 bytes
line B: [ obj.d obj.e ........................... ]      <- baaki 24 bytes
```

Isliye:
- Performance-critical structs ko `alignas(64)` (ya line ke multiple).
- `std::vector<T>` ka data allocator se ~16-B aligned aata; agar `sizeof(T)`
  64 ka divisor/multiple nahi, to elements lines ko straddle karte rehte.
- `sizeof(T)` ko 64 ka factor rakhna (8, 16, 32, 64) → har element predictably
  seated.

---

## Alignment tools

```cpp
alignas(64) struct Hot { ... };            // type/variable ko 64-align
static_assert(sizeof(Hot) % 64 == 0);      // koi straddle nahi
static_assert(alignof(Hot) == 64);

#include <cstddef>
offsetof(MyStruct, field);                  // field line ke andar kahan

// heap pe aligned:
void* p = ::operator new(n, std::align_val_t{64});   // C++17
// ya std::aligned_alloc(64, n)  (n must be multiple of 64)
```

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "sirf 4 bytes padh raha hoon, sasta hoga"
Nahi — 64 B transfer hote. Random-access pattern mein aap 64 laake 4 use karte
= 16x bandwidth waste (example `02`). Fix: line ka poora use karo (contiguous
iteration, SoA — lesson 09).

### Trap 2 — hot aur cold fields ek line pe
Ek struct jisme `price` (har tick padha) aur `debug_name` (kabhi nahi) ek hi
64-B line pe → har `price` load `debug_name` ke bytes bhi laata, cache jagah
waste. Fix: hot/cold split (lesson 15).

### Trap 3 — `alignas` bhool ke false sharing (lesson 07)
Do threads ke counters adjacent → same line → line har write pe cores ke beech
bounce. Example `04`: ~6-40x slowdown.

### Trap 4 — `alignas(64)` har chhoti cheez pe
Ek `alignas(64) int` = 4 bytes data + 60 bytes padding, aur arrays mein 16x
memory. Sirf **shared-by-threads** ya **straddle-prone hot** structs pe.

### Trap 5 — stack pe `alignas` maan lena guaranteed
Purane ABIs / kuch compilers stack ko sirf 16-align karte the. Modern x86-64
mostly theek, par heap ke liye explicit aligned-new use karo jab pakka chahiye.

### Trap 6 — vector of straddling structs
`struct P { double a,b,c; };` sizeof 24. `vector<P>` mein element 2 offset 48,
element 3 offset 72 → line straddle. `alignas(32)` ya ek dummy field se 32,
ya SoA.

---

## > **HFT relevance**

> - **Ek "message" object ek line mein.** Order/quote/fill jaisa hot object
>   ≤ 64 B rakho, `alignas(64)`, taaki ek access = ek line = ek possible miss,
>   do nahi.
> - **Ring buffer slots line-aligned.** SPSC queue (folder 28) ka har slot 64
>   ka multiple → producer/consumer alag lines pe → no false sharing.
> - **Head/tail pointers alag lines pe.** `alignas(64) atomic<size_t> head;`
>   `alignas(64) atomic<size_t> tail;` — warna producer ka `tail` update
>   consumer ke `head` line ko bounce karta (folder 28 mein yahi kiya tha).
> - **Hot struct ka `sizeof` 64 ka factor.** `static_assert` laga do — koi
>   accidental field add karke straddle na kar de.
> - **Cold data alag.** Logging strings, debug counters, config — alag struct,
>   alag allocation, hot line se door.

---

## Hands-on

```bash
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/01_cache_line_size.cpp
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/02_stride_access.cpp

# apne struct ka layout dekho:
#   g++ -std=c++20 -Xclang -fdump-record-layouts ...   (clang)
#   ya pahle_line pe: static_assert + offsetof print karo
```

Chhota experiment: ek `struct P { double a, b, c; };` banao, `vector<P>` of
1M, `sum += p.a` karo. Phir `alignas(32)` laga ke dubara. Time compare — kuch
elements ka straddle hat jaayega.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "cache byte-by-byte laata" | 64-byte line, hamesha, aligned |
| "chhota read = chhoti cost" | poora line transfer, chahe 1 byte chahiye |
| "line size CPU-dependent, pata nahi" | current x86 = 64; `hardware_..._interference_size` |
| "`alignas` sab jagah lagao" | sirf shared / straddle-prone hot structs |
| "struct kahin bhi rakh do" | line straddle = 2 misses/object |
| "64 random hai" | DRAM burst + spatial locality + tag overhead ka balance |

---

## Exercises

1. `struct Tick { int64_t ts; double px; int32_t qty; };` — `sizeof`? `alignof`?
   `vector<Tick>` mein element 5 kis offset pe, koi straddle?

   <details><summary>Answer</summary>

   `int64_t` 8 + `double` 8 + `int32_t` 4 = 20 data bytes; `alignof` = 8 (sabse
   bada member) → `sizeof` 8 ke multiple pe round → **24** (4 bytes tail
   padding). `vector<Tick>` element 5 offset 120, element 6 offset 144 —
   sab 8-aligned, koi single element line (64) ko straddle **nahi** karta jab
   tak base 64-aligned ho (vector data ~16-aligned aata, to element 3 offset
   72..95 line 64-127 ke andar — OK). Straddle tab hota jab `sizeof` 64 ka
   divisor na ho *aur* base off ho; yahan 24 se har ~8/3 elements line-boundary
   cross karti hai par ek element kabhi 2 lines mein nahi (24 < 64). Zyada
   important: 4 bytes/element padding waste — 1M elements = 4 MB junk.
   </details>

2. Example `01` ka ramp is box pe 64 pe flatten nahi hua, 128 tak chadha.
   Do reasons do.

   <details><summary>Answer</summary>

   (1) **HW prefetcher**: sequential access pe next-line / stream prefetcher
   aage ki lines pehle hi laa deta → stride 64 pe bhi thoda "free" data → cost
   64 pe cleanly plateau nahi hota, dheere-dheere chadhta rehta jab tak
   prefetcher poori tarah har-line-alag na dekh le. (2) **Adjacent-line /
   spatial prefetcher** lines ko jodo mein laata — stride 128 (har doosri
   line) pe woh pair ka aadha waste karta → per-access cost 128 tak badhta
   rehta. Bade stride pe **TLB coverage** bhi badalti (kam pages/pass).
   Isliye sharp 64-B cliff sirf prefetcher-defeated (random) test mein
   dikhta — example `02` PART 2.
   </details>

3. Aapke SPSC ring mein `struct { atomic<uint64_t> head; atomic<uint64_t>
   tail; char buf[4096]; };` — kya galat hai, fix?

   <details><summary>Answer</summary>

   `head` offset 0, `tail` offset 8 → **same 64-B line**. Producer har push pe
   `tail` likhta, consumer har pop pe `head` likhta → woh line dono cores ke
   beech har operation pe bounce (false sharing, lesson 07) → throughput gir
   jaata aur jittery. Fix: `alignas(64) atomic<uint64_t> head; char
   pad0[56]; alignas(64) atomic<uint64_t> tail; char pad1[56];` — ya bas
   dono ko `alignas(hardware_destructive_interference_size)`. Consumer-mostly
   aur producer-mostly fields ko alag lines pe rakho. (Folder 28 SPSC.)
   </details>

---

## Interview questions

1. Cache line kya, kitne bytes, aur woh "unit of transfer" ka matlab.
2. 64 hi kyun (na 16, na 256) — do-teen design pressures.
3. Line straddle — kaise hota, cost, kaise rokte.
4. `std::hardware_destructive_interference_size` kya batata, kahan use.
5. Ek struct mein hot + cold field ek line pe — problem aur fix.
6. Spatial locality "bet" — cache line us bet ko kaise implement karta.
7. `sizeof(T)` ko 64 ka factor rakhne se kya milta arrays mein.

---

## Next
→ [`03-cache-organization.md`](03-cache-organization.md)
