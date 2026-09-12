# 11 — TLB and huge pages

## Prerequisites
- `29-LINUX-SYSTEMS/04-mmap-demo.linux.cpp` aur `08-hugepages.linux.cpp`
- `31-CPU-ARCHITECTURE/06-out-of-order-execution.md`
- example `08_tlb_hugepages.cpp`

## Yeh topic abhi kyun
Ab tak "cache miss" = data cache miss. Ek doosra, chhupa miss hai:
**address translation miss.** Har memory access ko virtual → physical
translate karna padta hai, aur woh translations bhi ek chhote cache (TLB)
mein rakhe jaate hain. Bade working set pe TLB overflow → har access ke saath
ek "page walk" (upto 4 extra dependent memory accesses). **Huge pages** ek
TLB entry ko 512x zyada memory cover karwa ke isse theek karti hain — HFT
tuning ka standard step.

---

## Virtual → physical: page tables

Memory 4 KiB **pages** mein bata. Har process ka apna virtual address space;
ek **page table** virtual page number → physical frame number map karta.
x86-64 pe yeh **4 levels** deep hai (PML4 → PDPT → PD → PT), 2 MiB / 1 GiB
pages ke liye kam levels.

Ek translation ke liye "page walk":
```
VA -> PML4[i4] -> PDPT[i3] -> PD[i2] -> PT[i1] -> physical frame
       (memory)    (memory)   (memory)  (memory)
```
= **4 dependent memory accesses** (har level ka entry pichle se aata). Agar
woh entries cache mein nahi → 4 misses **serial** = ~400 ns just to find out
where your data is. (Kuch hardware "paging-structure caches" / MMU caches
in intermediate levels ko cache karte, so mostly < 4.)

---

## TLB — the translation cache

CPU recent translations ko **TLB** (Translation Lookaside Buffer) mein rakhta:

```
is box (AMD Zen 2, typical):
  L1 dTLB : ~64 entries, 4 KiB pages   -> reach 64 * 4 KiB  = 256 KiB
  L1 iTLB : ~64 entries
  L2 STLB : ~1536-2048 entries (shared) -> reach ~6-8 MiB
  (2 MiB pages: separate/fewer dTLB entries, but each covers 2 MiB)
```

Access flow: VA → L1 dTLB? hit → PA turant. miss → L2 STLB? hit → fill L1,
go. miss → **page walk** (the 4-level thing above), fill STLB + dTLB.

**TLB reach** = (entries × page size) = kitni memory bina page-walk ke
address ho sakti. 4 KiB pages: L1 dTLB reach sirf 256 KiB, STLB ~6 MiB. Iss
se bada working set random-touch karo → har naya page ek TLB miss.

---

## Measured — example `08`

Pointer-chase, ek slot per page (Sattolo single cycle), page count sweep:

| pages | footprint | curve A (1 page, TLB always hit) | curve B (N pages) |
|---|---|---|---|
| 16-64 | ≤ 256 KiB | 1.2 ns | ~3.4 ns |
| 128 | 512 KiB | 1.2 ns | ~5 ns |
| 256 | 1 MiB | 1.2 ns | ~10 ns |
| 1024 | 4 MiB | 1.2 ns | ~15 ns |
| **2048** | **8 MiB** | 1.2 ns | **~95 ns** |
| 8192 | 32 MiB | 1.2 ns | ~98 ns |

Curve A = control (ek page, TLB entry permanent) → flat 1.2 ns (L1 latency).
Curve B ka cliff **8 MiB ke aas-paas** — jahan working set STLB reach (~6 MiB)
**aur** L3 (8 MiB) dono cross karta.

⚠️ **Honest limitation**: 4 KiB pages ke saath page-count aur memory-footprint
saath badhte hain → yeh test TLB aur cache dono ko ek saath cross karta,
inhe cleanly separate nahi kar sakta. Pure-TLB isolate karne ke liye same
cache lines ko alag page-mappings se touch karna padta (`mmap` aliasing,
Linux-only). Par point khada rehta: **page-strided bada working set = TLB
aur cache dono se maar.**

---

## Huge pages: 2 MiB (aur 1 GiB)

Ek 2 MiB page = 512 × 4 KiB. Ek TLB entry ab **2 MiB cover** karta:
- STLB reach: 1536 × 2 MiB = **3 GiB** (vs ~6 MiB with 4K pages) → 512x.
- Page walk shorter: 3 levels instead of 4 (PT level skip).
- Fewer page-table pages → less memory for the tables themselves, better
  page-table cache hit.

