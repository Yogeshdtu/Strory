# 07 — mmap: file mapping, anonymous memory, shared mappings

## Prerequisites
- `05-file-descriptors.md` (open/read/write, page cache)
- `14-MEMORY` (virtual pages, page tables)
- `13-page-faults-and-mlock.md` padhne se pehle isko padho (yeh uska setup hai)

## Yeh topic abhi kyun
`mmap()` = "ek fd (ya anonymous memory) ko seedha apni address space mein map
kar do, aur usse ek array ki tarah use karo". Yeh do bade kaam karta HFT mein:
(1) **file I/O bina `read`/`write` syscalls** — bade market-data files ko memory
ki tarah access, (2) **shared memory IPC** ka mechanism (`08`). Aur `malloc`
bade blocks ke liye andar `mmap` hi karta — to yeh samajhna "meri memory kahan
se aati hai" ka jawab hai.

---

## `mmap` ka call

```cpp
#include <sys/mman.h>

void* p = ::mmap(nullptr,           // addr hint (nullptr = kernel chune)
                 length,            // bytes (page-multiple pe round hota)
                 PROT_READ | PROT_WRITE,   // access: PROT_NONE/READ/WRITE/EXEC
                 flags,             // MAP_SHARED | MAP_PRIVATE | MAP_ANONYMOUS ...
                 fd,                // file (ya -1 for anonymous)
                 offset);           // file offset (page-aligned)
if (p == MAP_FAILED) { perror("mmap"); }
// ... use p as a byte array ...
::munmap(p, length);
```

### Do axes

**File-backed vs anonymous:**
| | `fd` = valid file | `MAP_ANONYMOUS` (fd = -1) |
|---|---|---|
| kya | file ke bytes memory mein | zeroed memory, kisi file se juda nahi |
| use | file I/O without syscalls, code/data loading | `malloc` ka bada-block backend, arenas |

**Shared vs private:**
| | `MAP_SHARED` | `MAP_PRIVATE` |
|---|---|---|
| writes | file mein / dusre mappers ko dikhte | **copy-on-write**, sirf tumhe; file unchanged |
| use | IPC (`08`), file editing | `fork` semantics, read-mostly file, `.data` segment |

Combos:
- `MAP_PRIVATE | fd` — ELF loading (code shared read-only, data COW).
- `MAP_SHARED | fd` — memory-mapped database file, do process ek file edit.
- `MAP_PRIVATE | MAP_ANONYMOUS` — plain memory (what `malloc`/`new` use for big).
- `MAP_SHARED | MAP_ANONYMOUS` — `fork`-shared scratch region (no file).

---

## Lazy allocation — `mmap` RAM turant nahi deta

`mmap(256 MB)` **instant** return karta aur ~0 physical RAM leta. Sirf virtual
address range reserve hoti + ek VMA (kernel struct) banta. Physical page tab
allocate hota jab tum us page ko **pehli baar chhoote ho** → **page fault** →
kernel ek zeroed page (anonymous) ya file-cache page (file-backed) map karta.

**Example `04` measure karta:** 256 MB anonymous ko pehli baar touch karo →
~65k minor faults, ~200–800 ns/page. Doosri baar touch → 0 faults, ~10 ns/page.

Isi liye "allocation ki cost" `malloc` ke return pe nahi, **pehli use** pe hai —
aur woh trading hours mein nahi chahiye. Fix: `MAP_POPULATE` (mmap ke waqt hi
fault karao), ya `mlockall` + pre-touch (`13`).

---

## File mapping — `read`/`write` ke bina file I/O

```cpp
int fd = ::open("md.bin", O_RDWR);
struct stat st; ::fstat(fd, &st);
auto* base = static_cast<std::byte*>(
    ::mmap(nullptr, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
::close(fd);                              // mapping fd close ke baad bhi zinda

// ab file = array
uint64_t first_ts = *reinterpret_cast<uint64_t*>(base);
base[st.st_size - 1] = std::byte{0xFF};   // write -> page dirty -> kernel writeback

::msync(base, st.st_size, MS_SYNC);       // dirty pages abhi disk pe (durability)
::munmap(base, st.st_size);
```

Faayde:
- Random access — koi `lseek` nahi.
- Bade read-only datasets — page cache pages **saare readers ke beech share**;
  N processes same file map karein → ek copy RAM mein.
- Zero `read`/`write` syscalls in the access loop.

Trade-offs:
- Access pe page fault (pehli baar) — I/O ka cost `read()` ki jagah fault pe.
- File size change (`ftruncate` chhota) ke baad mapped region touch → `SIGBUS`.
- Page cache eviction under memory pressure → agli access = major fault (disk).

---

## `madvise` — kernel ko hint do

