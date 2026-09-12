# 25 — Exercises: ultra-low-latency C++

## Prerequisites
- Poora folder 36 (`01`–`24`) + `examples/`

## Kaise use karein
- **Part A** — hot-path bug hunt: har jitter/allocation/copy source list karo.
- **Part B** — "kaunsi technique + trade-off": diye scenario ke liye choose,
  aur kyun NAHI ka case bhi socho (lesson 24).
- **Part C** — profiler output diagnosis (`perf` top-down / annotate / `c2c`).
- **Part D** — hands-on: chalao, measure, compare. **Real observations.**

> `-O2`. Numbers = is box (AMD Zen 2, ~2 GHz, MinGW/Windows, unpinned;
> SSE2 baseline). **Ratios / shapes port; tail absolutes don't** — max on
> per-op-timed loops is OS-interrupt noise, p50/p99/p99.9 are the signal.

---

## Part A — Hot-path bug hunt

### A1
```cpp
void on_quote(const Quote& q) {
    auto key = std::to_string(q.symbol_id);
    auto it = books_.find(key);                     // std::unordered_map<std::string, Book>
    if (it == books_.end()) it = books_.emplace(key, Book{}).first;
    Book& b = it->second;
    b.levels.push_back({q.px, q.qty});               // std::vector<Level>
    if (b.levels.size() > 32) b.levels.erase(b.levels.begin());
    log_ << "quote " << q.symbol_id << " @ " << q.px << "\n";
}
```
Har allocation / syscall / cache-hostile access + fix.

<details><summary>Answer</summary>

- `std::to_string(q.symbol_id)` — a `std::string` per quote (SSO for small
  ids, heap for large). **Eliminate**: `symbol_id` is already an int.
- `books_.find(key)` / `emplace(key, ...)` — hash a string, chase a bucket +
  a node (~2-3 misses — 10 ex 1), and `emplace` allocates a node + copies
  the string key. **Eliminate**: `std::vector<Book> books_` indexed by a
  dense `symbol_id` → 1 load, no hash, no alloc. `emplace` (first-seen
  symbol) moves to a cold subscription path.
- `b.levels.push_back` — grows (realloc + copy) until capacity settles; and
  `erase(begin())` is O(n) (shifts 31 elements every quote past 32).
  **Eliminate**: a fixed `std::array<Level, 32>` + a head index used as a
  **ring** (lesson 15) — O(1) insert, no shift, no alloc, `reserve`d... it's
  fixed. Or if it's a sorted book, an incremental update of the changed
  level only (lesson 01 ex 3).
- `log_ << ...` — `operator<<` formats (may allocate for the int/double), a
  stream lock, a buffer, maybe a flush → a variable spike. **Eliminate**:
  push a fixed-size `{symbol_id, px, ts}` record into an SPSC log ring; a
  logger thread formats + writes (17, 35/16).
- `Book&` — fine, it's a reference (no copy), but make sure `Book` isn't
  copied anywhere; and hot/cold split `Book` if it carries cold fields (10).