Result: multi-MB working set jo 4K pages pe TLB-thrash karta tha, 2 MiB pages
pe TLB misses ~gayab (cache misses still there — huge pages cache size nahi
badhti).

### Linux — explicit huge pages (hugetlbfs)
```bash
# reserve at boot or runtime:
echo 1024 > /proc/sys/vm/nr_hugepages           # 1024 * 2 MiB = 2 GiB
cat /proc/meminfo | grep -i huge

# in code: mmap with MAP_HUGETLB
void* p = mmap(nullptr, size, PROT_READ|PROT_WRITE,
               MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, -1, 0);
# or MAP_HUGETLB | (21 << MAP_HUGE_SHIFT) for explicit 2 MiB
```
Deterministic — you either get huge pages or `mmap` fails. HFT prefers this.

### Linux — Transparent Huge Pages (THP)
```bash
cat /sys/kernel/mm/transparent_hugepage/enabled   # [always] madvise never
```
Kernel silently promotes 4K → 2 MiB when it can (`khugepaged` background, or
on fault with `always`). `madvise(addr, len, MADV_HUGEPAGE)` to opt-in a
region.

**THP downsides for HFT:**
- **Allocation stalls** — promotion/compaction can pause a thread (page
  fault takes longer, or `khugepaged` steals cycles). Bad for tail latency.
- **Fragmentation** — over time, hard to find contiguous 2 MiB.
- **Wasted memory** — a barely-used region promoted to 2 MiB.

→ HFT commonly sets THP to `madvise` or `never` and uses **explicit**
hugetlbfs for the known-hot regions, allocated + pre-faulted at startup.

### Windows large pages
`VirtualAlloc(..., MEM_LARGE_PAGES, ...)` — needs `SeLockMemoryPrivilege`
(admin grant), and memory must be available contiguously. Rarer in practice.

---

## Pre-faulting (goes with huge pages)

`mmap` sirf address space reserve karta — physical frame pehle access pe
allocate (page fault). Pehla touch of every page = a fault (~1-3 µs) + a TLB
fill. HFT: startup pe har page ko touch karo (`memset` ya a strided write)
+ `mlockall(MCL_CURRENT|MCL_FUTURE)` (no swap) → steady state mein zero page
faults, TLB warm. (`29-LINUX-SYSTEMS/16-17`.)

---

## ⚠️ Traps / Common mistakes

### Trap 1 — TLB misses ko data-cache misses samajhna
`perf stat -e cache-misses` low, par code phir bhi slow, big random working
set → check `dTLB-load-misses`, `dtlb_load_misses.walk_completed`. Alag
problem, alag fix (huge pages, not blocking).

### Trap 2 — huge pages maang liye par pre-fault nahi
2 MiB page ka pehla touch = ek bada fault (zero the whole 2 MiB). Startup pe
pre-fault karo, warna pehla real access pe latency spike.

### Trap 3 — THP `always` on a latency-sensitive box
`khugepaged` + compaction stalls = tail latency jitter. `madvise` or `never`,
explicit hugetlbfs for hot regions.

### Trap 4 — 1 GiB pages casually
1 GiB pages boot-time reservation, and waste is huge if under-used. Only for
genuinely huge, always-hot arenas.

### Trap 5 — small scattered allocations expecting TLB benefit
Huge pages help a **contiguous multi-MB** region. 10000 small `malloc`s
scattered across the heap won't be on huge pages (and THP can't coalesce
them). Arena-allocate into one big huge-page region.

### Trap 6 — forgetting NUMA first-touch with huge pages
The 2 MiB page is physically placed on the NUMA node of the **first thread
to touch it** (`29/15`, `31/14`). Pre-fault from the thread that will use it,
pinned to the right node.

---

## > **HFT relevance**

