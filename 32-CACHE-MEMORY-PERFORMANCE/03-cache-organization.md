# 03 — Cache organization: sets, ways, index/tag/offset

## Prerequisites
- `02-cache-lines.md`
- Binary / bit operations (`05-OPERATORS/05-bitwise-operators.md`)

## Yeh topic abhi kyun
"Line" ka pata chala. Ab: ek 32 KiB L1 mein 512 lines hoti hain — jab 513vi
line chahiye to **kaunsi evict hoti?** Aur do addresses jo bilkul alag data
hain par cache ko "same jagah" dikhte — woh ek doosre ko baar-baar nikalte
rehte hain (**conflict miss**), chahe cache mostly khaali ho. Yeh
"power-of-two stride poison hai" wali baat ka mechanism hai.

---

## Address ko cache kaise dekhta hai

Har physical address 3 hisso mein bata:

```
  ┌─────────── tag ───────────┬──── set index ────┬── block offset ──┐
   upper bits (identity)         kaunsa set             line ke andar (6 bits = 64 B)
```

- **offset** (neeche 6 bits): line ke andar konsa byte. Cache lookup mein use
  nahi, bas final byte select.
- **set index**: address kis "set" (row) mein map hota. `log2(num_sets)` bits.
- **tag**: baaki upper bits — set ke andar yeh confirm karne ke liye ki "haan
  yehi wali line hai" (kai alag lines same set mein aa sakti).

---

## Direct-mapped → set-associative → fully-associative

### Direct-mapped (1-way)
Har address ki **exactly ek** possible jagah: `set = (addr / 64) % num_sets`.
Simple, tez lookup (ek tag compare). Par: do hot addresses jinka same set
index hai → ek doosre ko har baar evict → thrash. 32 KiB direct-mapped mein
`A` aur `A + 32768` **hamesha** takraate.

### Fully-associative
Koi bhi line kahin bhi ja sakti. Zero conflict misses (sirf capacity). Par:
har lookup mein **saari** lines ke tags compare karne padte (CAM hardware) →
mehnga, sirf chhoti cache ke liye (TLB, victim cache).

### Set-associative (N-way) — real caches
Beech ka. Cache `num_sets` sets mein bata, har set mein **N lines** ("ways").
Ek address ek **fixed set** mein jaata (direct-mapped jaisa), par us set ke
andar **N mein se kisi bhi way** mein (fully-assoc jaisa, chhote scale pe).

```
is box ka L1d: 32 KiB, 64-B line, 8-way
  512 lines total / 8 ways = 64 sets
  set index = (addr >> 6) & 63          (6 bits)
  offset    =  addr       & 63          (6 bits)
  tag       =  addr >> 12
```

Lookup: set index se ek set chuno → us set ki 8 tags parallel compare → match
→ hit (offset se byte). Koi match nahi → miss → line laao, us set ki ek way
mein daalo (kis way? — replacement policy).

---

## Replacement policy

Set bhara hai, nayi line aani hai — kaunsi nikaale?

- **LRU** (least recently used): theoretically best, par 8-way ka exact LRU
  track karna mehnga (log2(8!) ≈ 15 bits/set). Sahi LRU sirf 2-way pe common.
- **Pseudo-LRU** (tree-PLRU): ek chhota bit-tree jo "roughly" LRU approximate
  karta, ~7 bits/set for 8-way. Modern L1/L2 mostly yehi.
- **Random / RRIP**: L3 pe RRIP (re-reference interval prediction) jaise —
  scan-resistant, streaming access se poora L3 flush nahi hota.

Practically: aap policy control nahi karte, par jaan lo ki ek set mein 8 se
zyada hot lines rakhoge to woh churn karengi chahe baaki cache khaali ho.

---

## Conflict misses aur "critical stride"

Do addresses **same set** mein map hote hain agar unke `(addr >> 6) & 63`
barabar hain — yaani woh **4096 bytes** (64 sets × 64 B) ke multiple se alag
hon.

```
critical stride = (cache size) / (associativity)
is box L1: 32768 / 8 = 4096 bytes
```

