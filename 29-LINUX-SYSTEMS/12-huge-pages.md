# 12 — Huge pages: TLB pressure, 4K vs 2M vs 1G

## Prerequisites
- `07-mmap.md` (mapping, MADV_HUGEPAGE)
- `06-proc-and-sys.md` (`/proc/meminfo`, `/sys/kernel/mm`)
- `14-MEMORY` (page tables, TLB idea)

## Yeh topic abhi kyun
Har memory access ke liye CPU ko virtual→physical translation chahiye. Woh
translation **TLB** (Translation Lookaside Buffer) mein cache hoti — aur TLB
chhota hota (few hundred entries). Bada working set + 4 KiB pages = TLB miss
storm = har miss pe ek page-table walk (extra memory refs). **Huge pages** ek
TLB entry ko 512× (2 MiB) ya 262144× (1 GiB) zyada memory cover karwate hain.
HFT: order books, big hash maps, market-data buffers huge pages pe. Example
`08` isse naapta hai.

---

## Page sizes aur TLB reach

x86-64 pe:

| Page size | Kaise | TLB "reach" (say 64 dTLB entries) |
|---|---|---|
| **4 KiB** | default | 64 × 4 KiB = 256 KiB |
| **2 MiB** | `pdpe1gb`? no — `pse`/`PDE.PS` | 64 × 2 MiB = 128 MiB |
| **1 GiB** | `pdpe1gb` CPU flag | 64 × 1 GiB = 64 GiB |

(Real CPUs ke paas separate L1 dTLB/iTLB + L2 STLB, sizes vary — idea same.)

Agar tumhara hot working set 200 MiB hai:
- **4 KiB**: ~51,200 pages. TLB (64–1536 entries) mein ~kabhi nahi aata →
  random access pe har baar ~TLB miss → **page-table walk**: 4-level, up to 4
  extra cache-line reads per translation (mostly cached, but still).
- **2 MiB**: ~100 pages. TLB mein comfortably fit → miss rate ~0.

**Page-table walk ki cost:** cached PT entries pe ~ few ns; cold (bade random
working set, PT entries bhi cache se evict) pe 10s of ns per access — silently.
Example `08`: 512 MiB random pointer-chase, 4K vs 2M → typically **~1.1×–1.5×**
speedup (workload/CPU pe depend).

---

## Do raaste: Transparent (THP) vs explicit (hugetlbfs)

### Transparent Huge Pages (THP) — automatic

Kernel background mein `khugepaged` thread se 4K pages ko 2M mein "collapse"
karta, aur bade allocations ko directly 2M deta.

```bash
cat /sys/kernel/mm/transparent_hugepage/enabled
#   [always] madvise never
```
- `always` — har eligible mapping ko THP dene ki koshish.
- `madvise` — sirf jab tum `madvise(p, len, MADV_HUGEPAGE)` bolo.
- `never` — off.

```cpp
::madvise(ptr, len, MADV_HUGEPAGE);        // is region ke liye THP maango
```

**THP ka HFT problem:** `khugepaged` collapse karte waqt ek **stall** deta (page
migration, TLB shootdown) — random ms-scale spike, trading hours mein. Aur
"defrag" (`/sys/kernel/mm/transparent_hugepage/defrag`) `always` ho to allocation
path pe synchronous compaction. Isi liye latency-critical boxes aksar
**`madvise` ya `never`**, aur explicit huge pages prefer karte.

### Explicit huge pages (hugetlbfs) — reserved, guaranteed

Boot ya runtime pe ek **pool** reserve karo:

```bash
# runtime (fragmentation se fail ho sakta -- boot better)
echo 1024 > /proc/sys/vm/nr_hugepages                 # 1024 × 2 MiB = 2 GiB
# boot cmdline (guaranteed, contiguous):
#   default_hugepagesz=2M hugepagesz=2M hugepages=1024
cat /proc/meminfo | grep -i huge
#   HugePages_Total: 1024   HugePages_Free: 1024   Hugepagesize: 2048 kB
```

Use karne ke 3 tareeke:
```cpp
// 1. Anonymous MAP_HUGETLB
void* p = mmap(nullptr, len, PROT_READ|PROT_WRITE,
               MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, -1, 0);

// 2. MAP_HUGETLB | (21 << MAP_HUGE_SHIFT) for explicit 2M, (30<<) for 1G

// 3. hugetlbfs file (shared memory on huge pages!)
//    mount -t hugetlbfs none /dev/hugepages
int fd = open("/dev/hugepages/ring", O_CREAT|O_RDWR, 0600);
void* p = mmap(nullptr, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
```

- **Guaranteed**: pool se aata, kabhi split/collapse nahi, no `khugepaged`
  stall.
