# 05 — Pre-allocation: startup pe sab, steady state mein kuch nahi

## Prerequisites
- `04-allocation-avoidance.md`
- `14-MEMORY` (heap, static/thread_local), `19-STL/26` ("reserve everything")
- `examples/05_pmr_containers.cpp`, `examples/11_page_fault_warmup.cpp`

## Yeh topic abhi kyun
Lesson 04: hot path pe zero allocation. Toh memory aayegi kahan se? Jawab:
**startup pe**. Yeh lesson ek design discipline hai — "allocate everything
up front, size it for the worst case, touch it, lock it, and never allocate
again."

---

## Do phases: warm-up vs steady state

```
  ┌──────────────── WARM-UP (startup, not latency-critical) ────────────────┐
  │  - parse config                                                          │
  │  - allocate ALL buffers, pools, rings, tables (sized for worst case)    │
  │  - touch every page (write to it) so it's resident (ex 11, lesson 18)  │
  │  - mlockall(MCL_CURRENT | MCL_FUTURE)  (Linux)                          │
  │  - run every code path once with dummy data (JIT-free but I-cache warm) │
  │  - pin threads, set affinity, drop into the run loop                    │
  └───────────────────────────────────────────────────────────────────────────┘
  ┌──────────────────── STEADY STATE (hot, latency-critical) ───────────────┐
  │  - ZERO allocation, ZERO page faults, ZERO first-touch                   │
  │  - pools / arenas / rings recycle the pre-allocated memory              │
  │  - if a pool would overflow -> a deliberate policy (drop / reject /     │
  │    a sized-big-enough-that-it-never-happens guarantee)                  │
  └───────────────────────────────────────────────────────────────────────────┘
```

Steady state mein ek naya `new`, ek naya page fault = ek bug, ek assert.

---

## Kya pre-allocate karna hai

| Cheez | Kaise |
|---|---|
| order / event / message objects | fixed **object pool**, sized to max in-flight (07) |
| per-message / per-tick temporaries | a fixed **arena**, `reset()` per unit (08) |
| STL containers (vector/map/...) | `reserve()` / `rehash()` at startup, ya **PMR** on a pre-allocated buffer (09), never grow in steady state |
| producer→consumer queues | fixed-capacity **ring buffers** (15) |
| lookup tables (symbol → id, id → book) | build once, `std::vector` indexed by id |
| strings you must build | fixed `char[N]` + length, or a dedicated arena |
| the logger's records | a fixed ring of fixed-size records (35/16) |
| scratch buffers | member variables / thread-locals, reused (`.clear()` keeps capacity) |

---

## Sizing: worst case, measured

Pre-allocation ka matlab hai tumhe **worst-case capacity** pata honi chahiye:

- **max orders in flight** — replay historical sessions, take the peak,
  multiply by a safety factor (2-4x).
- **max book depth** — per symbol, from historical data + a hard cap.
- **max message burst** — peak msgs/sec × your worst-case processing time =
  max queue depth before you catch up.
- **max temp objects per message** — from the parser's structure.

Yeh **guess nahi** — historical data se measure karo (folder 35 methodology).
Agar galat guess kiya:
- **too small** → pool overflow in production → drop / reject / crash.
- **too big** → wasted RAM (aur TLB pressure agar bahut bada — folder 32/11).

Trade-off: thoda over-provision karo (RAM sasti hai), par itna nahi ki
working set L3 / STLB reach se bahar chala jaaye.

---

## Steady-state guarantee: prove it

Pre-allocation sirf tab kaam ki jab tum **guarantee** kar sako ki steady
state mein allocation nahi hoti. Kaise:

1. **`std::pmr::null_memory_resource()`** as the upstream of your monotonic
   / pool resource → any unexpected allocation → `std::bad_alloc` → caught
   in test/CI (`05_pmr_containers.cpp` demonstrates this tripwire).
2. **Override global `operator new`** in a debug/CI build to assert /
   log / count → run your steady-state loop → assert `new` count == 0
   (`08_std_function_cost.cpp` uses this to *measure* allocations).
3. **`perf stat -e page-faults`** over a soak → should be ~flat (only the
   warm-up faults, then zero).
4. **`ltrace -e malloc`** / a `malloc` hook / heaptrack → zero `malloc`
   calls after warm-up.

Ek "zero-allocation steady state" claim bina in checks ke sirf ek ummeed hai.

---

## Warm-up is not just memory

Folder 35/09 (cold start) + lesson 20 (cache warming):
- **Code** — run every hot code path once (I-cache, BTB, uop cache warm).
- **Data caches** — touch the working set (L1/L2/L3 warm), but not so much
  that you evict what matters.
- **Branch predictor** — feed it representative data so it's trained.
- **Connections** — TCP handshakes, TLS, subscriptions all done before go-live.
- **The allocator itself** — do a burst of pool acquire/release so its
  free-lists are warm (though a good pool has nothing to warm).

