# 18 — Page fault avoidance: `mlockall`, pre-faulting, huge pages

## Prerequisites
- **`29-LINUX-SYSTEMS/07`** (page faults & `mlock`), **`32-CACHE-MEMORY-PERFORMANCE/11`**
  (TLB, huge pages), `35-PROFILING-BENCHMARKING/09` (cold start)
- `examples/11_page_fault_warmup.cpp`

## Yeh topic abhi kyun
Fresh memory ka pehla touch = a page fault: the kernel finds a physical
page, zeroes it, updates the page table (~1–3 µs minor; ms if disk-backed).
On the hot path that's a tail spike (`11_page_fault_warmup.cpp`: COLD p99
**2875 ns** vs WARM p99 **30 ns**). Steady state mein **zero** page faults —
this lesson is how.

---

## Fault types

| Fault | Cause | Cost |
|---|---|---|
| **minor** | page mapped, not yet resident (first touch of `malloc`/`mmap` memory, stack growth, COW after fork) | ~1–3 µs (kernel zeroes + PTE) |
| **major** | page must be read from disk (swap, file-backed, executable text) | ms |
| **COW** | write to a page shared after `fork` | minor-fault cost to copy |
| **TLB miss** (not a fault) | translation not cached | up to 4 dependent loads (page walk — 32/11) |

Steady-state HFT: **zero minor, zero major, minimal TLB misses.**

---

## The recipe (startup)

```cpp
// 1. allocate everything, sized for the worst case (lesson 05)
//    prefer MAP_POPULATE so the kernel pre-faults at mmap time:
void* p = mmap(nullptr, n, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);

// 2. lock it: no swap-out, and MCL_FUTURE pre-faults future mappings
mlockall(MCL_CURRENT | MCL_FUTURE);

// 3. touch every page you'll use (write, not just read — anonymous pages
//    map to a shared zero page until written)
for (std::size_t i = 0; i < n; i += PAGE) reinterpret_cast<volatile char*>(p)[i] = 0;

// 4. pre-fault the stack: recurse / alloca to the max depth once, then return
prefault_stack(MAX_STACK_BYTES);

// 5. run every hot code path once with dummy data (text pages resident,
//    I-cache/BTB warm — lesson 20)
```

`RLIMIT_MEMLOCK` (`ulimit -l`) must be high enough or `mlockall` fails
partially/silently — raise it in the systemd unit / `limits.conf`.

**Measured (`11`)**: COLD first-write mean 271 ns / p99 2875 ns vs WARM
mean 21 ns / p99 30 ns — ~13× on the mean, ~95× on p99. Every new page is a
fault; the touch pass eliminates all of them.

---

## Huge pages (2 MiB / 1 GiB)

A 4 KiB page covers little TLB reach (L1 dTLB ~64 entries → ~256 KiB; L2
STLB ~1.5K → ~6 MiB — 32/11). A working set bigger than STLB reach → a TLB
miss (page walk) per ~stride. **2 MiB pages → 512× the reach per entry** and
a shorter walk.

| Mechanism | Notes |
|---|---|
| **explicit hugetlbfs** (`mmap(MAP_HUGETLB)` / `mmap` a hugetlbfs file) | reserved pool (`vm.nr_hugepages`), deterministic, no `khugepaged` jitter — **HFT choice** |
| **THP** (transparent huge pages) | automatic promotion by `khugepaged` — but that background scan is a **jitter source**; `MADV_HUGEPAGE` to opt a region in, or `transparent_hugepage=never` and use explicit |
| **1 GiB pages** | for very large buffers; must be reserved at boot |

Pair with pre-fault + `mlock`. Also consider huge pages for the **text**
segment (iTLB — lesson 21).

---

## Other first-touch sources to kill

- **Stack growth** in a rarely-taken deep branch → fault. Pre-fault the
  stack to the max depth at startup.
- **TLS** on a newly-spawned thread → first-touch of the TLS block. Spawn
  all threads at startup and warm them.
- **A library's lazy init** (a logger, a metrics client, `std::locale`,
  `std::regex`, the first `std::cout`) → first-use allocation + fault. Force
  the init during warm-up.
- **`fork` after warm-up** → COW faults on the hot thread's next writes.
  Don't fork the hot process after go-live.
- **New `mmap`s in steady state** (a pool that grows, a file mapped on
  demand) → don't; pre-map everything.

---

## Verifying

```bash
perf stat -e page-faults,minor-faults,major-faults ./app   # ~flat after warm-up?
perf record -e page-faults -g ./app                        # the fault's stack = the culprit
/usr/bin/time -v ./app                                     # "Minor (reclaiming) page faults"
cat /proc/<pid>/status | grep -i vmlck                     # how much is actually locked
grep -i huge /proc/<pid>/smaps | ...                       # huge pages in use
```
A "zero-fault steady state" claim without `perf stat -e page-faults` over a
soak is just hope.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `malloc`/`mmap` without touching
Returns instantly (lazy). First **write** to each page faults. `MAP_POPULATE`
or an explicit write pass.