Agar aap ek loop mein exactly 4096 (ya 8192, 16384 — power-of-two) bytes ke
stride pe access karo, to har access **same set** mein girta. 9 aise addresses
= 8-way set overflow = har iteration eviction, chahe L1 99% khaali ho.

**Yahi wajah hai ki `float matrix[1024][1024]` ka column access (stride 4096 B)
brutal hai** — na sirf har element naya line (spatial), balki woh saare
elements same L1 set(s) ke liye ladte (conflict). Example `03`: col-major
~10x dheema.

Power-of-two array dimensions (256, 512, 1024) is problem ko **maximize**
karte. Ek classic fix: array ko **pad** karo — `float matrix[1024][1024+8]`
(rows ko non-power-of-2 banao) → columns ab alag sets mein distribute.

---

## VIPT aur page coloring (thoda deep)

L1 aksar **VIPT** hota — Virtually Indexed, Physically Tagged. Index virtual
address se (TLB lookup ke saath parallel, tez), tag physical se (aliasing
safe). Constraint: `index + offset bits <= page offset bits (12)`. 64 sets
(6) + 64 offset (6) = 12 → bilkul fit. Isliye L1 "32 KiB, 8-way" itna common
hai — bada karne ke liye ya associativity badhao ya bigger pages.

**Page coloring**: OS physical pages ko aise allocate kare ki consecutive
virtual pages alag cache sets ("colors") mein girein → conflict misses kam.
Linux mein kabhi-kabhi, huge pages ke saath automatic (2 MiB page poore L1 ko
cover karta).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — power-of-two strides / dimensions
`arr[i * 4096]`, `matrix[N][N]` with N = 512/1024, `struct` array with
`sizeof` = 256 accessed at fixed stride → sab same-set thrash. Fix: pad
dimensions (`N+1`, `N+8`), ya access order badlo.

### Trap 2 — "cache khaali hai to miss nahi hoga"
Conflict miss cache-occupancy se independent hai. 64 KiB working set 32 KiB
8-way L1 mein fit hona chahiye tha, par agar woh sab ~8 sets pe concentrate
hai to woh 8 sets overflow → misses, baaki 56 sets idle.

### Trap 3 — associativity ko infinite maan lena
"8-way" ka matlab ek set mein max 8 hot lines. 9va aata → ek jaata. Hot loop
mein 9+ arrays ko same-stride touch karna = self-inflicted thrash.

### Trap 4 — aligned allocation se accidental conflict
Sab arrays ko `aligned_alloc(4096, ...)` (page-aligned) karoge to sabke
element 0 **same set** mein → pehla access har array ka ek doosre se takrata.
Kabhi jaan-boojh ke thoda offset (`+ 64 * k`) daalna padta.

### Trap 5 — hash table bucket count power-of-two + bad hash
`buckets = 2^k`, aur hash ke low bits weak → keys kuch buckets pe pile,
woh buckets same cache sets pe → double thrash. Good hash (mix high bits down)
+ ya prime bucket count.

---

## > **HFT relevance**

> - **Array dimensions power-of-two mat rakho** jab aap unhe strided access
>   karte ho. `book[SYMالبOLS][LEVELS]` — `LEVELS` ko 8/16 nahi, 9/17 rakho,
>   ya SoA.
> - **Hot arrays ko deliberately alag sets pe.** Agar 3-4 arrays hot loop mein
>   same index se touch hote, unke base addresses ko cache-set ke hisaab se
>   spread karo (padding), warna woh ek set ke 8 ways ke liye ladenge.
> - **`perf` se pakdo:** `L1-dcache-load-misses` high par working set L1 se
>   chhota → conflict misses suspect karo (`perf c2c` nahi, yeh single-thread).
>   `perf stat -e l1d.replacement`.
> - **Huge pages conflict misses bhi kam karti** — 2 MiB page ke andar OS
>   coloring guaranteed contiguous physical → predictable set distribution.

---

## Hands-on