`11_page_fault_warmup.cpp` measured: COLD first-write mean **271 ns** / p99
**2875 ns** vs WARM re-write mean **21 ns** / p99 **30 ns** — ~13x on mean,
~95x on p99. Har naya page ek minor fault; warm-up eliminates it.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — allocate but not touch
`malloc(1 GB)` returns instantly (lazy). The **first write to each page**
faults (~µs). Pre-allocation ke baad **poora buffer write karo** (ek pass).

### Trap 2 — `reserve` but then exceed it
`v.reserve(1000)` then push 1001 → realloc. Size for the real worst case +
margin, and assert on overflow.

### Trap 3 — `mlockall` without `RLIMIT_MEMLOCK`
Linux: `ulimit -l` / `RLIMIT_MEMLOCK` caps how much you can lock. Raise it
(`/etc/security/limits.conf` or the systemd unit) or `mlockall` fails
silently / partially.

### Trap 4 — pool sized from a guess
"1024 should be enough" → production burst hits 1100 → overflow. Size from
historical peak × safety factor, and monitor headroom.

### Trap 5 — over-provisioning into TLB/cache pressure
A 4 GB pool "to be safe" → the working set doesn't fit STLB reach → every
access a page walk (folder 32/11). Right-size + huge pages.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "startup pe allocate = done" | + touch every page + `mlock` + warm code paths |
| "`malloc` succeeded so memory is ready" | lazy — first write faults; pre-fault it |
| "reserve ek baar, safe" | size for worst case + margin; assert on overflow |
| "zero-alloc steady state" (unverified) | prove it: `null_memory_resource` / `new` hook / `perf page-faults` |
| "bada pool = safe" | TLB/cache pressure; right-size + huge pages |

---

## Exercises

1. Ek engineer kehta "maine startup pe `std::vector<Order> pool(100000)`
   bana liya, ab hot path allocation-free hai." Do problems batao.

   <details><summary>Answer</summary>

   (1) **Pages not resident.** `std::vector<Order> pool(100000)` allocates
   *and* value-initializes (writes) 100000 `Order`s — so actually this one
   *does* touch every page (the zero-fill). ✓ that part is fine. **But** if
   he'd written `pool.reserve(100000)` instead, the memory would be
   allocated but **untouched** → first use of each slot faults (~µs). And
   `mlockall` still needed so it can't be swapped out. And nothing warmed
   the *code* paths.
   (2) **How does the hot path use it?** A `std::vector<Order>` gives you
   contiguous storage, but if the hot path does `pool.push_back(...)` beyond
   100000 → realloc. If it uses indices into a fixed region with a free-list
   → that's an object pool (07), and `std::vector` is the wrong abstraction
   (it has no free-list, no O(1) release of a middle element). He needs a
   real pool with acquire/release, sized to **max in-flight** (not total
   orders ever), plus the touch + `mlock` + warm-up.
   (3) **Sizing** — is 100000 the measured worst-case max-in-flight, or a
   guess? Replay historical peak.
   </details>

2. Tum `mlockall(MCL_CURRENT | MCL_FUTURE)` call karte ho aur woh success
   return karta hai, par `perf stat -e page-faults` steady state mein abhi
   bhi ~20 minor faults/sec dikhata. Kya ho sakta?

   <details><summary>Answer</summary>

   `mlockall(MCL_CURRENT)` locks pages **already mapped and resident** at
   call time; `MCL_FUTURE` locks **future** mappings *as they are made
   resident* — but a page that was `mmap`'d but never touched is not yet
   resident, so the **first touch still faults** (then stays locked).
   Candidates for the residual faults:
   (1) **A buffer allocated but not pre-faulted** — `mlockall` + `MCL_FUTURE`
   doesn't pre-fault; you must write to every page once. Add an explicit
   touch pass, or `mmap(..., MAP_POPULATE)`, or `madvise(MADV_WILLNEED)`.
   (2) **Stack growth** — a deep call path in a rarely-taken branch grows
   the stack into fresh pages → fault. Pre-fault the stack (recurse/alloca
   to the max depth once at startup) or set a guaranteed stack size and
   touch it.
   (3) **A library doing lazy allocation** — a logger, a metrics client, an
   `std::function` somewhere, TLS init on a new thread — each a first-touch.
   (4) **COW after fork** — if you `fork`'d, writes to shared pages fault to
   copy. Don't fork after warm-up on the hot thread.
   Track them: `perf record -e page-faults -g` → the fault's stack shows
   exactly which allocation/access.
   </details>

---

## Interview questions

1. Warm-up phase vs steady state — steady state ki 3 "zero" guarantees.
2. Pool/arena/ring sizing — worst case kaise nikaalo, over/under ke costs.
3. "Zero-allocation steady state" ko **prove** karne ke 3 tareeke.
4. `malloc` succeeded ≠ memory ready — kyun, aur fix.
5. Warm-up mein memory ke alaawa kya-kya (code, predictor, connections).

---

## Next
→ [`06-memory-pools.md`](06-memory-pools.md)