End state: resolve id (or it's already the id), `Book& b = books_[id]`, an
O(1) ring/array update or an incremental level update, one ring push.
Zero allocation, bounded work, no syscall.
</details>

### A2
```cpp
struct Strategy {
    std::function<void(const MdEvent&)> on_event;   // set once at startup
    std::vector<Order> pending;
    void handle(const MdEvent& e) {
        on_event(e);
        for (auto& o : compute_orders(e))            // returns std::vector<Order> by value
            pending.push_back(o);
        gateway_.send(pending);                       // send(const std::vector<Order>&)
        pending.clear();
    }
};
```
<details><summary>Answer</summary>

- `on_event` is a `std::function` — a type-erased indirect call per event,
  and if the callable it was assigned had a capture > SBO, a heap block it
  chases each call (14). It's set **once at startup** → this is the perfect
  case for a **plain function pointer** (or `function_ref`, or making
  `Strategy` a template on the handler type — 23). One well-predicted
  indirect call, no heap chase.
- `compute_orders(e)` returns `std::vector<Order>` **by value** — a heap
  allocation per event (even with RVO, the vector's buffer is allocated;
  and if it's built with `push_back` internally, it grows). **Fix**: have
  `compute_orders` write into a caller-provided buffer / a pre-sized member
  `pending` directly (`compute_orders(e, pending)`), or return a `std::span`
  into a reused member buffer.
- `pending.push_back(o)` in a loop — `pending` was `clear()`'d (keeps
  capacity) so after warm-up it won't realloc *if* it was `reserve`d to the
  max orders-per-event once at startup. Add that `reserve`. Also `push_back(o)`
  copies `o`; `push_back(std::move(o))` or `emplace_back` if `Order` isn't
  trivially copyable.
- `gateway_.send(pending)` taking `const std::vector<Order>&` — fine (no
  copy), but consider `std::span<const Order>` so the gateway isn't coupled
  to `std::vector`, and so a fixed array works too.
End state: `on_event` → fn pointer; `compute_orders` writes into a
pre-`reserve`d `pending`; `send(std::span)`. Zero allocation per event.
</details>

### A3
```cpp
// two threads: feed handler produces, strategy consumes
struct Shared {
    std::atomic<uint64_t> seq{0};
    Book book;
    uint64_t last_update_ns{0};
};
// feed:     s.book.apply(msg); s.last_update_ns = now(); s.seq.fetch_add(1, relaxed);
// strategy: uint64_t v = s.seq.load(relaxed); read s.book; use s.last_update_ns;
```
<details><summary>Answer</summary>

- **`relaxed` on `seq`** — the feed's `book.apply()` and `last_update_ns`
  writes have **no happens-before** relationship with the strategy's reads.
  On x86 you might get lucky (TSO); on ARM the strategy reads a torn book.
  Even on x86 the *compiler* can reorder. This is a **seqlock** pattern
  done wrong (28/05): the writer must `seq.fetch_add(1, release)` **after**
  writing the data (and ideally bump `seq` to odd *before*, even *after*),
  and the reader must `acquire`-load `seq`, read the data, `acquire`-load
  `seq` again, and retry if it changed or is odd. Then the data reads are
  properly ordered.
- **`seq`, `book`, and `last_update_ns` share cache lines** — the feed
  writes `seq` (and `last_update_ns`) every message, invalidating the line
  in the strategy's cache; the strategy is reading `book` fields on the same
  or adjacent lines → false sharing / coherence traffic (11). `alignas(64)`
  the `seq` (and pad), keep `book` on its own lines, and note `last_update_ns`
  is written by the feed so it belongs near `seq`, not in `book`.
- **`now()`** — if that's `clock_gettime(CLOCK_MONOTONIC_RAW)` it's a
  syscall per message (17). Use vDSO `CLOCK_MONOTONIC` or `rdtsc`.
Fix: a proper seqlock (writer: `seq` odd → write book + ts → `seq` even,
with release; reader: acquire-load seq, read, acquire-load seq, retry),
`alignas(64)` on the seq/metadata block separate from `book`, `rdtsc` for
the timestamp.
</details>

---

## Part B — Which technique + trade-off

### B1
Ek stage 500k messages/sec handle karta, har message ~10 chhote temporary
objects banata parse ke dauran, aur unhe message ke end pe drop kar deta.
p99.9 = 1.2 µs (budget 800 ns), `perf` dikhata ~30% time `operator new`/
`free` mein. Technique? Trade-off? Kab yeh galat hota?

<details><summary>Answer</summary>

**Technique: a per-message arena** (lesson 08). All 10 temporaries allocated
from a fixed per-thread arena buffer; `arena.reset()` (one store) at the end
of the message frees them all. `04_arena_allocator.cpp` measured this shape:
new/delete per msg p50 ~500 ns → arena+reset ~30 ns, and the p99.9 flattens
(no allocator internals).
**Trade-offs**: (1) the temporaries can't be individually freed — fine here,
they all die at message end; (2) they must be **trivially destructible** (or
you track dtors) — check the parse temp types; (3) the arena must be sized
for the **worst-case single message** — measure from historical data, and
have an overflow policy (a bigger secondary arena chosen from the message
header, or reject) — never `malloc` as a fallback; (4) a temp pointer that
escapes the message scope = UAF — audit that nothing stores one.
**When this is wrong**: if the "temporaries" actually need to outlive the
message (e.g. an order object created during parse that lives until the
exchange acks) — those go in an **object pool** (07), not the arena. Or if
the stage isn't actually over budget (it is here — 1.2 vs 0.8 µs) don't
bother. Or if the 30% is really *one* pathological allocation (a giant
`std::vector` grow) — fix that one, an arena is overkill.
</details>

### B2
Ek dispatch hot loop `std::vector<std::unique_ptr<Handler>>` pe iterate karke
har element ka `virtual handle()` call karta. 4 handler types, protocol se
fixed. `perf` dikhata indirect-call mispredict + the handlers aren't inlined.
Technique? Trade-off? Kab `virtual` rakhna sahi?

<details><summary>Answer</summary>

**Technique**: the set is closed (4 protocol types) → `std::variant<H1, H2,
H3, H4>` + `std::visit`, or a tag `enum` + `switch` (lesson 13). Store
`std::vector<HandlerVar>` (no `unique_ptr`, no per-handler heap — pool/arena
the vector). `visit`/switch compiles to a 4-way jump table with **inlined**
bodies. `07_dispatch_comparison.cpp`: heterogeneous virtual ~7 ns vs
variant/switch ~4.5 ns, and the common cases inline + optimize. Or, if you
process in batches, **bucket by type** first → homogeneous per-type loops
that predict perfectly and vectorize.
**Trade-offs**: closed set (adding a 5th type edits the `variant` decl +
every `visit` — the compiler enforces exhaustiveness, which is a feature);
`sizeof(variant)` = largest alternative + tag (keep them similar in size, or
`variant<small...>` + pointers to big state); `visit`'s machinery is a bit
of code.
**When `virtual` is right**: the handler set is **genuinely open** — user
plugins, strategies loaded at runtime from config/`.so`, a framework where
third parties add handlers. Then `virtual` (or a function-pointer table) is
the only option; keep the interface small, and bucket-by-type if you process
in loops to tame the mispredict.
</details>

### B3
Ek engineer chahta hai busy-poll kare the market data socket "for lowest
latency." Box 8 cores, aur us pe strategy + risk + 2 backtest workers + the
OS + monitoring bhi chalte. Advice?

<details><summary>Answer</summary>

Busy-polling `recv(MSG_DONTWAIT)` in a spin loop takes a core to **100% CPU
forever** (17). That's only acceptable on an **isolated, dedicated** core
that nothing else needs. On this box:
- 8 cores, and the workload is: OS + monitoring (need ~1-2 cores), strategy
  (1), risk (1), 2 backtest workers (2), + the poll thread (1). That's
  ~7-8 cores already, with nothing isolated.
- If the poll thread busy-spins on a core the OS / a backtest worker also
  uses → it starves them (or gets preempted → wake jitter, defeating the
  point), and it burns power/heat for a core that's contended anyway.
Advice: (1) **First, is busy-poll even needed?** If the budget is met with
`epoll`/blocking `recv` on an isolated core, don't busy-poll. (2) If it is
needed, the box must be **re-provisioned**: dedicate + `isolcpus`/`nohz_full`/
`rcu_nocbs` a core for the poll thread, another for strategy, another for
risk (lesson 19); move the **backtest workers to a different box** (they're
throughput jobs, latency-insensitive — folder 24 case 4) or to the non-
isolated cores at low priority; monitoring on the OS cores. (3) A middle
ground on a shared box: `SO_BUSY_POLL` / `net.core.busy_poll` (kernel spins
briefly then sleeps) — some of the latency benefit without a permanently
pegged core. (4) Or `io_uring` with `SQPOLL` — a kernel poll thread you can
pin/share. But genuine hot-path busy-poll requires the hardware budget for
dedicated cores; you can't have it *and* run backtests on the same 8 cores.
</details>

---

## Part C — Profiler output diagnosis

### C1
`perf stat -M TopdownL1` on the strategy hot path: `Retiring 20%`, `Frontend
Bound 15%`, `Backend Bound 58%`, `Bad Spec 7%`. `perf stat -d`: `L1-dcache-
load-misses 12%`, `LLC-load-misses` high, `dTLB-load-misses` high. Diagnose
+ first 3 fixes.

<details><summary>Answer</summary>

**Backend Bound 58%**, and the detail says it's **memory** (high LLC-load-
misses = DRAM trips, high dTLB-load-misses = page walks). The strategy is
**memory-latency-bound** — it's chasing data that doesn't fit cache and
isn't TLB-covered. Not compute (`Retiring` only 20%), not frontend, not
branches. Fixes (folder 10 / 32):
(1) **Shrink / re-layout the working set.** Hot/cold field split the big
structs (`Book`, `Order`) so the hot path touches dense arrays of just the
needed fields (10). AoS→SoA for anything you scan (10, 32/05). This directly
cuts both L1 misses and TLB pressure (less memory touched).
(2) **Huge pages** for the big data structures (the book array, pools) —
`dTLB-load-misses` high means 4 KiB pages aren't covering the working set;
2 MiB pages give 512× the reach (18, 32/11). `mlock` + pre-fault them.
(3) **Kill pointer-chasing** — if the strategy walks linked structures
(a `std::map`, a linked list of levels), replace with flat arrays + indices
(10, 32/08). Each avoided pointer hop is ~one avoided DRAM miss.
Then re-measure top-down: Backend/Memory Bound should drop, Retiring rise.
Prefetch (`__builtin_prefetch`) is a *last* resort — measured often neutral
or harmful (32/06).
</details>