```bash
# conflict dikhane ke liye: ek array ko stride 4096 B pe access karo vs 4032 B
# (4096-64). Same access count, par 4096 wala ~kai-x dheema (same-set thrash).

./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/03_matrix_traversal.cpp
# col-major (stride N*4 = 16384 B = power of 2) ~10x dheema -- spatial + conflict

# is box ki cache geometry:
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/07_cpu_info.cpp
#   Linux: `lscpu -C` ya `getconf -a | grep CACHE`
```

Experiment: `int a[64][4096]`. Loop 1: `for j: for i: sum += a[i][j]` (stride
16 KiB, power-of-2). Loop 2: same par `a[64][4097]` (stride 16388, odd). Loop
2 kaafi tez — conflict misses hat gaye.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "cache = big flat array" | sets × ways; address → fixed set, N-way choice |
| "khaali cache = no miss" | conflict miss occupancy-independent |
| "8-way = 8x more room anywhere" | 8 lines **per set**; 9th same-set line evicts |
| "power-of-2 dimensions clean/fast" | max conflict misses on strided access |
| "index tag se hota" | index = middle bits (set); tag = upper (identity) |
| "replacement is true LRU" | pseudo-LRU / RRIP; approximate |

---

## Exercises

1. L2 = 512 KiB, 64-B line, 8-way. Kitne sets? Set index kitne bits? Critical
   stride?

   <details><summary>Answer</summary>

   Lines = 512 KiB / 64 = 8192. Sets = 8192 / 8 = **1024**. Set index =
   log2(1024) = **10 bits** (bits 6..15 of the address). Critical stride =
   512 KiB / 8 = **64 KiB** — is stride (ya uske multiple) pe access karne se
   sab same L2 set mein girega.
   </details>

2. `double m[512][512]`. Column-major loop `for j: for i: s += m[i][j]`. Har
   inner step ka byte-stride? Yeh L1 (32 KiB, 8-way, 4 KiB critical stride) ke
   liye kya matlab?

   <details><summary>Answer</summary>

   Step = ek row aage = 512 × 8 = **4096 bytes** = bilkul L1 ka critical
   stride. → inner loop ke consecutive accesses **sab same L1 set** mein
   girte. 8 iterations mein set full, 9th evicts pehla — poora inner loop
   (512 iters) L1 mein ek bhi line reuse nahi kar paata, har access L2+ se.
   Plus spatial waste (har access naya line, 8 me se 1 double use). Double
   whammy. `m[512][520]` karne se stride 4160 → alag sets → conflict gayab.
   </details>

3. Aapki hash map `buckets.size() == 1<<20`, `bucket = hash & (size-1)`. Hash
   function `key * 2654435761u` (Knuth) — low 20 bits kaafi mixed hain. Ab
   `key` values sab 4096 ke multiple hain (aligned pointers). Kya hoga?

   <details><summary>Answer</summary>

   `key = p * 4096`. `hash = p * 4096 * 2654435761u`. `4096 = 2^12`, to hash
   ke low 12 bits **hamesha 0** → `bucket = hash & 0xFFFFF` ke low 12 bits
   bhi 0 → sirf woh buckets use hote jo 4096 ke multiple hain → 1M buckets me
   se ~256 → 4096x collision rate, aur woh buckets few cache sets pe → thrash.
   Fix: hash ke **high bits ko low mein mix** karo (`h ^= h >> 33` type
   finalizer), tab low bits `p` pe depend karenge, `4096` factor pe nahi.
   </details>

---

## Interview questions

1. Address ka index/tag/offset split — har hissa kya karta, kitne bits.
2. Direct-mapped vs set-associative vs fully-associative — trade-offs.
3. Conflict miss — definition, aur woh capacity miss se kaise alag.
4. "Critical stride" formula aur woh power-of-two dimensions ko kyun poison karta.
5. Ek conflict-miss problem ka fix (padding) — kyun kaam karta.
6. VIPT L1 — kyun "32 KiB 8-way" itna universal hai (index+offset ≤ 12 bits).
7. Pseudo-LRU kyun (true LRU ke bajaye) 8-way cache mein.

---

## Next
→ [`04-cache-misses.md`](04-cache-misses.md)