- **Pre-faulted-ish**: hugetlb pages allocation pe hi committed (lazy nahi jitna
  4K), aur swappable nahi (implicitly locked).
- **Cost**: pool RAM reserve kar leta chahe use na ho; `mmap` fail agar
  `HugePages_Free` kam.

### 1 GiB pages
`pdpe1gb` CPU flag chahiye (`grep pdpe1gb /proc/cpuinfo`). Boot:
`default_hugepagesz=1G hugepagesz=1G hugepages=8`. Sirf **boot-time** reliably
(1 GiB contiguous runtime pe milna almost impossible). Bahut bade static
structures (multi-GB order books, huge lookup tables) ke liye.

---

## Internal working

- 4-level paging: PML4 → PDPT → PD → PT → 4 KiB page. **2 MiB page**: PD entry
  ka `PS` bit set → PT level skip, entry directly 2 MiB frame. **1 GiB**: PDPT
  entry `PS` → 2 levels skip.
- Fewer levels + fewer entries → smaller page tables (memory saving) + fewer
  TLB entries needed + shorter walk on miss.
- `khugepaged`: scans mm's, eligible 2M-aligned 512-page ranges ko ek 2M page
  se replace karta — ismein pages copy/migrate + PT rewrite + TLB flush (IPI to
  other cores). Yeh woh stall hai.
- hugetlb pages ek separate allocator/pool — buddy allocator se contiguous 2M/1G
  blocks pehle hi nikaal ke rakhe.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — THP `always` on a latency box
`khugepaged` collapse + synchronous defrag = random ms spikes. Latency-critical:
`madvise` (or `never`) + explicit hugetlbfs.

### Trap 2 — huge pages se "sab tez ho jayega" maan lena
Fayda sirf jab working set >> TLB reach **aur** access random. Cache-resident
(~few MiB) ya sequential (prefetcher hides walk) data pe farak ~0. Measure
(`08`).

### Trap 3 — `nr_hugepages` runtime pe set karke bharosa karna
Memory fragmented ho to `echo 1024` sirf 300 de paata (`HugePages_Total` <
maanga). Boot cmdline se reserve — contiguous memory abhi available hai.

### Trap 4 — hugetlb `mmap` fail hone pe silent 4K fallback
`MAP_HUGETLB` `mmap` `ENOMEM` deta agar pool khatam — agar tum error ignore
karke `MAP_ANONYMOUS` retry karo, chup-chaap 4K pe chale jaao. Explicitly check
+ log; startup pe hard-fail agar huge pages nahi mile.

### Trap 5 — internal fragmentation
Ek 2 MiB page allocate ki 100 KiB data ke liye → 1.9 MiB waste (RSS pe dikhega,
`AnonHugePages`). Huge pages bade contiguous structures ke liye; chhote scattered
allocations ke liye nahi.

### Trap 6 — NUMA + huge pages
`nr_hugepages` global; pool per-node bhi (`/sys/devices/system/node/nodeN/
hugepages/`). Galat node se huge page = remote access (`14`). `numactl
--membind` ya per-node reserve.

### Trap 7 — `MAP_HUGETLB` region ka `madvise(MADV_DONTNEED)` / partial munmap
Huge page ko partially free nahi kar sakte — poora 2M unit. Partial ops
`EINVAL` ya poora page drop.

---

## > **HFT relevance**

> - **Explicit hugetlbfs for the big static structures:** order book arrays,
>   symbol→state hash maps, market-data ring buffers. Boot-reserve the pool
>   (`hugepages=` on cmdline), `mmap(MAP_HUGETLB)` at startup.
> - **THP = `madvise` or `never`** on trading hosts. The `khugepaged`
>   collapse/defrag stall is exactly the kind of random ms spike you're
>   eliminating everywhere else.
> - **Shared-memory rings on hugetlbfs** (`08`): `mmap("/dev/hugepages/md_ring",
>   MAP_SHARED)` → cross-process ring with huge-page TLB benefits + implicitly
>   locked (no swap).
> - **1 GiB pages** for very large, permanent tables — boot-reserve only.
> - **Measure the actual win** (`08`): don't assume. On cache-resident hot
>   paths it's ~0; on multi-hundred-MB random-access structures it's real.
> - **Fewer page faults too** — a 2 MiB huge page = one fault instead of 512
>   (helps startup warm-up, `13`).

---

## Hands-on

