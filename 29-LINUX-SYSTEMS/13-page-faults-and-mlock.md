# 13 — Page faults ki cost, mlockall, memory warming

## Prerequisites
- `07-mmap.md` (lazy allocation, MAP_POPULATE)
- `12-huge-pages.md` (fault count vs page size)
- `14-MEMORY` (heap, `malloc` internals)

## Yeh topic abhi kyun
`malloc`/`new`/`mmap` **RAM allocate nahi karte** — woh sirf address space
reserve karte. Physical page tab milta jab tum use **pehli baar touch** karo →
**page fault**. HFT mein woh pehli touch trading hours mein nahi honi chahiye —
woh ek unpredictable ~µs (minor) ya ~ms (major) spike hai. Yeh lesson: fault ki
cost, aur **memory warming** — startup pe sab kuch fault kar do, `mlock` kar do,
taaki steady state mein page fault count = 0. Example `07` yeh naapta hai.

---

## Page fault kya hai

CPU ne ek virtual address access kiya jiske liye page table mein valid physical
mapping **nahi** hai → hardware trap → kernel ka fault handler chalta:

| Fault type | Kya | Cost (typical) |
|---|---|---|
| **Minor fault** | page RAM mein hai (zero page, page cache, shared lib), bas is process ke PT mein map karna | ~0.2–2 µs |
| **Major fault** | page disk pe (swapped out, ya file-backed not-yet-read) — I/O chahiye | ~ms (SSD) se ~10 ms (spinning) |
| **COW fault** | shared RO page ko write kiya → kernel copy banata, RW karta | ~1–3 µs (copy + TLB) |
| **Invalid** | address kisi VMA mein nahi, ya perms galat | `SIGSEGV` |

Har `mmap`'d-but-untouched page ki pehli touch = ek minor fault. Ek 256 MiB
anonymous buffer = ~65,536 minor faults pehli baar (example `04`, `07`).

### Fault handler ke andar (minor, anonymous)
1. Trap → kernel, faulting address + error code.
2. VMA lookup: yeh address kis mapping mein, perms?
3. Anonymous + write → ek fresh **zeroed** physical page allocate (buddy
   allocator; agar zero-page pool khali to CPU cycles zeroing pe).
4. PT entry likho (PML4→PDPT→PD→PT), possibly allocate intermediate PT pages.
5. TLB (is core ka) update; return to faulting instruction, retry.

Steps 3 (zeroing) aur 4 (PT page allocation) ke worst cases = woh "one page
took 40 µs" spike jo example `07` ke "worst" column mein dikhta.

---

## `getrusage` — fault count dekho

```cpp
#include <sys/resource.h>
rusage ru{}; getrusage(RUSAGE_SELF, &ru);
// ru.ru_minflt  = minor faults total
// ru.ru_majflt  = major faults total  (yeh 0 hona chahiye trading hours mein)
```
`/usr/bin/time -v ./prog` bhi deta ("Minor (reclaiming a frame) page faults",
"Major ... "). `/proc/self/stat` fields 10–12.

**HFT self-monitoring:** har few seconds `ru_majflt` delta check karo — kabhi
bhi non-zero = memory swap ho raha ya file page evict ho raha → alert.

---

## Memory warming — 3 steps

### Step 1 — `mlockall` (swap se bacho, pin in RAM)

```cpp
#include <sys/mman.h>
if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
    perror("mlockall (RLIMIT_MEMLOCK / CAP_IPC_LOCK chahiye)");
// MCL_CURRENT: ab tak ki saari memory lock
// MCL_FUTURE:  aage jo bhi map hoga woh bhi auto-lock
// (Linux 4.4+: MCL_ONFAULT -- lock only pages as they fault, saves RAM)
```

`mlockall` guarantee karta ki locked pages **kabhi swap-out / reclaim nahi**
honge → koi surprise major fault. Requires `RLIMIT_MEMLOCK` raised (systemd:
`LimitMEMLOCK=infinity`; `/etc/security/limits.conf`: `* - memlock unlimited`)
ya `CAP_IPC_LOCK`.