```cpp
::madvise(p, len, MADV_SEQUENTIAL);   // aggressive readahead
::madvise(p, len, MADV_RANDOM);       // readahead band (random access pattern)
::madvise(p, len, MADV_WILLNEED);     // abhi prefetch kar do
::madvise(p, len, MADV_DONTNEED);     // pages free karo (anon: next touch = zero page)
::madvise(p, len, MADV_HUGEPAGE);     // THP is region pe (12)
```

---

## `mmap` vs `read`/`write` — kab kaunsa

| Situation | Behtar |
|---|---|
| Bada file, random access, read-mostly | `mmap` |
| Same file N processes padhte | `mmap` (shared page cache) |
| Sequential streaming, once | `read` with big buffer (readahead already good) |
| Small files, many | `read` (mmap ka per-call setup + TLB cost) |
| Need explicit error handling per I/O | `read`/`write` (mmap errors = `SIGBUS`, harder) |
| Low-latency append log | `write` / `io_uring` (mmap writeback timing unpredictable) |

---

## Internal working

- `mmap` ek **VMA** (`vm_area_struct`) add karta process ke memory descriptor
  mein. `/proc/self/maps` mein ek line = ek VMA.
- Page fault handler: address kis VMA mein → anonymous? zeroed page do. file?
  page cache mein hai? map karo (minor fault). nahi? disk se padho (major
  fault, ~ms).
- `MAP_SHARED` file writes: page "dirty" mark hoti; kernel `pdflush`/writeback
  threads use disk pe likhte (`dirty_ratio`, `dirty_writeback_centisecs`), ya
  `msync` pe force.
- `munmap` VMA hatata, page tables clear karta, aur **TLB shootdown** —
  baaki cores ko IPI bhejta apni TLB entries invalidate karne ko (multi-core
  pe ~µs, ek jitter source).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `mmap` ke baad "allocation done" maan lena
Virtual reservation hui, physical RAM nahi. Latency-critical arena ke liye
`MAP_POPULATE` + pre-touch + `mlock` (`13`), warna pehli trade pe fault storm.

### Trap 2 — file chhoti, mapping badi → `SIGBUS`
`ftruncate(fd, small)` ya file truncate ho gayi, phir mapped region ke us hisse
ko touch → `SIGBUS` (not `SIGSEGV`). Mapping se pehle `fstat` se size lo; grow
karo to `ftruncate` pehle.

### Trap 3 — `munmap` galat length
`mmap` ne length ko page-size pe round-up kiya, par `munmap(p, wrong_len)` sirf
utna hi unmap karega — leak, ya partial unmap se crash. Stored length rakho.

### Trap 4 — `MAP_SHARED` bina `msync`/`munmap` ke durability maan lena
Dirty pages kernel apni marzi se writeback karta (seconds later). Crash ho jaye
to data gaya. `msync(MS_SYNC)` ya `munmap` (jo flush karta) se guarantee.

### Trap 5 — private mapping ko IPC ke liye use
`MAP_PRIVATE` writes COW hote — dusra process (ya post-fork parent/child) tumhare
writes **nahi** dekhega. IPC ke liye `MAP_SHARED` (`08`).

### Trap 6 — bahut chhote mmaps
Har `mmap` ek VMA + fault + possible TLB cost. 10000 chhote objects ke liye
10000 `mmap` = disaster. Ek bada mmap arena lo, usme khud sub-allocate (pool —
folder `14`).

### Trap 7 — mmap'd file pe `read()` bhi karna, offset confusion
`mmap` fd ka offset nahi badalta; parallel `read(fd)` alag offset track karta.
Mix karne pe confusion. Ek hi access method chuno.

---

## > **HFT relevance**

> - **Market data replay:** din bhar ka capture file `mmap(MAP_PRIVATE|
>   MAP_POPULATE)` → backtester file ko ek `struct Packet[]` array ki tarah
>   iterate karta, zero I/O syscalls, OS readahead ki bhi zaroorat nahi.
> - **Shared memory rings (`08`):** feed handler process data ko
>   `mmap(MAP_SHARED)` region (often `/dev/shm` ya hugetlbfs file) mein likhta;
>   strategy processes usi ko map karke bina syscall padhte.
> - **Pre-fault the world:** startup pe saari pools/arenas
>   `mmap(MAP_ANONYMOUS|MAP_POPULATE)` + `memset` + `mlock`. Trading hours mein
>   page fault = 0 (`13`).
> - **Huge-page mmap:** `mmap(MAP_HUGETLB)` order book / big hash tables ke liye
>   → kam TLB misses, kam faults (`12`).
> - **Avoid mmap for the hot append log** — writeback timing unpredictable;
>   lock-free ring + async `writev` behtar.

---

## Hands-on