### Trap 2 — `mlockall` "succeeded" but faults continue
`MCL_CURRENT` locks what's resident *now*; `MCL_FUTURE` locks pages *as they
become resident* — an untouched page still faults on first touch (then stays
locked). You must still pre-fault. And `RLIMIT_MEMLOCK` may have capped it.

### Trap 3 — reading to "touch" anonymous pages
Anonymous pages map to a shared read-only zero page until **written**. A read
pass doesn't make them private/resident. **Write** to each page.

### Trap 4 — THP jitter
`khugepaged` scanning + compaction is a periodic stall on your isolated core.
`transparent_hugepage=never` + explicit hugetlbfs, or `MADV_HUGEPAGE` on
specific regions with `defrag` tuned.

### Trap 5 — forgetting the stack and TLS
Pre-fault the data heap but not the stack / thread-local blocks → a deep
rare branch or a new thread still faults.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`malloc` gave me the memory" | lazy — first write faults; pre-fault it |
| "`mlockall` = no faults" | + pre-fault (untouched pages still fault once) + `RLIMIT_MEMLOCK` |
| "reading the buffer warms it" | anonymous pages need a **write** to become resident |
| "THP is free performance" | `khugepaged` is a jitter source; explicit hugetlbfs for HFT |
| "heap is pre-faulted, done" | + stack + TLS + library lazy-init + no new mmaps |

---

## Exercises

1. Ek engineer startup pe `pool_.reserve(1'000'000)` karta hai aur
   `mlockall(MCL_CURRENT | MCL_FUTURE)` (success). Steady state mein `perf
   stat` abhi bhi ~50 minor-faults/sec dikhata jab naye orders pool se
   allocate hote hain. Kyun, aur fix?

   <details><summary>Answer</summary>

   `reserve(1'000'000)` allocates the backing storage but **does not touch
   it** (no elements constructed — `reserve`, not `resize`). So the pages are
   mapped but not resident. `mlockall(MCL_FUTURE)` will lock each page *when
   it first becomes resident* — i.e. **on first touch, which still faults**
   (once per page), then stays locked. As the hot path allocates orders from
   previously-untouched regions of the pool, each new page → one minor fault
   (~1–3 µs spike) → your ~50/sec.
   Fixes: (1) After `reserve`, do an explicit **write pass** over the whole
   buffer — e.g. `std::memset(pool_.data(), 0, pool_.capacity() * sizeof(T))`,
   or construct all slots (`resize` + a recycle pool — lesson 07), or a
   loop writing one byte per 4 KiB page. Now every page is resident and
   locked before go-live. (2) Or `mmap(..., MAP_POPULATE)` the pool's
   storage so the kernel pre-faults at map time. (3) Verify: `perf stat -e
   page-faults` over a soak → should drop to ~0 after warm-up; if not,
   `perf record -e page-faults -g` shows which access still faults.
   </details>

2. `mlockall` ke baad `perf record -e page-faults -g` dikhata ki faults ki
   stack hamesha ek rarely-called error-handling function mein hai jo ek
   bada local `char buf[65536]` declare karta. Kya ho raha, aur do fixes.

   <details><summary>Answer</summary>

   The error handler is on a **cold path** — it's only entered rarely (an
   error). When it *is* entered, it declares a 64 KiB local array, which
   grows the stack by 64 KiB into pages that were **never touched during
   warm-up** (warm-up ran the hot paths, not this rare error branch). The
   stack extension faults page-by-page (~16 minor faults for 64 KiB) — and
   because errors correlate with unusual market conditions, this spike lands
   exactly when you least want it.
   Fixes:
   (1) **Pre-fault the stack** at startup to at least the maximum depth any
   code path (including error handlers) can reach: a helper that recurses or
   `alloca`s down to `MAX_STACK` and writes one byte per page, then returns.
   Combined with `mlockall(MCL_FUTURE)`, those pages stay resident and
   locked.
   (2) **Don't put a 64 KiB buffer on the stack** — make it a pre-allocated,
   pre-faulted static / member / thread-local buffer the error handler
   reuses. The handler runs rarely; a shared buffer is fine, and it's warm.
   (3) Also mark the handler `[[gnu::cold]]` / `[[gnu::noinline]]` so it
   doesn't bloat the hot path's I-cache (lesson 21) — orthogonal, but while
   you're there.
   </details>

---

## Interview questions

1. Minor vs major vs COW fault — causes and costs.
2. The startup recipe: allocate + `MAP_POPULATE` + `mlockall` + write-touch + stack pre-fault.
3. Why a **read** pass doesn't make anonymous pages resident.
4. Huge pages — hugetlbfs vs THP, and why THP is a jitter source for HFT.
5. First-touch sources beyond the heap (stack, TLS, library lazy-init, `fork` COW).
6. Verifying zero-fault steady state (`perf stat -e page-faults`).

---

## Next
→ [`19-cpu-pinning-strategy.md`](19-cpu-pinning-strategy.md)
