# 13 — Interview revision sheets

One compressed sheet per topic — the night-before pass. Full Q&A banks:
folder `46-INTERVIEW-PREP` (file numbers in parens). Practice problems:
folder `47-CODING-PROBLEMS`.

---

## Sheet 1 — C++ core (46/02)

- **Declaration vs definition**; ODR; `inline` = "may appear in multiple TUs".
- Init forms: `{}` (list, rejects narrowing — prefer), `()`, `=`. `int x;` at
  block scope = **uninitialized** (UB to read); static/global = zero-init.
- `int` "practically 4 B" — standard guarantees only ≥16 bits; use `int32_t` when exact.
- **UB vs implementation-defined vs unspecified** — know an example of each.
- `const` binds left (right-to-left read). `constexpr` = compile-time-usable;
  `consteval` = must. `constinit` = static init, not const.
- Order of evaluation of function args is **unspecified**; `i = i++` etc. avoid.
- `static` = internal linkage (file) / lifetime (local) / no-`this` (member).
- RAII is the answer to almost every "how do you avoid leaking X".

## Sheet 2 — Pointers, memory, RAII (46/03)

- **Pointer vs reference**: reference can't be null/rebound, no arithmetic, must init.
- Stack (fast, auto, LIFO, small) vs heap (`new`/`malloc`, manual, large, slow).
- `unique_ptr` (move-only, zero overhead) / `shared_ptr` (atomic refcount +
  control block) / `weak_ptr` (breaks cycles). **`shared_ptr` count is
  thread-safe; the pointee is not.** Don't pass `shared_ptr` by value on a hot path.
- `make_shared` = 1 alloc (obj+cb) but weak refs keep the whole block alive.
- Dangling sources: return ref/ptr to local; `string_view`/iterator outliving
  its container; capturing `[&]` in a stored lambda.
- Custom allocators: arena/bump (O(1), reset-all), free-list pool (O(1)
  acquire/release), slab (size classes). No `malloc` on the hot path.
- `alignas`, placement `new` + explicit `~T()`, `mlockall` + prefault.

## Sheet 3 — OOP & the object model (46/04, folder 25)

- **Virtual dtor** whenever you `delete` through a base pointer (else UB / leak).
- vtable: per-class array of fn pointers; each polymorphic object has a vptr.
  Virtual call = load vptr → load slot → indirect call (mispredict-prone, blocks inline).
- Calling a virtual **in a ctor/dtor** resolves to the current class's version.
- **Slicing**: `Base b = derived;` copies only the base subobject.
- `override` / `final` always. `final` can let the compiler devirtualize.
- Empty class `sizeof == 1`; EBO removes it as a base. `[[no_unique_address]]`.
- Rule of 0 (prefer) / 3 / 5. `= default` / `= delete`.
- Prefer composition; CRTP or templates for zero-overhead polymorphism.

## Sheet 4 — Move semantics (46/07, folder 18)

- **Value categories**: lvalue (has identity), prvalue (pure value), xvalue
  (expiring). `T&&` binds rvalues.
- `std::move` = a **cast** to `T&&`; it moves nothing itself.
- Moved-from object = valid but unspecified state (safe to destroy/assign).
- **Move ctor/assign must be `noexcept`** or `vector` reallocation copies instead.
- `T&&` in a *deduced* context = **forwarding reference** → `std::forward<T>`.
- `return std::move(local)` **pessimizes** — kills NRVO. Just `return local;`.
- Rule of 5: if you write one of {dtor, copy×2, move×2}, consider all.
- Copy elision (guaranteed for prvalue returns since C++17).

## Sheet 5 — Templates (46/06, folder 21)

- Instantiated on use; errors at instantiation; two-phase name lookup.
- `typename T::X` / `obj.template f<Y>()` disambiguation.
- Forwarding ref + reference collapsing (`& & → &`, `& && → &`, `&& && → &&`).
- `if constexpr` — compile-time branch, dead branch not instantiated.
- SFINAE → **concepts** (C++20): `requires`, better errors.
- **CRTP**: `struct D : Base<D>` — static polymorphism, no vtable; different
  base type per D (no common container).
- Cost: code bloat, compile time, error walls. `extern template` to dedupe.

## Sheet 6 — STL (46/05, folder 19)

- Container choice on 3 axes: **complexity / mutation pattern / cache**.
- `vector` default. `reserve` before a known number of `push_back`s.
- Iterator invalidation (memorize): `vector` realloc → all; `deque` push ends →
  iters invalid, refs valid; `list`/`map` → only the erased node;
  `unordered_map` rehash → iters invalid, refs valid.
- `emplace` helps only when it saves a temporary + move.
- Comparator = **strict weak ordering** (`<`, not `<=`) or `std::sort` is UB.
- `vector<bool>` is a proxy bitset, not a container of `bool`.
- Don't full-`sort` for less: `nth_element` (k-th, O(n)), `partial_sort` (top-k).
- `std::string_view` — non-owning, not null-terminated.

## Sheet 7 — Concurrency (46/08, folder 26)

- Race vs **data race** (unsynchronized, ≥1 write → UB).
- `lock_guard` (simple) / `unique_lock` (defer/timed/CV) / `scoped_lock` (2+ locks, deadlock-free).
- `cv.wait(lk, pred)` — predicate form handles spurious + lost + stolen wakeups.
- Deadlock = Coffman's 4; break circular wait with a **lock order** or `scoped_lock`.
- gdb deadlock signature: ≥2 threads in `__lll_lock_wait`.
- Prefer **shared-nothing** — one thread owns the data, hand off via a queue.
- `jthread` auto-joins + `stop_token`. `std::async` future dtor **blocks**.
- False sharing: two hot vars, one line → `alignas(64)`.