> - **Explicit 2 MiB huge pages for every hot arena** — order pool, book
>   arrays, market-data ring, signal buffers. `MAP_HUGETLB`, allocated and
>   **pre-faulted** at startup, `mlockall`.
> - **THP off (`madvise`/`never`)** — no `khugepaged` jitter.
> - **`perf stat -e dTLB-load-misses,iTLB-load-misses,dtlb_load_misses.
>   walk_active`** in your bench harness. On a tuned box these should be
>   ~0 in steady state.
> - **Code section huge pages too** — a large hot text segment can iTLB-miss.
>   `hugepage_text` / linker options / `libhugetlbfs` remap the `.text`.
> - **1 GiB pages** for a single very large always-resident structure (a full
>   day's book snapshot, a big model) — boot reservation.
> - **First-touch on the right NUMA node** — pin the init thread.

---

## Hands-on

```bash
./build.ps1 fast 32-CACHE-MEMORY-PERFORMANCE/examples/08_tlb_hugepages.cpp
#   curve B cliff ~8 MiB (STLB reach + L3 together)

# Linux:
cat /sys/kernel/mm/transparent_hugepage/enabled
grep -i huge /proc/meminfo
perf stat -e dTLB-loads,dTLB-load-misses,dtlb_load_misses.walk_completed ./prog
# compare 4K vs explicit MAP_HUGETLB build of the same program
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "cache miss hi ek problem hai" | TLB miss = page walk, upto 4 serial mem accesses |
| "TLB bahut bada hai" | 4K pages: L1 dTLB reach ~256 KiB, STLB ~6 MiB |
| "huge pages cache bhi badhati" | sirf TLB reach; cache size wahi |
| "THP always = free win" | khugepaged/compaction stalls = tail jitter for HFT |
| "mmap ke baad memory ready" | address space only; first touch = fault; pre-fault |
| "huge pages random mallocs pe" | contiguous multi-MB region chahiye; arena |

---

## Exercises

1. Working set 20 MiB, purely random 8-byte reads. 4 KiB pages, STLB reach
   6 MiB. Roughly kya fraction of accesses TLB-miss karenge, aur ek 2 MiB
   huge page setup mein?

   <details><summary>Answer</summary>

   20 MiB / 4 KiB = 5120 pages, STLB holds ~1536 → only ~30% of pages fit →
   ~**70% of random accesses TLB-miss** → each a page walk (~some cached
   levels, say ~40-100 ns extra) on top of the data miss. With 2 MiB pages:
   20 MiB / 2 MiB = **10 pages** → all 10 fit in the dTLB (let alone STLB) →
   ~**0% TLB miss** in steady state. The data cache misses (20 MiB > L3) are
   unchanged — huge pages fixed the translation half only, but that was a big
   half.
   </details>

2. Aapne `MAP_HUGETLB` se 512 MiB arena liya aur startup pe use `mmap` ke
   turant baad hot loop mein daal diya. Pehli 256 iterations slow. Kyun, fix?

   <details><summary>Answer</summary>

   Har 2 MiB huge page ka **pehla touch** = a page fault where the kernel
   zeroes the whole 2 MiB and installs the mapping + TLB entry. 512 MiB / 2
   MiB = 256 huge pages → the first 256 distinct pages you touch each eat one
   big fault (~µs each) → the "first 256 iterations slow". Fix: after `mmap`,
   **pre-fault** — `memset(arena, 0, size)` or a strided write touching one
   byte per 2 MiB — during startup, plus `mlockall`. Then the hot loop hits
   only warm pages.
   </details>

3. `perf stat` on your feed handler: `cache-misses` low, IPC ~0.9 (expected
   ~2.5), `dtlb_load_misses.walk_completed` very high. Diagnosis aur 2 fixes.

   <details><summary>Answer</summary>

   Low data-cache misses but low IPC + high dTLB walks → the stalls are
   **page walks**, not data misses. The handler's working set spans many 4 KiB
   pages accessed with poor page-locality (e.g. a big hash map, or per-symbol
   structures scattered across a large arena). Fixes: (1) **2 MiB huge pages**
   for the arena / hash map region — collapses thousands of translations into
   a handful. (2) **Improve page locality** — pack per-symbol hot state
   contiguously (SoA, arena by symbol) so a tick touches few pages; shrink
   the working set. (3) Pre-fault + `mlockall` so walks aren't also faulting.
   </details>

---

## Interview questions

1. Page walk — x86-64 pe kitne levels, kitne dependent memory accesses.
2. TLB reach — formula, aur 4 KiB pages pe L1 dTLB / STLB ke approx numbers.
3. Huge pages TLB ko kaise theek karti — "512x reach" ka matlab.
4. Huge pages cache misses ko theek karti hain? (Nahi — kyun.)
5. THP ke 3 downsides jo HFT ko explicit huge pages choose karwate hain.
6. Pre-faulting — kya, aur huge pages ke saath kyun zaroori.
7. `perf` se TLB problem ko data-cache problem se kaise distinguish karte.

---

## Next
→ [`12-store-buffers.md`](12-store-buffers.md)