`MCL_CURRENT` **abhi** locked pages ko fault-in bhi karta (resident banata).
Par `MCL_FUTURE` ke saath future mappings tab lock hote jab touch ho — isi liye
pre-touch bhi chahiye.

### Step 2 — pre-fault (har page touch karo)

```cpp
// ek arena/pool ko allocate ke baad:
volatile char* p = static_cast<volatile char*>(arena);
for (size_t i = 0; i < arena_size; i += 4096) p[i] = 0;   // har page ki pehli touch ABHI
```

Ya `mmap(..., MAP_POPULATE)` (fault at mmap time), ya `madvise(MADV_WILLNEED)`,
ya `memset(arena, 0, size)` (touches every byte). `volatile` / `memset` isliye
taaki compiler dead-store eliminate na kare.

### Step 3 — warm the allocator + code paths

`malloc` free lists, `std::vector`/`std::unordered_map` internal buffers, thread
stacks — startup pe:
- Apne object pools se N objects alloc/free karo (folder `14`) → `malloc`
  arenas grow + fault ho jaate.
- Ek "dry run": fake market data se poora feed→book→strategy→order path chalao
  N baar (orders bheje bina) → har code page (`.text`) fault-in, har branch
  predictor/BTB warm, har data structure resized.
- Thread stacks: recursion ya bada local array se stack ko deep touch karo, ya
  `pthread_attr_setstacksize` + pre-fault.

**Example `07`** step 1+2 dikhta hai: `mlockall` + `MAP_POPULATE` + `memset` ke
baad "first real touch" bhi fault-free (~9 ns/page vs cold ~360 ns/page).

---

## `mlock` / `munlock` (region-specific)

```cpp
mlock(ptr, len);      // sirf yeh region pin
munlock(ptr, len);
mlock2(ptr, len, MLOCK_ONFAULT);   // lock as pages fault in
```

Use jab poori process lock nahi karni (RAM budget) — sirf hot arenas, rings,
lookup tables.

---

## Internal working / gotchas

- **`RLIMIT_MEMLOCK`**: kitni memory ek unprivileged process lock kar sakta
  (default often 64 KiB — useless). Raise karo. Locked memory swappable nahi →
  system-wide RAM pressure, isi liye limit hai.
- **`vm.swappiness`**: `1` (ya `0`) set karo (`06`) — kernel ko batao anon
  memory reclaim last resort. `mlockall` ke saath belt-and-suspenders.
- **Disable swap entirely** (`swapoff -a`) HFT boxes pe common — koi swap
  device hi nahi to major fault (swap-in) impossible; sirf OOM-kill risk (jise
  monitoring se manage karo).
- **THP + pre-fault interaction**: 2 MiB huge page = 1 fault instead of 512
  (`12`) — warming faster.
- **`fork` after warming**: COW pages RO ho jaate — child (ya parent) ki pehli
  write = COW fault. Agar warmed parent `fork` karta, warming ka faayda partly
  lost. Warm **after** fork, in each process.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `malloc` ke baad "memory ready" maan lena
`new int[10'000'000]` turant return — 0 physical pages. Pehla loop jo isse
bharega = 10M/512 ≈ ~19,500 minor faults, ek unpredictable ms-scale burst.
Pre-fault at startup.

### Trap 2 — `mlockall` bina limit raise
`mlockall` → `-1`/`ENOMEM` (default `RLIMIT_MEMLOCK` 64 KiB). Process silently
unlocked chalta. Return check + systemd `LimitMEMLOCK=infinity`.

### Trap 3 — pre-fault loop ko compiler ne hata diya
```cpp
for (size_t i = 0; i < n; i += 4096) buf[i] = 0;   // dead store -> -O2 removes
```
`volatile` pointer, ya `memset` (jise compiler as-is chhodta zyaadatar), ya
`asm volatile("" ::: "memory")` barrier, ya read-and-accumulate into `volatile`.