### C2
`perf annotate` (`-e cycles:pp`) on the order-encode function: 3% here, 4%
there, then **48% on a single `call operator new`**. The function builds the
outgoing message with a `std::string`. Diagnose + fix.

<details><summary>Answer</summary>

48% of the encode function's time is in `operator new` — it's **allocating**
to build the message (a `std::string` / `std::stringstream` / a `std::vector<char>`
that grows). On the order path, per order. That's both the p50 cost and the
allocator-tail p99.9 spike (lesson 04). Fix:
(1) **Encode into a fixed, pre-allocated buffer.** A `char buf[MAX_MSG]`
(thread-local or a member), and write fields with `std::to_chars` (numbers,
no alloc, no locale, no throw — 22) / `std::memcpy` (fixed bytes). Return a
`std::span<const std::byte>` / `{ptr, len}` into that buffer.
(2) If it's a **binary** protocol: overlay the wire struct on the buffer and
set fields directly (22) — no string at all.
(3) The buffer is reused per order (`len = 0` at the start) — zero
allocation on the hot path. Size `MAX_MSG` for the largest possible message.
After: `perf annotate` should show the time spread across the actual field
writes / byte-swaps, and the `operator new` gone. Re-measure the stage's
p99.9 — the allocator tail should vanish.
</details>