```bash
# Linux pe -- example 08: 4K vs 2M random pointer-chase
echo 512 | sudo tee /proc/sys/vm/nr_hugepages      # ~1 GiB pool
g++ -std=c++20 -O2 29-LINUX-SYSTEMS/examples/08_hugepages.linux.cpp -o /tmp/hp && /tmp/hp

grep -i huge /proc/meminfo
cat /sys/kernel/mm/transparent_hugepage/enabled
cat /sys/kernel/mm/transparent_hugepage/defrag

# ek process kitne huge pages use kar raha
grep -i huge /proc/$(pidof trader)/smaps_rollup

# TLB misses khud dekho (folder 35 preview)
perf stat -e dTLB-load-misses,dTLB-loads /tmp/hp
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "huge pages hamesha tez" | sirf working-set >> TLB reach + random access |
| "THP on = free win" | `khugepaged` collapse/defrag = latency spikes; use `madvise` |
| "`echo N > nr_hugepages` reliable" | fragmentation se kam mil sakta; boot-reserve |
| "hugetlb mmap fail = 4K se chal jayega" | explicit check; silent fallback = hidden regression |
| "2M page thode data ke liye theek" | 1.9 MiB waste (internal fragmentation) |
| "huge page fault bhi lazy 4K jaisa" | hugetlb allocation pe committed + unswappable |

---

## Exercises

1. Working set 2 MiB, pure sequential scan. Huge pages se kitna faayda?

   <details><summary>Answer</summary>

   ~Zero. 2 MiB 4K-page-count = 512 pages — L2 STLB (1024–2048 entries) mein
   easily fit, aur sequential access pe hardware prefetcher + walk overlap se
   TLB miss almost free. Huge pages tab matter karte jab working set TLB reach
   se bada **aur** access pattern random/pointer-chasing ho.
   </details>

2. Latency box pe `transparent_hugepage/enabled` = `always`. Ek random ~800 µs
   spike har few minutes. Connection?

   <details><summary>Answer</summary>

   Likely `khugepaged` ek region ko collapse kar raha: 512 4K pages ko ek 2M
   page se replace — pages migrate/copy + page-table rewrite + TLB shootdown
   (IPI to all cores touching that mm). Woh IPI + flush tumhare hot thread ko
   stall karta. Fix: `echo madvise > .../enabled`, `echo defer >
   .../defrag`, aur bade structures explicit hugetlbfs pe.
   </details>

3. `mmap(MAP_HUGETLB, 100 MiB)` succeed hua, `HugePages_Free` 50 se 0 ho gaya
   (2M pages). Par `top` RSS 100 MiB se zyada dikha raha kabhi-kabhi. Kyun?

   <details><summary>Answer</summary>

   100 MiB / 2 MiB = 50 pages exactly — RSS 100 MiB. Agar tumne 101 MiB maanga
   hota to 51 pages = 102 MiB reserve (internal fragmentation, 1 MiB waste).
   `AnonHugePages`/`HugePages` accounting `smaps` mein alag dikhta. (THP ke
   case mein `top` RSS aur `AnonHugePages` ka mismatch aur common — collapse
   ke beech ka state.)
   </details>

4. HFT box pe huge pages boot cmdline se reserve karte ho, runtime `echo` se
   nahi. Kyun?

   <details><summary>Answer</summary>

   Runtime pe physical memory fragmented ho chuki hoti (page cache, running
   processes) → kernel ko 1024 contiguous 2 MiB (ya 8 contiguous 1 GiB) blocks
   dhoondhne mein dikkat → tum 1024 maango, 700 mile. Boot pe memory abhi
   pristine/contiguous — `hugepages=1024` cmdline se guaranteed full pool.
   Deterministic infra > runtime convenience.
   </details>

5. 2-socket box. `nr_hugepages=1024` set kiya. Feed handler node 1 ke cores pe
   pinned, par uska huge-page-backed buffer node 0 se aa raha. Symptom + fix.

   <details><summary>Answer</summary>

   Har buffer access remote (cross-socket UPI) → higher latency + interconnect
   contention, huge-page TLB benefit ke bawajood net loss possible. Fix: per-node
   huge page reserve
   (`/sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages`),
   aur allocation ko node 1 pe bind (`numactl --membind=1`, ya `mbind` on the
   mmap'd region). `numactl -H` + `/proc/<pid>/numa_maps` se verify (`14`).
   </details>

---

## Interview questions

1. TLB kya hai, "TLB reach" ka matlab, huge pages usse kaise badhate.
2. Page-table walk — 4-level, aur 2M/1G page walk ko kaise chhota karte.
3. THP vs hugetlbfs — mechanism aur latency trade-off.
4. `khugepaged` stall — kya hota, HFT box pe kyun avoid.
5. Huge pages kis workload pe faayda deta, kis pe nahi.
6. `nr_hugepages` runtime vs boot reservation — kyun boot.
7. NUMA + huge pages — kya galat ho sakta.

---

## Next
→ [`13-page-faults-and-mlock.md`](13-page-faults-and-mlock.md)