### Trap 4 — sirf `mlockall(MCL_FUTURE)`, pre-touch nahi
`MCL_FUTURE` future mappings ko lock karta **jab woh fault** hote — pehli touch
abhi bhi fault. `MCL_CURRENT` + explicit pre-touch chahiye.

### Trap 5 — thread stacks aur code pages bhool jaana
Data warm ki, par `.text` (code), thread stacks, aur library data segments cold
— pehla real request unhe fault karega. "Dry run" pura path chala ke sab warm
karo.

### Trap 6 — warming ke baad memory free/re-alloc
`std::vector` jo warm-up mein grow hua, phir `shrink_to_fit` / clear+dealloc →
pages `munmap`/`MADV_DONTNEED` ho gaye → dubara faultenge. Warm-up ke buffers
ko rakho (reserve capacity, don't shrink).

### Trap 7 — `mlockall` + huge `MAP_NORESERVE` mapping
Ek bada sparse mapping (e.g. 100 GB address space for a hash table you'll fill
1%) ko `MCL_FUTURE` sab lock karne ki koshish karega → OOM. Sparse structures
ko `MLOCK_ONFAULT` ya explicit region `mlock` only for the used part.

---

## > **HFT relevance**

> - **Zero page faults during trading hours** is a hard target. `ru_majflt`
>   delta = 0 always; `ru_minflt` delta = 0 in steady state (all touched at
>   startup).
> - **Startup warm-up routine** (runs before "go live"):
>   1. `mlockall(MCL_CURRENT | MCL_FUTURE)` (limits raised via systemd unit).
>   2. Allocate every pool / arena / ring / table at full size; `memset` or
>      `MAP_POPULATE` every page.
>   3. Dry-run the full pipeline N thousand times with synthetic data, orders
>      suppressed — warms `.text`, branch predictors, allocator, container
>      buffers, TLB.
>   4. Assert `getrusage` fault counts flatline over a final idle period.
> - **`swapoff -a`** + `vm.swappiness=1` — belt and suspenders against major
>   faults.
> - **Huge pages** (`12`) make warming cheaper (fewer, bigger pages).
> - **Warm per-process, after fork** — COW resets the benefit.

---

## Hands-on

```bash
# Linux pe -- example 07: cold vs warm vs mlock+populate, per-page latency + fault counts
g++ -std=c++20 -O2 29-LINUX-SYSTEMS/examples/07_page_faults.linux.cpp -o /tmp/pf && /tmp/pf
/usr/bin/time -v /tmp/pf 2>&1 | grep -iE 'page fault'

# limit check (mlockall ke liye)
ulimit -l                    # "max locked memory" -- unlimited chahiye
cat /proc/self/status | grep VmLck

# swap status
swapon --show ; cat /proc/sys/vm/swappiness

# ek running process ke faults
grep -E 'minflt|majflt' /dev/null; cat /proc/$(pidof trader)/stat | awk '{print "minflt="$10" majflt="$12}'
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`malloc` ne RAM de di" | virtual only; physical page pehli touch pe (fault) |
| "minor fault sasta hai, ignore" | ~0.2–2 µs each; 20k of them at once = ms burst |
| "`mlockall` ne pre-fault bhi kar diya" | `MCL_CURRENT` haan; `MCL_FUTURE` sirf on-fault — pre-touch chahiye |
| "pre-fault loop kaam kar raha" | `-O2` dead-store elim; `volatile`/`memset` |
| "data warm ki, done" | code pages, stacks, allocator, containers bhi warm karo |
| "swap off hai to memory tuning ki zaroorat nahi" | minor faults + COW faults abhi bhi; warm karo |

---

## Exercises

1. `std::vector<Order> v; v.reserve(1'000'000);` — kitne page faults abhi, kitne
   pehli baar `v.push_back` × 1M pe?

   <details><summary>Answer</summary>

   `reserve` ek `mmap`/`malloc` karta (~`1e6 * sizeof(Order)` bytes) — abhi ~0
   faults (lazy). Pehli baar jab tum 1M elements push karoge (har naye page ko
   likhoge) → `total_bytes / 4096` minor faults, ek burst. Fix: `reserve` ke
   baad ek pre-fault pass — `for (i=0;i<cap;i+=4096) ((volatile char*)v.data())[i]=0;`
   at startup (careful: before any real push, ya `memset` on `v.data()` up to
   capacity bytes).
   </details>

2. `mlockall(MCL_CURRENT | MCL_FUTURE)` call kiya, return 0. Baad mein ek naya
   64 MiB `mmap` — uski pehli touch pe fault hoga?

   <details><summary>Answer</summary>

   Haan, ek minor fault (per page) hoga — par woh page turant locked ho jaayega
   (`MCL_FUTURE`), aur phir kabhi swap nahi hoga. `MCL_FUTURE` "future pages ko
   fault hone pe lock karo" hai, "future pages ko pehle se fault kar do" nahi.
   Zero-fault chahiye to `MAP_POPULATE` ya pre-touch bhi karo.
   </details>

3. Warm-up ke baad process `fork` karta hai ek helper ke liye. Parent ki warmth
   ko kya hota?

   <details><summary>Answer</summary>

   `fork` sab writable pages ko RO mark karta (COW). Parent ka agla write us
   page pe = ek **COW fault** (~1–3 µs) + copy — warm-up ka "no faults" property
   toota. Child ki to har first-write bhi COW fault. Fix: `fork`/`exec` helper
   ko **warm-up se pehle** karo (startup order), ya helper ek alag pre-forked
   process ho, ya `posix_spawn` (`03`) use karo jo COW hi nahi karta.
   </details>

4. `/usr/bin/time -v ./trader` ke baad "Major page faults: 4127". Kya galat,
   pehla step?

   <details><summary>Answer</summary>

   4127 major faults = 4127 baar kernel disk se page laaya — swap-in ya
   file-backed page jo evict ho gaya tha. Trading process ke liye yeh 0 hona
   chahiye. Steps: (1) `swapon --show` — swap on hai? `swapoff -a`. (2)
   `vm.swappiness=1`. (3) `mlockall` — succeed ho raha? `ulimit -l`. (4) koi
   `mmap`'d file (config, market data) jo page-cache pressure mein evict ho
   raha — `mlock` woh region bhi. (5) RAM actually enough hai? OOM ke aas-paas
   to reclaim aggressive.
   </details>

5. Pre-fault loop `for (i=0; i<n; i+=4096) buf[i] = 0;` `-O2` pe kuch nahi
   karta. 3 tarike isse "stick" karne ke.

   <details><summary>Answer</summary>

   (1) `buf` ko `volatile char*` banao — har store observable, eliminate nahi
   hota. (2) `std::memset(buf, 0, n)` — compiler ise as-is chhodta (ya libc
   call). (3) Har iteration ke baad `asm volatile("" : : "r"(buf+i) : "memory");`
   — compiler ko lagta memory read/observed hui. (4) Read version: `volatile
   char sink; for (...) sink = buf[i];` — anonymous pages read-fault pe
   shared zero-page map karta (write-fault se alag; write chahiye to actually
   commit karne ke liye). HFT: write-touch use karo (`memset` / volatile write).
   </details>

---

## Interview questions

1. Minor vs major vs COW fault — kya, cost order-of-magnitude.
2. `malloc` ke baad memory "resident" kab banti?
3. `mlockall` `MCL_CURRENT` vs `MCL_FUTURE` — kya farak, pre-touch kyun phir bhi.
4. `RLIMIT_MEMLOCK` — kyun default itna chhota, kaise raise.
5. Memory warming ke 3 steps (lock, pre-fault, dry-run).
6. Pre-fault loop ko compiler kyun hatata, kaise roko.
7. `fork` warming ke baad — kya toota, kaise handle.

---

## Next
→ [`14-numa.md`](14-numa.md)