### C3
`perf c2c report`: one hot cache line, HITM high, two offsets — offset 0
written by the feed thread, offset 8 written by the strategy thread. The
struct is `struct Stats { uint64_t msgs_in; uint64_t orders_out; };` and
there's one global `Stats g_stats;`. Diagnose + fix.

<details><summary>Answer</summary>

**False sharing** (lesson 11). `g_stats` is 16 bytes → `msgs_in` (offset 0)
and `orders_out` (offset 8) are on the **same 64-byte cache line**. The feed
thread does `g_stats.msgs_in++` every message; the strategy thread does
`g_stats.orders_out++` every order. Each write **invalidates the other
thread's copy of the line** → the line ping-pongs between the two cores'
caches (HITM = hit-modified-in-another-core), each increment ~100+ cycles
instead of ~1, and it's a jitter source (32/04: 6–44× run-to-run).
Fixes:
(1) **Separate the counters onto different lines**: `struct alignas(64)
Counter { uint64_t v; char pad[56]; }; Counter g_msgs_in, g_orders_out;` —
each on its own line, zero cross-invalidation.
(2) Better: **per-thread counters, summed on read** — `thread_local uint64_t
t_msgs_in;` incremented with zero coherence traffic; a stats reader sums the
registered thread-locals periodically (26/03: local+combine is far faster
than any shared counter).
(3) The counters are for stats, not correctness — they don't even need to be
atomic if each is written by exactly one thread and the reader tolerates a
slightly stale sum (a `relaxed` load is enough, or a plain read with a
seqlock if you need consistency across the pair).
Verify: `perf c2c` after → the `g_stats` line no longer shows HITM; the
feed/strategy IPC recovers; throughput variance drops.
</details>

