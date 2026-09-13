# What I Still Need To Learn — Gap Tracker

Yeh file honest hai. Iska kaam yeh **nahi** hai ki achha dikhe. Iska kaam yeh hai ki
aapko exactly bataye ki **abhi kya baaki hai**.

Jab yeh file har section mein "NONE" bolegi, tab course complete hoga.

Last updated: **Batch 11 part 5 COMPLETE. PHASE 33: folder 49 (PROJECTS) — every content folder (00–49) is now built.** Connected, difficulty-ordered projects (spec's "concepts ko projects mein jodo"). 5 lesson files — `01` beginner (calculator/shunting-yard · number-guess with logic split from I/O + `<random>` done right · student manager · expense tracker with integer money · word stats · INI parser with line-numbered errors), `02` intermediate (inventory with append-only log + `replay()` · bank with **atomic transfer** + integer interest + a 20k-op invariant property test · CSV parser as a **4-state field machine** · log analyzer · KV store with **WAL + CRC + compaction + torn-tail recovery**), `03` advanced (custom `Vector<T>` with placement `new` + Rule of 5 + `move_if_noexcept` relocate + **strong exception guarantee** · 3 allocators arena/pool/segregated **measured ~20–28× vs `::operator new` at `-O2`** · thread pool with `packaged_task`/`future` + exception propagation · concurrent queues: mutex bounded / **lock-free SPSC no-CAS** / **MPSC Vyukov** with in-order stress tests · **epoll echo server** Linux-only · JSON parser: recursive descent + `std::variant` tree + **`line:col` errors** + **depth limit** + round-trip), `04` a v1-first approach + testing (invariant/property/golden/determinism-replay) + **code-review checklist** doc, `05` a pointer to folder 44 mapping each advanced project onto its HFT counterpart. **17 verified reference implementations** (`examples/{beginner,intermediate,advanced}/`) — all 16 non-linux ones compile strict-clean and run + assert; `05_epoll_server.linux.cpp` is `*.linux.cpp` (skipped, hand-reviewed). Examples in subdirs → `build.ps1 folder` N/A; `checkall` covers them recursively. **No new C++-language gaps** — this folder *applies* folders 01–35 in connected programs. Mojibake: **0**. **Folders 00–49 all built. Only the final gap audit remains. Next: final gap audit.** Prev: **Batch 11 part 4 (PHASE 33: folder 48 CHEATSHEETS).** The **quick-reference layer** — 13 markdown sheets distilling folders `01`–`47` into scannable tables + terse bullets + code snippets, each cross-linking its deep source folder: `01` syntax · `02` STL containers (memory / complexity / per-container **iterator + reference invalidation** rules / "pick one") · `03` `<algorithm>`/`<numeric>`/ranges grouped by job + the erase–remove idiom · `04` complexity tables (growth rates, sorting matrix, structure ops, graph algos, problem patterns, the amortized-vs-worst-case p99.9 trap) · `05` compiler flags (the repo's strict warning set, `-O` levels, `-march`, sanitizers, codegen knobs with trade-offs, linking, the 3 builds you actually use) · `06` GDB (run/step, breakpoints + watchpoints + `commands`, inspect, threads, `-O2` debugging reality, TUI, `.gdbinit` starter) · `07` perf/valgrind/sanitizers (`perf stat` signal→fix table, `perf` for latency bugs, the sanitizer matrix + CI golden build, a no-dependency benchmark harness) · `08` **HFT production tuning checklist** (BIOS → kernel cmdline `isolcpus`/`nohz_full`/`rcu_nocbs` → IRQ affinity → `mlockall`+prefault+hugetlbfs → per-thread pinning → NIC/kernel-bypass → warm-up → continuous `perf` verification + a gotchas table) · `09` memory ordering (`atomic` vs `volatile`, the 6 orders, the release/acquire handoff, fences, `cmpxchg` weak/strong, ABA + fixes, lock-free vs wait-free, x86-TSO vs weak) · `10` **latency numbers** (the core ladder L1 ~1 ns … DRAM ~60–100 ns … same-DC RTT ~10–100 µs, bandwidth, sizes, derived rules of thumb — consistent with 46/12) · `11` UB catalog (memory/pointer, integer/arithmetic, language, STL, "worked on my machine" traps + how to catch each) · `12` HFT glossary (order book, order types, participants & economics, microstructure/strategy, infrastructure, protocols, regulation/venues) · `13` **13 one-page interview-revision sheets** (one per topic, night-before pass) + the 6 behavioural stories + the make-a-market drill. **No `.cpp`, no new C++-language gaps** — this folder *condenses* folders 01–47. Mojibake: **0**. **Wrap-up track: folders 45 + 46 + 47 + 48 done; only 49 (projects) + the final gap audit remain. Next: 49-PROJECTS.** Prev: **Batch 11 part 3 (PHASE 33: folder 47 CODING-PROBLEMS).** A graded **practice bank** — 10 themed problem files, **250 problems** (basics 30 · arrays/strings 40 · pointers/memory 25 · OOP/design 20 · STL 35 · templates 20 · concurrency 25 · lock-free 15 · optimize-this 20 · HFT 20). Each problem = statement + `Pattern:` hint + `<details>` (approach + complexity); "solve it yourself first". `11-solutions/` = README + 10 per-category writeups with full worked code for the problems that need it (arena/free-list/slab allocators, `unique_ptr`, `memmove`, Treiber stack + ABA + tagged-pointer fix, SPSC ring, seqlock, `flat_map`, custom hashes, the price-indexed order book, the latency histogram, …). **10 verified runnable examples** (`examples/*.cpp`, one per theme — `./build.ps1 folder 47-CODING-PROBLEMS` → **10/10 OK** strict, every binary runs + asserts): `01_basics_kata` (reverse-int overflow, `nCr`, `isqrt`, `powmod`) · `02_arrays_strings_kata` (product-except-self, subarray-sum-k, first-missing-positive, trap-rain-water) · `03_arena_and_pool` · `04_scope_guard_and_result` · `05_flat_map` · `06_crtp_and_detect` · `07_blocking_queue` (N-prod/M-cons checksum balances) · `08_seqlock` (2M writes, 3 readers, **torn reads = 0**, run at `-O2`) · `09_optimize_row_vs_col` · `10_order_book_ops` (add/cancel/match + `crossed()` invariant). File `09` ("optimize this") targets are grounded in folders 32/43's **measured** numbers; `09_optimize_row_vs_col.cpp` measures **~45–70× at `-O2`** on this box — a **Rule 2** moment: far bigger than folder 32's "~7×" pointer-chase figure, because cache-line waste + 4K-page TLB thrash + row-major auto-vectorization stack. The file teaches the measured number, not the textbook one. **No new C++-language gaps** — this folder *drills* folders 01–46. Mojibake: **0**. **Wrap-up track: folders 45 + 46 + 47 done; 48–49 + final gap audit remain. Next: 48-CHEATSHEETS.** Prev: **Batch 11 part 2 (PHASE 33: folder 46 INTERVIEW-PREP).** A **layered** question bank (per the spec's "build toward HFT interviews, don't jump"): `02`–`14` are Layer 1 (types/control/functions) through Layer 13 (feed handler / order book / risk / OMS / tick-to-trade architecture), each answer cross-referencing the source course folder — pointers/memory/RAII · classes/vtables/virtual-dtor · STL/complexity/cache · templates/SFINAE/concepts/CRTP · value categories/move/forwarding/Rule-of-5 · threads/mutex/cv/deadlock · atomics/memory-ordering/happens-before/lock-free · Linux syscalls/scheduling/mmap · TCP-UDP/multicast/epoll/kernel-bypass · CPU/branch-prediction/cache/false-sharing (with the latency numbers to memorize) · profiling/optimization/latency-vs-throughput. Plus `01` (how HFT interviews work — market-maker vs latency/prop vs quant vs exchange-infra firms, the pipeline, the 3 grading axes, how to talk out loud), `15` (system-design arc: clarify → constraints → interfaces → components → hot path → bottleneck → failure modes → trade-offs → measure; + 4 worked designs + a rubric), `16` (brainteasers/probability — EV, the make-a-market game = two-sided quote + update on the trade for information AND inventory, Monty Hall / 100 seats / HH-vs-HT wait 6 vs 4 / 25 horses / ants on a stick, Fermi estimation, mental-math drills, Kelly + volatility-drag), `17` (30+ C++ trick questions — unsequenced modification, signed-overflow UB, dangling `const&`/`string_view`, slicing, most-vexing-parse, `vector<bool>`, `reserve` vs `resize`, `printf` format mismatch, macro precedence, one-past-the-end — each with the right answer + why), `18` (behavioural — STAR + the 6 stories to prepare + discussing this course's projects credibly), `19` (4 full mock scripts with strong/weak answer tracks + rubrics), `20` (HFT resume & the 7 projects that matter — order book, matching engine, SPSC queue, feed handler, pipeline optimization, mini HFT engine, async logger). 20 lessons + 8 **verified** coding examples (`examples/*.cpp` → `./build.ps1 folder 46-INTERVIEW-PREP` → **8/8 OK**, strict: reverse list, LRU, lock-free SPSC ring, fixed-point price parse, object pool, L2 top-of-book, atoi + overflow clamp, O(1) moving average + division-free signal) + 4 design docs + 4 mock transcripts. **No new C++-language gaps** — this folder *tests* folders 01–45; it completes `HFT-COMPLETENESS-AUDIT.md` Section L (interview readiness). Mojibake sweep: **6 → 0**. **Wrap-up track: folders 45 + 46 done; 47–49 + final gap audit remain. Next: 47-CODING-PROBLEMS.** Prev: **Batch 11 part 1 (PHASE 33: folder 45 DEBUGGING).** The full debugging toolchain: mindset (hypothesis→test loop, reproduce-first, `git bisect`, root cause ≠ symptom — mirrors folder 43's optimization loop) · GDB basics + advanced (break/step/print/bt/frame/finish; watchpoints, conditional breakpoints, `commands`, `tbreak`, `catch throw`, `.gdbinit`/`-x` scripting — **real transcripts captured on this box**, MinGW GCC 15.1.0 + GDB 16.3, against `01_gdb_practice.cpp`) · crashes (SIGSEGV/SIGABRT/SIGFPE/SIGBUS, exit `128+sig`, core dumps via `ulimit -c`/`coredumpctl`/`bt full`, stack overflow, Windows WER) · debugging `-O2` builds (`<optimized out>`, garbage function-entry values — real transcript shows `accts`="length 364181", `factorial(5)` constant-folded away, inlined frames, `-Og`, frame-pointer, UB-only-at-`-O2` table) · sanitizers (ASan shadow memory/redzones/quarantine, UBSan, TSan happens-before, MSan; combine rules; CI golden build — MinGW has no libasan/libubsan/libtsan so Linux/WSL workflow, expected output in the example footers) · valgrind (memcheck definitely/indirectly/possibly-lost, helgrind, DRD, massif; vs-sanitizers table) · multithreaded (`thread apply all bt`, deadlock signature ≥2 in `__lll_lock_wait`, missing-unlock vs cycle, heisenbugs) · reverse debugging (`rr record`/`replay`, `watch` + `reverse-continue` = "who set this value" in one step, `--chaos`) · logging strategy (levels, structured logs, log-and-throw antipattern, **HFT async logging**: hot thread enqueues a POD ~20–40 ns to an SPSC ring, background thread formats+writes, queue-full → drop + count never block, binary log + mmap ring + pinned logger core) · `perf` for bugs (off-CPU analysis, syscall storms via `perf trace`, page-fault storms, false sharing via `perf c2c` — "correct but 100× slow") · a **40+-entry bug catalog** (Memory / Concurrency / Logic / Integer / Lifetime, each symptom → tool → fix, with a symptom→family quick index). 13 lessons + 5 standalone examples (`01_gdb_practice` bug-free · `02_segfault_debug` NULL-deref · `03_memory_bugs` 6-mode menu leak/uaf/oob/double/uninit/stackuaf · `04_race_debug` data race, `--safe`=atomic · `05_deadlock_debug` AB/BA, default safe / `--deadlock` hangs) + `06_buggy_programs/` (10 compile-clean/runtime-buggy "find & fix" drills with `// BUG/SYMPTOM/TOOL/FIX` footers). `./build.ps1 folder 45-DEBUGGING` → **5/5 OK** under strict warnings; buggy programs compile-clean in `checkall`. **Rule-2 note carried through:** `10_data_race.cpp` races ~15–20% of runs at `-O0` but **hides at `-O2`** (register promotion of the RMW) — taught, not hidden. **No new C++-language gaps** — this is applied tooling; every underlying bug family was covered under folders 09/12/13/14/17/18/22/25/26/27/28. Mojibake sweep: **34 → 0**. **Wrap-up track: folder 45 done; 46–49 + final gap audit remain. Next: 46-INTERVIEW-PREP.** Prev: Batch 10 part 9 (PHASE 32: folder 44 HFT-PROJECTS — THE CAPSTONE), the course climax where everything from folders 01–43 was assembled into one `MiniHftEngine` — MarketData → Parser → L2Book → Strategy → Risk → OMS → Venue → fills → PnL — single-threaded and fully deterministic (no wall clock; every timestamp from the event stream, so replay is byte-identical). 15 lessons + 12 example drivers + 11 shared `mh_*.hpp` headers, `./build.ps1 folder 44-HFT-PROJECTS` → **12/12 OK** under strict warnings. **Reuse over rewrite** (the capstone's whole point): `mh_types.hpp` `#include`s folder-40's `matching_engine.hpp` (the `Price`/`Qty`/`OrderId` types **and** the `MatchingEngine` itself, used as the venue); `07_spsc_queue.cpp` `#include`s folder-41's `spsc_queue.hpp`; book/pools/strategy follow folder 39/14/36/43 patterns. **The folder-43 optimization loop, applied end-to-end:** `MiniHftEngine` is `template <class Venue>` — `NaiveEngine` (std::map `MatchingEngine` venue) vs `OptimizedEngine` (`FastVenue`: flat-array aggregate book + per-level FIFO + IOC sweep). Profiling found the venue mirror was the `book` stage's bulk (~150 ns/msg); `FastVenue` halves it (~67 ns/msg) → ~1.5–1.6× end-to-end. `12_integration_tests.cpp` proves `NaiveEngine == OptimizedEngine` byte-for-byte (fills, qty, P&L, position) across 5 seed/config combos **before** the speedup is reported, both deterministic, invariants (|position| ≤ risk max, zero gaps, OMS settles) held. Per-component measured (Zen 2, ratios): L2Book apply ~17 ns/msg (BBO matches a `std::map` reference exactly); FixedPool ~2.0×/~5.0× (p50/p99.9) vs `new`/`delete`; ObjectPool stale-handle → nullptr even post-recycle; SPSC ~6 M msg/s in-order; risk 14/14 checks; feed parser v1≈v3 at `-O2` (honest Rule-2 null, cf. 43/08). **No alpha** — `SpreadCrossStrategy` is mechanical (37 SPECIALIZED list). Mojibake 16 → 0. **The HFT build track (folders 36–44) is COMPLETE. All C++/systems/low-latency prerequisites + the full HFT track (folders 00–44) done. Only wrap-up folders 45–49 + the final gap audit remain. Next: 45-DEBUGGING.** Prev: Batch 10 part 8 (PHASE 31: folder 43 HFT-OPTIMIZATION) — the optimization methodology folder (measure → profile → change one → re-measure → explain, correctness gate first).

---

## Abhi ki status (PHASE 0–34 complete — **COURSE STRUCTURALLY COMPLETE**. All 50 content folders (00–49) built; 381 `.cpp` under `./build.ps1 checkall` on GCC 16.2 (353 OK, 0 real fail, 1 broken-on-purpose, 27 Linux-only); the final gap audit (PHASE 34) is done — every section below reads NONE, every genuinely-specialist topic is in the explicit SPECIALIZED list.)

### MAJOR C++ LANGUAGE GAPS

**NONE.** Har core-language topic covered (evidence list neeche — sab
`~~struck~~ ✅`). Final gap audit (PHASE 34) mein `CPP-COMPLETENESS-AUDIT.md`
ke stale `[~]`/`[ ]` markers verify + fix kiye — Namespaces, `if constexpr`,
branch prediction, storage-duration/lifetime sab dedicated lessons mein confirm
hue.

**Covered:**

- Control flow: ~~`if`, `switch`, loops~~ ✅ folders 06 + 07 done
- Functions, overloading, call stack, recursion: ~~folder 08~~ ✅ done
  (declaration/definition/ODR, params, return/RVO, **call stack deep dive**,
  scope/lifetime, default args, overload resolution, recursion, `inline`,
  `constexpr`, attributes, `main(argc,argv)`)
- ~~Arrays, array decay, `std::array`, `std::span`~~ ✅ folder 09
- ~~Strings, `std::string`, `string_view`, SSO~~ ✅ folder 10
- ~~Structs, unions, enums, bitfields, padding/alignment~~ ✅ folder 11
- ~~Pointers (poora): `&`/`*`/`nullptr`, arithmetic, arrays↔pointers, `const`
  combos, `->`, `int**`, `void*`, function pointers, dangling/UAF, bug catalog~~
  ✅ folder 12
- ~~References: alias model, vs pointer, pass-by-ref, `const T&` + lifetime
  extension, returning refs, range-`for` copy/alias, reference members,
  `reference_wrapper`, reference bug catalog~~ ✅ folder 13.
  **`T&&`/move ka intro** 13 mein; ~~value categories ki poori taxonomy + move
  semantics ki depth~~ ✅ folder 18
- ~~Memory: layout revisit, stack/heap deep dive, `new`/`delete`, leaks,
  UAF/double-free, `static`/`thread_local`, allocation cost (measured tail),
  fragmentation, placement-new/arena/pool intro, memory tools~~ ✅ folder 14
- ~~Classes: encapsulation/invariants, members/`this`, access specifiers,
  constructors (default/param/delegating/`= default`/`= delete`), member init
  lists (+ order trap), destructors (+ Rule 3/5 preview), `const` methods +
  `mutable`, static members, operator overloading (`+=`/`+`/`<<`/`<=>`), `friend`
  (hidden-friend), `explicit`, nested/local classes, class layout (EBO,
  standard-layout, no-vptr)~~ ✅ folder 15
- ~~OOP: inheritance (access modes, layout, name hiding), ctor/dtor order (+
  virtual-call-in-ctor trap), virtual functions, **vtable/vptr deep dive**,
  `override`/`final`, abstract classes/interfaces, **virtual destructors**
  (leak demo), object slicing, multiple inheritance, virtual inheritance/diamond,
  RTTI/`dynamic_cast`, **virtual dispatch cost (measured ~10x)**, **CRTP**
  (zero-cost static poly), composition vs inheritance, SOLID (+ HFT
  reconciliation)~~ ✅ folder 16
- ~~RAII aur smart pointers~~ ✅ folder 17 (resource concept, RAII idiom,
  stack unwinding + 5 dtor-skip gaps, `unique`/`shared`/`weak_ptr`, control block
  + atomic refcount, `make_shared` 1 vs 2 allocs, custom deleters + EBO,
  ownership spectrum + sink idiom, RAII for fd/lock/FILE, Rule of Zero,
  smart-pointer perf — **`shared_ptr` copy ~93x raw measured**)
- ~~Copy/move semantics, Rule of 0/3/5, perfect forwarding~~ ✅ folder 18
  (copy ctor/assign + copy-and-swap, **value categories** full taxonomy +
  2-question model + `decltype((x))`, rvalue/forwarding refs + reference
  collapsing, move ctor/assign, `std::move` = cast + gotchas, Rule of Five/Zero,
  **copy elision RVO/NRVO**, **perfect forwarding** `std::forward` + factory,
  **`noexcept` move + `std::vector` growth — measured ~3x**)
- ~~Poora STL~~ ✅ folder 19 — 26 lessons (STL architecture, every container +
  complexity + **measured cache cost**, iterators + invalidation table,
  `<algorithm>` ×4 + erase-remove idiom, `<numeric>`, `<ranges>`, utility types
  (`optional`/`variant`/`any`/`expected`), `<functional>` + **`std::function`
  cost measured**, `<chrono>` timing discipline, `<random>`, `<filesystem>`,
  `<regex>` (why slow), `<type_traits>`, `<bit>`, allocators, **PMR**, container
  performance table, **STL in HFT** synthesis)
- ~~Algorithms & DSA (cache-aware)~~ ✅ folder 20 — 17 lessons (complexity + its
  limits, arrays/two-pointer/sliding-window/prefix-sums, sorting family + bench,
  **`std::sort` = introsort**, binary search, linked lists + intrusive arena,
  stacks/queues + ring buffer, **own open-addressing hash table**, trees/BST,
  heaps, tries, graphs BFS/DFS/Dijkstra/topo, greedy + how to verify it, DP,
  bitmask, string algos KMP/rolling-hash/Z, **cache-aware DSA** synthesis) +
  graded problem sets. **Measured: list vs vector iterate ~40x, flat vs pointer
  tree 4–11x, memo vs naive fib ~11,500x.**
- ~~Templates, SFINAE, concepts, metaprogramming~~ ✅ folder 21 — 16 lessons
  (function/class templates, deduction, NTTP + template-template params, full +
  partial specialization, variadic + fold expressions, `if constexpr`,
  `<type_traits>` internals + writing your own, **SFINAE**, **concepts (C++20)**
  + subsumption, **CRTP** + policy-based design, tag dispatch, TMP via
  `constexpr`, two-phase lookup / `typename`/`template`, instantiation model +
  code bloat + `extern template`, **templates-in-HFT** synthesis).
  **Measured: CRTP ~0.56 vs virtual (real boundary) ~2.43 ns/call; virtual ~2.51
  vs template ~1.14 vs `variant`+`visit` ~1.13 ns.**
- ~~Modern C++ (11 → 23)~~ ✅ folder 22 — 14 lessons (standard-by-standard audit
  C++11/14/17/20/23 with every feature catalogued) + deep dives: lambdas ·
  `constexpr`/`consteval`/`constinit` · ranges · **coroutines** (hand-rolled
  `Generator<T>`, frame/HALO) · **modules** (`export`/`import`, BMI, working
  multi-file demo) · `<=>` · attributes · `std::format` (custom formatters,
  `format_to_n`) · legacy→modern migration guide. **Measured: ranges pipeline
  ~1.5x FASTER than a conditional-accumulate hand loop (vectorization) — taught,
  not hidden.**
- ~~Exceptions aur error handling~~ ✅ folder 23 — the whole landscape (return
  codes / `errno` / exceptions / `expected` / `error_code` / assertions), stack
  unwinding, exception safety (basic/strong/nothrow + copy-and-swap), `noexcept`
  deep, custom hierarchies, **measured exception cost** (~4000× a return),
  **`-fno-exceptions`** rationale + toolkit, **`std::expected`** (monadic),
  `std::error_code` + custom categories, assertions / `static_assert` /
  `std::unreachable` / `[[assume]]`, **UB catalog** + sanitizers
- ~~Namespaces, linkage, ODR~~ ✅ folder 24 — translation units + the 4-phase
  model, preprocessor deep, include guards / IWYU / pImpl, **ODR deep** (loud vs
  silent IFNDR), linkage (internal/external/module, visibility), storage
  specifiers, name mangling / `extern "C"` / ABI, object files & ELF, static vs
  dynamic linking (PLT/GOT, `-fPIC`), every linker error, Make, CMake, build
  performance, binary tools, **LTO and PGO**
- ~~Object model, lifetime, UB (object-model depth), casts, RTTI~~ ✅ folder 25 —
  what an object *is*, object lifetime (storage vs lifetime, reuse, `std::launder`,
  implicit-lifetime types), static init order fiasco (reproduced), temporaries &
  lifetime extension (the 4 dangling cases), trivial/trivially-copyable/standard-
  layout/POD/`has_unique_object_representations`, object vs value representation +
  padding, alignment deep, placement new, **strict aliasing** (`bit_cast` vs
  `reinterpret_cast` — measured `-O2` divergence), type punning, the four casts +
  `dynamic_cast` internals, vtable layout exact (MI thunks, virtual inheritance),
  **ABI** (Itanium, break catalog, stable-API design), **the UB catalog** (50+),
  **how the compiler exploits UB**
- ~~Concurrency — threads, races, mutexes, deadlock~~ ✅ folder 26 — concurrency
  vs parallelism (+ Amdahl), process vs thread, `std::thread`/`jthread`, passing
  data (decay-copy, `std::ref`, dangling captures), **race conditions & data
  races** (data race = UB; measured ~70% lost updates), critical sections,
  `std::mutex` (uncontended CAS vs contended futex), lock guards, **deadlock**
  (Coffman 4 + fixes), `std::shared_mutex` (vs snapshot pattern), **condition
  variables** (predicate form, lost/spurious wakeup), futures/promises/`async`,
  **built a thread pool** (measured), **C++20 sync** (`jthread`/`stop_token`/
  `latch`/`barrier`/`semaphore`), `thread_local`, **false sharing** (measured
  ~10×), concurrency bug catalog + TSan
- ~~Atomics + the C++ memory model, CAS, memory ordering (6), happens-before,
  fences, x86 TSO vs ARM~~ ✅ folder 27 — the **formal data-race definition**
  (⇒ whole-program UB), atomicity (non-atomic RMW / torn reads / "aligned on x86
  is hw-atomic but still a data race"), `std::atomic<T>` + every op + x86
  instruction mapping, **CAS** (`weak`/`strong`, spurious failure, `expected`
  overwrite, CAS loops, retry storms), **why ordering exists** (compiler + CPU
  reorder, the store buffer), `relaxed` / **acquire-release** (synchronizes-with →
  happens-before) / **`seq_cst`** (single total order, the default, the store-side
  `mfence`), **happens-before / synchronizes-with / sequenced-before** (the formal
  vocabulary + the edge list), **fences** (`atomic_thread_fence` vs compiler-only
  `atomic_signal_fence`), **x86-TSO vs ARM/POWER** (store→load reorder,
  multi-copy atomicity, per-op cost on ARM), `std::atomic_ref`, lock-free /
  wait-free / obstruction-free definitions, **ABA** + tagged word, **litmus
  tests** (SB / MP / LB / IRIW). **Measured: store `relaxed`/`release` ~0.7 ns
  vs `seq_cst` ~13 ns (~18×) on x86; loads & RMW order-insensitive; store
  buffering observed — `release/acquire` does NOT stop it, `seq_cst` does.**
- ~~Lock-free data structures, ABA, hazard pointers / RCU / epochs, seqlock~~
  ✅ folder 28 — why lock-free (contended-mutex costs; priority inversion /
  convoying / deadlock structurally impossible), the **rules** (no lock/alloc/
  blocking on the shared path, pre-allocate, deliberate ordering, ABA-proof,
  helping), **cache-line padding & false sharing**, **SPSC ring buffer** (built +
  optimized + benchmarked — release/acquire index hand-off, no CAS, wait-free in
  practice, no ABA; naive ~52 → cached-index ~16–20 ns/msg), **SPSC
  optimizations** (mask / padding / **cached opposite index** / batching /
  zero-copy slot API), **Vyukov bounded MPMC**, **Michael-Scott queue** (dummy
  node, helping), **Treiber stack** (tagged head, no DWCAS), **the memory
  reclamation problem** (fixed pool / hazard pointers / epochs / RCU / QSBR — the
  unbounded-memory failure mode), **hazard pointers**, **epoch-based / RCU**,
  **seqlock** (1-writer/N-reader snapshot — measured ~80–100× faster reads than
  `std::shared_mutex`), **testing lock-free** (invariant stress → TSan → model
  checkers → forced interleaving), **when NOT to go lock-free**. **Measured &
  taught, not hidden (CLAUDE.md Rule 2): (1) cache-line padding of `head_`/`tail_`
  *alone* ≈ noise on this box — the cached opposite index is the real win;
  (2) a correct, ABA-safe, contended lock-free Treiber stack is ~5× SLOWER than
  `mutex + std::vector` — lock-free ≠ fast; shape decides.**

### MAJOR STANDARD LIBRARY GAPS

**HO CHUKA:** ~~Poora STL — containers, iterators, `<algorithm>`, `<numeric>`,
`<ranges>`, `<chrono>`, `<random>`, `<filesystem>`, `<regex>`, `<functional>`,
`<type_traits>`, `<bit>`, utility types (`optional`/`variant`/`any`/`expected`)~~
✅ folder 19. ~~Smart pointers (`<memory>`)~~ ✅ folder 17. ~~Allocators + PMR —
standard Allocator interface, `polymorphic_allocator`, `monotonic_buffer_resource`
/ `pool_resource` / `null_memory_resource`, stack-buffer zero-heap (measured)~~
✅ folder 19 (files 23–24, examples 09–10); HFT hot-path pool application → 36.

**HO CHUKA (Batch 7 part 2):** ~~`<type_traits>` internals + `<concepts>`~~
✅ folder 21 (writing your own traits, `void_t`/`declval` detection, the standard
concepts, `requires`-expressions, subsumption). ~~`<coroutine>` machinery~~
✅ folder 22 file 09 (hand-rolled `Generator<T>`, promise hooks, frame/HALO —
though C++23 `std::generator` and async/executors aren't covered). ~~`<format>`
deep~~ ✅ folder 22 file 13 (custom `std::formatter<T>`, `format_to_n`).

**HO CHUKA (Batch 8 part 1):** ~~`<exception>` / `<stdexcept>` / `<system_error>`~~
✅ folder 23 — `std::exception` hierarchy, `std::runtime_error`/`logic_error`,
`std::exception_ptr` / `current_exception` / `rethrow_exception`, `std::nested_exception`,
`std::terminate` / `set_terminate`, `std::uncaught_exceptions`; **`std::error_code`
/ `error_condition` / `error_category`** (custom categories, `errno` →
`error_code`, `std::system_error`), `<cassert>` / `static_assert` /
`std::unreachable` (`<utility>`). ~~`<expected>`~~ ✅ folder 23 file 10 + example —
full monadic API, `std::unexpected`, `expected<void,E>`; the example **compiles +
runs under both `-std=c++20`** (hand-rolled `Expected<T,E>`, identical API) **and
`-std=c++23`** (real `std::expected`).

**HO CHUKA (Batch 8 part 2):** ~~threading library — `<thread>` / `<jthread>` /
`<mutex>` / `<shared_mutex>` / `<condition_variable>` / `<future>` / `<latch>` /
`<barrier>` / `<semaphore>` / `<stop_token>`~~ ✅ folder 26 — `std::thread`/
`jthread` (join semantics, `native_handle`), the `mutex` family (recursive/timed/
shared, `call_once`), `lock_guard`/`unique_lock`/`scoped_lock`/`shared_lock`,
`condition_variable` + `condition_variable_any` (predicate pattern), `future`/
`promise`/`shared_future`/`async`/`packaged_task` (exception propagation),
`latch`/`barrier`/`counting_semaphore`/`binary_semaphore`, `stop_token`/
`stop_source`/`stop_callback`. A thread pool **built** from these.

**HO CHUKA (Batch 8 part 3):** ~~`<atomic>` + the C++ memory model~~ ✅ folder 27
— `std::atomic<T>` (trivially-copyable, non-copyable, brace-init,
`is_lock_free` / `is_always_lock_free`, `std::atomic_flag`), `load` / `store` /
`exchange` / `fetch_add` / `fetch_or` / … (x86 `mov` / `xchg` / `lock xadd` /
`lock cmpxchg`; returns OLD; no `fetch_max`), `compare_exchange_weak` / `strong`
(spurious failure, `expected` overwrite, success / failure orders), all 6
`std::memory_order` values + when-which, `std::atomic_thread_fence` /
`std::atomic_signal_fence`, `std::atomic_ref` (C++20), C++20 `wait` / `notify` /
`atomic_flag::test`. Used lock-free throughout folder 28.

**NONE.** `<cmath>` is used + explained in context throughout (03 float
pitfalls, 20 DSA, 47/49 katas); `<numbers>` (C++20 named constants) is in
`22-MODERN-CPP/04`; the C library headers (`<cstring>` 10/01, `<cstdlib>` 14/09,
`<cstdio>` **dedicated** 04/11–12) are taught, not just used. The C++23-only
library types that need `-std=c++23` (`std::generator` / `std::flat_map` /
`std::mdspan` / deducing `this`) are **discussed** in folder 22 (04, 05, 09) —
`std::expected` is compile-verified both ways; the rest are lesson-level only
because the repo default is `-std=c++20` (a deliberate toolchain choice) →
listed explicitly in **SPECIALIZED / DOMAIN-SPECIFIC**, not an open gap.

### MAJOR MODERN C++ GAPS

**HO CHUKA:** ~~C++11 se C++23 tak — systematic standard-by-standard audit~~
✅ folder 22 (14 lessons): C++11 (move, `auto`, lambdas, `nullptr`, smart
pointers, `<atomic>`) · C++14 (generic lambdas, `make_unique`, relaxed
`constexpr`) · C++17 (`optional`/`variant`/`string_view`, structured bindings,
`if constexpr`, guaranteed copy elision) · C++20 (concepts, ranges, coroutines,
modules, `<=>`, designated init, `<bit>`, `std::format`) · C++23 (`std::expected`,
`std::print`, `std::generator`, deducing `this`, `flat_map`) · deep dives on
lambdas / `constexpr` family / ranges / coroutines / modules / `<=>` / attributes
/ `std::format` / a legacy→modern migration guide.

**NONE.** Every standard's features are catalogued standard-by-standard in
folder 22 (C++11 → C++23), each with a deep dive, and every earlier folder uses
the modern version of its own topic. **`std::expected` IS compile-verified**
under both `-std=c++20` (hand-rolled fallback) and `-std=c++23` (real) in
`23-ERROR-HANDLING/examples/05_expected.cpp`. The remaining C++23-only library
types (`std::generator` / `std::flat_map` / `std::mdspan` / deducing `this`) are
discussed but not compile-verified — a deliberate toolchain scope boundary (repo
default `-std=c++20`), listed in **SPECIALIZED / DOMAIN-SPECIFIC**.

### MAJOR SYSTEMS C++ GAPS

**HO CHUKA (Batch 9 part 1):** ~~Linux internals (29)~~ ✅ folder 29 — kernel vs
userspace / ring 0-3, **syscall cost** (~300 ns trap vs ~20 ns vDSO `clock_gettime`),
`fork`/COW/`exec`/`wait`/`clone`, **async-signal-safety** + `signalfd`, fd table
(3 levels) + short read/write + `O_CLOEXEC`, `/proc` & `/sys` tuning, `mmap`
(lazy first-touch fault) + POSIX shared memory (folder-28's SPSC ring in
`/dev/shm`), pipes/FIFO/UDS + `SCM_RIGHTS`, CFS/EEVDF vs `SCHED_FIFO` ("isolated
core + CFS busy-poll often wins"), `sched_setaffinity` + `isolcpus`/`nohz_full`/
`rcu_nocbs` isolation + SMT siblings, huge pages (THP vs hugetlbfs, `khugepaged`
stall), **`mlockall` + pre-fault + dry-run = zero page faults in steady state**,
NUMA first-touch + `mbind` + `numa_balancing=0`, IRQ/softirq affinity +
`irqbalance` off + coalescing, `CLOCK_MONOTONIC`/`rdtscp`/`clocksource=tsc` +
busy-spin for sub-µs "act at T", cgroups/rlimits (`RLIMIT_MEMLOCK`, `cpu.max`
throttle = spike, container-awareness), and the full **HFT production tuning
checklist** (BIOS → kernel cmdline → sysctl → runtime → systemd unit → app-side →
verify-by-effect). ~~networking (30)~~ ✅ folder 30 — OSI/TCP-IP + wire overhead,
IP/MTU/**fragmentation**/PMTUD/ARP, **TCP deep** (handshake / window / RTO
~200 ms vs 3-dup-ACK fast retransmit / SACK / cwnd / TIME_WAIT), **Nagle +
delayed-ACK ~40 ms deadlock** → `TCP_NODELAY` + one `writev`/msg, **UDP** (why
market data is UDP; loss/reorder handling; **A/B feed arbitration** by sequence;
kernel drops; `recvmmsg`), **multicast** (`IP_ADD_MEMBERSHIP` = kernel filter +
IGMP report; snooping failure modes; explicit `imr_ifindex`; SSM), sockets API +
**framing** (length prefix) + `getaddrinfo` at startup, blocking vs non-blocking
(`EAGAIN`, **busy-poll**, `SO_BUSY_POLL`, ET draining), `select`/`poll` O(N) +
`FD_SETSIZE` → **`epoll`** O(ready) (LT vs ET, event-loop skeleton, `EPOLLOUT`
discipline, `EPOLLONESHOT`/`EXCLUSIVE`, timerfd/signalfd/eventfd), socket options
(`SO_RCVBUF` cap + 2× readback, `SO_REUSEPORT`, keepalive + `TCP_USER_TIMEOUT`),
**zero-copy** (`sendfile` + `TCP_CORK`, `splice` for capture/replay,
`MSG_ZEROCOPY` + ERRQUEUE, `io_uring` SQ/CQ + SQPOLL = zero syscalls), **kernel
bypass** (DPDK / Onload / ef_vi / VMA / **AF_XDP** — ~1–5 µs kernel RX path →
~100–300 ns; what you take on; the adoption ladder), **hardware timestamping**
(`SO_TIMESTAMPING` HW/SW + cmsg parse + TX on the error queue; clock domains;
**PTP** + `ptp4l` + `phc2sys`; MiFID II), network tuning (NIC rings, coalescing,
**GRO/LRO off**, RSS/RPS, qdisc, sysctl), and **building servers** (the **V1
blocking echo → V2 epoll echo → V3 UDP-multicast feed handler** progression).
Folder 29: 18 lessons + 10 examples; folder 30: 16 lessons + 10 examples.
**Examples are all `*.linux.cpp`** — real, idiomatic Linux C++20 that does **not
compile on this MinGW/Windows box** (no WSL); `build.ps1` skips them as
`SKIP (linux-only)`. Benchmark numbers live in each example's `EXPECTED OUTPUT`
comment block as **typical Linux figures, explicitly "not measured on your
machine"** (CLAUDE.md Rule 2 honest treatment); the learner takes real numbers in
`NN-exercises.md` Part D on Linux/WSL.

**HO CHUKA (Batch 9 part 2):** ~~CPU architecture (31)~~ ✅ folder 31 —
fetch-decode-execute-retire + cycles/ns · registers (sub-register scheme,
`rflags`, `xmm`/`ymm`/`zmm`, architectural vs physical + **register renaming**,
spilling, `mxcsr` flush-to-zero) · ISA / µops / macro-fusion / **microcode**
(`div`, denormals, `gather`, transcendentals) / `-march=x86-64-v2/v3/v4` ·
**pipelining** (latency vs throughput, 4 hazard types, ~15–20 cyc flush/refill) ·
**superscalar** (execution ports, ILP, IPC, limiting-port analysis) ·
**out-of-order** (ROB / register renaming / reservation stations, speculation +
rollback, the OoO window, **why a dependent chain of cache misses — pointer
chasing — can't be hidden**) · **branch prediction** (gshare/TAGE, BTB/RAS,
indirect/virtual dispatch, `[[likely]]`, warm-up; **measured ~6–7×** mispredict
cost) · **speculative execution** (Spectre/Meltdown in one paragraph, KPTI /
retpoline cost, **`mitigations=off` trade-off** + preconditions + when it barely
helps a busy-poll loop) · **instruction latency/throughput** (Agner Fog /
uops.info / `llvm-mca`, computing the **critical path**, the division problem:
constant → compiler, invariant → hoist reciprocal, struct-time → `libdivide`) ·
**SIMD basics** (SSE2→AVX→AVX2+FMA→AVX-512, good/bad-for, **SoA prerequisite**,
alignment/tail-handling, AVX-512 downclock) · **SIMD intrinsics** (`_mm256_*`
naming, load/store/arith/cmp→mask/blend/movemask/shuffle/hreduce, `target("avx2")`
+ CPUID dispatch, auto-vec vs manual vs Highway/xsimd) · **hyperthreading**
(SMT shares L1/ports/ROB/predictor → jitter → **HFT disables it** or isolates
the sibling; size compute pools to *physical* cores) · **frequency & power**
(P-states/turbo, C-states + ~tens-µs wake, thermal throttle, AVX frequency
offset, uncore — **lock the frequency, kill deep C-states**) · **NUMA hardware**
(sockets + UPI/Infinity Fabric, **chiplet/CCX & Intel SNC = NUMA within a
socket**, the L1→L2→L3-local→L3-remote→local-DRAM→remote-DRAM latency ladder,
NIC's node) · **CPU differences** (Intel vs AMD per-CCX L3, `pdep` microcoded on
Zen 1/2, ARM64/Graviton re-audit, "benchmark on the deployment target,
frequency-locked"). 15 lessons + `16-exercises.md` + 8 **portable** `.cpp`
examples (SIMD + `rdtsc` + CPUID run on x86-64 incl. MinGW/Windows — 8/8 OK,
benchmarks measured at `-O2`). **CLAUDE.md Rule 2 in action:** examples `01`/`03`
first showed *no effect* (`-O2` DCE'd the ADD/DIV chains, if-converted the branch
to `cmovge`); fixed with `keep()` inline-asm barriers + `#pragma GCC
optimize("no-if-conversion")` + threaded `carry`, and the "compiler already made
it branchless" observation is taught explicitly. Measured (AMD Zen 2, ~2 GHz
throttled — **shapes/ratios, not production absolutes**): ILP serial→4-parallel
**~4×**; unpredictable branch **~6–7×** slower; branchless **~6–7×** on random
data (but ~1.2× *slower* on predictable data — the honest "when NOT branchless");
float sum scalar → SSE **4.1×** → AVX2 **~9.5×** (converges to ~1× when
bandwidth-bound); same-code auto-vectorized **~2.5–3×**; `rdtsc` self-cost
~20 cyc vs `lfence`/`rdtscp` serialized ~40–90 cyc.

**HO CHUKA (Batch 9 part 3):** ~~cache & memory performance (32)~~ ✅ folder 32 —
**memory hierarchy** (registers→L1→L2→L3→DRAM→disk, the "1 second = L1" latency
ladder, this box's geometry) · **cache lines** (64 B = unit of transfer /
coherence, why 64, `hardware_destructive_interference_size`, line straddle,
`alignas(64)`) · **cache organization** (direct-mapped → N-way, index/tag/offset,
pseudo-LRU/RRIP, **critical stride** = size/assoc, power-of-two dims poison,
VIPT) · **the 3 C's** (compulsory / capacity / conflict + coherence, a
symptom→cause table, **MLP** — which misses OoO hides, which it can't) ·
**locality** (spatial/temporal, loop interchange / fusion / fission / tiling,
prefetcher-friendly vs hostile patterns) · **prefetching** (HW prefetchers +
limits; `__builtin_prefetch` rw/locality/distance; `PREFETCHNTA`) · **false
sharing deep** (MESI ping-pong, `perf c2c`, padding / per-thread-local,
constructive sharing, and why it's *jittery*) · **cache-friendly data
structures** (`map` ~20 serial misses vs `btree_map` ~3–4 vs `flat_hash_map` ~1;
arena + `uint32` index links; CSR vs adjacency lists; SBO types) · **AoS vs SoA
deep** (three access patterns → three winners, AoSoA, SoA as the vectorization
prerequisite) · **data-oriented design** (philosophy, `vector<Base*>` + `virtual`
hot-loop cost breakdown, existence-based processing, handles + generation
counters, "group by what you do") · **TLB & huge pages** (4-level page walk,
dTLB/STLB reach math, 2 MiB → 512× reach, explicit hugetlbfs vs THP jitter,
pre-fault + `mlockall`) · **store buffers** (RFO = writes cost a read,
store-to-load forwarding stalls, write-combining, non-temporal stores +
`sfence`) · **memory bandwidth** (latency- vs bandwidth-bound, Little's Law → one
core ≈ 18 % of socket peak, STREAM, roofline / arithmetic intensity, "SIMD a
memory-bound loop = wasted", the add-threads test) · **measuring** (`perf stat` +
MPKI, `--topdown`, `perf record`/`annotate`, `perf c2c`, cachegrind, VTune/uProf,
a 7-step workflow) · **11 optimization recipes** in impact order, each with its
trade-off. 15 lessons + `16-exercises.md` + 8 **portable** `.cpp` examples (all
run on x86-64 incl. MinGW/Windows — 8/8 OK under strict flags) +
`09_perf_analysis.sh`. **CLAUDE.md Rule 2 fired three times, all taught not
hidden:** (a) `-O2` col-major traversal **~10× slower**, but `-O3 -march=native`'s
`-ftree-loop-interchange` swaps the nest → ratio ~1.0 (compiler fixed it);
(b) `__builtin_prefetch` on a light gather **~1.1×** (MLP already overlaps ~10
misses), on a memory-saturated heavy loop **~0.33× = 3× SLOWER**; (c) naive
64×64 blocked matmul **~10–15 % *slower*** than a plain `ikj` loop order (needs a
tuned microkernel). Other measured (AMD Zen 2, ~2 GHz — ratios port): sequential
vs random line access **~7×** (14 vs 2 GB/s); false sharing **~6× to ~44×,
run-to-run** (the variance *is* the lesson); AoS/SoA — SoA **~2.0–2.4×** on
sequential scans, AoS **~3×** on random whole-record; TLB/cache latency cliff
**~1.2 → ~95 ns** at ~8 MiB working set.

**HO CHUKA (Batch 9 part 4):** ~~compiler optimization (33)~~ ✅ folder 33 —
`-O` levels (`-O0→-Ofast`; the `-O0→-O1` **~6× cliff** measured), godbolt/
`-fopt-info-*`/`objdump`/`llvm-mca` workflow, **inlining** (`inline` = ODR not a
directive; heuristics; `always_inline`/`noinline`/`flatten`; the cross-TU/no-LTO
wall; inlining as the *enabler* of const-fold/CSE/vectorize/devirt across a
boundary — noinline **1.31 ns/iter** vs inlined ~1.0), **loop opts** (LICM,
strength reduction, unroll + the "one accumulator → serial chain" trap, fusion/
fission, interchange `-O3`-only, unswitch, rotation, IV elimination), **auto-vec**
(the conditions; **a float reduction won't vectorize at `-O2`** — reassociation —
measured **~4× only with `-ffast-math`/`#pragma omp simd`**, plain map **~3.5×**;
`-fopt-info-vec[-missed]`; `ivdep`/`assume_safety` = unchecked promises),
**const-fold/prop/DCE** (whole functions → `mov eax,const`; `constexpr`/
`consteval`/`constinit`/`if constexpr`/`[[assume]]`), **devirtualization**
(exact-type/`final`/speculative-LTO/PGO; the honest "won't fire for a
heterogeneous container"), **branch hints** (`[[likely]]`/`[[unlikely]]`/
`__builtin_expect` = **code layout not prediction**; measured **~1.3×** — HW
predictor already nails a 1/1000 branch; wrong hint = regression; PGO better),
**aliasing & `__restrict`** (blocks LICM/CSE/vectorize; TBAA / strict-aliasing UB;
`std::bit_cast` not `*(T*)`; local-copy fix; **measured ~3.5×** ex 04 — and the
pessimism **only appears with `[[gnu::noinline]]`**, inlining/LTO lets the
compiler prove non-aliasing from the call site), **LTO** (cross-TU inline/
const-prop/devirt/DCE/ICF; ThinLTO; flag consistency; ODR-exposure risk;
**measured ~2.3×** on a *throughput* loop, **no gain** on a *carried* loop — Rule
2 nuance), **PGO** (3-step + AutoFDO; representative-profile hazard; HFT =
replayed session), **`-march`/`-mtune`** (x86-64-v1..v4; `native` wrong for a
shipped binary; AVX-512 downclock; function multi-versioning), **`-ffast-math`
dangers** (the 8 sub-flags; **`-ffinite-math-only` compiles your `isnan` guard to
`false`**; `-fassociative-math` changes results; `-ffp-contract`; safe scoped
alternatives; **never on priced/audited code**), **preventing optimization**
(`DoNotOptimize`/`ClobberMemory` = zero instructions; `volatile` sink cost;
barrier placement; measured **no-barrier loop = 0.00 ms**), **reading optimized
output** (the verification checklist). 15 lessons + `16-exercises.md` + 8 examples
(6 portable `.cpp` → 6/6 OK). **Rule 2 fired three times, all taught.**
~~assembly (34)~~ ✅ folder 34 — reading x86-64 asm (never writing) for
verification/optimization/debugging: registers + sub-register zeroing + `xmm`/
`ymm`/`zmm` + rflags · **AT&T vs Intel** (the 5 differences; `gcc -S`/`objdump`/
`gdb`/`perf` = AT&T, godbolt/`./build.ps1 asm` = Intel) · the ~20 common
instructions (`lea` computes an address ≠ `mov [..]`; `cmp`/`test` = flags only;
recognize `idiv` = slow non-constant divisor, `rep movs` = a copy idiom, `lock`/
`xchg`/`mfence` = atomics) · **addressing modes** (`base+index*scale+disp` →
recover `sizeof(struct)` and field offsets from a loop) · **stack frames**
(prologue/epilogue ± frame pointer; spills = register pressure; tail call → `jmp`
loop; Win64 32-byte shadow space; stack canary) · **calling conventions** (System
V — args `rdi,rsi,rdx,rcx,r8,r9` — vs Windows x64 — `rcx,rdx,r8,r9` — where
`this` is, large-struct return via a hidden pointer, `extern "C"`, SysV varargs
`al`) · **pattern recognition** (`if` / if-converted `cmov` / loop (backward jXX
+ entry guard) / `while` / dense `switch` (jump table) & sparse (if-chain) / call
/ virtual `call [reg+off]` / constant-folded / reciprocal-multiply division) ·
**SIMD asm** (scalar `ss`/`sd` vs packed `ps`/`pd`/`d`; width from `add ptr,
16/32`; the horizontal-reduce cluster; `vzeroupper`; `vgather` is slow) ·
**inline asm** (the 4 sections; `"=r"`/`"+r"`/`"=m"`/`"memory"`/`"cc"`; a missing
clobber = silent `-O2`-only corruption; use it only for the barrier / `cpuid` /
`rdtsc` / `pause` / MSRs — everything else → intrinsics) · **`rdtsc` timing**
(invariant-TSC counts reference ticks not core cycles; fence with `lfence`/
`rdtscp`; calibrate for ns; core hopping; `clock_gettime` via vDSO for ≥ 1 µs —
measured calibration **~2.0 ticks/ns**, plain **~1 tick**, fenced **~20 ticks**)
· **disassembly tools** (`g++ -S` vs `objdump -dS` vs `perf annotate :pp` vs
`gdb disassemble /s` vs `addr2line -i` vs `llvm-mca` — which for which question).
13 lessons + 7 examples (5 `.cpp` → 5/5 OK + a hand-annotated `.s` + 10 asm
puzzles).

**HO CHUKA (Batch 9 part 5 — BATCH 9 DONE):** ~~profiling & benchmarking (35)~~
✅ folder 35 — **why measure** (intuition fails; **Amdahl's law** with the
numbers table — a 5% hotspot 10× faster = ~4.7% total; premature-optimization =
"*measure kiye bina*", not "timing"; **throughput vs latency** table — batching
helps one, hurts the other; "your HFT metric is always a latency percentile") ·
**`<chrono>`** (steady vs system vs high_resolution — the **alias trap**;
resolution vs precision vs accuracy vs **self-cost** — `steady_clock::now()`
**~38 ns** measured, so batch or use `rdtsc` for < ~100 ns regions;
`duration_cast` truncates; `clock()` = CPU-time, **1 ms** on this MinGW box) ·
**`rdtsc`** (fencing recipes `lfence;rdtsc;lfence` / `rdtscp;lfence`; **ticks→ns
calibration via busy-wait not `sleep`** — ~1.996 ticks/ns; self-cost subtraction;
core-hop → pin or check `rdtscp` `aux`; `clock_gettime` via vDSO for ≥ 1 µs) ·
**statistics** (mean lies on right-skewed latency — tail pulls it above the peak;
median = "typical"; **min = jitter-free floor** for micro-bench; σ useless for
non-normal → **CV / MAD / IQR**; **bimodal = two code paths**, mean lands in the
empty gap; how many samples per "nine") · **percentiles** (nearest-rank vs
interpolation; "nines" language; **fan-out tail amplification** — 100 calls at
p99 → 63% of requests hit a slow one, "the tail at scale"; **coordinated
omission** — a stalled load-tester stops sending → tail vanishes → HdrHistogram
`recordValueWithExpectedInterval`; **percentiles don't average** → merge
histograms) · **jitter** (SW sources: timer tick / scheduler preemption / other
IRQs / page faults / `malloc` on the hot path / syscalls / contended locks /
signals / THP compaction; HW & firmware: frequency scaling / C-states / **SMI**
(OS-invisible) / SMT sibling / NUMA remote / thermal throttle; measure with
per-iteration `rdtsc` + `cyclictest` / `rtla` / `hwlatdetect`; **the "quiet core"
recipe** — BIOS (turbo/C-states/SMT) + kernel cmdline `isolcpus`+`nohz_full`+
`rcu_nocbs`+`irqaffinity` + runtime pin/`SCHED_FIFO`-carefully/`mlockall`/
pre-allocate/no-syscalls/busy-poll — **spike count + p99.9 are the robust jitter
metrics, not a single `max`**) · **histograms** (why one linear bucket width
can't hold bulk-resolution + tail-range; **log-linear / HdrHistogram** — relative
error ≤ 1/`SUB` bounded at every scale; mergeable + O(1) record + fixed memory +
coordinated-omission correction; **CDF / percentile plot** (log x, "nines" y) >
PDF bars; `08_latency_recorder.cpp`: **~2 ns/record, 29.5 KB fixed, ≤1.3%
percentile error vs exact**, log-plot reveals **bimodal**) · **Google Benchmark**
(`for (auto _ : state)` — library picks iteration count; `benchmark::DoNotOptimize`
/ `ClobberMemory`; `->Range()`/`->Args()`/`SetBytesProcessed`; `PauseTiming` +
its own ~hundreds-ns cost; `BENCHMARK_F` fixtures; `--benchmark_repetitions` +
`_median` + `_cv` gate; the 10 mistakes the library **can't** fix; a
`minibench.hpp` shim so the example runs dependency-free, one-line switch to the
real lib) · **benchmark pitfalls** (all demoed BUG/FIX in `04`: **DCE** — no
sink → loop deleted → `0.000 ns/op`; **const-fold** — literal input → compile-time
solved; **loop-invariant hoist** — `f(x)` with fixed `x` → hoisted, you measure
`add`; **cold start** — code/data/page-faults/predictor cold, discard `run[0]`;
**timer > op** — `now()` per op ≈ 37 ns, batch → 3 ns; **one run** — min-of-N +
spread; **alignment & code-layout noise** — same source ±10%, trust only
differences > noise, check `perf` counters; **frequency scaling** — report
cycles/op (frequency-invariant); **denormals** — FTZ/DAZ; **bench ≠ reality** —
working-set / co-tenancy / predictor state / frequency differ in situ) ·
**`perf` basics** (counting `perf stat` vs sampling `perf record`; **IPC**
`<0.5`=stalled / `~1` / `>2`=healthy; cache-miss & branch-miss rates;
**top-down** Retiring / Frontend-Bound / Backend-Bound / Bad-Speculation with a
fix direction each — `perf stat -M TopdownL1`; `perf record -F 999 -g`
frame-pointer vs `--call-graph dwarf` vs `lbr`; `perf report` self vs children;
`perf list`; `perf_event_paranoid`; `[unknown]` symbols; sample skid) · **`perf`
advanced** (`perf annotate` per-instruction % + **skid → `-e cycles:pp`
(PEBS/IBS)** for correct attribution; `cycle_activity.stalls_l3_miss` to quantify
memory-bound; **`perf mem`** — per-access latency + "served from L1/L2/L3/local/
remote DRAM"; **`perf c2c`** — false sharing / **HITM** cache-line ping-pong;
**LBR** → basic-block profiles → **AutoFDO**; `perf script` → flame graphs;
**counter multiplexing** — ask for > ~4 events → each scaled, noisy) · **flame
graphs** (width = time, **X = alphabetical NOT chronological**, Y = stack depth;
**top plateau = optimize target**, wide-lower = distributed; `perf script |
stackcollapse-perf.pl | flamegraph.pl`; **on-CPU vs off-CPU** (blocked time is
invisible on-CPU — need `perf sched` / eBPF `offcputime`); **differential**
(red/blue); icicle; `hotspot` / Firefox Profiler / Speedscope) · **Valgrind**
(DBI on a synthetic CPU — **deterministic** but 20–100× + simulated cache model;
**cachegrind `Ir`** = exact hardware-independent instruction count → **CI
regression gate**; cache-miss counts are **directional ratios** only; callgrind +
**KCachegrind** GUI call graph; **massif** heap-over-time + alloc stacks;
**DHAT** per-alloc-site total/live/lifetime + **read/write counts** → over-alloc
/ churn / realloc chains) · **VTune / top-down** (recursive tree
Backend→Memory-Bound→DRAM-Bound→**Bandwidth vs Latency** — *different fixes*:
bandwidth → move fewer bytes; latency → overlap more misses; analysis types
Hotspots / Microarch-Exploration / Memory-Access / Threading / Anomaly-Detection;
**`toplev.py`** (pmu-tools) brings the same recursive top-down to plain `perf` on
Linux/any-CPU; AMD uProf / Instruments / Arm Streamline) · **sanitizers** (ASan =
OOB / UAF / leaks ~2×; UBSan = signed overflow / null / misalign / bad shift
~1.2× — always run; TSan = data races + lock-order ~5–15×; MSan = uninit reads,
needs instrumented STL → rarely used; **`volatile bool` is NOT a thread flag** →
`std::atomic<bool>`; **never benchmark under a sanitizer** — redzones/shadow
change cache & timing; ASan+UBSan+TSan CI matrix with `-fno-sanitize-recover`;
MinGW fallback = `_GLIBCXX_ASSERTIONS` + `-fstack-protector-all`) · **production
measurement** (6 requirements: low-overhead / always-on / full distribution /
per-stage / aggregatable / queryable; **inline `rdtsc` + per-thread
HdrHistogram**, snapshot every 1 s by a housekeeping thread on a non-trading
core; **SPSC ring → aggregator** so the hot path does zero math; sampling —
1-in-N / time-gated / **exceedance** (only record > threshold, with a context
dump); **coordinated omission in prod** = timestamp at **arrival**, not dequeue —
queue wait counts; per-thread → merge, **never average p99s**; **white box vs
black box** — inline instrument sees only its stages, a NIC HW timestamp / wire
tap / synthetic prober catches the blind spots (NIC queue, kernel, network);
USDT (near-zero detached) / uprobe (~µs/hit) / eBPF / LTTng). 16 lessons +
`17-exercises.md` + 8 examples (5 `.cpp` → 5/5 OK + a Google-Benchmark shim dir +
2 self-contained Linux `perf`/flamegraph scripts). Measured on an **unpinned
Windows/MinGW Zen 2 box — and that itself is taught**: `02`/`03` tail numbers are
deliberately run-to-run unstable, contrasted with a pinned Linux isolated core.
**⚠️ Rule 2:** the "BUG" benchmarks in `04`/`05` really report **0.000 / 0.49
ns/op** at `-O2` — the taught result, next to its FIX; cold-start is only ~1.2×
here (Windows commits pages eagerly). Mojibake sweep: 52 → 0.

**NONE.** — **saare C++ / systems / low-latency prerequisites, HFT domain
knowledge, market data, order book, matching engine, HFT concurrency, HFT
networking, optimization methodology, the capstone mini HFT engine, the full
debugging toolchain, interview readiness (layered question banks + system design
+ brainteasers + mocks), the 250-problem graded practice bank + `11-solutions/`,
the 13 quick-reference sheets, AUR the 17 end-to-end projects (folders 00–49)
poore ho chuke.** HFT build track (36–44) COMPLETE. Wrap-up folders
(~~45 debugging~~ ✅, ~~46 interview prep~~ ✅, ~~47 coding problems~~ ✅,
~~48 cheatsheets~~ ✅, ~~49 projects~~ ✅) + **~~final gap audit~~ ✅ (PHASE 34)** —
sab done.

### MAJOR HFT-RELEVANT C++ GAPS

**NONE.** low-latency C++ **ho chuka** (folder 36 — pools/arena/PMR,
branchless, dispatch elimination, ring buffers, batching, syscall & page-fault
avoidance, CPU pinning, cache warming, I-cache, zero-copy, compile-time
dispatch, + an honest trade-off catalogue), **HFT fundamentals bhi ho
chuke** (folder 37 — exchange architecture, microstructure, order types,
matching rules, risk systems, latency budgeting, regulation), **market
data bhi ho chuka** (folder 38 — ITCH-style protocol, zero-copy parsing
measured 22.2x, A/B arbitration measured 49.5x, gap detection, framing,
timestamping, conflation), **order book bhi ho chuka** (folder 39 —
3 versions built + measured, V2's genuine regression root-caused, V3 wins
every metric), **matching engine bhi ho chuka** (folder 40 — Limit/
Market/IOC/FOK, self-trade prevention, a genuine FOK+STP precheck bug
found + fixed + fuzz-verified against an independent reference engine),
**HFT concurrency bhi ho chuka** (folder 41 — production SPSC queue,
a working LMAX Disruptor built from scratch, seqlock snapshots measured
~2500-3500x faster than shared_mutex under contention, busy-spin-vs-
blocking's real CPU-cost measured, a real 3-stage pipeline running 40's
actual MatchingEngine end to end), **HFT networking bhi ho chuka**
(folder 42 — kernel-bypass ladder, an `INetworkReceiver` abstraction,
TX+RX same-clock-domain hardware timestamping, NIC/IRQ tuning, TCP
order-gateway tuning, a full wire-to-wire capstone — majority of this
folder's code is Linux-only, hand-reviewed since this dev box has no WSL),
**aur HFT optimization methodology bhi ho chuka** (folder 43 — the
6-step measure/profile/change-one/re-measure/**explain** loop with a
correctness gate that runs *first*, applied to a connected v0→v3 pipeline:
~60–80× end-to-end with byte-identical output; per-technique measured —
fixed-point `0.1×10 ≠ 1.0` exact, division `div`→shift ~17× / →reciprocal
~7% [only the divide timed], struct fat→SoA ~8×, and an honest Rule-2
**null** result on hot/cold splitting [~1% — frontend isn't the
bottleneck at this scale]), **aur ab THE CAPSTONE bhi ho chuka** (folder
44 — everything from 01–43 assembled into one deterministic
`MiniHftEngine`: MarketData → Parser → L2Book → Strategy → Risk → OMS →
Venue → fills → PnL; **reuse over rewrite** — folder-40's `MatchingEngine`
is the venue, folder-41's `SpscQueue` is the hand-off; the folder-43
optimization loop applied end-to-end via `template <class Venue>` —
`NaiveEngine` [std::map venue] vs `OptimizedEngine` [`FastVenue`
flat-array + per-level FIFO sweep], proven byte-identical across 5 configs
**before** the ~1.5–1.6× end-to-end speedup [book stage ~150 → ~67 ns/msg];
per-component measured — L2Book ~17 ns/apply [BBO = std::map ref exactly],
FixedPool ~2.0×/~5.0× vs new/delete, ObjectPool stale-handle rejection,
SPSC ~6 M msg/s in-order, risk 14/14, OMS accounting always settles; feed
parser v1≈v3 at `-O2` — another honest Rule-2 null; **no alpha** —
`SpreadCrossStrategy` is mechanical, 37 SPECIALIZED list).
**HFT build track (36–44) COMPLETE.** Ab sirf wrap-up folders (45–49) +
final gap audit. Detail ke liye `HFT-COMPLETENESS-AUDIT.md`
dekho.

---

## Kya ABHI TAK cover ho chuka hai ✅

**PHASE 0–34 — poore ho chuke hain (folders 00–49).** Batch 9 (29–35) + Batch 10
(36 low-latency-cpp, 37 HFT-fundamentals, 38 market-data, 39 order-book,
40 matching-engine, 41 HFT-concurrency, 42 HFT-networking, 43 HFT-optimization,
**44 HFT-PROJECTS — the capstone**) + Batch 11 (45 debugging, 46 interview-prep,
47 coding-problems, 48 cheatsheets, 49 projects, **+ the final gap audit**) DONE.
**HFT build track (36–44) COMPLETE. Course structurally complete.**


- Programming aur computer ke fundamentals
- C++ ka pehla program — har token ka meaning
- Compilation pipeline: source → preprocessor → compiler → assembler → linker →
  executable → OS loader → CPU
- Comments, statements, semicolons, blocks
- Variables, data, memory ka basic model
- Fundamental types: `int`, `char`, `bool`, `float`/`double`
- Signed/unsigned, sizes, ranges, overflow
- Floating-point precision traps
- Fixed-width types
- Initialization forms aur narrowing
- `auto`, `const`, `constexpr` (intro level)
- Type conversions aur integer promotion
- `sizeof`, `<limits>`
- Compiler flags, warnings, error reading
- Development environment setup
- **Poora I/O**: `cout`/`cin` deep, buffering aur flushing, `cerr`/`clog`,
  `getline`, input validation aur stream states, manipulators, `std::format` (C++20),
  file streams (text + binary), string streams, `printf` family aur uske dangers,
  I/O performance aur HFT logging rules
- **Poore operators**: arithmetic aur uske 4 traps, `++`/`--`, comparisons
  (signed/unsigned, float, NaN), logical + short-circuit, **saare bitwise operators**,
  bit manipulation toolkit, C++20 `<bit>`, compound assignment, ternary,
  precedence aur associativity, evaluation order aur sequencing
- **Conditions (folder 06)**: `if`/`else`/`else if`, truthiness, dangling else,
  nested conditions + guard clauses, `switch` (fallthrough, `[[fallthrough]]`,
  case scope, `enum class` completeness), `switch` vs `if` (jump table / arithmetic
  / compares — real assembly), `if`-with-initializer (C++17), 7 classic condition
  bugs + unke warnings, **branch prediction ka pehla measured parichay (~7x
  sorted vs unsorted), branchless intro, `[[likely]]`/`[[unlikely]]`**
- **Loops (folder 07)**: `while`/`do-while`/`for` + equivalence, range-`for`
  (`auto`/`auto&`/`const auto&`, ~50x copy cost measured, structured bindings),
  nested loops + complexity (O(n²)/O(n³)), `break`/`continue` + nested-exit +
  `goto` avoid, 7+ loop bugs (off-by-one, infinite, unsigned underflow, iterator
  invalidation preview), loop patterns (accumulator/search/filter/min-max/
  two-pointer/sliding-window), **loop performance: cache locality row-vs-column
  ~8x measured, loop unrolling (flag-dependent), auto-vectorization intro**
- **Arrays (folder 09)**: contiguous layout, `a[i] == *(a+i)`, init forms,
  **array decay** (`sizeof` trap), 2D row-major + flat, `std::array` (no decay,
  zero overhead), **`std::span`** (modern array param, dangling), **AoS vs SoA
  (~4x measured)**, OOB bug catalogue + `_GLIBCXX_ASSERTIONS`/ASan
- **Strings (folder 10)**: C-strings (`'\0'`, `<cstring>` dangers), `std::string`
  (O(1) size, `.at`, grows), **SSO** (measured 15-char threshold, 2x capacity
  growth), **`std::string_view`** (zero-copy, dangling rules), `from_chars`/
  `to_chars` (**measured ~8x vs `stoi`**), allocation-avoidance playbook, UTF-8
  (bytes ≠ characters), string bug catalogue
- **Structs (folder 11)**: aggregate/DMI/designated init, nested inline layout,
  pass/return semantics, **padding & alignment deep dive** (member reorder →
  30-45% smaller, measured), `alignas`, **packed wire structs + `memcpy` decode +
  `static_assert` layout**, **AoS vs SoA (~1.6-4x measured)**, `union`/
  `std::bit_cast`, **`std::variant` + `visit`**, **`enum class`**, bitfields
  (why masks win), `struct` vs `class`
- **Functions (folder 08)**: declaration vs definition, ODR, header/source,
  `undefined reference`; params (value/ref/ptr, `const&`, `string_view`); return
  values + RVO/copy elision + `[[nodiscard]]`; **call stack deep dive** — frames,
  prologue/epilogue, return address, calling convention (Win x64 vs SysV), stack
  growth, real assembly, **stack overflow (crashed it)**; scope vs lifetime,
  static locals, dangling refs/views; default arguments; overload resolution
  (phases, conversion ranks, ambiguity, name mangling); recursion (base case,
  **fib exponential measured ~11x/+5**, tail-call/TCO, → iteration); `inline`
  (ODR meaning, not "fast", **~1.9 ns/call measured**); `constexpr`/`consteval`/
  `if constexpr`, compile-time tables; attributes (`noexcept`, `[[likely]]`,
  `[[gnu::const]]`, ...); `main(int argc, char* argv[])` — CLI parsing
- **Pointers (folder 12)**: `&`/`*`/`nullptr` (vs `NULL`/`0`), pointer arithmetic
  (scaled by `sizeof`, in-bounds rule, one-past-end, `p2-p1`), arrays↔pointers +
  decay recap, `const int*` vs `int* const` vs both, `->` vs `(*p).`, `int**`
  (dependent loads, C-2D vs flat), `void*` (type erasure, C APIs, cast rules,
  no-inline cost), **function pointers** (syntax, callbacks, dispatch table, →
  virtual dispatch preview), dangling / use-after-free, full pointer-bug catalog,
  memory diagrams throughout, `08_swap_via_pointers` bridge to references
- **References (folder 13)**: reference = alias (`&r == &x`, no rebind, no null),
  vs pointer (full table: init/rebind/null/arithmetic/syntax/containers/sizeof),
  pass-by-reference + output params, **`const T&`** + lifetime-extension rules
  (and where extension does NOT apply — call/return/member), returning references
  + dangling, range-`for` copy vs alias (`auto`/`auto&`/`const auto&`), reference
  members + `std::reference_wrapper` + what special members disappear, **`T&&` /
  rvalue-reference / `std::move` / move-ctor intro** (copy vs move, forwarding
  refs, reference collapsing — full depth folder 18), reference bug catalog.
  **Measured: by-value vs `const&` `std::vector` ~190x**
- **Memory (folder 14)**: process layout revisit (per-variable region, `.bss`
  demand-zero, Win vs Linux caveat); **stack deep dive** (frames, `sub rsp`,
  grows-down, limits, `alloca`/VLA why-not, thread stacks); **heap deep dive**
  (allocator fast/slow path, tcache/bins, `brk`/`mmap`, `new` vs `malloc`,
  headers, `operator new` hook); `new`/`delete` (2-step, forms, `nothrow`, mixing
  = UB, RAII replacement); **leaks** (patterns, counted-`new` detection, RAII
  prevention); **UAF / double-free** (why "sometimes works", block reuse,
  `delete this`); `static` / `thread_local` (4 durations, 2-phase init,
  **static-init-order fiasco + fix**); **allocation cost — measured** (stack vs
  heap ~37 ns vs <1 ns on GCC 16.2; latency distribution p50/p90/p99/p99.9/max via rdtsc; tail = lock/
  syscall/page-fault); **fragmentation** (internal/external, why RSS creeps, no
  compaction in C++); **custom allocation intro** (placement new, arena/bump,
  fixed-size pool — measured ~21x on a 64-order burst + flat tail, `std::pmr` preview); **memory
  tools** (ASan/LSan/MSan/UBSan/Valgrind/heaptrack tool→bug matrix + MinGW
  limits & workarounds)
- **Classes (folder 15)**: encapsulation + invariants (BankAccount /
  HFT-style Order state machine); members / `this` / method resolution; access
  specifiers (`public`/`private`/`protected`, standard-layout & mixed access);
  constructors (default / parameterized / delegating / `= default` / `= delete`,
  `initializer_list` gotcha); **member initializer lists** (vs body assignment,
  const/ref/no-default members, **declaration-order trap** with `-Wreorder`);
  destructors (when they run, reverse order, trivial vs non-trivial, Rule of 3/5
  preview, virtual-dtor preview); `const` member functions (+ `mutable` legit
  use, const/non-const `at()` overload, const-correctness chain); `static`
  members (`inline static`, static methods, static factory pattern, init-order
  fiasco); **operator overloading** (`+=` member vs `+` free, unary,
  prefix/postfix `++`, `<<` free, C++20 `<=>`/`==` default); `friend`
  (hidden-friend idiom, not transitive/inherited); `explicit` (single/multi-arg,
  `explicit operator bool`, "by default" rule); nested & local classes (access,
  pImpl sketch); class layout (`sizeof`, empty class = 1, **EBO**,
  `[[no_unique_address]]`, `is_polymorphic`). **Encapsulation = zero runtime
  cost** throughout.
- **OOP (folder 16)**: inheritance (`public`/`protected`/`private`, layout with
  base at offset 0, upcast cost, name hiding + `using`); construction/destruction
  order (bases → members → body, reverse dtor, **virtual-call-in-ctor trap**,
  exception during construction); virtual functions (static vs dynamic dispatch,
  `override` catching signature bugs, template-method pattern); **vtable/vptr
  deep dive** (vptr per-object / vtable per-class, 2 dependent loads + indirect
  call, `sizeof` +8, `virtual` breaks `memcpy`/triviality, ABI); `override` /
  `final` (compiler check, devirtualization); abstract classes / interfaces
  (pure virtual `= 0`, pure-virtual body, pure-virtual dtor); **virtual
  destructors** (measured leak demo — non-virtual base dtor → skipped
  `~Derived()`; "public+virtual OR protected+non-virtual"); object slicing (what
  is cut, 4 places, `= delete` copy / `clone()`); multiple inheritance
  (interfaces safe, pointer adjustment / `this`-thunks); virtual inheritance +
  diamond (2 base subobjects → 1 shared, most-derived-inits rule, layout cost);
  **RTTI** (`typeid` exact vs `dynamic_cast` is-a, ptr-null / ref-throw, cost,
  `-fno-rtti`, hot-path alternatives); **virtual dispatch cost — measured**
  (virtual ~23 ns vs direct/CRTP ~2.2 ns vs `variant`+`visit` ~16 ns per call;
  BTB effect); **CRTP** (zero-cost static polymorphism, mixins, `friend D` guard,
  EBO); composition vs inheritance ("prefer composition", forwarding inlines
  away); **SOLID** (all 5 with C++ examples + HFT reconciliation: move
  open/closed & DIP to compile-time — `variant` / templates — on hot paths).
- **RAII (folder 17)**: what a resource is (heap / fd / lock / FILE / socket);
  the RAII idiom (acquire in ctor, release in dtor, `= delete` copy or define
  move); why it works (stack unwinding runs dtors on every exit path — return,
  exception, break) and the **5 gaps where dtors don't run** (`std::abort`,
  `std::exit`, `longjmp`, uncaught exception past `main`, object never deleted);
  `std::lock_guard`/`scoped_lock`; move-only wrapper shape (`fd_ = -1`, guarded
  dtor, move steals + nulls, copy `= delete`); **`unique_ptr`** (zero overhead,
  move-only, `sizeof` == 8 via EBO, `release`/`reset`, `unique_ptr<T[]>`);
  **`shared_ptr`** (control block, **atomic** strong/weak counts, `sizeof` == 16,
  `make_shared` 1 alloc vs `shared_ptr(new)` 2, `enable_shared_from_this`);
  **`weak_ptr`** (breaks `shared_ptr` cycles, `lock()`/`expired()`); custom
  deleters (stateless struct → EBO → 8, function pointer → 16, `shared_ptr`
  deleter type-erased → always 16); ownership-semantics spectrum (value /
  `unique` / `shared` / `weak` / raw `T*`) + sink-parameter idiom; Rule of Zero.
  **Measured**: `shared_ptr` copy+destroy ~33.6 ns ≈ **93x** a raw-pointer copy;
  `unique_ptr` create/destroy == raw; `make_shared` 1 allocation vs 2.
- **Copy & Move (folder 18)**: copy ctor / copy assignment (deep vs shallow,
  self-assignment guard, allocate-before-free for the strong guarantee,
  copy-and-swap idiom); Rule of Three; **value categories** (lvalue / prvalue /
  xvalue / glvalue / rvalue, **2-question model** — identity? movable?,
  `decltype(x)` vs `decltype((x))`, "a named rvalue reference is an lvalue");
  rvalue references `T&&`, **forwarding references** (deduced `T&&` / `auto&&`),
  **reference collapsing** (`& + anything → &`; only `&& + && → &&`); move ctor /
  move assign (steal + null the source, moved-from = "valid but unspecified",
  `std::move` on each member by hand); **`std::move` = `static_cast<T&&>`** — a
  cast, zero runtime cost, moves nothing itself; gotchas — `std::move` on `const`
  → copy, `return std::move(local)` → pessimization (`-Wpessimizing-move`,
  disables NRVO), `std::move` on a POD → no-op; Rule of Five; Rule of Zero;
  **copy elision** — RVO (prvalue return, **guaranteed C++17**, unaffected by
  `-fno-elide-constructors`), NRVO (named local, best-effort); **perfect
  forwarding** (`std::forward<T>` conditional cast, canonical `make<T>(Args&&...)`
  factory); **`noexcept` move** — `std::vector` reallocation moves only if the
  move ctor is `noexcept` (`std::move_if_noexcept`), else copies for the strong
  guarantee, `is_move_constructible` vs `is_nothrow_move_constructible`.
  **Measured**: `noexcept` move → vector MOVES on realloc ~152 ms vs
  non-`noexcept` → COPIES ~468 ms (**~3x**); copy 1M `std::string`s ~177 ms vs
  move the vector ~0.0001 ms.
- **STL (folder 19)** — 26 lessons: STL architecture (containers + iterators +
  algorithms decoupled → M+N, half-open ranges `[first, last)`, 6 iterator
  categories + tag dispatch); **`std::vector`** (layout, geometric growth,
  `reserve` vs `resize`, full invalidation table, `clear` keeps capacity,
  swap-with-empty, `emplace_back`, `vector<bool>` gotcha); **`std::array` /
  `std::span`** (zero-overhead, non-owning `{ptr,len}`, dangling rules);
  **`deque` / `list` / `forward_list`** (chunk map, node cache cost — measured
  **~27x** list vs vector iterate, when each is genuinely right); **`map` / `set`**
  (RB-tree, ordered guarantees, `operator[]` insert-on-read trap, strict-weak
  comparator, `multimap`); **unordered containers** (buckets + chaining, load
  factor / rehash, custom `hash` + `hash_combine`, open addressing vs
  `std::unordered_map`, `reserve`); **container adapters** (`stack` / `queue` /
  `priority_queue` — restricted interface, max-heap default, heap-ordered vector);
  **iterators in depth** (categories, `advance`/`next`/`distance` O(n) traps,
  inserter / stream iterators, full invalidation table, writing your own);
  **`<algorithm>` ×4** — non-modifying (find / count / all_of / search / mismatch),
  modifying + **erase-remove idiom** (`std::remove` doesn't resize) + C++20
  `std::erase`/`erase_if`, sorting family (introsort — guaranteed O(n log n),
  `stable_sort`, `partial_sort`, **`nth_element` O(n)**, `partition`),
  binary-search + set ops + heap ops + permutations (all need sorted input);
  **`<numeric>`** (`accumulate` — **init type = accumulator type** traps — vs
  `reduce` — assoc/commut, FP reorder; `transform_reduce` / `inner_product`;
  `partial_sum` / `inclusive_scan` / `adjacent_difference`; `iota`);
  **`<ranges>`** (lazy non-owning views composed with `|`, `filter`/`transform`/
  `take`/`iota`/…, lazy-eval proof, range algorithms + projections, dangling /
  single-pass / non-O(1)-`begin()` caveats); **utility types** (`pair`/`tuple` +
  structured bindings, **`optional`** — no heap, `value()` throws vs `*` UB,
  **`variant`** + `visit` jump-table dispatch + exhaustiveness, `any`,
  **`expected`** — C++23, discussed); **`<functional>`** — **`std::function`
  cost measured** (type-erased indirect no-inline ~5 ns + heap alloc for a big
  closure), template / fn-ptr / `function_ref` / `bind_front` / `std::ref`;
  **`<chrono>`** (`duration<Rep,Period>` unit safety, `steady_clock` vs
  `system_clock` vs `high_resolution_clock`, `duration_cast` truncates, **correct
  microbenchmark discipline** — `-O2` + warm-up + min-of-reps + sink + clock
  resolution); **`<random>`** (engine vs distribution, `mt19937`, `seed_seq`
  seeding, `thread_local` engines, why `rand() % n` is biased);
  **`<filesystem>`** (`path` string ops + `operator/`, disk queries,
  `directory_iterator` cached `entry`, throwing vs `error_code`, TOCTOU);
  **`<regex>`** (3 ops, **why slow** — µs construction, backtracking / ReDoS,
  ~10x PCRE/RE2 — `static const` hoisting, alternatives); **`<type_traits>`**
  (predicate `_v` / transformation `_t`, `if constexpr` fast paths,
  `is_trivially_copyable` / `is_nothrow_move_constructible`, `void_t` detection);
  **`<bit>`** (`bit_cast`, `popcount` / `countr_zero`, `bit_ceil`, `rotl`,
  `endian` / `byteswap` → single CPU instructions); **allocators** (C++17 minimal
  surface, converting ctor + `operator==`, `allocator_traits`, arena/bump + pool,
  EBO); **PMR** (`memory_resource` vs `polymorphic_allocator`, `monotonic_buffer_
  resource` on a stack buffer → **0** heap allocs measured, `null_memory_resource`
  upstream as a no-alloc assertion, `release()` reuse, non-propagation on move);
  **container performance** (the full table + "performance ≈ cache misses, not
  operations" — measured `map` 1049 / sorted-vec 357 / `unordered_map` 113
  ns/lookup; iterate 1M `vector` 0.67 / `list` 18 / `set` 197 ms); **STL in HFT**
  (3 tiers — control plane / warm / hot; what's used as-is, what's replaced with
  flat preallocated structures, why the hot-path answer is often "no STL
  container at all"). 11 examples, all `-Wall -Wextra -Wshadow` clean.
- **Algorithms & DSA (folder 20)** — 17 lessons: complexity analysis **and its
  limits** (Big-O counts operations, hardware counts cache misses) · arrays —
  two-pointer / sliding window (fixed + variable) / prefix sums / difference
  array · sorting family — bubble/selection/insertion/merge/quick(median-of-3)/
  heap, each implemented + benchmarked, plus counting/radix · **`std::sort`
  internals = introsort** (quicksort + heapsort depth-limit fallback + insertion
  finish) · binary search — half-open `[lo,hi)` invariant, `lower_bound`/
  `upper_bound`, "binary search the answer", the 5 classic traps · linked lists
  — singly/doubly/circular, reverse, Floyd cycle detect, **why they lose (~40x)**
  + the intrusive-list-over-an-arena pattern · stacks/queues — array vs linked,
  monotonic stack, **circular buffer** (power-of-two mask, full/empty,
  back-pressure) · **write your own hash table** — open addressing (linear
  probing, tombstones, resize), splitmix64 finalizer, benchmarked vs
  `std::unordered_map` · trees & BST — traversals, 3-case delete, balancing (why
  1023 ascending inserts → height 1023) · heaps — array representation, sift
  up/down, `make_heap` O(n), top-k, running median, indexed heap · tries —
  array/map/radix nodes, prefix queries, flattening · graphs — adjacency
  list/matrix/CSR, BFS/DFS(rec+iter)/Dijkstra(lazy decrease-key)/topo(Kahn) ·
  greedy — the pattern, when it fails, **how to prove/verify** (exchange
  argument, brute-force diff) · DP — the recipe, memo vs tabulation, the classic
  families, fib/knapsack/LCS · bit manipulation — int-as-set, `<bit>` helpers,
  submask enumeration, bitmask DP (TSP/assignment), the classic tricks · string
  algos — KMP (prefix function), rolling hash / Rabin-Karp, Z, why not
  `std::regex` · **cache-aware DSA** (the synthesis: latency-number table,
  pointer-chase dependency chains, the textbook→flat transforms, when NOT to) ·
  graded problem sets (easy→hard + systems/HFT-flavoured). 8 examples.
  **Measured: O(n²) sorts ~600–730 ms vs `std::sort` ~1 ms (n=20k); list sum 5M
  ~70 ms vs vector ~1.7 ms (~40x); open-addr hash build/lookup ~66/~45 ns vs
  `unordered_map` ~295/~83; fib(40) naive ~244 ms vs memo ~0.02 ms (~11,500x);
  flat tree traversal 4–11x a pointer tree.** (A quicksort median-of-3 bug — it
  picked the max, not the median → O(n²) on sorted input — was caught by a hung
  benchmark and fixed.)
- **Templates & generic programming (folder 21)** — 16 lessons: why templates
  (compile-time code generation, zero runtime cost, vs `void*`/virtual/macros) ·
  function templates — argument deduction (by-value/`const&`/`T&&` rules, **no
  implicit conversion**), explicit args, `auto`/`decltype(auto)` return, overload
  resolution (exact non-template beats template), `extern template` · class
  templates — out-of-class members, member function templates, **CTAD** +
  deduction guides · template parameters — type / **non-type (NTTP → inline
  storage + unrolled loops)** / template-template; `auto` and structural-class
  NTTPs · **full + partial specialization** ("most specialized wins"; function
  templates specialize-by-overload) · variadic templates — parameter packs,
  `sizeof...`, expansion patterns, recursion vs **the 4 fold-expression forms**,
  perfect-forwarding a pack · **`if constexpr`** (discarded branch not
  instantiated — replaces tag dispatch + much SFINAE) · `<type_traits>`
  internals — `integral_constant`, partial-spec traits, **`void_t` + `declval`
  detection idiom**, `conditional_t`, compiler intrinsics · **SFINAE** —
  "substitution failure not an error", immediate context, `enable_if` in 3
  places, expression SFINAE · **concepts (C++20)** — define (from traits /
  `requires`-expr with the 4 requirement kinds), 4 constraint syntaxes,
  **subsumption**, one-line errors · **CRTP** — static polymorphism, the
  template-method pattern, mixins, `friend Derived` guard, `enable_shared_from_
  this` · tag dispatch (iterator categories) · **TMP** — `constexpr` functions +
  `if constexpr` + folds *instead of* recursive-template computation;
  `index_sequence`; compile-time tables · **two-phase lookup** —
  `typename`/`.template` disambiguators, dependent-base `this->`, ADL for
  dependent calls (`using std::swap`) · instantiation model — per-TU
  instantiation, COMDAT merge, **code bloat** + `extern template` + factoring
  T-independent code + type erasure · **templates in HFT** (compile-time dispatch
  vs `virtual`, policy classes, NTTP containers, `constexpr` tables). 8 examples.
  **Measured: CRTP ~0.56 ns/call vs virtual through a real polymorphic boundary
  ~2.43 ns (`sizeof` unchanged — no vptr); virtual ~2.51 vs template ~1.14 vs
  `std::variant`+`std::visit` ~1.13 ns per call; devirtualization caveat noted.**
- **Modern C++ 11 → 23 (folder 22)** — 14 lessons: a **systematic
  standard-by-standard audit** — C++11 (move semantics + the memory model,
  `auto`, lambdas, `nullptr`, range-`for`, `constexpr`, smart pointers,
  `<thread>`/`<atomic>`, `enum class`, `= default`/`= delete`,
  `override`/`final`, variadic templates, `noexcept`, raw string literals) ·
  C++14 (generic lambdas, `auto` return deduction, relaxed `constexpr`, variable
  templates, init-capture, `std::make_unique`, `std::exchange`, transparent
  comparators) · C++17 (structured bindings, `if`/`switch` with initializer,
  **`if constexpr`**, fold expressions, CTAD, inline variables, **guaranteed copy
  elision**, `std::optional`/`variant`/`any`, `std::string_view`, `std::byte`,
  `std::filesystem`, `<charconv>`, parallel algorithms, `std::scoped_lock`) ·
  C++20 (**concepts, ranges, coroutines, modules**, `operator<=>`, designated
  init, `consteval`/`constinit`, `constexpr` almost everywhere, abbreviated
  function templates, `[[likely]]`/`[[no_unique_address]]`, `std::span`, `<bit>`,
  `<numbers>`, `std::format`, calendar `<chrono>`, `std::jthread`/`<stop_token>`,
  `<latch>`/`<barrier>`/`<semaphore>`, `std::erase`/`erase_if`, `std::bind_front`)
  · C++23 (**`std::expected`**, `std::print`/`std::println`, **`std::generator`**,
  `std::mdspan`, `std::flat_map`/`flat_set`, `std::ranges::to`, new range
  adaptors, `std::byteswap`, `std::move_only_function`, `import std;`, **deducing
  `this`**, `if consteval`, `[[assume]]`, multidim `operator[]`) · deep dives:
  **lambdas** (desugaring, all capture forms, `mutable`, generic, cost) ·
  **`constexpr`/`consteval`/`constinit`** (three guarantees; `.rodata` tables →
  zero-init deterministic startup; kills init-order fiasco + the init guard) ·
  **ranges deep** (range concepts, view semantics, `filter_view::begin()` is
  O(n), re-iteration re-runs, dangling; **measured pipeline ~1.5x faster than a
  branchy hand loop — taught, not hidden**) · **coroutines** (promise hooks,
  `coroutine_handle`, the frame + HALO, generators vs async tasks, per-element
  cost — C++20 has no `std::generator`) · **modules** (`#include` vs `import`,
  BMI, macro containment, module-private names, build order; a working multi-file
  `05_modules_demo/`) · **`<=>`** (`= default` gives all six, custom needs a
  hand-written `==`, comparison categories, NaN → all-false, rewrite + reversed
  candidates) · **attributes** (diagnostic vs codegen; `[[likely]]` layout;
  `[[assume]]` = unchecked axiom; `[[gnu::hot/cold]]` i-cache) · **`std::format`**
  (spec mini-language, custom `std::formatter<T>`, `format_to_n` zero-alloc, vs
  `printf`/iostreams) · **legacy → modern migration guide** (before/after table +
  strategy). 8 examples (7 `.cpp` + the multi-file modules demo).
- **Error handling (folder 23)** — 13 lessons: the full landscape + **the two
  questions** that pick a mechanism (expected vs exceptional? caller can handle
  or only report?) · exceptions mechanics (catch by `const&` + order, `throw;` vs
  `throw e;`, `std::exception_ptr`) · **stack unwinding** (ctor-mid throw → only
  constructed members' dtors, `~T` doesn't run; `noexcept` boundary →
  `std::terminate`; `-fno-exceptions` keeps RAII) · **exception safety** —
  no-throw / strong / basic / none, commit-or-rollback + copy-and-swap, **leak
  on throw vs RAII rollback shown via a live-object counter**, `noexcept` move ⇒
  `std::vector` realloc moves not copies · **`noexcept`** deep (contract not
  hint; violation → `terminate`, no search phase; `move_if_noexcept`; conditional
  `noexcept(expr)`) · custom exception hierarchies (`std::runtime_error` base,
  `throw_with_nested`) · **measured exception cost** — happy-path try/catch vs
  return-code **~1.0×** (zero-cost model), **~6000+ ns per throw+catch vs ~1.5 ns
  per return-code check → ~4000×**, 0.1% error rate → ~5× · **`-fno-exceptions`**
  — the 4 reasons HFT disables exceptions + the exception-free toolkit
  (`[[nodiscard]] enum class`, `std::expected` with a cheap enum `E`,
  `error_code` at OS boundaries, `new(nothrow)` / pools, `abort` for corrupt
  invariants) · **`std::expected`** — monadic `and_then`/`transform`/`or_else`/
  `transform_error`, `E` choice · **`std::error_code`** / `error_condition` /
  custom `error_category` · assertions / `static_assert` / `std::unreachable` /
  `[[assume]]` / contracts (C++26) · **UB catalog** — UB vs unspecified vs
  impl-defined, how the optimizer exploits UB, by category, UBSan/ASan/TSan. 7
  examples (incl. one that builds under both `-std=c++20` and `-std=c++23`, and
  one that builds with and without `-fno-exceptions`).
- **Compilation, linking & build systems (folder 24)** — 15 lessons: translation
  units + the 4-phase model · preprocessor deep (macros, `#`/`##`, `__VA_OPT__`,
  X-macros, predefined macros, `_Pragma`, missing-parens / double-eval traps —
  measured) · include guards / `#pragma once` / IWYU / self-contained headers /
  pImpl · **ODR deep** — loud "multiple definition" vs **silent IFNDR**
  (`sizeof(Config)` 12 vs 16 links fine, caught only by `-flto -Wodr`),
  `_GLIBCXX_USE_CXX11_ABI` / `_GLIBCXX_DEBUG` flag drift · linkage
  (internal/external/module, anon namespaces vs `static`, `const` default-internal,
  `-fvisibility=hidden`) · storage (`static` 3 meanings, `extern` decl/def,
  `inline` variables, `thread_local` + TLS cost, `constinit`, init-order fiasco +
  construct-on-first-use) · name mangling (Itanium grammar) / `extern "C"` / ABI
  / `c++filt` · object files & ELF (`.text`/`.rodata`/`.data`/`.bss`, `.symtab`
  codes, relocations link-time vs load-time) · **static vs dynamic linking** —
  `.a` member selection (`nm app` proves `calc::huge_unused` isn't pulled), `.so`
  PLT/GOT indirection cost, `-fPIC`, RUNPATH/`$ORIGIN`, **why HFT static-links** ·
  every common linker error + fix (incl. "function exists but undefined
  reference" = signature/ABI mismatch, "undefined reference to vtable") · Make
  (`-MMD -MP` auto-deps — `touch header` → all `.o` rebuild; order-only prereqs;
  `.PHONY`) · CMake (targets + `PUBLIC`/`PRIVATE`/`INTERFACE` usage requirements,
  `find_package`, generator expressions, install) · build performance (header
  hygiene, PCH, unity builds + their ODR risk, ccache, ninja, `mold`/`lld`) ·
  binary tools (`nm`/`objdump`/`readelf`/`ldd`/`strings`/`size`/`strip`/`objcopy`/
  `addr2line`/`bloaty`) · **LTO and PGO** — LTO removes the TU optimization
  boundary (IR in the `.o`, whole-program at link), ThinLTO, costs; PGO 3-step
  flow, block-layout / inlining / function-ordering wins; **LTO + PGO + static +
  `-march=native` = the HFT release config**. Examples: 6 multi-file directories
  (each with its own `build.sh`/`build.ps1`, `.cxx`/`.hpp` so the repo checker
  skips them) + `07_binary_inspection.sh` + 2 self-contained `.cpp`.
- **Object model & UB (folder 25)** — 16 lessons: what an object *is* (region of
  storage, subobjects, `sizeof ≥ 1`) · **object lifetime** (ctor-complete →
  dtor-start; **storage duration vs lifetime**; storage reuse + `std::launder`;
  implicit-lifetime types; out-of-lifetime = UB) · four storage durations ·
  **static init order fiasco** — *reproduced* (`02_static_init_fiasco/`: link
  order B prints `BADLOG[1/(null)]`, the `std::string` member's ctor hadn't run)
  + construct-on-first-use + `constinit` · **temporaries & lifetime extension**
  (full-expression rule; `const&`/`&&`/`auto&&` local binding extends transitively;
  the 4 non-extension / dangling cases — through a return, ctor member-init,
  `string_view` into a temp, C++20 range-for over `f().member`) · **trivial /
  trivially-copyable / standard-layout / POD / `has_unique_object_representations`**
  — which unlocks `memcpy` the bytes / `offsetof` / `memcmp` for equality /
  `malloc`+use (measured `memcmp == -1` without `memset` first) · **object vs
  value representation**, padding, `-Wpadded` · **alignment deep** (`alignas`,
  over-aligned types, aligned `new` vs `malloc` = UB, `std::align`,
  `hardware_destructive_interference_size`) · **placement new** & manual lifetime
  (pools, a `FixedOptional<T>`) · **strict aliasing** (the sanctioned glvalue
  types + the `char`/`byte` exception; `std::bit_cast`/`memcpy` vs
  `reinterpret_cast`+deref / union-inactive-read = UB; TBAA — measured `-O2`
  load-reuse divergence, `aliasing_trap` `delta == 0`; `-fno-strict-aliasing` is
  a crutch) · **type punning** (every method + verdict) · **the four casts deep**
  (`dynamic_cast` internals: vptr → `type_info` → `__dynamic_cast` graph walk;
  cost; hot-path alternatives) · **vtable layout exact** (slots, `offset-to-top`,
  `type_info`; MI → two vptr + thunks; virtual inheritance) · **ABI** (Itanium;
  the ABI-break catalog; `abidiff` / `static_assert`; stable-API design =
  `extern "C"` + opaque handles + versioning) · **the UB catalog** (50+ cases by
  category) · **how the compiler exploits UB** (null-check removal;
  `overflow_check(INT_MAX)` = `1` at every `-O`, `0` only with `-fwrapv`; load
  reuse — real `-O2` examples). 8 examples (7 `.cpp` + `.cxx` fiasco dir).
- **Concurrency (folder 26)** — 17 lessons: concurrency vs parallelism (+ Amdahl
  — the serial fraction / a shared lock caps scaling) · process vs thread (shared
  address space vs per-thread stack/registers/TLS; creation ~10× cheaper; context
  switch + TLB flush for processes; `fork()` COW hazard in a threaded program;
  isolation → process-per-component + shared-memory ring buffers) ·
  `std::thread`/`jthread` (construct=start, **join-or-`std::terminate`**,
  move-only, `native_handle` for pinning) · **passing data** (arguments are
  decay-copied — `std::ref` for a real reference; dangling captures in detached
  threads; `this` lifetime in a member-fn thread) · **race conditions & data
  races** (race condition vs data race = **UB**; the `++counter` load-add-store
  interleaving — **measured ~70–75% lost updates, different every run**;
  cached-flag / torn-read / compiler-assumption consequences) · critical
  sections & mutual exclusion (1 thread; keep tiny; no I/O / alloc under the
  lock) · `std::mutex` (uncontended ~15 ns CAS vs contended futex + context
  switch = a P99 spike; the family; `std::call_once`) · **lock guards**
  (`lock_guard` / `unique_lock` / `scoped_lock`, the unnamed-temporary-guard
  trap, `unique_lock` only for CVs) · **deadlock** (Coffman's 4 conditions;
  break any one — consistent lock order / `std::scoped_lock` / lock hierarchy /
  `try_lock`+backoff-livelock / one lock / lock-free; self-deadlock; detection
  via gdb bt, TSan, `timed_mutex`) · `std::shared_mutex` (2–5× costlier acquire,
  reader-count cache bouncing → the **immutable snapshot + atomic pointer swap**
  pattern usually wins) · **condition variables** (the predicate pattern,
  state-change-under-lock-then-notify, **lost wakeup** + **spurious wakeup**,
  `notify_one` vs `notify_all` thundering herd, a 2-CV bounded producer-consumer)
  · futures & promises (`std::async` policies + its **blocking dtor**,
  `packaged_task`, exception propagation via `get()`) · **build a thread pool**
  (mutex+cv queue, `submit` → `future`, run job outside the lock, drain-vs-cancel
  shutdown; measured serial ~271 ms vs pool(8) ~48 ms, fib(30)×64) · **C++20
  sync** (`jthread`+`stop_token`, `latch`, `barrier`+completion fn,
  `counting_semaphore` — often a single futex, faster than `mutex`+`cv`) ·
  `thread_local` (TLS access cost — initial-exec `%fs:[K]` vs general-dynamic
  `__tls_get_addr`; hoist it in hot loops; `Context&` vs TLS) · **false sharing**
  (**measured ~10×** — packed vs `alignas(64)` per-thread counters; MESI
  line-bouncing; the SPSC head/tail case; "accumulate locally + combine" beats
  even perfect padding) · concurrency bug catalog + detection (TSan / Helgrind /
  stress+asserts / `perf c2c`). 9 examples (races/deadlock/false-sharing show the
  *effect*; ⚠️ TSan/ASan not on this MinGW — READMEs give the Linux commands).
- **Atomics & the C++ memory model (folder 27)** — "course ka sabse mushkil
  folder", 16 lessons: the **formal data-race definition** (same location, ≥1
  write, neither happens-before the other, ≥1 non-atomic, different threads ⇒
  **whole-program UB** — not just a wrong number; "same location" for struct
  members vs adjacent bit-fields; `-O0` hides it) · **atomicity** (indivisible;
  non-atomic RMW = load-modify-store; torn / misaligned reads; x86 aligned ≤8B is
  hardware-atomic but *still a data race* because the compiler reorders / caches /
  coalesces) · `std::atomic<T>` (trivially-copyable, non-copyable, brace-init,
  `is_always_lock_free`, `atomic_flag`, C++20 wait/notify) · **atomic operations**
  (`load` / `store` / `exchange` / `fetch_*`; x86 `mov` / `xchg` / `lock xadd` /
  `lock cmpxchg`; `fetch_add` returns the OLD value; no `fetch_max`) · **CAS**
  (`compare_exchange_weak` vs `strong`, spurious failure on LL/SC, `expected` is
  overwritten with the current value on failure, the CAS-loop pattern for
  arbitrary RMW, success vs failure memory order, livelock / retry storm,
  `fetch_add` beats a CAS loop where it fits) · **why memory ordering exists**
  (compiler *and* CPU reorder; the store buffer → store→load reordering even on
  x86) · **`relaxed`** (atomicity only — exact count, zero ordering; standalone
  counters, `shared_ptr` refcount ++) · **acquire / release** (a release store
  synchronizes-with the acquire load that reads its value → everything before the
  release happens-before everything after the acquire; the publish/subscribe
  pattern; free on x86) · **`seq_cst`** (acquire+release *plus* a single total
  order all threads agree on; the default; the store-side `mfence` — measured
  ~18× a relaxed store on x86; loads & RMW order-insensitive on x86) ·
  **happens-before / synchronizes-with / sequenced-before** (the three relations,
  the synchronizes-with edge list — release↔acquire, thread create/join,
  `mutex::unlock`↔`lock`, `promise`↔`future`, C++20 sync — transitivity, the
  value a non-atomic read sees, the non-edges: wall-clock time, `relaxed` pairs,
  different-variable release/acquire) · **fences** (`std::atomic_thread_fence` —
  x86: only the `seq_cst` fence emits `mfence`, acquire/release fences are
  compiler barriers; `std::atomic_signal_fence` — compiler-only, no instruction,
  useless across cores; release fence + relaxed store; the `seq_cst`-fence trick
  to keep flags `relaxed`) · **x86-TSO vs ARM/POWER** (x86 allows only store→load
  reordering; ARMv8/POWER allow all four; multi-copy atomicity — x86 & ARMv8 yes,
  POWER/old-ARM no → IRIW; C++→ARM instruction mapping `ldar`/`stlr`/`dmb` with a
  real per-op cost unlike x86; "passes on x86" ≠ correct) · `std::atomic_ref`
  (C++20 — a temporary atomic view over a plain object; `required_alignment`;
  all-access-via-the-ref rule) · **lock-free / wait-free / obstruction-free**
  (exact progress guarantees; "no OS lock / alloc / blocking syscall on the
  shared path"; `is_lock_free()` vs "the algorithm is lock-free"; helping;
  **lock-free ≠ fast**) · **ABA** (CAS checks value equality, not "nothing
  changed"; the Treiber-pop A→B→A interleaving; needs a recycled resource; fixes:
  tagged/versioned word `{idx:32,tag:32}` in one `uint64_t` — lock-free
  everywhere, no DWCAS — reclamation schemes, or monotonic-counter designs; tag
  width; non-pointer ABA) · **litmus tests** (SB / MP / LB / IRIW — the minimal
  shapes real ordering bugs take, which reordering each needs, and which memory
  order kills it; x86-TSO forbids all but SB). 8 examples. **Measured & taught,
  not hidden (CLAUDE.md Rule 2): non-atomic counter ~3M / 16M WRONG (`volatile`
  only to defeat `-O2` coalescing); store buffering observed on x86 —
  `r1==r2==0` for `relaxed` ~70–330/200k, `release/acquire` ~1000–4200/200k (rel/
  acq shows *more*, not fewer — it does not stop store→load), `seq_cst` always 0;
  store `relaxed`/`release` ~0.72 ns vs `seq_cst` ~13 ns (~18×), load ~0.74 ns
  and `fetch_add` ~13 ns at *every* order, contended `fetch_add` ~27 ns/op;
  absolute ns drift with machine state — the *ratios* are the lesson; MP + IRIW:
  0 bad reads / 0 disagreements at every order on x86 (expected — x86 forbids
  every litmus reordering except SB and is multi-copy-atomic).**
- **Lock-free programming (folder 28)** — 14 lessons: **why lock-free** (a
  contended `std::mutex` = a futex syscall + 2 context switches + the lock-word
  cache line bouncing + an unbounded stall if the holder is preempted → a P99/
  P99.9 spike; **priority inversion / convoying / deadlock are structurally
  impossible** without a held lock; but **lock-free ≠ fast** — measured) · **the
  rules** (no OS lock / `new` / `malloc` / blocking syscall on the shared path;
  pre-allocate pools & rings; a deliberate memory order per shared access;
  ABA-proof any CAS on a recycled value; "helping"; trivially-copyable payloads
  or publish a pointer; the shapes that actually work) · **cache-line padding &
  false sharing** (MESI line-bouncing on independent writes; measured ~3.5–4× on
  a pure SPSC-index-pair case; `std::hardware_destructive_interference_size` + the
  `-Winterference-size` / ABI gotcha → hardcode 64/128; `alignas` field + a
  trailing pad; **the key nuance — padding `head_`/`tail_` apart does *not* speed
  up an SPSC ring if you still `acquire`-load the opposite index every op; you
  must cache the opposite index**) · **SPSC ring buffer** (a bounded power-of-two
  ring; the producer owns `head_` — writes it `release`; the consumer owns
  `tail_` — writes it `release`; each side `acquire`-loads the other; **no CAS,
  no mutex**; wait-free in practice; **no ABA** — the indices are monotonic
  counters, never reused; N−1 usable slots to tell full from empty; enforce
  exactly one producer + one consumer) · **SPSC optimizations** (power-of-two
  mask instead of `%`; cache-line padding; **the cached opposite index** — a
  producer-local copy of `tail_`, only reload the real one when the cache says
  full → steady-state the producer never touches the consumer's line; batching
  one `release` store per burst; a zero-copy `prepare`/`commit` slot API) ·
  **Vyukov bounded MPMC queue** (each cell carries its own atomic `seq` turnstile
  — `seq == pos` free, `seq == pos+1` has data, `seq == pos+N` freed for the next
  lap; the shared `enq_`/`deq_` position advances with a `relaxed` CAS, the data
  ordering lives on the cell's `release`/`acquire` `seq`; monotonic positions →
  no ABA, no allocation; lock-free not wait-free — contended producers retry) ·
  **the Michael-Scott queue** (an unbounded linked FIFO; a permanent dummy node;
  `tail_` can lag by one → **helping** — any thread that sees a lagging `tail_`
  swings it forward, completing a stalled enqueuer's operation; the two hard
  problems it introduces — ABA and reclamation; usually the wrong tool for the
  hot path) · **the Treiber stack** + ABA-in-practice (the smallest lock-free
  structure — one atomic head, push/pop each a CAS loop; the pop/pop/push
  interleaving that corrupts an untagged stack; the tagged-head fix —
  `{idx:32,tag:32}` in a `std::atomic<uint64_t>`, tag++ every push — lock-free
  everywhere, no `cmpxchg16b`; tag width) · **the memory reclamation problem**
  (a thread holds a raw internal-node pointer loaded *before* the unlink →
  use-after-free if you free too soon; the schemes — fixed pool / never-free
  (the HFT default, + ABA tags), atomic per-node refcount, hazard pointers,
  epochs/RCU, QSBR; why `std::atomic<shared_ptr>` is wrong on a hot path —
  usually not lock-free, an RMW per node) · **hazard pointers** (each thread
  publishes the node it's about to touch and re-validates it's still linked; a
  reclaimer scans all hazard slots and skips any listed node; **bounded garbage**
  `≤ T·R`; a stalled thread pins at most `K` nodes) · **epoch-based reclamation /
  RCU / QSBR** (near-free read side — enter/exit a tiny read section, or a
  quiescent point at the top of a poll loop; the reclaimer frees a batch only
  after a grace period — "every thread that could have had a reference has moved
  on"; needs 3 epochs; **the failure mode — one stalled reader pins *every*
  retired node → unbounded retained memory**, so pinned threads + microsecond
  read sections + a watchdog; copy-swap publication — never mutate a live object)
  · **seqlock** (a 1-writer/N-reader consistent snapshot — the writer bumps
  `seq` to odd, writes the payload, bumps `seq` to even with a `release`; the
  reader brackets its copy with two `seq` reads + an `acquire` fence and retries
  if they differ or `seq` is odd; the payload copy is *formally* a data race →
  use `relaxed` atomics / `atomic_ref` for the fields; multiple writers →
  serialize them separately or shard one seqlock per symbol; payload size = the
  retry window; **THE HFT market-data snapshot primitive** — measured **~80–100×
  faster reads than `std::shared_mutex`**, torn = 0) · **testing lock-free code**
  (the pyramid — single-thread unit tests → invariant stress (oversubscribed,
  `-O2`/`-O3 -march=native`, structural invariant: multiset-of-popped ==
  pushed / torn == 0 / sum == count) → **TSan** (finds missing acquire/release
  and payload races; *silent* on ABA) → **model checkers** (herd7 / GenMC /
  CDSChecker — exhaustive over a small kernel, the *only* thing that *proves* the
  ordering; 2–4 threads is where the bugs are) → **forced-interleaving
  regression tests** (a `phase` handshake reproduces an ABA deterministically);
  "passes on x86" proves nothing about the memory model — add an ARM CI leg) ·
  **when NOT to go lock-free** (the honest costs — correctness risk, a whole
  reclamation subsystem, retry storms, portability, hard to compose; when a
  `std::mutex` is the right answer — low/bursty contention, a short cache-friendly
  critical section, a genuine multi-step transaction, correctness > the last µs;
  **sharding** — K low-contention shards, each a plain mutex, beats one clever
  lock-free structure for most N→M problems; prefer a vetted library — folly /
  TBB / liburcu — over hand-rolling; keep the mutex baseline as a test oracle +
  a production fallback; the decision checklist). 7 examples. **Measured & taught,
  not hidden (CLAUDE.md Rule 2): SPSC ring naive ~19 M msg/s / ~52 ns/msg; the
  optimization ladder — V0 naive ~25–27 ns/msg → V1 +cache-line padding ~24–32
  ns/msg (≈ noise / sometimes *worse* — the producer still reloads `tail_` every
  push, so padding alone doesn't remove the cross-core read) → V2 +cached
  opposite index ~16–20 ns/msg (~1.3–1.6×, the real win); Vyukov MPMC ~1.6× vs
  `mutex + std::deque`; ⚠️ a correct, ABA-safe, contended lock-free Treiber stack
  ~7 M op/s vs `mutex + std::vector` ~40 M op/s — lock-free ~5× SLOWER (retry
  storm on one hot head; the mutex baseline is tiny and cache-friendly);
  seqlock ~180 M reads/s vs `std::shared_mutex` ~2 M (~80–100×), torn = 0; SPSC
  hand-off p50 ~0.4 µs / ~15 M msg/s vs `mutex+queue` ~1.2 µs and `mutex+cv` ~6 µs
  (a futex wake per message) — here lock-free clearly wins; pure false sharing on
  an index pair ~3.5–4× (adjacent ~24 ns/op vs `alignas(64)` ~6 ns/op).**

- **Linux systems programming (folder 29)** — 18 lessons + 10 `*.linux.cpp`
  examples: kernel vs userspace / ring 0-3; **syscall cost** (`syscall` instr →
  ~300 ns fixed overhead with KPTI/retpoline; **vDSO** serves `clock_gettime` in
  ~20 ns with no trap; ratio syscall:userspace ~100–300×; count-reduction via
  buffering / `mmap` / shm / `recvmmsg` / `io_uring` / busy-poll); `fork` (COW
  page-table copy), `exec` (image replace), `wait` (zombie/orphan), `clone`
  flags, `posix_spawn`/`pidfd`; **signals** — async-signal-safety (only `write` +
  atomics + `_exit`), "flag + main-loop" pattern, `signalfd`, hot-thread signal
  masking; **file descriptors** — 3-level table, short read/write loops,
  `O_CLOEXEC`, `dup2`/pipe redirection, `RLIMIT_NOFILE`, `writev`; `/proc` &
  `/sys` (introspection + sysctl tuning); **`mmap`** — file/anon × shared/private,
  lazy first-touch fault, `MAP_POPULATE`, TLB shootdown on `munmap`; **shared
  memory** — `shm_open` + `mmap`, cross-process SPSC ring (folder-28's ring in
  `/dev/shm`/hugetlbfs, no shared locks), layout rules (offsets not pointers);
  pipes/FIFO/UDS — `SCM_RIGHTS` fd passing, latency shm ~ns vs UDS/pipe ~µs vs
  loopback TCP ~10s µs; **CPU scheduling** — CFS/EEVDF (`vruntime` + `nice`),
  `SCHED_FIFO` hazards (starvation / priority inversion / `sched_rt_runtime_us`
  throttle), "isolated core + CFS busy-poll often matches/beats FIFO with less
  risk"; **CPU affinity** — `sched_setaffinity`, `isolcpus`/`nohz_full`/
  `rcu_nocbs`/`irqaffinity`, SMT sibling idling, topology-aware pinning, startup
  self-check; **huge pages** — TLB reach, THP vs hugetlbfs, `khugepaged`
  collapse stall; **page faults & mlock** — minor/major/COW cost, `mlockall` +
  pre-fault + dry-run ⇒ **zero page faults in steady state**, `getrusage`
  counts; **NUMA** — first-touch placement + the "init thread allocates
  everything" bug, `numactl`/`mbind`, `numa_balancing=0`, NIC's node; **IRQ
  affinity** — hard IRQ → softirq, `smp_affinity`, `irqbalance` off, coalescing;
  **clocks** — `CLOCK_MONOTONIC` (vDSO) vs `_RAW` (trap) vs `_COARSE`,
  `rdtscp` + calibration + invariant-TSC flags, `clocksource=tsc`, busy-spin to
  a deadline for sub-µs "act at T"; **cgroups & rlimits** — `RLIMIT_MEMLOCK/
  NOFILE/RTPRIO`, `cpu.max` bandwidth throttle = latency spike, container-
  awareness (`hardware_concurrency()` lies); **HFT tuning checklist** — BIOS
  (SMT/turbo/C-states) → kernel cmdline (`isolcpus`/`nohz_full`/`mitigations=off`/
  `clocksource=tsc`/hugepages) → sysctl → runtime (governor / IRQ / NIC) →
  systemd unit → app-side (mlock + warm-up + pin) → **verify by effect**
  (`cyclictest`/`turbostat`/ctx-switch counts), not by config string. Examples
  don't compile on this MinGW box (`SKIP (linux-only)`); numbers in each
  `EXPECTED` block are typical / "not measured on your machine" (Rule 2).

- **Networking (folder 30)** — 16 lessons + 10 `*.linux.cpp` examples:
  OSI/TCP-IP + **encapsulation + wire overhead** (a 40-byte order → ~98-byte
  frame → ~138 bytes wire → ~110 ns at 10G — the serialization floor); **IP** —
  subnets/routing, MTU vs MSS, **fragmentation** (one lost fragment = whole
  datagram lost), **PMTUD black hole**, ARP (static entries for HFT peers);
  **TCP deep** — 3-way handshake (1 RTT), seq/ack/sliding-window, **RTO ~200 ms
  minimum vs 3-dup-ACK fast retransmit**, SACK, `cwnd` vs `rwnd`, TIME_WAIT +
  `SO_REUSEADDR`; **TCP latency issues** — the **Nagle + delayed-ACK ~40 ms
  deadlock** (fix: `TCP_NODELAY` on both ends + one `writev`/msg), `TCP_CORK`,
  `tcp_slow_start_after_idle=0`, head-of-line blocking; **UDP** — why market
  data is UDP (stale data + retransmit-blocks-newer + one-to-many + predictable
  latency), sequence-number gap/reorder/dup handling, **A/B redundant feeds**
  (fill most single-path loss with zero round-trips), snapshot / retransmit
  recovery off the hot loop, silent kernel drops (`/proc/net/udp`, big
  `SO_RCVBUF` + `net.core.rmem_max`), `recvmmsg` batching, `connect()`ed UDP;
  **multicast** — `IP_ADD_MEMBERSHIP` = kernel filter + IGMP report, IGMP
  snooping failure modes, explicit `imr_ifindex` on multi-NIC, SSM (`232/8`),
  TTL/LOOP, **A/B arbitration by sequence**; **sockets API** — call sequences,
  `recv` 0 (FIN) vs <0 + errnos, **framing** (length prefix — TCP is a byte
  stream), `getaddrinfo` at startup, `SIGPIPE`/`MSG_NOSIGNAL`, non-blocking
  `connect` + `SO_ERROR`; **blocking vs non-blocking** — `EAGAIN` on recv vs
  send, **busy-poll** on an isolated core, `SO_BUSY_POLL` (kernel spins then
  sleeps), ET draining; **select/poll** — O(N) copy+scan every call, `FD_SETSIZE`
  = 1024, why `epoll` exists; **`epoll` deep** — O(ready), level vs
  edge-triggered (drain to `EAGAIN`), event-loop skeleton, `EPOLLOUT` add/remove
  discipline, `EPOLLONESHOT`/`EXCLUSIVE`/`RDHUP`, `timerfd`/`signalfd`/`eventfd`
  in one loop; **socket options** — the HFT short list, `SO_RCVBUF` cap + 2×
  readback, `SO_REUSEPORT`, keepalive + `TCP_USER_TIMEOUT` (fail a stall in
  seconds), `SO_LINGER {1,0}`; **zero-copy** — `sendfile` (+`TCP_CORK` header),
  `splice` for capture/replay, `MSG_ZEROCOPY` (pin + ERRQUEUE, ~10 KB
  threshold), **`io_uring`** (SQ/CQ rings, SQPOLL = zero syscalls); **kernel
  bypass** — DPDK / OpenOnload / ef_vi / VMA / **AF_XDP** (~1–5 µs kernel RX
  path → ~100–300 ns; what you take on: your own protocol/TCP, a spinning core,
  hugepages, NIC seizure, `tcpdump` blind; the adoption ladder tune → busy-poll
  → AF_XDP → DPDK); **timestamping** — `SO_TIMESTAMPING` (HW/SW RX+TX), `recvmsg`
  cmsg parse, TX stamps on the error queue, **clock domains** (discipline or
  jitter-only), **PTP** (`ptp4l` disciplines the NIC PHC, `phc2sys` the system
  clock; boundary/transparent-clock switches), MiFID II; **network tuning** —
  NIC RX rings, coalescing min/off, **GRO/LRO off**, RSS queues + pinned IRQs,
  RPS off for hot queues, pause frames off, `pfifo_fast`/`mq` qdisc, sysctl,
  persist + verify-by-drop-counters; **building servers** — the **V1 blocking
  echo → V2 epoll echo → V3 UDP-multicast feed handler** progression, each
  version removing the previous one's bottleneck (single-connection → sleep/
  wakeup → syscall), mapped onto the real HFT box (feed handler / order gateway
  / control plane / housekeeping). Same Rule 2 treatment of the loopback numbers.

- **CPU architecture (folder 31)** — 15 lessons + `16-exercises.md` + 8
  **portable** `.cpp` examples (SIMD + `rdtsc` + CPUID run on x86-64 incl.
  MinGW/Windows; 8/8 OK, benchmarks measured at `-O2`):
  fetch-decode-execute-retire + clock/cycles vs ns; **registers** — x86-64 GP
  sub-register scheme, `rflags`, SIMD `xmm`/`ymm`/`zmm`, architectural vs
  physical + **register renaming** (kills WAW/WAR, not RAW), register spilling,
  `mxcsr` flush-to-zero / denormals-are-zero; **ISA** — x86-64, CISC front-end /
  RISC µop back-end, macro-fusion (`cmp`+`je` → 1 µop) / micro-fusion,
  **microcode** (`div` ~20–45 cyc, denormal assists ~100+ cyc, `gather`,
  transcendentals), `-march=x86-64-v2/v3/v4`, `lea` for non-address arithmetic;
  **pipelining** — latency vs throughput (the core distinction), the 4 hazard
  types (data/control/structural/memory), flush + refill ~15–20 cyc; **superscalar**
  — execution ports, ILP, IPC (what values mean latency-/throughput-/memory-bound),
  the limiting-port analysis via `llvm-mca`; **out-of-order** — ROB / register
  renaming / reservation stations, speculation + rollback, the OoO window (ROB
  size = how far ahead it looks), **why a dependent chain of cache misses
  (pointer chasing — linked list, `std::map`) can't be hidden**; **branch
  prediction** — static → 2-bit counter → gshare → TAGE, BTB / RAS,
  indirect/virtual/`switch` dispatch, `[[likely]]`/`[[unlikely]]`, predictor
  warm-up; **speculative execution** — Spectre/Meltdown in one paragraph
  (speculate an illegal access, leak via a cache-timing side channel; µarch
  state isn't rolled back), KPTI (~2–3× slower syscalls) / retpoline / `lfence`
  mitigations, **`mitigations=off`** trade-off + the exact preconditions
  (single-tenant, not internet-facing, trusted, policy OK) + why it barely helps
  a pure busy-poll hot loop; **instruction latency/throughput** — the two
  numbers precisely, representative x86 table, Agner Fog / uops.info / `llvm-mca`,
  **computing the critical path** (longest chain of dependent latencies = the
  floor), the division problem (constant → compiler reciprocal-multiply,
  loop-invariant → hoist `1.0/d`, struct-time-known → `libdivide`, power-of-two →
  shift); **SIMD basics** — SSE2 (baseline) → AVX → AVX2+FMA (`x86-64-v3`) →
  AVX-512, what SIMD is good at (map/reduce/filter) and bad at (per-element
  branch, gather, cross-lane, short arrays), **SoA layout as the prerequisite**,
  aligned vs `loadu`, tail handling, the AVX-512 frequency downclock; **SIMD
  intrinsics** — `_mm256_*` naming (`_mm<width>_<op>_<type>`), load/store/set1/
  arith/`cmp`→mask/`blendv`/`movemask`/shuffle/horizontal-reduce, `_MM_SHUFFLE`
  reversed-order gotcha, `__attribute__((target("avx2")))` + CPUID runtime
  dispatch, auto-vec vs manual vs Highway/xsimd; **hyperthreading (SMT)** —
  shares execution ports / L1 / µop cache / TLB / branch predictor and halves
  the OoO window → a hot thread's timing depends on the uncontrolled sibling =
  jitter, fat p99 → **HFT disables SMT** (BIOS / `nosmt`) or keeps it on with
  each hot core's sibling isolated + idle; non-latency work keeps SMT for
  ~1.1–1.3× throughput; size compute pools to *physical* cores; also closes
  cross-sibling side channels; **frequency & power** — P-states / turbo
  (opportunistic → non-deterministic → some HFT disable turbo for a *guaranteed*
  base frequency), C-states (deep-idle wake = **+tens–100 µs latency + cold
  cache** → busy-poll or `processor.max_cstate=1` on latency cores), thermal
  throttling (silently defeats a locked frequency; monitor `PkgTmp`), the
  AVX/AVX-512 frequency offset (can make a 4× SIMD loop a net loss on
  Skylake-X/CLX), uncore/mesh frequency (pin high); **NUMA hardware** — sockets
  + own memory controller + **UPI / Infinity Fabric** interconnect (remote DRAM
  ~1.5–2.2× + shared-bandwidth), **chiplets/CCX (AMD) and Intel Sub-NUMA
  Clustering = NUMA-like tiers within one socket**, the full L1 → L2 → L3-local
  → L3-remote → local-DRAM → remote-DRAM latency ladder, the NIC's NUMA node,
  cross-socket false sharing; **CPU differences** — Intel vs AMD (per-CCX L3 not
  shared across CCXs, `pdep`/`pext` microcoded ~18 cyc on Zen 1/2 vs ~3 on
  Intel/Zen 3+, different `perf` events, chiplet NUMA), ARM64 / Graviton (weak
  memory model → re-audit every atomic + ARM CI, NEON/SVE, `cntvct_el0` not
  `rdtsc`), **"which box?" is the first question of any benchmark or tuning
  decision** — build for the deployment floor + CPUID assert/dispatch, benchmark
  on the production box frequency-locked + SMT-off + C-states-off, re-benchmark
  on every hardware refresh.
  **CLAUDE.md Rule 2 in action:** examples `01`/`03` first showed *no effect* —
  `-O2` computed the ADD/DIV dependency chains in closed form / DCE'd them
  (`01` ADD latency printed `0.000`) and if-converted `if (x >= thr) s += x` to
  a branchless `cmovge` (`03` RANDOM == SORTED). Fixed with `keep()` inline-asm
  optimization barriers (`asm volatile("" : "+r"(v))`, the Google-Benchmark
  `DoNotOptimize` technique — zero instructions emitted), `#pragma GCC
  optimize("no-if-conversion", ...)`, and a threaded `carry` argument to defeat
  loop-invariant call hoisting. The "for a simple predicate the compiler already
  does the branchless transform" observation is **taught as the lesson** (files
  03, 07), not hidden. **Measured (AMD Ryzen 7 4700U, Zen 2, ~2 GHz throttled —
  shapes/ratios, NOT production absolutes; files 13/15 explain why):** serial
  `imul` chain vs 4 independent chains **~4×** (ILP; 8 chains give nothing more —
  multiplier port saturated); unpredictable branch **~6–7×** slower than
  predictable (RANDOM ~4–8 ns/elem vs SORTED ~0.5–1.4 ns/elem); branchless
  **~6–7×** faster than branchy on random data, but **~1.2× *slower*** on
  predictable data (the honest "when NOT to go branchless"); float sum
  scalar 1.66 ns/elem → SSE **4.1×** → AVX2 **~9.5×** (L2-resident = compute-
  bound; a >L3 dataset converges to ~1× as it becomes bandwidth-bound —
  measured); same map function auto-vectorized **~2.5–3×** vs
  `no-tree-vectorize`; `rdtsc` self-cost ~20 cyc vs `lfence;rdtsc;lfence` /
  `rdtscp;lfence` ~40–90 cyc; calibrated ~2.0 cycles/ns (~2.0 GHz effective).

- **Cache & memory performance (folder 32)** — 15 lessons + `16-exercises.md` +
  8 **portable** `.cpp` examples (all run on x86-64 incl. MinGW/Windows; 8/8 OK
  under strict flags) + `09_perf_analysis.sh` (Linux `perf` workflow):
  **memory hierarchy** — registers → L1 → L2 → L3 → DRAM → NVMe → HDD, the
  "if L1 = 1 second then DRAM = 1.5 minutes, SSD = 14 hours" analogy, this box's
  geometry (L1d 32 KiB 8-way / L2 512 KiB / L3 8 MiB per CCX), a `load`'s full
  path (TLB → L1 → L2 → L3 → DRAM), "same big-O, 100× speed" is the whole point;
  **cache lines** — 64 B = the unit of transfer *and* of coherence, why 64
  (DRAM burst + spatial-locality bet + tag overhead), line straddle = 2 misses/
  object, `std::hardware_destructive_interference_size` (+ the MinGW guard),
  `alignas(64)` + `sizeof` a factor of 64 + `static_assert`; **cache
  organization** — direct-mapped → N-way set-associative, address = tag | set
  index | block offset, pseudo-LRU / RRIP, **critical stride = cache size /
  associativity** (why power-of-two array dimensions and 4096-B strides thrash a
  single set), VIPT + page coloring; **the 3 C's** — compulsory / capacity /
  conflict + coherence (4th), a symptom → cause diagnosis table (pass-1-vs-2,
  working-set-vs-cache, thread-scaling), **MLP** = ~10 line-fill buffers → the
  OoO engine overlaps *independent* misses (~2.5× seen) but a *dependent* chain
  (pointer chasing) gets the full latency serially; **locality** — spatial /
  temporal, loop interchange / fusion / fission / tiling, the prefetcher's
  favourite (monotonic stride in a page) and least-favourite (indirect, pointer
  chase, cross-page random) patterns, working-set thinking; **prefetching** —
  HW prefetchers (next-line / adjacent / stride / region) + their limits,
  `__builtin_prefetch(addr, rw, locality)` + distance tuning, `PREFETCHNTA`;
  ⚠️ **Rule 2**: SW prefetch on a big OoO core is **~1.1×** on an independent
  gather (MLP already there) and **~0.33× = 3× SLOWER** on a memory-saturated
  loop (prefetch requests contend for LFBs/bandwidth) — "a scalpel, not free
  money" (wins on in-order / narrow cores, or a verified MLP deficit); **false
  sharing deep** — MESI ping-pong on independent writes to one line, `perf c2c`
  (HITM = the signature), fixes (padding / per-thread-local + combine / structure
  splitting), constructive sharing (the other direction); **measured ~6× to
  ~44×, run-to-run** — false sharing is *jittery*, not just slow (the variance
  *is* the lesson); **cache-friendly data structures** — flat vs pointer-based,
  `std::map` ~20 serial misses/lookup vs `absl::btree_map` ~3–4 vs
  `absl::flat_hash_map` ~1, `std::list`/`std::unordered_map` (per-node malloc,
  scattered) vs arena + `uint32` index links, CSR vs adjacency lists, SBO types;
  **AoS vs SoA deep** — the three access patterns → three winners (scan-few-
  fields **SoA ~2.0×**, scan-most-fields **SoA ~2.4×** — the gap doesn't shrink
  because SoA vectorizes and AoS's 32-B stride doesn't, random whole-record
  **AoS ~3×** because it's 1 line vs 8), AoSoA hybrid, SoA as the vectorization
  prerequisite; **data-oriented design** — "the purpose of the program is to
  transform data", the `std::vector<Base*>` + `virtual` hot-loop cost breakdown
  (pointer chase + vtable load + indirect branch + no SIMD, per element),
  existence-based processing, handles + generation counters, "group by what you
  do not what things are", and where OOP still fits (cold code, singular state);
  **TLB & huge pages** — the 4-level page walk (up to 4 dependent memory
  accesses), L1 dTLB / L2 STLB reach (4 KiB → ~256 KiB / ~6 MiB), **2 MiB huge
  pages → one TLB entry covers 512 × 4 KiB → 512× reach** + a shorter walk,
  explicit hugetlbfs (`MAP_HUGETLB`) + pre-fault + `mlockall` vs THP downsides
  (`khugepaged` / compaction jitter → HFT uses explicit), Windows large pages,
  NUMA first-touch; **store buffers** — RFO (a write to a cold line reads it
  first → `memset` ≈ read bandwidth), store-to-load forwarding + its stalls
  (size/align mismatch), write-combining buffers, **non-temporal stores**
  (`_mm256_stream_si256` + `_mm_sfence()` — no RFO, no cache pollution, for
  write-only bulk > LLC); **memory bandwidth** — latency- vs bandwidth-bound,
  Little's Law (concurrency = BW × latency ≈ 56 lines → one core with ~10 LFBs
  gets ~18 % of socket peak), STREAM copy/scale/add/triad, the **roofline
  model** (arithmetic intensity = FLOP/byte; left of the ridge = memory-bound →
  "SIMD is wasted effort, reduce bytes / passes"), the add-threads test
  (bandwidth is shared, MLP is per-core); **measuring** — `perf stat` + MPKI,
  `perf stat --topdown` / toplev (Retiring / Bad-Spec / Frontend / Backend →
  Memory Bound), `perf record -e cache-misses` + `perf annotate`, `perf c2c`,
  `perf mem`, cachegrind (simulated, no root, relative only), VTune / AMD uProf /
  `likwid`, a 7-step workflow; **11 optimization recipes** in impact order
  (measure → loop interchange → shrink working set → hot/cold split → SoA/AoSoA →
  kill pointer chasing → blocking → align/pad shared → huge pages → NUMA → NT
  stores → SW prefetch last), each with its explicit trade-off, and the
  apply-one-fix-then-re-measure discipline.
  **CLAUDE.md Rule 2 fired three more times, all taught not hidden:** (a)
  example `03` — `-O2` column-major traversal **~10× slower**, but at
  `-O3 -march=native` GCC's `-ftree-loop-interchange` swaps the loop nest →
  ratio **~1.0** (the compiler fixed the cache-hostile loop — but only because
  it's a simple provably-safe perfect nest); (b) example `06` — `__builtin_
  prefetch` on a light gather **~1.1×**, on a heavy bucket-scan **~0.33×
  (3× slower)**; (c) example `07` — a naive 64×64 blocked matmul is
  **~10–15 % *slower*** than a plain `ikj` loop order (ikj already
  auto-vectorizes + L3 absorbs the reuse; blocking needs a tuned register-tiled
  microkernel to pay). **Measured (AMD Ryzen 7 4700U, Zen 2, ~2 GHz throttled —
  shapes/ratios, NOT production absolutes):** sequential stride ramp ns/access
  ~0.32 (1 B) → ~3.4 (64 B) → ~5.4 (128 B), knee smeared 64→128 B by the HW
  prefetcher; sequential vs random 64-B line access **~7×** (13.8 vs 2.0 GB/s),
  random still < true ~80 ns because MLP overlaps ~10 independent misses;
  row- vs column-major 4096² traversal **~10×** at `-O2`; false sharing
  **~6× to ~44×**, run-to-run (padded ~0.2 ns/inc stable); AoS vs SoA — SoA
  **~2.0–2.4×** on sequential scans, AoS **~3×** on random whole-record access;
  pointer-chase latency cliff **~1.2 ns (1 page) → ~15 ns (4 MiB) → ~95 ns
  (8 MiB+, DRAM)**. Mojibake sweep 139 → 0.

- **Compiler optimization (folder 33)** — 15 lessons + `16-exercises.md` +
  8 examples (6 portable `.cpp` → `./build.ps1 folder` 6/6 OK, + `06_lto_demo/`
  multi-file `.cxx`, + `07_pgo_workflow.sh`): **`-O` levels** (`-O0`→`-O1`→`-O2`
  →`-O3`→`-Os`→`-Ofast`; the **`-O0`→`-O1` ~6× cliff** measured — 80 ms → 13.4 ms
  — and `-O1`..`-O3`..`-Os` equal for a scalar-reduction workload; per-file/
  -function override via `#pragma GCC optimize` / `optimize` attribute; `-g` on
  every release build; `gcc -O2 -Q --help=optimizers` to see what's on) ·
  **godbolt / offline workflow** (the 7-item hot-asm checklist; `-fopt-info-vec
  [-missed]` / `-fopt-info-inline`; `objdump`; `llvm-mca`; per-file `.s` diff in
  the PR) · **inlining** (`inline` = ODR permission NOT "inline me"; the size/
  call-count/hotness heuristic; `[[gnu::always_inline]]` / `noinline` / `flatten`;
  the **cross-TU / no-LTO wall** — a hot function in another `.cpp` can't be
  inlined; inlining is the *enabler* of const-fold, CSE, DCE, vectorization and
  devirtualization across the function boundary — that's the real value, not the
  call/ret cost; measured noinline **1.31 ns/iter** vs inlined **~1.0**; over-
  inlining → I-cache bloat → slower) · **loop optimizations** (loop-invariant code
  motion + how aliasing blocks it; **strength reduction** (`i*4`→`i<<2`, `x/const`
  → reciprocal-multiply, address IVs → running pointers); **unrolling** — the
  benefits (overhead amortize, ILP with N accumulators) and the **"one
  accumulator → serial chain, no ILP gain"** trap, plus I-cache/spill cost;
  fusion / fission; **interchange** (`-ftree-loop-interchange`, `-O3`-only — don't
  rely on it); unswitching; rotation; induction-variable elimination; the blocker
  list + `-fopt-info-loop`) · **auto-vectorization** (the exact conditions —
  countable trip count, no loop-carried dep, contiguous access, no aliasing
  doubt, no calls, simple body; **a `float` reduction (`s += a[i]`) does NOT
  vectorize at `-O2`** — partial sums = reassociation = different rounding, which
  the compiler won't do without `-ffast-math` / `-fassociative-math` / `#pragma
  omp simd reduction` — measured **~4×** with `-ffast-math` vs a plain MAP loop's
  **~3.5×** at plain `-O2`; PREFIX never vectorizes (true dependency); `-fopt-info
  -vec-missed` literally says "possible aliasing" / "control flow in loop" /
  "relevant stmt not supported"; `#pragma GCC ivdep` / `assume_safety` = unchecked
  promises → UB if wrong) · **constant folding / propagation / DCE** (whole
  functions collapse to `mov eax, <const> ; ret`; inter-procedural constant
  propagation + `-fipa-cp-clone`; `constexpr` vs `consteval` vs `constinit` vs
  `if constexpr` (the last isn't even compiled for the false branch); `[[assume]]`
  / `std::unreachable` as unchecked facts; **`constexpr` lookup tables** built by
  the compiler; and this is *why* benchmarks need barriers) · **devirtualization**
  (fires on: exact type known / `final` class or method / speculative-via-LTO
  (`-fdevirtualize-speculatively`) / anonymous-namespace types / PGO; the honest
  **"it will NOT fire for a heterogeneous `std::vector<Base*>` iterated in a hot
  loop"** — that's the DOD anti-pattern; the fix is `variant`+`visit` / CRTP /
  tag-`switch` / batching by type — measured elsewhere virtual ~23 ns vs CRTP
  ~2.2 ns) · **branch hints** (`[[likely]]`/`[[unlikely]]`/`__builtin_expect` =
  **code LAYOUT, not prediction** — the HW predictor already nails a strongly-
  biased branch; measured effect **~1.3×** in a micro-loop (the cold `slow_path`
  moved out-of-line); matters in a *big* function with many rare checks
  (I-cache); a **wrong hint inverts the layout = regression**; `[[assume]]` is a
  fact not a hint (false → UB); **PGO does all this from real data** and is
  better for anything beyond obvious error paths) · **aliasing & `__restrict`**
  (how "two `T*` might overlap" blocks LICM, CSE, hoisting and vectorization —
  `*scale` reloaded every iteration, loop stays scalar; **TBAA / strict aliasing**
  (`-fstrict-aliasing` on at `-O2`) — incompatible types assumed not to alias,
  and **type-punning via `*(int*)&f` is UB that bites at `-O2`** → use
  `std::bit_cast` / `memcpy` (free); `T* __restrict` = an unchecked promise the
  pointers don't overlap; alternatives — inline + `-flto` (proves it from the
  call site), loop versioning (compiler auto), **local-copy of the invariant
  scalar** (safe manual LICM); **measured ~3.5×** (may-alias 0.43 → `__restrict`
  0.12 ns/elem) — but only because the example functions are `[[gnu::noinline]]`;
  **inlined, the compiler proves non-aliasing itself and the gap vanishes** —
  that IS the lesson: LTO / header-only hot code solves aliasing for you) ·
  **LTO** (compile-time IR in the `.o` + a link-stage optimizer over the merged
  program → cross-TU inlining / constant propagation / speculative
  devirtualization / whole-program DCE / ICF; full LTO vs **ThinLTO** (per-TU
  summaries, ~10× faster link, ~90% of the benefit); all TUs must build `-flto`
  with **consistent flags** (a `-O`/`-march`/`-ffast-math` mismatch → the linker
  picks one, silently); **LTO exposes latent ODR violations as miscompiles**
  (`-Wodr`); measured **~2.3×** on a *throughput* loop (`out[i] = f(in[i])`,
  cross-TU `call` inlined away) but **no gain on a *carried* loop** (`acc =
  f(acc^i)` — latency-bound on the hash critical path, the ~2-cyc call overlaps
  with the OoO engine) — a Rule-2 nuance; typical real-world gain ~2–10% on a big
  branchy binary, and it shrinks the binary) · **PGO** (the 3-step `-fprofile-
  generate` → run representative workload → `-fprofile-use` + `-fprofile-
  correction`; **AutoFDO** = sample a normal `-O2 -g` binary with `perf record -b`
  (no instrumented build, always-fresh); what it improves — branch layout,
  selective inlining, loop unroll from the observed trip-count distribution,
  function ordering / hot-cold splitting, speculative devirt on the dominant
  type; realistic gain **~5–20% on branchy/large code**, **~0–5% on a tight
  kernel**; the **representative-profile hazard** — wrong workload = pessimization
  (train on a quiet market → volatile open regresses); HFT = a replayed captured
  session, merge quiet + busy + burst, re-train on code changes) · **`-march` /
  `-mtune`** (`-march=X` may *emit* X's instructions (older CPU → `SIGILL`),
  `-mtune=X` only *schedules* for X (portable); **x86-64-v1/v2/v3/v4** feature
  bundles — v3 = AVX2 + FMA + BMI2 + `popcnt`/`movbe` = the pragmatic modern
  target; **`-march=native` is wrong for a shipped binary** (it's the build
  machine's CPU); AVX-512 → **frequency downclock** can net-lose system-wide,
  `-mprefer-vector-width=256` keeps the features and caps the width; **function
  multi-versioning** (`target_clones` + an IFUNC resolver — resolves once,
  steady-state = a direct call) for one binary across a heterogeneous fleet) ·
  **`-ffast-math` dangers** (it's a bundle of 8 sub-flags; **`-ffinite-math-only`
  compiles `std::isnan(x)` / `x != x` to `false`** → your NaN guards are deleted
  → a stray NaN silently corrupts every downstream `+`/`*` — real production
  incidents; `-fassociative-math` changes summation order / results;
  `-freciprocal-math`, `-fno-signed-zeros`, `-fno-trapping-math` each break
  something; `-Ofast` = `-O3 -ffast-math`; **`-ffp-contract=fast` (FMA) is ON by
  default at `-O2`** and makes `a*b+c` differ from a non-FMA reference; **NEVER
  `-ffast-math` on anything priced / reported / audited** — if a specific kernel
  needs SIMD reduction, write N named accumulators in the source (visible
  reassociation) or `#pragma omp simd reduction` scoped, and diff against a
  strict-IEEE reference) · **preventing optimization in benchmarks**
  (`DoNotOptimize(x)` = `asm volatile("" : "+r,m"(x) : : "memory")` — **zero
  machine instructions**, pure information; `ClobberMemory()` for writes;
  `volatile` sink works but adds a store/iter; `volatile` on the *accumulator* is
  wrong (measures memory round-trips); latency-vs-throughput → carried vs
  independent iterations; a per-iteration barrier in a throughput loop serializes
  it; measured **no-barrier `sum_squares(200M)` loop = 0.00 ms** (deleted) vs
  DoNotOptimize ~48 ms) · **reading optimized output** (the per-hot-function
  checklist — loop body size, vectorized (`ymm` + packed suffix) vs scalar, `call`
  in the loop (inlined? `-flto`?), `call [reg]` (devirtualize), branch vs `cmov`,
  `idiv`/`__divdi3` (constant divisor?), per-iteration reloads (aliasing),
  `[rsp+..]` spill churn (register pressure), cold code out-of-line, constants as
  immediates; **right-looking asm + unchanged time = you were bound elsewhere
  (memory)** — pair every asm review with a benchmark). **CLAUDE.md Rule 2 fired
  three times and is taught:** the float-reduction-won't-vectorize-at-`-O2`, the
  aliasing-pessimism-only-with-`[[gnu::noinline]]`, and LTO-helps-throughput-not-
  carried-loops.

- **Assembly (folder 34)** — 13 lessons (`01`–`12` + `13-exercises.md`) + 7
  examples (5 `.cpp` → `./build.ps1 folder` 5/5 OK, + `01_simple_functions.s`
  hand-annotated, + `07_asm_puzzles.md` — 10 "which C++ made this asm?"):
  **you read asm, you don't write it** — for **verification** (did `__restrict` /
  `inline` / `[[unlikely]]` / `-march` / `final` do what I asked? — each with a
  concrete "check for X in the asm"), **optimization** ("why is this loop slow?"
  → scan for `idiv`, a `call`, scalar where you wanted packed, spills, per-iter
  reloads, a long dependency chain), **debugging** (crash address → instruction →
  source line → what was in which register). Contents: **x86-64 registers** (16
  GP + the sub-register **zeroing rule** — writing `eax` zeroes `rax`'s top 32 —
  + `xmm`/`ymm`/`zmm` + `rflags` (ZF/SF/CF/OF and which `jXX` reads which)) ·
  **AT&T vs Intel** (the 5 mechanical differences — operand order, `%`/`$`
  prefixes, `disp(base,idx,scale)` vs `[base+idx*scale+disp]`, mnemonic vs
  operand size suffix; **`gcc -S` / `objdump` / `gdb` / `perf` default to AT&T**,
  godbolt / `./build.ps1 asm` to Intel) · **the ~20 common instructions**
  (`mov`/`movzx`/`movsx`/`lea` — **`lea` computes an address, never touches
  memory** — `add`/`sub`/`imul`/`and`/`or`/`xor`/`shl`/`shr`/`sar`, `cmp`/`test`
  = flags only, `jmp`/`jXX`/`call`/`ret`/`push`/`pop`, `cmovXX`/`setXX`
  branchless; recognize `idiv`/`div` (~20-40 cyc, non-constant divisor),
  `rep movs`/`stos` (a `memcpy`/`memset` idiom the compiler emitted for your
  loop), `lock`/`xchg`/`mfence` (atomics — folder 27)) · **addressing modes**
  (the `base + index*scale + disp` formula → **recover `sizeof(struct)` and field
  offsets from a loop's addressing**; a big constant added to the offset each
  iteration = strided/column access = a cache problem; `[rip + x]` = a global/
  `.rodata`; `fs:`/`gs:` = TLS/stack-canary) · **stack frames** (prologue/
  epilogue **with a frame pointer** (`push rbp; mov rbp,rsp; sub rsp,N`) vs
  **omitted at `-O2`** (`sub rsp,N` alone, one more usable register, but
  unwinding needs CFI); a leaf with no big locals has neither; **spills**
  (`mov [rsp+k],reg` / reload) = register pressure = over-unroll / big struct by
  value / too many accumulators; **tail call → `jmp` loop** (no stack growth) vs
  real recursion (`call` to itself); Win64 **32-byte shadow space**; the stack
  canary `fs:[0x28]` check) · **calling conventions** (**System V** — int args
  `rdi,rsi,rdx,rcx,r8,r9`, float `xmm0-7`, return `rax`/`xmm0`, callee-saved
  `rbx rbp r12-15`, 128-B red zone — **vs Windows x64** — args `rcx,rdx,r8,r9`
  (int OR float positionally), `rsi`/`rdi`/`xmm6-15` also callee-saved, 32-B
  shadow space, no red zone; **`this` = arg 1** (`rdi` SysV / `rcx` Win64);
  large-struct return via a hidden pointer arg (shifts everything); `extern "C"`
  changes the *name* not the convention; SysV varargs `al` = count of xmm float
  args) · **pattern recognition** (`if` (forward `jXX`) vs **if-converted `cmov`**
  (`-O2` for simple bodies — good for unpredictable branches, bad for predictable
  ones); a **loop** (backward `jXX` + entry guard from rotation + `add ptr,16/32`
  = vectorized + counter-replaced-by-pointer-compare = IV elimination + 4 loads
  in a row = unrolled ×4); `while (*p)` (no guard, or `call strlen`); **dense
  `switch`** (`jmp [rip + table + idx*8]` jump table) vs **sparse** (if-chain);
  direct `call name` vs indirect `call [reg+off]` (vtable / fn-ptr /
  `std::function`); constant-folded (`mov eax, 3628800 ; ret`) / empty (`ret`
  only = deleted); **reciprocal-multiply division** (`movabs <magic> ; imul ; sar`
  = `x / constant`, no `idiv`)) · **SIMD asm** (**scalar `ss`/`sd` vs packed
  `ps`/`pd`/`d`** = the one-glance "did it vectorize"; `xmm` also holds one
  scalar; width from `add ptr, 16` (SSE) / `32` (AVX2) / `64` (AVX-512);
  `vfmadd...ps` = FMA-vectorized; the compare→mask→`blendv` / `movemask`+`tzcnt`
  = a branchless per-lane filter / find-first; the `vextractf128`/`vshufps`/
  `vaddps` cluster after a loop = the horizontal reduction (once, not
  per-element); `vzeroupper` = required SSE/AVX hygiene; **`vgather` is a vector
  instruction that internally does N loads** ≈ lane-count cycles) · **inline asm**
  (the 4 sections — template / outputs / inputs / clobbers; constraints `r`/`m`/
  `i`/`a-d`/`S`/`D`/`x`/`v`, `=` write-only / `+` read-write; `"cc"` = flags,
  **`"memory"` = an optimization barrier (compiler, NOT a CPU fence)**;
  `volatile` needed for any side-effecting asm; **a missing clobber = silent
  `-O2`-only, register-allocation-dependent corruption**; the `DoNotOptimize`
  barrier is empty-template inline asm; **use inline asm only for the barrier /
  `cpuid` / `rdtsc` variants / `pause` / MSRs — everything else → intrinsics**
  (`<immintrin.h>`, `__builtin_*`) which are portable and the compiler schedules)
  · **`rdtsc` timing** (**invariant TSC counts reference ticks at the base rate,
  NOT core cycles** — turbo/throttle don't change the TSC rate but do change
  actual cycles; **`rdtsc` is not serializing** → fence with `lfence;rdtsc;
  lfence` or `rdtscp+lfence` for a short region (~20 ticks self-cost measured)
  vs plain `__rdtsc` (~1 tick, fine for long intervals); **calibrate** ticks→ns
  against `clock_gettime` (measured **~2.0 ticks/ns** on this ~2 GHz box);
  **core hopping** → cross-core TSC skew → nonsense deltas → pin the thread or
  check `rdtscp`'s `aux`; take the min of thousands of runs; **prefer
  `clock_gettime(CLOCK_MONOTONIC)` via vDSO (~20 ns, returns ns directly, no
  calibration) for anything ≥ 1 µs**) · **disassembly tools** (`g++ -S` = source
  → asm per-TU pre-link; **`objdump -dS -M intel`** = the shipped binary with
  real inlining/LTO, source-interleaved (needs `-g`); **`perf annotate` (`-e
  cycles:pp`)** = asm with per-instruction sample % — the #1 production-triage
  tool; **`gdb disassemble /s` + `info registers`** for crash analysis;
  **`addr2line -f -C -i`** = `prog+0x...` → `file:line` + inlined frames;
  **`llvm-mca`** = static pipeline / port-pressure / throughput estimate;
  `size`/`bloaty` for binary size). Measured (`05_rdtsc.cpp` / `06_inline_asm.cpp`,
  ~2 GHz Zen 2): calibration **~2.0 ticks/ns**, `__rdtsc` self-cost **~1 tick
  (~0.5 ns)**, `lfence;rdtsc;lfence` / `rdtscp+lfence` **~20 ticks (~10 ns)**, a
  100M dependent-LCG loop **~1.98 ticks/iter**, CPUID vendor **AuthenticAMD**,
  the `DoNotOptimize` barrier emits **zero instructions**. Note: examples
  `02`/`03`/`04` drop `keep()` — non-`static` functions emit their standalone
  assembly regardless of whether `main`'s calls constant-fold, and a `keep()` on
  a compile-time-constant value hits "impossible constraint in 'asm'" at `-O2`.
  Mojibake sweeps: folder 33 (14 → 0), folder 34 (3 → 0).

- **Profiling & benchmarking (folder 35)** — 16 lessons (`01`–`16`) +
  `17-exercises.md` + 8 examples (5 `.cpp` → `./build.ps1 folder` 5/5 OK, +
  `05_google_benchmark/` = a `minibench.hpp` Google-Benchmark shim + `bench.cxx`
  + build scripts, + `06_perf_workflow.sh` / `07_flamegraph.sh` self-contained
  Linux scripts). **Measure, don't guess** — the folder that verifies every
  claim in folders 31–34. Contents: **why measure** (intuition fails; **Amdahl's
  law** with the table — a 5% hotspot 10× faster ≈ +4.7% total; "premature" =
  *measure kiye bina*; **throughput vs latency** — different metrics, batching
  helps one and hurts the other) · **`<chrono>`** (steady vs system vs
  high_resolution — the **alias trap**; resolution / precision / accuracy /
  **self-cost** — `steady_clock::now()` **~38 ns** measured; `clock()` = CPU-time
  **1 ms** on MinGW; `duration_cast` truncates) · **`rdtsc`** (fencing recipes;
  **ticks→ns calibration via busy-wait not `sleep`** — ~1.996 ticks/ns; self-cost
  subtraction; core-hop → pin; `clock_gettime` vDSO for ≥ 1 µs) · **statistics**
  (**mean lies on right-skewed latency**; median / **min = jitter-free floor**;
  σ useless for non-normal → CV / MAD / IQR; **bimodal = two code paths**,
  mean lands in the empty gap; samples-per-nine) · **percentiles** (nearest-rank;
  "nines"; **fan-out tail amplification** — 100 calls at p99 → 63% hit a slow one,
  "the tail at scale"; **coordinated omission** + HdrHistogram interval
  correction; **percentiles don't average** → merge histograms) · **jitter**
  (SW: timer tick / scheduler / IRQ / page fault / `malloc` / syscall / lock;
  HW & firmware: freq scaling / C-states / **SMI** (OS-invisible) / SMT sibling /
  NUMA / thermal; measure with per-iter `rdtsc` + `cyclictest`/`rtla`/
  `hwlatdetect`; the **"quiet core" recipe** — BIOS + `isolcpus`+`nohz_full`+
  `rcu_nocbs` + pin/`mlockall`/no-alloc/busy-poll; **spike count + p99.9 are the
  robust metrics, not a single `max`**) · **histograms** (why one linear width
  can't hold bulk-resolution + tail-range; **log-linear / HdrHistogram** —
  relative error ≤ 1/`SUB` at every scale, mergeable, O(1), CO-correction;
  **CDF / percentile plot > PDF bars**; `08_latency_recorder.cpp`: **~2 ns/record,
  29.5 KB fixed, ≤1.3% error**, log-plot reveals bimodal) · **Google Benchmark**
  (`for (auto _ : state)`; `DoNotOptimize`/`ClobberMemory`; `Range`/`Args`/
  `SetBytesProcessed`; `PauseTiming` + its own cost; `BENCHMARK_F` fixtures;
  `--benchmark_repetitions` + `_median` + `_cv`; the 10 mistakes the library
  can't fix) · **benchmark pitfalls** (all demoed BUG/FIX: **DCE** → `0.000 ns/op`;
  **const-fold**; **loop-invariant hoist**; **cold start** (code/data/faults/
  predictor); **timer > op** (batch); **one run** (min-of-N + spread);
  **alignment & code-layout noise** — trust only > noise, check counters;
  **frequency scaling** — report cycles/op; **denormals** — FTZ/DAZ; **bench ≠
  reality**) · **`perf` basics** (counting vs sampling; **IPC** `<0.5`/`~1`/`>2`;
  cache & branch-miss rates; **top-down** Retiring/Frontend/Backend/Bad-Spec with
  fix directions; `-g` frame-pointer vs dwarf vs lbr; skid; `perf_event_paranoid`)
  · **`perf` advanced** (`annotate` % + **skid → `-e cycles:pp` (PEBS/IBS)**;
  `cycle_activity.stalls_l3_miss`; **`perf mem`** (per-access latency + served-
  from); **`perf c2c`** (false sharing / **HITM**); LBR → AutoFDO; counter
  multiplexing) · **flame graphs** (width=time, **X=alphabetical NOT time**,
  Y=depth; **top plateau = target**; `stackcollapse` → `flamegraph.pl`;
  **on-CPU vs off-CPU** (blocked time invisible on-CPU); **differential** red/blue;
  `hotspot`/Firefox Profiler/Speedscope) · **Valgrind** (DBI — deterministic but
  20–100× + simulated cache; **cachegrind `Ir`** = exact HW-independent
  instruction count → **CI regression gate**; miss counts = directional ratios;
  callgrind + **KCachegrind**; **massif** heap-over-time; **DHAT** per-alloc-site
  size-vs-usage / churn / realloc) · **VTune / top-down** (recursive tree
  Backend→Memory→DRAM→**Bandwidth-vs-Latency** — *different fixes*; analysis
  types; **`toplev.py`** for the same on plain `perf`; AMD uProf / Instruments) ·
  **sanitizers** (ASan OOB/UAF/leak ~2× / UBSan overflow/null/shift ~1.2× — always
  run / TSan races + lock-order ~5–15× / MSan uninit — rarely used; **`volatile
  bool` ≠ thread flag** → `std::atomic`; **never benchmark under a sanitizer**;
  ASan+UBSan+TSan CI matrix; MinGW fallback = `_GLIBCXX_ASSERTIONS` +
  `-fstack-protector-all`) · **production measurement** (6 requirements; **inline
  `rdtsc` + per-thread HdrHistogram** snapshot by a housekeeping thread; **SPSC
  ring → aggregator** for zero hot-path math; sampling — 1-in-N / time-gated /
  **exceedance**; **coordinated omission in prod** = timestamp at **arrival**;
  per-thread → merge, **never average p99s**; **white box vs black box** + NIC HW
  timestamps; USDT / uprobe / eBPF / LTTng). Measured on an **unpinned
  Windows/MinGW Zen 2 box** (~2 GHz, SSE2): chrono resolution 100 ns, TSC calib
  ~1.996 ticks/ns, `steady_clock::now()` self-cost ~38 ns; 94 µs workload chrono
  vs rdtsc agree to 0.02%; `04` BUG benchmarks **0.000 ns/op** vs FIX ~0.85–1.0;
  `05` `BM_reduce_NO_barrier` **0.49 ns/iter (DELETED)** vs `_WITH_barrier` 1030;
  `memcpy` 128 GB/s vs byte-loop 4 GB/s ~30×; `08` recorder ~2 ns/record, ≤1.3%
  error, bimodal log-plot. **⚠️ Rule 2:** the "BUG" benchmarks really report
  `0.000 / 0.49 ns/op` at `-O2` (the taught result, next to its FIX); `04` uses a
  `volatile` global sink/source because `keep()`'s `"+r,m"` hits "impossible
  constraint" on a folded value; cold-start is only ~1.2× here (Windows commits
  pages eagerly); `02`/`03` tail numbers are deliberately run-to-run unstable —
  the lesson being that this is an unpinned desktop, contrasted with a pinned
  Linux isolated core (03/06). Mojibake sweep: 52 → 0.

- **Ultra-low-latency C++ (folder 36)** — 24 lessons (`01`–`24`) +
  `25-exercises.md` + 12 examples (`./build.ps1 folder` → **12/12 OK**). **The
  synthesis folder**: folders 12–35's techniques applied to hot-path
  engineering, and — per spec Rule 12–13 — **every technique paired with its
  hidden cost and an explicit "when NOT to"**. **latency / throughput / jitter**
  (three axes, budget thinking — your HFT metric is always a latency
  percentile) · **tail latency** (optimize the *tail source*, not p50; **C++ has
  no GC** — the tail sources are allocator internals / faults / syscalls /
  locks, all eliminable) · **jitter-source audit checklist** (every hot-path
  line → eliminate / bound / make-rare) · **allocation avoidance** (measured
  `new` mixed-churn p99.9 **2585 ns** — the disaster; the hidden allocations —
  vector grow, string SSO, map node, `std::function` capture, `throw`,
  `std::to_string`, page fault) · **pre-allocation** (warm-up vs steady state;
  size from historical peak; **prove** zero-alloc — `null_memory_resource`
  upstream / a global-`new` hook / `perf stat -e page-faults`) · **memory
  pools** (build a `FixedPool` — intrusive free list in the free slots, `O(1)`,
  `p99.9` **30 ns flat** vs `new` 180; cross-thread "goes home to be freed";
  double-free detection) · **object pools** (construct-on-acquire vs
  **recycle-pre-constructed** — p50 30 vs 20 ns; recycle's danger = stale
  fields; `std::launder` with placement new) · **arenas / bump / monotonic
  buffer / PMR** (`allocate`=align+bump, `reset`=one store; ~16× vs new/delete;
  `pmr::vector` on a stack buffer = **0 global `new`**; `null_memory_resource`
  tripwire; overflow policy; escaped pointer = UAF) · **custom allocators**
  (`std::pmr` vs classic `Allocator<T>`; `scoped_allocator_adaptor` /
  pmr-propagation; why a single-size pool can't back a `pmr::unordered_map`) ·
  **cache locality** (hot/cold field split, AoS/SoA, flat structures — recap of
  32 + practice) · **false sharing** (measured **3.5–44×, run-to-run** — a
  *jitter* source; `perf c2c` / HITM; pad / align / per-thread + combine) ·
  **branch-free** (when it wins vs when it *loses* on predictable branches;
  `-O2` if-conversion — `06` measured the "branchy" `if` becoming `cmov`;
  switch-vs-table **~12×** on random data) · **virtual dispatch elimination**
  (CRTP / `std::variant`+`visit` / tag-`switch` / fn-table, homo vs hetero data
  — `07`: virtual **7.0 ns** vs CRTP **0.6 ns** hetero; bucket-by-type;
  devirtualization caveat) · **`std::function` cost** (`function_ref` — 2 words,
  no alloc ever; the SBO cliff — `08`: a 64-byte capture → `operator new` in the
  ctor, proven with a `new` counter) · **ring buffers** (pow-2 `& mask` not `%`;
  **monotonic counters** → no ABA, no wasted slot; release-store/acquire-load of
  the two indices, no CAS; **cached opposite index** = the ladder's biggest win;
  `alignas(CL)` on head/tail/buf; drop-don't-block; + **4 "wrong tool" cases**)
  · **batching** (the throughput vs **head-of-line latency** curve — `10`: B=1
  6.7 ns/item HoL 6.7 → B=1024 1.5 ns/item HoL **3095 ns**, knee ~B=16–32;
  **opportunistic batching** — take what's queued, never wait to fill) ·
  **syscall avoidance** (syscall cost 100–300× a userspace call, blocking →
  reschedule; **busy-poll** + its 100%-CPU-per-core cost; `SO_BUSY_POLL`;
  **`io_uring` SQ/CQ + `SQPOLL`** = zero syscalls on submit + registered
  buffers/files; kernel bypass; move the syscall to a housekeeping thread) ·
  **page-fault avoidance** (`MAP_POPULATE` / `mlockall(MCL_CURRENT|MCL_FUTURE)`
  / write-touch every page / stack pre-fault / TLS / library lazy-init; huge
  pages & THP `khugepaged` jitter — `11`: COLD p99 **2875 ns** vs WARM **30 ns**;
  a **read** pass doesn't make anonymous pages resident) · **CPU pinning** (a
  concrete core plan — NIC-poll / decode / strategy / risk / OS;
  `isolcpus`+`nohz_full`+`rcu_nocbs`+IRQ affinity; SMT sibling idle; NUMA
  first-touch from the pinned owner; the `SCHED_FIFO` busy-spin box-hang hazard;
  verify `cpu-migrations`~0) · **cache warming** (cold start vs cache decay
  during quiet periods; **dry-run** the real hot path with synthetic data — the
  `dry_run` flag must be `[[likely]]`/branchless, side-effect-proof,
  representative; keep the core busy = no C-state) · **I-cache** (Frontend Bound
  in `perf` top-down; `[[gnu::cold]]`+`[[unlikely]]`+`.text.unlikely`;
  **PGO/LTO/BOLT** function ordering; why `always_inline` everything *increases*
  Frontend Bound. ⚠️ Rule 2: `12` hot/cold split measured **no difference** in a
  micro-bench — the hot loop already fit L1i — kept honest) · **zero-copy**
  (`string_view` / `span` / `{ptr,len}` — non-owning, the lifetime rule;
  **in-place parsing** of a binary protocol — overlay a `packed` struct — with
  the 4 caveats: alignment / endianness / lifetime / aliasing; `std::from_chars`
  / `std::to_chars` for alloc-free / locale-free / exception-free number↔text;
  `iovec`/`writev`, `sendfile`/`splice`, `MSG_ZEROCOPY`) · **compile-time
  dispatch** (`if constexpr` — the untaken branch isn't *generated*; non-type
  template params per venue; "instantiate once, pick once at startup" via a
  function pointer; 2^N instantiation bloat → Frontend Bound) · **honest
  trade-offs** (lesson 24 — a hidden-cost table for every technique; **six
  explicit "when NOT to"**: not on the hot path / budget already met / cold path
  / throughput not latency / not measurably faster / can't maintain it /
  correctness at risk; the measure → profile → one change → re-measure → explain
  process; documenting a floor you can't beat). **⚠️ Rule 2 fired 3× (all
  taught):** `-O2` if-converted the branchy loop (`06`), a latency-bound loop
  ties templated/fn-ptr/`function_ref` (`08` — OoO hides the call overhead; a
  throughput loop would differ), hot/cold split measured nothing in a micro-bench
  (`12`). Measured on the unpinned Windows/MinGW Zen 2 box — ratios/shapes port,
  per-op-timed `max` is OS-interrupt noise, p50/p99/p99.9 are the signal.
  Mojibake sweep: 11 → 0.

- **HFT fundamentals (folder 37)** — 16 lessons (`01`–`16`) +
  `17-exercises.md` + 4 examples (`./build.ps1 folder` → **4/4 OK**). **The
  first folder of the HFT domain track — business/market knowledge, not more
  C++ mechanism.** **What HFT is/isn't** (myths vs reality: front-running,
  "guaranteed profit", manipulation — all corrected; HFT vs algo-trading vs
  quant-investing matrix) · **exchange architecture** (order gateway →
  matching engine → market-data-out vs trade-confirm, two *alag* paths with
  their own race conditions) · **market microstructure** (liquidity ≠
  volume; price discovery as emergent from order flow; **adverse
  selection** — the fundamental market-making risk, worked example) ·
  **order types** (market/limit/IOC/FOK/stop/iceberg/post-only — exact fill
  behavior each, book diagrams; IOC vs FOK for multi-leg arb) ·
  **bid-ask-spread** (bps normalization — why absolute spread misleads
  across price levels; **microprice** — size-weighted mid, formula +
  intuition; order imbalance — measured across 5 quotes) · **order book
  concept** (`std::map` two-sided book; price as **integer ticks, not
  `double`** — ties straight back to 03-VARIABLES' float-equality trap) ·
  **price-time priority vs pro-rata — measured side-by-side**: same
  4-order book, same incoming order, FIFO gives orders 1/2/3 fills and
  order 4 nothing, pro-rata gives all 4 a proportional share; which rule
  makes raw speed matter more · **tick size / lot size / price bands**
  (why price-as-integer-ticks is an exchange rule, not just an engineering
  choice) · **maker vs taker** (resting vs crossing, not buy/sell;
  maker-taker fee/rebate model; why fee/rebate can flip a spread-capture
  trade's sign) · **strategy categories** (market making / stat-arb /
  latency-arb / event-driven — each one's latency-critical path —
  **explicitly concepts only, no alpha/signal research**) · **colocation**
  (the speed-of-light propagation floor that no code can fix; cross-connect
  cable-length equalization; fair-access mechanisms; colocation is one
  layer among many) · **full HFT system architecture diagram** (feed
  handler → order book → strategy → risk → OMS → gateway, mapped box-by-box
  to folders 38–44; market-data-path and order-path as two concurrent
  flows) · **risk systems** (pre-trade check table; fat-finger as *relative*
  not absolute anomaly detection; kill switch; **fail-closed as the
  default**, not fail-open) · **tick-to-trade latency budget** (a measured
  calculator tool — finds the ONE stage whose *tail overrun* dwarfs the
  rest, not just "which stages are over," since almost every stage is
  slightly over its p50-based budget at p99.9 — that's normal) · **India vs
  US market structure** (NSE/BSE consolidated vs Nasdaq/NYSE+ATSs
  fragmented; ITCH/OUCH as the well-known public protocol reference; Reg
  NMS → smart order routing) · **regulatory basics** (algo-ID tagging — useful
  to the firm itself, not just regulators; audit-trail logging as a
  hot-path *engineering* problem — async/ring-buffered, not "free"; SEBI vs
  SEC/FINRA framing). All 4 examples are **conceptual/deterministic** (no
  timing measurement — this folder has no benchmarks, unlike 35/36).
  Mojibake sweep: 3 → 0.

- **Market data (folder 38)** — 16 lessons (`01`–`16`) + `17-exercises.md`
  + 10 examples + one shared `wire_protocol.hpp` (`./build.ps1 folder` →
  **10/10 OK**). **The HFT domain track's first build** — CLAUDE.md's full
  engineering process run end-to-end for the first time on real domain
  code. **What market data is** (feeds, "tick" as an event not a price
  increment, the feed-handler's place in 37/14's latency budget) ·
  **L1/L2/L3** (top-of-book vs aggregated-per-level vs individual-order;
  L3⊃L2⊃L1, derivable one-way only) · **snapshots vs incremental**
  (incremental is bandwidth-efficient but NOT self-sufficient — a worked
  silent-book-drift example) · **sequence numbers** (gap/duplicate/
  out-of-order detection — measured: 96/20000 gaps, sanity-verified exact
  match against injected drops) · **binary vs text protocols** (measured
  byteswap cost **0.753 ns/swap** — corrects the "binary is fast because
  swap is avoided" oversimplification; the real win is eliminating
  text-to-number parsing entirely) · **ITCH-style protocol built**
  (16-byte header + 5 message types: Add/Execute/Cancel/Delete/Replace,
  two-phase header-then-body parsing, `hdr.length`-driven framing not
  hardcoded struct sizes) · **FIX/FAST** (text still dominant in
  order-entry; when FAST's template-compression is chosen) · **SBE**
  (schema-first codegen; native-endian's *honest* reason — codegen
  simplicity/correctness, not raw swap-cost savings, since swap is already
  cheap) · **zero-copy parsing — MEASURED**: naive owned-copy-into-vector
  vs overlay-cast-into-fixed-sink, same feed same run, **p99.9 ratio
  22.2× (771.5→40.1 ns), p50 identical (30.1 ns)** — the allocation tail,
  not typical-case work, is what moved · **endianness handling**
  (`if constexpr(std::endian::native==...)`, `__builtin_bswap*` pre-C++23,
  alignment-safe `memcpy` reads vs UB pointer-cast) · **message framing**
  (length-prefix vs delimiter; TCP stream has NO boundaries — mandatory
  framing; UDP datagram-preserves-boundary but batching still needs it;
  partial-header vs partial-body vs carry-over) · **A/B feed arbitration —
  MEASURED**: two independently-lossy (~1% each) copies of one feed,
  arbitrated → **~0.02% loss, ~49.5× improvement, zero round-trips** ·
  **recovery/retransmission** (3-layer ladder: A/B → TCP retransmit-request
  → snapshot resync, increasing cost order; fail-closed "go stale" state
  machine, 37/13's principle reapplied) · **timestamping/clocks**
  (exchange-ts vs receive-ts, clock skew — a negative-latency diagnostic,
  PTP sync, why `seq_num` beats timestamp for ordering) · **conflation**
  (safe for "latest state matters" — L2 display; never for trades/own-order
  fills/L3 strategy signals) · **capstone**: a full `FeedHandler` class
  (framing+gap-detection+zero-copy dispatch+fixed book state) measured
  end-to-end **p50 30.1 / p99.9 40.1 ns — identical to the standalone
  optimized parser**, proving the extra logic added zero tail cost.
  Mojibake sweep: 4 → 0.

- **Order book (folder 39)** — 17 lessons (`01`–`17`) + 9 examples + 5
  shared headers (`./build.ps1 folder` → **9/9 OK**). **"HFT ka sabse
  classic interview question aur sabse important data structure"** —
  built **3 times** on an identical operation workload, measured every
  time (the spec's mandatory "pehle correct, phir fast" process). **V1**
  (`std::map`+`std::list`+`unordered_map`, correct baseline: p50 140.3 /
  p99.9 1502.9 ns — allocation/tree-rebalance is the Add-tail source) ·
  **V2** (sorted `std::vector`+`std::deque` — a **genuine measured
  regression**, p50 150.3 / p99.9 2745.2 ns, OVERALL WORSE than V1 despite
  Add improving 3.9× via contiguous-array access; root cause traced to a
  specific implementation choice: the order-id index can't safely store
  a stable iterator across vector mutation, so it stores only `{side,
  price}`, forcing an `O(level size)` linear `std::find_if` scan on every
  cancel/execute that V1 never had — a textbook Rule-2 finding, not
  contrived) · **V3** (tick-indexed flat `std::array<Level,256>` per side
  — O(1) price→index arithmetic, no search — + intrusive doubly-linked
  order lists in a pre-allocated arena with `uint32_t` index links not
  pointers + a free-list allocator, zero per-order heap allocation — +
  a custom tombstone-based open-addressed flat hash for order-id→arena-
  slot lookup, O(1) direct access, fixing V2's regression — wins EVERY
  metric: p50 120.2 / p99.9 661.3 ns, ~2-4× better than both V1 and V2).
  **Price-time priority (FIFO)** proven identical across all three via an
  `ids_at_price()` accessor (arrival order comes from each container's
  natural append-order, no explicit sort; partial-reduce preserves
  position, Replace does not — new order_id = new, last position).
  **Top-of-book**: free in V1/V2 (`begin()`/`front()`, container
  invariant); V3 needs explicit cached `best_bid_idx_`/`best_ask_idx_`,
  honestly characterized as "typical O(1), worst-case O(NUM_LEVELS) scan
  on best-level-empties" — not oversold as guaranteed. **A real bug**
  surfaced during test-writing itself: calling `best_bid()` without
  `has_bid()` on out-of-range test prices crashed V3 on an out-of-bounds
  `std::array` read — fixed and documented as an explicit precondition,
  not silently patched. **Correctness**: cross-version equivalence
  checked after EVERY op (20000+ checkpoints, 4 seeds) + a 30000-op fuzz
  run injecting ~3000 deliberately-invalid ops (duplicate adds, ops on
  nonexistent ids, over-execute clamping, replace-of-already-gone-orders)
  against an independent `O(n)` reference model — **zero disagreements**.
  Explicit, documented trade-offs: bounded price range (`NUM_LEVELS`) and
  bounded order capacity (arena `max_orders`) — V3's speed is not free.
  Lesson 15 gives the full mechanism-level "what changed and why"
  accounting (spec's explicit requirement). Mojibake sweep: 3 → 0.

- **Matching engine (folder 40)** — 17 lessons (`01`–`17`) + 8 examples +
  2 shared headers (`./build.ps1 folder` → **8/8 OK**). Price-time-
  priority matching built on top of 39's order book: all 4 order types
  (**Limit** — resting vs aggressing, leftover rests on ITS OWN side;
  **Market** — no price limit, sweeps until qty==0 or book empties,
  unfilled remainder voided by explicit design choice; **IOC** — like
  Market but price-limited; **FOK** — needs a pre-match precheck, "poora
  ya kuch nahi"), 3 self-trade-prevention modes (CancelNewest/
  CancelOldest/CancelBoth, each with distinct resting-fate + incoming-
  continuation semantics). **A genuine correctness finding:** a naive FOK
  precheck ("sum total resting qty at crossable levels") silently
  violates FOK's all-or-nothing contract once combined with STP — STP
  can skip (CancelOldest) or abort (CancelNewest/CancelBoth) on self-
  owned liquidity the naive sum still counted, producing a *partial*
  fill on an order that promised full-or-nothing. Fixed with an
  STP-mode-aware precheck (`available_qty()`) that walks the exact same
  priority order the real match uses. Verified two ways: a hand-crafted
  test demonstrating the exact divergence (naive sum 70 ≥ target 50 →
  would wrongly PASS; true-available 30 < 50 → correctly REJECTS), AND a
  30000-command fuzz run against an independent `O(n)` reference engine
  (`RefEngine`, plain `vector` + linear scan) whose own FOK precheck uses
  a *completely different* strategy (copy-the-state, run-a-practice-
  match, discard) — **0 disagreements, 0 self-trade leaks, 0 FOK
  violations across 3774 FOK orders hit**. **Determinism proven, not
  claimed**: an identical 20000-command log replayed into two independent
  fresh engines produces byte-identical trades and final state. 43/43
  scripted unit tests pass. **Single-threaded-per-symbol design**
  explicitly justified (locks stop data races, not thread-scheduling
  non-determinism — sharding by symbol is the real source of parallelism,
  full treatment deferred to 41). A use-after-erase dangling-reference
  bug (reading a resting order's `qty` after `std::list::erase()` had
  already freed the node) caught during development. Mojibake sweep: 1 → 0.

- **HFT concurrency (folder 41)** — 14 lessons (`01`–`14`) + 8 examples +
  2 shared headers (`./build.ps1 folder` → **8/8 OK**). Turns 40's
  single-threaded matching engine into a real thread-per-stage pipeline:
  a production `SpscQueue<T,Capacity>` (28's design, packaged +
  reused across 3 more examples), a working simplified **LMAX Disruptor
  built from scratch** (single-producer, N *independent* fan-out
  consumers — every consumer sees every event, unlike an SPSC queue's
  work-distribution — gating sequences, natural batching, correctness-
  verified over 4M events with 0 mismatches), a `Seqlock<T>` snapshot,
  and a genuine capstone (`08_pipeline_demo.cpp`) — 3 real threads
  (feed/match/sink) carrying 40's ACTUAL `MatchingEngine`, with
  end-to-end latency measured via timestamp-at-source propagation.
  **Two headline measured findings:** (1) seqlock beats
  `std::shared_mutex` by **~2500-3500×** under an adversarial max-rate-
  writer stress test — far more dramatic than 28's original ~80-100×
  under gentler conditions (`shared_mutex` collapses to ~0.04 M
  reads/s; both `torn=0`); (2) core-pinning (`SetThreadAffinityMask`)
  reduced scheduling-hiccup FREQUENCY (511 vs 782 / 20M iterations) but
  produced a WORSE single max-delay outlier (15.6M vs 829K ticks) — an
  honest demonstration that thread affinity is not core isolation (real
  tail-latency control needs `isolcpus`/`nohz_full`, already in folder
  29). **A real toolchain quirk found and fixed:** `std::thread::
  native_handle()` on this MinGW posix-threading build returns a
  `pthread_t`, not a Win32 `HANDLE` — `GetThreadTimes()` silently fails
  on it; fixed via `DuplicateHandle(GetCurrentThread())` called from
  inside the thread, needed to measure the CPU-cost half of the
  busy-spin-vs-blocking trade-off (28/06 had only measured latency).
  Async logging measured ~14× hot-path cost reduction. A counter-
  intuitive pacing finding in the pipeline capstone (slower feed made
  p99 WORSE, not better — longer test duration meant more OS-scheduler-
  jitter exposure, not more queue backlog) is documented as a genuine
  Rule-2 case study. Mojibake sweep: 0 → 0.

- **HFT networking (folder 42)** — 17 lessons (`01`–`17`) + 8 examples
  (`./build.ps1 folder` → 0 real fail, 6/8 correctly skipped as
  Linux-only). The full wire-to-wire path: an `INetworkReceiver`
  abstraction (`08_bypass_abstraction.hpp`) making the kernel-bypass
  backend (kernel-socket now; Onload/ef_vi/DPDK config-swappable later)
  a deployment decision, not an application rewrite. The full bypass
  ladder (kernel-socket → `SO_BUSY_POLL` → Onload → ef_vi → DPDK)
  explained with an honest trade-off table. **Two genuine fixes found
  and corrected during this folder's own development:** (1) hardware
  timestamping — 30/09 could only compare RX-kernel-ts against
  app-steady_clock (different epochs, relative-jitter-only); this
  folder takes TX timestamps too (`MSG_ERRQUEUE`, async retry-poll)
  from the SAME kernel clock domain as RX, making the subtraction a
  genuinely meaningful absolute latency number; (2) a draft comment on
  the TCP order-gateway example incorrectly claimed `writev`'s win
  would recreate 30/03's ~40ms Nagle/delayed-ACK stall — caught on
  review and corrected: that specific stall needs the SENDER's Nagle
  to be ON, which `TCP_NODELAY` here explicitly prevents; `writev`'s
  actual win is a smaller, consistent one (fewer syscalls, fewer
  packets/ACKs). NIC/IRQ tuning built as a real, `bash -n`
  syntax-checked script (ring buffers, coalescing, GRO/LRO off, RSS +
  per-queue IRQ pinning kept OFF 41/08's hot-path cores, sysctls,
  verify-by-drop-counters). A full **wire-to-wire capstone**
  (`06_wire_to_wire.linux.cpp`) chains kernel timestamps across 2
  network hops + an internal-processing stage, all in one shared clock
  domain — an explicit breakdown, not one opaque total. FPGA covered as
  an explicit overview-only (out of course scope, per the SPECIALIZED/
  DOMAIN-SPECIFIC list). **Majority of this folder's code is genuinely
  Linux-only** (multicast sockets, `SO_BUSY_POLL`, `SO_TIMESTAMPING`,
  `MSG_ERRQUEUE`, TCP tuning) — this dev box is Windows/MinGW with no
  WSL installed, so these 6 examples could not be compiled or run here
  (the established `.linux.cpp` pattern from 29/30); they were
  hand-reviewed instead of compile-verified, catching and fixing real
  bugs: a `-Wsign-compare` risk (`ssize_t` vs `sizeof`, fixed with
  explicit casts in 3 spots), an unbounded-blocking-`recv()` hang risk
  (fixed with `SO_RCVTIMEO` safety nets in 3 files), and empty-vector
  `.back()` UB in report functions (guarded in 3 files). Every number
  is presented as an explicit unmeasured estimate, never fabricated as
  "measured." Mojibake sweep: 0 → 0.

- **HFT optimization methodology (folder 43)** — 17 lessons (`01`–`17`) +
  8 examples (`pipeline.hpp` shared header + `02_profile_analysis.sh`
  Linux/`perf` workflow + 7 portable `.cpp`; `./build.ps1 folder` →
  **7/7 OK** under strict warnings). The 6-step loop — measure → profile
  → hypothesize → change (one) → re-measure → **explain what changed and
  why** — with a **correctness gate that runs before the speedup gate**.
  Spine: `pipeline.hpp` carries two full tick-to-order pipelines on one
  deterministic feed — `PipelineV0` (naive: `substr`+`std::stod` parse,
  `std::map<double>`+`std::list` book, `std::deque` re-sum SMA,
  `std::string` encode) and `PipelineV3` (hand integer parse +
  fixed-point price, flat-array book + cached top-of-book + dense-id
  direct index, ring-buffer running-sum SMA with **zero division**, POD
  encode). V3's signal math is the exact integer equivalent of V0's
  float cross-condition; `04_before_after.cpp` proves the order-fire tick
  stream is byte-identical (110/110) **before** it will report the
  ~60–80× speedup (v0 ~1.9–2.7 µs/tick → v3 ~25–45 ns/tick; parse ratio
  ~25×, book ratio ~25–29× — v3 per-stage absolutes are rdtsc-probe-cost
  limited, an explicit measurement-overhead lesson). Per-technique
  measured (Zen 2, unpinned — ratios): division `div`→shift **~17×** /
  →magic-multiply ~13× / →reciprocal-multiply ~7× (**only the divide
  timed** — the CLAUDE.md warning about a prior benchmark hiding `%` in
  the loop explicitly heeded, verified in the example's own comments);
  struct field-reorder 24→16 B (34% smaller, 0 behaviour change), fat
  struct → SoA **~8×** (memory-traffic-bound scan); fixed-point `0.1`
  added 10× (double) = `0.99999999999999988898` ≠ 1.0, int64 exact
  (correctness is the point, the ~20% arithmetic-speed edge a bonus).
  **An honest Rule-2 null result:** hot/cold path splitting
  (`[[gnu::cold]]` + `[[unlikely]]`) measured **~1%** on this box — the
  frontend simply isn't the bottleneck at this scale (carries 36/12);
  the `[[gnu::cold]]`→`.text.unlikely` move was verified in the asm, and
  the attribute is kept anyway because its cost is 0. `16-when-to-stop.md`
  is the diminishing-returns / judgement lesson (noise-floor, effort/
  payoff knee, "fast enough" is venue-defined, opportunity cost).
  `02_profile_analysis.sh` (Linux/`perf`, `bash -n`-checked, not run —
  no `perf` on Windows) maps each `perf` signal (frontend-bound /
  branch-miss / LLC-miss / `idiv` / `perf c2c` HITM) to the lesson that
  addresses it. **No new C++-language gaps** — every technique was
  already covered under folders 19/21/25/27/28/32/33/36; this is the
  applied methodology proof. Mojibake sweep: **298 → 0** (stray
  Devanagari/Cyrillic homoglyphs across the drafts, all fixed).

- **HFT capstone — the mini HFT engine (folder 44)** — 15 lessons
  (`01`–`15`) + 12 example drivers + 11 shared `mh_*.hpp` headers
  (`./build.ps1 folder 44-HFT-PROJECTS` → **12/12 OK** under strict
  warnings). The course climax: everything from folders 01–43 assembled
  into one `MiniHftEngine` — **MarketData → Parser → L2Book → Strategy →
  Risk → OMS → Venue → fills → PnL** — single-threaded and **fully
  deterministic** (no wall clock anywhere; every timestamp comes from the
  event stream, so replay is byte-identical). **Reuse over rewrite** (the
  capstone's whole point): `mh_types.hpp` `#include`s folder-40's
  `matching_engine.hpp` — the `Price`/`Qty`/`OrderId` types **and** the
  `MatchingEngine` itself, used as the venue via `ExecutionSimulator`;
  `07_spsc_queue.cpp` `#include`s folder-41's `spsc_queue.hpp` for the
  wire→engine hand-off; the book/pools/strategy follow folder 39/14/36/43
  patterns. **The folder-43 optimization loop, applied end-to-end:**
  `MiniHftEngine` is `template <class Venue>` — `NaiveEngine`
  (`ExecutionSimulator`: folder-40 `std::map` `MatchingEngine`, one tree
  insert + one `std::list`-node `malloc` per market message) vs
  `OptimizedEngine` (`FastVenue`: flat-array aggregate book + per-level
  FIFO + IOC sweep). Profiling found the venue mirror was the `book`
  stage's bulk (~150 ns/msg); `FastVenue` halves it (~67 ns/msg) →
  **~1.5–1.6× end-to-end** (this unpinned Zen 2 box; p50/p99 the reliable
  comparison — the `max` outlier is scheduler jitter, 41/13).
  **Correctness gate first:** `12_integration_tests.cpp` proves
  `NaiveEngine == OptimizedEngine` byte-for-byte (fills, qty, P&L,
  position) across **5 seed/config combos**, both deterministic,
  invariants (|position| ≤ risk max, zero sequence gaps, OMS always
  settles) held. **Per-component measured (Zen 2, ratios):** L2Book apply
  ~17 ns/msg (BBO matches a `std::map` reference **exactly**); FixedPool
  alloc+free **~2.0× (p50) / ~5.0× (p99.9)** vs `new`/`delete`;
  ObjectPool: a released handle → `nullptr` **even after the slot
  recycles** (generation-checked use-after-free guard); SPSC hand-off
  **~6 M msg/s**, every message in order; RiskEngine **14/14** checks
  (rate-limit is backpressure, not a kill-switch trigger); OMS accounting
  always settles (no leaked orders, no phantom fills). Feed parser v1 vs
  v3: **~1.0× at `-O2`** — an honest Rule-2 null (the compiler is already
  optimal at this scale, cf. 43/08). **No alpha** — `SpreadCrossStrategy`
  is a mechanical rule to exercise the pipeline; its backtest P&L has zero
  predictive meaning (37 SPECIALIZED list). **No new C++-language gaps** —
  every technique (fixed-point integer math, `constexpr`,
  `template <class Venue>` dispatch, flat-array structures,
  generation-checked handles, intrusive free lists, SPSC lock-free
  hand-off, `bit_cast`/`memcpy` parsing) was already covered under folders
  14/19/21/25/27/28/36/39/40/41/43. **The HFT build track (folders 36–44)
  is COMPLETE.** Mojibake sweep: **16 → 0**.

- **Debugging — the full toolchain (folder 45)** — a **skill folder**
  (like 20-DSA / 35-PROFILING): 13 lessons + 5 standalone examples + 10
  "find & fix" buggy programs (`./build.ps1 folder 45-DEBUGGING` →
  **5/5 OK** under strict warnings; buggy programs compile-clean in
  `checkall`). Covers: **mindset** (hypothesis→test loop, reproduce-first,
  `git bisect`, root cause ≠ symptom — mirrors folder 43's optimization
  loop) · **GDB** basics + advanced (break/step/print/bt/frame/finish;
  watchpoints, conditional breakpoints, `commands`/`tbreak`/`catch throw`,
  `.gdbinit`/`-x` scripting) with **real transcripts captured on this box**
  (MinGW GCC 15.1.0 + GDB 16.3) against `01_gdb_practice.cpp` · **crashes**
  (signals + exit `128+sig`, core dumps `ulimit -c`/`coredumpctl`/`bt full`,
  stack overflow, Windows WER) · **`-O2` debugging** (`<optimized out>`,
  garbage function-entry values — real transcript `accts`="length 364181",
  `factorial(5)` constant-folded out of the binary, inlined frames, `-Og`,
  frame-pointer, "UB only at `-O2`" table) · **sanitizers** (ASan shadow
  memory / redzones / quarantine, UBSan, TSan happens-before, MSan;
  combine matrix; CI golden build — MinGW lacks the libs so a Linux/WSL
  workflow with expected output in the example footers) · **valgrind**
  (memcheck lost-kinds, helgrind, DRD, massif; vs-sanitizers table) ·
  **multithreaded** (`thread apply all bt`, deadlock signature ≥2 in
  `__lll_lock_wait`, missing-unlock vs cycle, heisenbugs) · **reverse
  debugging** (`rr record`/`replay`, `watch` + `reverse-continue` = "who
  set this value" in one command, `--chaos`) · **logging** (levels,
  structured logs, log-and-throw antipattern, **HFT async logging**: hot
  thread enqueues a POD ~20–40 ns to an SPSC ring, background thread
  formats+writes, queue-full → drop + count never block, binary log +
  mmap ring + pinned logger core) · **`perf` for bugs** (off-CPU
  analysis, syscall storms via `perf trace`, page-fault storms, false
  sharing via `perf c2c` — "correct but 100× slow" bugs) · a **40+-entry
  bug catalog** (Memory / Concurrency / Logic / Integer / Lifetime, each
  symptom → tool → fix, with a symptom→family quick index). **Rule-2 note
  carried through:** `10_data_race.cpp` races ~15–20% of runs at `-O0`
  but **hides at `-O2`** (register promotion of the RMW) — taught, not
  hidden. **No new C++-language gaps** — applied tooling; every bug family
  was covered under folders 09/12/13/14/17/18/22/25/26/27/28. Mojibake
  sweep: **34 → 0**.

- **Interview readiness — the layered question bank (folder 46)** — 20
  lessons + 8 **verified** coding examples + 4 design docs + 4 mock
  transcripts (`./build.ps1 folder 46-INTERVIEW-PREP` → **8/8 OK**,
  strict). Per the spec's "build toward HFT interviews, don't jump":
  `02`–`14` are **Layer 1 (types/control/functions) → Layer 13 (feed
  handler / order book / risk / OMS / tick-to-trade architecture)**, every
  Q&A answer cross-referencing the source course folder. Plus `01` (how
  HFT interviews work — firm types, pipeline, the 3 grading axes, talking
  out loud), `15` (system-design arc + 4 worked designs + rubric), `16`
  (brainteasers/probability — EV, the make-a-market game, Monty Hall /
  100 seats / HH-vs-HT wait / 25 horses / ants, Fermi, mental math,
  Kelly + volatility-drag), `17` (30+ C++ trick questions with the right
  answer + why), `18` (behavioural — STAR + the 6 stories + discussing
  this course's projects), `19` (4 mock scripts with strong/weak tracks +
  rubrics), `20` (HFT resume & the 7 projects that matter, all folders
  39–44). Coding examples: reverse list, LRU, **lock-free SPSC ring**,
  fixed-point price parse, object pool, L2 top-of-book, atoi + overflow
  clamp, O(1) moving average + division-free signal. **No new
  C++-language gaps** — this folder *tests* folders 01–45; it completes
  `HFT-COMPLETENESS-AUDIT.md` Section L (interview readiness). Mojibake
  sweep: **6 → 0**.

- **The graded practice bank (folder 47)** — **250 problems** across 10
  themed files (basics 30 · arrays/strings 40 · pointers/memory 25 ·
  OOP/design 20 · STL 35 · templates 20 · concurrency 25 · lock-free 15 ·
  optimize-this 20 · HFT 20), each = statement + `Pattern:` hint +
  `<details>` (approach + complexity). `11-solutions/` = README + 10
  per-category worked-code writeups. **10 verified runnable examples**
  (`./build.ps1 folder 47-CODING-PROBLEMS` → **10/10 OK** strict, all run
  + assert): `03_arena_and_pool`, `04_scope_guard_and_result`,
  `05_flat_map`, `06_crtp_and_detect`, `07_blocking_queue` (checksum),
  `08_seqlock` (torn reads = 0), `10_order_book_ops` (`crossed()`
  invariant), + two kata files, + `09_optimize_row_vs_col`. **Rule 2:**
  `09_optimize_row_vs_col` measures **~45–70× at `-O2`** here (cache +
  TLB + vectorization stack) — bigger than folder 32's ~7×, reported not
  rounded. **No new C++-language gaps** — drills folders 01–46. Mojibake:
  **0**.

- **The quick-reference layer (folder 48)** — 13 markdown sheets
  distilling folders `01`–`47`, each cross-linking its deep source
  folder: `01` syntax · `02` STL containers (per-container **iterator +
  reference invalidation** rules) · `03` `<algorithm>`/`<numeric>`/ranges
  by job · `04` complexity tables (+ the amortized-vs-worst-case p99.9
  trap) · `05` compiler flags (warnings / `-O` / `-march` / sanitizers /
  codegen trade-offs / the 3 builds) · `06` GDB · `07`
  perf/valgrind/sanitizers (`perf stat` signal→fix table) · `08` HFT
  production tuning checklist · `09` memory ordering (6 orders,
  release/acquire handoff, ABA) · `10` latency numbers (consistent with
  46/12) · `11` UB catalog · `12` HFT glossary · `13` 13 one-page
  interview-revision sheets + behavioural + make-a-market. **No `.cpp`, no
  new gaps** — this folder *condenses* folders 01–47. Mojibake: **0**.

- **The end-to-end projects (folder 49)** — connected, difficulty-ordered
  projects (spec's "concepts ko projects mein jodo"). 5 lesson files
  (17 project specs, each spec → milestones v1→v2→v3 → concepts → tests →
  extensions; + `04` a v1-first / testing / **code-review checklist** doc;
  + `05` a folder-44 pointer) + **17 verified reference implementations**
  (`examples/{beginner,intermediate,advanced}/`). Highlights: calculator
  (shunting-yard) · number-guess (logic split from I/O, `<random>` right) ·
  expense tracker (integer money) · inventory (**append-only log +
  `replay()`**) · bank (**atomic transfer** + integer interest + 20k-op
  invariant property test) · CSV parser (**4-state field machine**) ·
  KV store (**WAL + CRC + compaction + torn-tail recovery**) · custom
  `Vector<T>` (placement `new`, Rule of 5, **strong exception guarantee**) ·
  3 allocators (**measured ~20–28× vs `::operator new` at `-O2`**) ·
  concurrent queues (mutex / **lock-free SPSC no-CAS** / **MPSC Vyukov**) ·
  epoll echo server (Linux only) · JSON parser (**`line:col` errors**,
  **depth limit**, round-trip). All 16 non-linux examples compile
  strict-clean + run + assert. **No new C++-language gaps** — this folder
  *applies* folders 01–35 in connected programs. Mojibake: **0**.

---

## SPECIALIZED / DOMAIN-SPECIFIC

Yeh topics **jaan-boojh kar** is course ke scope se bahar rakhe gaye hain — ya sirf
introduction level pe cover honge. Inhe silently chhodna galat hota, isliye yahan
explicitly list kar raha hoon:

| Topic | Kyun bahar | Kahan tak cover hoga |
|---|---|---|
| **`std::stacktrace` (C++23)** | Is repo ke MinGW GCC 16.2 build mein `<stacktrace>` header hai par `__cpp_lib_stacktrace` define nahi (library backtrace support ke bina bani) — compile-verify nahi ho sakta. Baaki C++23 ab boundary nahi raha. | Concept + feature-test pattern `22/15` mein; stack traces ke liye gdb/sanitizers (folder 45). **Baaki C++23 compile-verified hai** — `22/15` + `*.cpp23.cpp` examples: deducing `this`, `std::generator`, `flat_map` (measured), `mdspan`, `move_only_function`, `std::print`, ranges additions, `import std;` (setup + measured). |
| **C++26** (reflection, contracts, `std::execution` senders/receivers, `std::hive`, `<hazard_pointer>`, `<rcu>`, `std::simd`, `inplace_vector`, `#embed`, pack indexing) | Naya standard (2026); compiler + library support abhi adhoora aur badal raha hai. GCC 16.2 pe probe kiya: **chalte hain** — pack indexing, `_` placeholder, `= delete("reason")`, `#embed`, `std::inplace_vector`, `<debugging>`, static reflection (`-freflection`), contracts (`-fcontracts`); **is library build mein nahi** — `std::hive`, `<hazard_pointer>`, `<rcu>`, `std::execution::just`. Course ka core C++20 hai, C++23 deepening ke saath. | Concept level only. Jo ideas pehle se padhaye gaye hain unke standard versions: hazard pointers + RCU = folder 28 ki techniques; `inplace_vector` = fixed-capacity no-alloc containers (folder 36); `std::execution` = folder 26/41 ke thread pools/pipelines. Toolchain mature hone pe `22/15` ki tarah probe + compile-verified lesson. |
| **FPGA / Verilog / HLS** | Hardware design ek alag career hai | Folder 42 mein sirf "yeh kya hai aur kab use hota hai" |
| **Full DPDK application development** | DPDK apne aap mein ek badi library hai | Folder 30/42 mein concepts + minimal example |
| **RDMA / InfiniBand programming** | Specialized, mostly HPC | Folder 30 mein concept level |
| **Exchange-specific protocol specs** (NSE NEAT, CME MDP 3.0, Nasdaq TotalView-ITCH exact spec) | Proprietary/licensed documents | Folder 38 mein ITCH-style generic protocol + parser |
| **Actual trading strategies (alpha)** | Yeh IP hai, koi public nahi karta | Folder 37/44 mein strategy *framework*, alpha nahi |
| **Quant finance math** (stochastic calculus, options pricing) | Alag domain — quant researcher ka kaam, C++ engineer ka nahi | Folder 37 mein basic microstructure only |
| **Regulatory/compliance detail** (SEBI, MiFID II, Reg NMS) | Legal domain | Folder 37 mein overview |
| **GPU / CUDA programming** | HFT mein aksar irrelevant (latency-wise) | Mention only |
| **Windows systems programming** | HFT Linux pe chalta hai | Setup guide tak |
| **Embedded C++ / MISRA** | Alag domain | Nahi |
| **Boost library deep dive** | Standard library pehle | Selected parts jahan relevant ho (folder 19) |
| **Qt / GUI programming** | HFT ke liye irrelevant | Nahi |
| **C++ standard committee process / wording** | Language lawyering | Nahi (par UB aur ODR jaise rules cover honge) |

Agar aapko in mein se koi topic chahiye, alag se bolna — main uske liye supplementary
material bana dunga.

---

## Kaise use karein yeh file

Har batch ke baad yeh file update hoti hai. Aapka goal:

```
Batch 1  ->  har section mein "BAAKI HAI" ki lambi list
Batch 8  ->  list chhoti ho jaati hai   (folders 00-28 done)
Batch 9  ->  folders 29-35 DONE (Linux/net/CPU-arch/cache/compiler-opt/asm/profiling) — BATCH 9 COMPLETE
Batch 10 ->  folder 36 (ultra-low-latency C++) DONE · folder 37 (HFT fundamentals)
             DONE · folder 38 (market data) DONE · folder 39 (order book)
             DONE · folder 40 (matching engine) DONE · folder 41 (HFT
             concurrency) DONE · folder 42 (HFT networking) DONE · folder 43
             (HFT optimization methodology) DONE · folder 44 (HFT-PROJECTS,
             the CAPSTONE) DONE  ·  HFT build track (36-44) COMPLETE
Batch 11 ->  folder 45 (DEBUGGING) DONE · folder 46 (INTERVIEW-PREP)
             DONE · folder 47 (CODING-PROBLEMS, 250-problem practice
             bank) DONE · folder 48 (CHEATSHEETS, 13 quick-ref sheets)
             DONE · folder 49 (PROJECTS, 17 reference impls) DONE ·
             FINAL GAP AUDIT (PHASE 34) DONE  <- COURSE STRUCTURALLY COMPLETE
```

Aur ab yeh dikhta hai:

```
### MAJOR C++ LANGUAGE GAPS
NONE

### MAJOR STANDARD LIBRARY GAPS
NONE   (C++23 compile-verified in 22/15; std::stacktrace [absent in this build] + C++26 -> SPECIALIZED)

### MAJOR MODERN C++ GAPS
NONE   (C++23 verified in 22/15; C++26 -> SPECIALIZED)

### MAJOR SYSTEMS C++ GAPS
NONE

### MAJOR HFT-RELEVANT C++ GAPS
NONE
```

**Course ab structurally complete hai** — 50 content folders (00–49), 381
`.cpp` files under `./build.ps1 checkall` on GCC 16.2 (353 OK, 0 real fail), har genuinely-specialist
topic explicitly SPECIALIZED list mein. Lekin yaad rakhna: **file complete hona
aur aapka seekhna complete hona — alag cheezein hain.** Content likha hona kaafi
nahi, aapko woh code likhna, chalana, todna aur samajhna padega.