```bash
# Linux pe -- example 04: file mapping + anon + lazy fault cost
g++ -std=c++20 -O2 29-LINUX-SYSTEMS/examples/04_mmap_demo.linux.cpp -o /tmp/mm && /tmp/mm

# apni mappings dekho
cat /proc/self/maps            # kisi bhi process pe: [heap] [stack] libs (mmap'd)
grep -c '' /proc/$(pidof firefox)/maps 2>/dev/null   # bade app mein saikdon VMAs

# mmap vs read benchmark idea:
#   same 1GB file ko (a) read() 64KB-buffer loop se sum karo, (b) mmap karke sum karo
#   cold cache (echo 3 > /proc/sys/vm/drop_caches) pe dono ~disk-bound;
#   warm cache pe mmap thoda tez (no copy) par TLB pe depend.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`mmap` ne RAM allocate ki" | virtual reservation; RAM pehli touch pe (fault) |
| "mapped file ka write turant disk pe" | dirty page; writeback later; `msync`/`munmap` se force |
| "`MAP_PRIVATE` se IPC ho jaayega" | writes COW — dusre process ko nahi dikhte; `MAP_SHARED` |
| "file chhoti ho to bas 0 milega" | mapped-but-truncated touch = `SIGBUS` |
| "mmap hamesha read se tez" | small/sequential pe read jeetta; setup+TLB cost |
| "munmap length koi bhi chalega" | exact (rounded) length; warna leak/partial |

---

## Exercises

1. `mmap(MAP_ANONYMOUS, 1 GB)` turant return ho gaya. `top` mein `RES` nahi
   badha, `VIRT` 1 GB badha. Kyun?

   <details><summary>Answer</summary>

   `VIRT` = virtual address space reserved (1 GB VMA bani). `RES` (RSS) =
   resident physical pages — abhi 0 kyunki koi page touch nahi hua. Jaise-jaise
   tum pages likhoge, minor faults se `RES` badhega. `MAP_POPULATE` ya
   `memset` se abhi hi `RES` = 1 GB.
   </details>

2. Ek process file ko `MAP_SHARED` map karke edit karta, doosra usi file ko
   `MAP_SHARED` map karke padhta. Reader ko writes turant dikhte? Reader `read()`
   se padhe to?

   <details><summary>Answer</summary>

   `MAP_SHARED` dono ko **same physical page cache pages** deta — writer ka
   store reader ki agli load mein turant dikhta (same RAM, hardware coherence).
   `read()` bhi page cache se hi aata, to woh bhi latest dekhega. Sirf disk pe
   kab jaata woh alag baat (`msync`).
   </details>

3. Backtester 200 GB market-data file `mmap` karta hai 32 GB RAM wali machine
   pe. Chalega? Kya hoga?

   <details><summary>Answer</summary>

   Haan chalega — mapping virtual hai. Jaise-jaise sequential iterate karoge,
   pages fault-in honge (disk se), aur memory pressure pe purane pages evict
   (file-backed clean pages bina writeback ke drop ho jaate). Effectively OS ne
   tumhare liye streaming kar diya. `MADV_SEQUENTIAL` + `MADV_DONTNEED` peeche
   ke pages pe → smooth. Random access hota to thrash.
   </details>

4. `MAP_POPULATE` `mmap` call ko slow (ms) kar deta hai. HFT ke liye yeh accha
   ya bura?

   <details><summary>Answer</summary>

   Accha — startup pe. `MAP_POPULATE` saare faults **abhi** (init time) le
   aata, taaki trading hours mein pehli touch fault-free ho. "Cost ko predictable
   time pe shift karo" — HFT ka core principle. Bina `MAP_POPULATE` ke pehla
   market tick fault storm trigger karega.
   </details>

5. `mmap`'d region pe `munmap` ke baad ek jitter spike dikhta hai baaki threads
   mein. Kyun?

   <details><summary>Answer</summary>

   TLB shootdown. `munmap` page tables badalta; kernel ko har doosre core ko
   IPI (inter-processor interrupt) bhejna padta taaki woh apni TLB se un
   addresses ki entries flush karein. IPI + flush = us core pe ~µs stall.
   Isi liye HFT: mappings startup pe set karo, trading hours mein
   `mmap`/`munmap`/`mprotect` **bilkul nahi** (yeh "hot path pe koi syscall" se
   bhi aage — page-table mutation poore process ko touch karta).
   </details>

---

## Interview questions

1. `mmap` ke 4 flag-combos aur unke use (private/shared × file/anon).
2. Lazy allocation — `mmap` return ke waqt kya hota, physical page kab?
3. File mapping `read()` se kab behtar, kab worse?
4. `MAP_SHARED` vs `MAP_PRIVATE` writes ka fate.
5. `msync` / dirty page writeback — durability guarantee kab?
6. `MAP_POPULATE` + `mlock` — HFT startup mein kyun.
7. `munmap`/`mprotect` se TLB shootdown — kya hai, jitter kyun.

---

## Next
→ [`08-shared-memory.md`](08-shared-memory.md)