---

## Part D — Hands-on

### D1 — The allocation tail
`./build.ps1 fast 36-LOW-LATENCY-CPP/examples/01_allocation_cost.cpp`.
Record p50/p99/p99.9 for the 3 workloads. Which has the worst p99.9 and why?
Then `02_memory_pool.cpp` — how does `FixedPool`'s p99.9 compare to `new`'s
for the same churn pattern? What is `FixedPool`'s cost in instructions
(read the header / think about `allocate`)?

### D2 — Construct vs recycle
`./build.ps1 fast .../03_object_pool.cpp`. Compare `pool + construct` vs
`pool + recycle + reset` p50. The difference ≈ the cost of `Order`'s
constructor (a 16-int fill). When is that difference worth the "recycle
means stale fields" risk?

### D3 — Arena
`./build.ps1 fast .../04_arena_allocator.cpp`. new/delete-per-msg vs
arena+reset — the ratio. Then `05_pmr_containers.cpp` — `pmr::vector` on a
stack buffer vs `std::vector`: the p50 ratio, and the "0 global new" line.
Trigger the `null_memory_resource` guard yourself by shrinking the buffer.

### D4 — Branchless reality
`./build.ps1 fast .../06_branchless.cpp`. (a) Test 1 vs Test 2 (random vs
sorted) — did the "branchy" version get faster on sorted? What does that
tell you (`./build.ps1 asm .../06_branchless.cpp` — `cmovg` or `jg`?).
(b) Test 3 switch vs table — the ratio, and why the switch is slow on random
data.

### D5 — Dispatch
`./build.ps1 fast .../07_dispatch_comparison.cpp`. Fill in the table:
virtual / variant / fn-table / switch / CRTP, for HOMOGENEOUS vs
HETEROGENEOUS. Which mechanisms degrade most from homo→hetero, and why?
Which is flat?

### D6 — `std::function`
`./build.ps1 fast .../08_std_function_cost.cpp`. templated vs fn-ptr vs
function_ref vs `std::function` — the numbers. Why are the first three
~equal here (what is the loop bound by)? What does `[ctor heap bytes: 64]`
prove for the fat-capture case?

### D7 — Ring buffer
`./build.ps1 fast .../09_ring_buffer.cpp`. push/pop p50. Read the class —
find: the power-of-two `& mask`, the `alignas(CL)` on `head_`/`tail_`/`buf_`,
and the `cached_tail_`/`cached_head_` logic. What does the cached index save
per op?

### D8 — Batching curve
`./build.ps1 fast .../10_batching.cpp`. Plot ns/item and head-of-line ns vs
B. Where's the amortization knee? At what B does head-of-line latency exceed
a 1 µs budget? What B would you pick for a throughput-bound logging stage vs
the order-send path?

### D9 — Page faults
`./build.ps1 fast .../11_page_fault_warmup.cpp`. COLD vs WARM mean/p99. The
ratio. What does this imply you must do at startup for every buffer the hot
path will touch (lesson 18)?

### D10 — Hot/cold split (Rule 2)
`./build.ps1 fast .../12_hot_cold_split.cpp`. A/B/C — the numbers.
They're ~equal — why (lesson 21)? `./build.ps1 asm .../12_hot_cold_split.cpp`
— can you see `cold_outlined` placed away from the hot loop? When *would*
the split show a measurable win?

---

## Challenge