## Sheet 8 — Atomics / lock-free (46/09, folders 27–28)

- `atomic` ≠ `volatile` (atomicity + ordering vs neither).
- Six orders: `relaxed` (counters) / `acquire` (reader) / `release` (writer) /
  `acq_rel` (RMW) / `seq_cst` (default, global order) / `consume` (don't).
- **Release store → acquire load** handoff = the canonical pattern; plain store is a bug.
- `cmpxchg_weak` in loops (spurious fail OK), `_strong` for one-shots.
- **ABA**: fix with tagged pointer / hazard pointers / RCU / epochs.
- SPSC ring = **no CAS** (one writer per index) → wait-free; `alignas(64)` head/tail.
- lock-free = *someone* progresses; wait-free = *everyone* in bounded steps.

## Sheet 9 — CPU & cache (46/12, folders 31–32) — *know the numbers*

- L1 ~1 ns · L2 ~4 ns · L3 ~10–20 ns · **DRAM ~60–100 ns** · mispredict ~3–5 ns
  · syscall ~0.1–0.3 µs · ctx switch ~1–5 µs · SSD ~10–100 µs · same-DC RTT ~10–100 µs.
- **Cache line = 64 B** — the unit. A miss ≈ 100 instructions.
- 3 C's: compulsory / capacity / conflict (+ coherence).
- Branch predictor ~95–99% on predictable; a 50/50 hot branch ≈ 4× slower →
  branchless or sort.
- Row-major vs column-major; **AoS vs SoA** (SoA wins scans/SIMD, AoS wins
  whole-record random access).
- Pointer chasing can't pipeline (each load depends on the last).
- False sharing / MESI / `perf c2c`.

## Sheet 10 — Optimization method (46/13, folder 43)

- **Measure → profile → one change → re-measure → explain.** Never guess.
- `-O2`, realistic data, stop DCE from deleting the result. `-O0` benchmark = worthless.
- **Correctness gate before the speedup gate** — prove old == new first.
- Latency vs throughput; batching helps throughput, adds head-of-line latency.
- Report **p99.9**, not the mean (tail-at-scale, coordinated omission).
- `perf stat` signal → problem class: low IPC / branch-miss / LLC-miss /
  page-faults / ctx-switches → the matching fix.
- A surprising/null result is a **lesson**, not something to hide (Rule 2).
- "When to stop": not on a profiled hot path & doesn't move p99.9 → leave it clear.

## Sheet 11 — Linux / systems (46/10, folder 29)

- Syscall cost (~100–300 ns); `mmap` uses; minor vs major page fault.
- `epoll` (O(ready)) vs `select` (O(fds)); edge vs level triggered.
- CFS is throughput-fair → bad for HFT tails; `SCHED_FIFO` is dangerous
  (spinning FIFO thread starves per-core kernel work).
- **Affinity ≠ isolation**: `sched_setaffinity` pins; you still need
  `isolcpus` + `nohz_full` + `rcu_nocbs` + IRQ affinity.
- NUMA first-touch; `mlockall(MCL_CURRENT|MCL_FUTURE)` + **prefault**.
- `clock_gettime` (vDSO, ~20 ns) vs `rdtsc` (`rdtscp`/`lfence` to serialize).
- Signal handlers: async-signal-safe functions only.

## Sheet 12 — Networking (46/11, folders 30, 42)

- Market data = UDP **multicast** (one-to-many, lossy → sequence numbers +
  A/B + snapshot recovery). Orders = TCP (reliable, ordered).
- `TCP_NODELAY` (disable Nagle) on order sockets; Nagle + delayed-ACK = ~40 ms stall.
- TCP head-of-line blocking; `SO_REUSEPORT` for multi-queue accept.
- Kernel-bypass ladder: kernel socket → `SO_BUSY_POLL` → Onload → `ef_vi` → DPDK.
- NIC: disable RX interrupt coalescing (or pure busy-poll); LRO/GRO off for latency.
- Hardware timestamping — TX and RX in the **same clock domain**.
- `recvmmsg` batches — off the critical path only.

## Sheet 13 — HFT architecture (46/14–15, folders 37–44)

- **Tick-to-trade pipeline**: NIC → feed decode → book build → strategy →
  pre-trade risk → order encode → NIC. Name each stage's data structure & failure mode.
- **One thread per instrument** (shared-nothing), shard across cores; hand off
  via SPSC rings.
- **Determinism → replay**: drive everything off event timestamps, no wall clock.
- Order book = **flat array indexed by tick** + cached BBO + bounded rewalk +
  dense `id → location` index. L2 vs L3 (MBO).
- Matching = price-time priority (FIFO per level); IOC cancels remainder, FOK
  needs an all-or-nothing **precheck before** matching (the classic bug).
- Pre-trade risk = **5 O(1) checks** ordered cheapest/most-likely-to-reject
  first: max qty, price band, message rate, position, notional.
- OMS = state machine (New→PendingNew→Working→Filled/Cancelled) + generation-
  checked handles. Kill switch (latched) ≠ rate limiter (throttle).

---

## The 6 behavioural stories to have ready (46/18)

hardest bug you fixed · an optimization with a **measured** result · a technical
disagreement · something you built end-to-end and are proud of · a time you
didn't know something · why HFT / low-latency. STAR structure. Numbers, not adjectives.

## The make-a-market drill (46/16)

Asked to "make a market in X": give a **two-sided quote** (bid/ask around your
estimate, width = your uncertainty). When they trade, **update** — both for the
information (why did they lift/hit?) and your inventory (skew to mean-revert it).

## Next
→ [`../49-PROJECTS/00-README.md`](../49-PROJECTS/00-README.md)