Open-ended — answer key nahi. Har technique ke baad **pehle vs baad** ka p50 **aur** p99.9
likho (sirf mean nahi). Koi technique kaam na aaye to woh bhi likho — file 24 ka point yahi hai.

### Challenge 1 — zero-allocation hot path ka audit
Global `operator new` / `operator delete` ko counters ke saath replace karo (folder 14
file 05 wali "counted `operator new`" technique; kyun zaroori — file 04). Ek mini pipeline
chalao — parse → book update → signal — 1M messages. Target: **warmup ke baad 0
allocations**. Phir jaan-boojh kar ek preallocation hatao (jaise ek `std::vector` ka
`reserve`) aur dikhao: allocation count kitna bada, aur p50 / p99.9 pe kya asar pada. Asar
p50 pe dikha, tail pe, dono pe, ya kahin nahi — jo naapa wahi likho.

### Challenge 2 — pool vs arena vs PMR vs `new`: lifetime ke hisaab se faisla
Ek hi workload, teen lifetime patterns: (a) sab objects chhote-jeevan wale, (b) sab lambe,
(c) mix. Char allocators pe chalao: `new`/`delete`, `examples/02_memory_pool.cpp` ka
`FixedPool`, `examples/04_arena_allocator.cpp` ka `Arena`, aur
`std::pmr::unsynchronized_pool_resource` / `monotonic_buffer_resource` (`examples/05`). Har
combination ka p50 / p99.9 per op aur memory high-water mark. Aakhir mein ek decision table:
"yeh lifetime pattern → yeh allocator, kyunki ...".

### Challenge 3 — jitter ka shikaar
Ek tight loop 60 seconds chalao jo har iteration ka latency record kare (folder 35 ka
histogram). Tail ke spikes ke source dhoondho: page faults (file 18), context switches,
timer interrupts, frequency scaling (file 03). Phir mitigations **ek-ek karke** lagao:
prefault/warmup (`examples/11_page_fault_warmup.cpp`), CPU pinning (file 19), cache warming
(file 20). Har step ke baad p99 / p99.9 / p99.99 / max. Jo mitigation is OS/machine pe kaam
nahi aaya, uska kaaran likho.

---

## Interview questions (folder-wide)

1. Latency / throughput / jitter — three axes; HFT's primary metric; budget thinking.
2. Why hot-path allocation is a disaster (measured tail); the "hidden" allocations.
3. Pre-allocation discipline — warm-up vs steady state; proving zero-alloc.
4. Fixed pool / object pool / arena — design, cost, and each one's core trade-off.
5. PMR vs classic `Allocator<T>`; `null_memory_resource` as a tripwire.
6. Branchless — when it wins, when it *loses*; `-O2` if-conversion.
7. The 5 dispatch mechanisms; homo vs hetero data; when `virtual` is still right.
8. `std::function` costs; `function_ref`; the SBO cliff.
9. SPSC ring — power-of-2, monotonic counters, release/acquire, cached opposite index.
10. Batching — the throughput/head-of-line trade-off; opportunistic batching.
11. Syscall avoidance — busy-poll (and its cost), `io_uring`+`SQPOLL`, kernel bypass.
12. Page-fault avoidance — `MAP_POPULATE`/`mlockall`/write-touch/stack pre-fault; huge pages & THP jitter.
13. CPU pinning — `isolcpus`/`nohz_full`/`rcu_nocbs`, SMT, NUMA, `SCHED_FIFO` hazard.
14. Cache warming — cold start vs decay; dry-run + its `dry_run`-flag requirement.
15. I-cache — Frontend Bound; hot/cold split; PGO/LTO/BOLT; why `always_inline` everything hurts.
16. Zero-copy — views/spans/overlays; the 4 overlay caveats; `from_chars`/`to_chars`.
17. Compile-time dispatch — `if constexpr`, non-type params, startup fn-pointer pick; instantiation bloat.
18. The honest process — measure → profile → one change → re-measure → explain; six "when not to".

---

## Next
→ [`../37-HFT-FUNDAMENTALS/00-README.md`](../37-HFT-FUNDAMENTALS/00-README.md)
