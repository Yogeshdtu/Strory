# Build Status

Yeh repo batches mein banaya ja raha hai. Yahan track hota hai kya ban chuka hai.

---

## Gap-fix pass (post-PHASE 34) — part 1: content gaps — ✅ COMPLETE

**Kyun:** original spec ke against ek independent audit (audit files ki "NONE" pe
bharosa kiye bina) ne 7 gaps nikaale. Topics lagbhag sab covered the; gaps delivery
(Hinglish) aur kuch chhoti content holes mein the. Yeh part content gaps band karta hai.

**Toolchain change:** MinGW GCC 15.1 → **GCC 16.2** (GDB 17.2). Poora repo 16.2 pe
dobara verify kiya (neeche numbers). Purane lessons ke measured numbers "GCC 15.1"
label ke saath valid rehte hain.

**Kya kiya:**
- **Build:** `*.cpp23.cpp` naming convention — `build.ps1` aur `Makefile` aisi files ko
  apne aap `-std=c++23 -lstdc++exp` se build karte hain (`-lstdc++exp` MinGW pe
  `std::print` ke liye zaroori — measured link error). `checkall` ab `build/` (CMake
  output) skip karta hai.
- **C++23 (gap: survey-only, compile-verification deferred):**
  `22-MODERN-CPP/15-cpp23-in-practice.md` + `examples/09_cpp23_in_practice.cpp23.cpp` +
  `examples/10_cpp23_what_happens_next.cpp23.cpp`. 26 C++23 features probe kiye
  (GCC 16.2: 24 direct, `import std;` setup ke saath, `std::stacktrace` is build mein
  nahi). **Rule 2:** `flat_map` vs `map` measured — 55 keys pe map tez (0.91–0.93×),
  232k keys pe flat ~5×, random insert ~22× slow; `import std;` ~1.7× per file par
  7.8 s one-time module build. `import std;` ke baad `#include` = redefinition errors
  (verified trap). `15-exercises` → `16-exercises`; `05` survey ka "flat_map tez" claim theek kiya.
- **Namespaces (gap: audit `[✓]` sirf cheatsheet + linkage lesson pe tha):**
  `08-FUNCTIONS/14-namespaces.md` + `examples/08_namespaces.cpp` — apne namespaces,
  nested, alias, using-declaration vs directive, anonymous (`nm` se internal linkage
  dikhaya), inline (ABI versioning), ADL. `14-exercises` → `15-exercises` (+ namespace
  questions). `08/08` ka "ADL — folder 24" pointer theek (folder 24 ADL padhata hi nahi tha).
- **Challenge sections (gap: 7 exercises files mein nahi the):** 21, 22, 34, 35, 36,
  37, 45 + `20/18-problem-sets.md` — har folder mein 2–3 open-ended, measure-karo challenges.
- **"What happens next?" format (gap: 0 files):** `07`, `17`, `18`, `26` exercises
  mein naya part + `08/14` + `22/15` — saare jawab GCC 16.2 pe chala ke (vector growth
  pe throwing move → COPY, discarded `std::async` future ~200 ms block, joinable
  `std::thread` → terminate, etc.).
- **C++26 (gap: silently absent):** `WHAT-I-STILL-NEED-TO-LEARN.md` SPECIALIZED mein
  explicit row — GCC 16.2 probe: reflection (`-freflection`), contracts (`-fcontracts`),
  `#embed`, `inplace_vector`, pack indexing chalte hain; `hive`, `<hazard_pointer>`,
  `<rcu>`, `std::execution` is library build mein nahi.
- **Audits:** `CPP-COMPLETENESS-AUDIT.md` ke aakhri 2 `[~]` → `[✓]`, naya C++23 library
  row, namespaces row ab `08/14` cite karta hai; score table rows se recount kiya
  (purana table stale tha: 230 ✓ / 0 ~ / 0 ☐).
- **Pehle ki fixes (isi pass mein):** capstone `44/14` ka p99.9 regression explain +
  measured bitmap fix (extension); 3 `## Next` headings; stray `-p` dir.

**Repo state (GCC 16.2):** `./build.ps1 checkall` → **381 `.cpp` scanned, 353 OK,
0 real fail, 1 expected (`broken_on_purpose`), 27 skipped (`*.linux.cpp`)**. Links: 0 broken.

**Agla — part 2:** Hinglish pass (lesson prose + code comments), folder by folder.
Audit mein `09`–`12`, `16`–`22`, `24`–`28`, `30`–`36`, `44`, `46`–`49` ki prose aur
`18`–`22`, `28`, `41`, `46`, `47`, `49` ke code comments English-heavy mile.
`12-POINTERS` prose ho chuka hai.

---

## Batch 11 (part 6) — ✅ COMPLETE (PHASE 34) — the final gap audit — 🏁 COURSE STRUCTURALLY COMPLETE

**Kaam:** `WHAT-I-STILL-NEED-TO-LEARN.md` ke har section ko "NONE" pe le jaana,
aur genuinely-specialist topics ko explicitly SPECIALIZED list mein rakhna
(silently chhode nahi) — CLAUDE.md §7 ka final goal.

**Kya kiya:**

- **`CPP-COMPLETENESS-AUDIT.md`** — 9 stale `[~]`/`[ ]` markers verify karke fix
  kiye (earlier batches se bache the, content actually mojood hai):
  - `Namespaces` `[ ]→[✓]` — `24-COMPILATION-LINKING/05-linkage.md` (internal/
    external/**module** linkage, `static` at namespace scope, anonymous
    namespaces, API hygiene).
  - `if constexpr` `[~]→[✓]` — **dedicated lesson** `21-TEMPLATES/07-if-constexpr.md`.
  - `Branch prediction` `[~]→[✓]` — full treatment `31-CPU-ARCHITECTURE/04`
    (pipeline/BTB/TAGE, sorted-vs-unsorted **measured ~6–7×**).
  - `Scope & lifetime` `[~]→[✓]` — full storage-duration + object lifetime +
    `std::launder` + the 4 lifetime-extension cases in `25-OBJECT-MODEL`.
  - `<cmath>` / `<numbers>` `[ ]→[✓]` — `<cmath>` used + explained throughout;
    `<numbers>` in `22/04`.
  - C library headers `[ ]→[✓]` — `<cstring>` (`10/01` + `memmove` in `47`),
    `<cstdlib>` (`14/09`), `<cstdio>` (**dedicated** `04/11`–`12`).
  - `Google Benchmark` `[ ]→[✓]` — `35/08`–`09` + a shim example dir.
  - `Compiler Explorer workflow` `[ ]→[✓]` — **dedicated lesson**
    `33-COMPILER-OPTIMIZATION/02-godbolt-workflow.md`.
- **`HFT-COMPLETENESS-AUDIT.md`** — `constexpr` / compile-time computation
  `[~]→[✓]` (full treatment folder 21).
- **Two rows stay `[~]` — deliberately, not as gaps:** deducing `this` (C++23)
  and `<coroutine>`→`std::generator`. The *machinery* is covered lesson-level in
  folder 22; only the `-std=c++23` **compile step** is deferred (repo default is
  c++20). Both now cross-reference the **SPECIALIZED / DOMAIN-SPECIFIC** list,
  which got a new explicit row: *"C++23-only library types compile-verified
  (`std::generator`, `std::flat_map`, `std::mdspan`, deducing `this`)"* — with
  `std::expected` noted as the exception (compile-verified both ways in `23/05`).
- **`WHAT-I-STILL-NEED-TO-LEARN.md`** — all 5 `MAJOR … GAPS` sections now read
  **NONE**; the "Jab yeh dikhe" target template is met; status header → PHASE
  0–34, "COURSE STRUCTURALLY COMPLETE".

**Repo state:** 50 content folders (00–49); **378 `.cpp`**, `./build.ps1
checkall` → **350 OK / 0 real fail / 1 expected (`broken_on_purpose`) / 27
skipped (`*.linux.cpp`)**. Mojibake across the repo: 0.

---

## Batch 11 (part 5) — ✅ COMPLETE (PHASE 33) — the projects folder (LAST content folder)

| Folder | Files | Status |
|---|---|---|
| `49-PROJECTS/` | 5 lesson files (`01`–`05`) + 17 verified reference implementations (`examples/{beginner,intermediate,advanced}/`) | ✅ Full — all 16 non-linux examples compile strict-clean + run + assert; `05_epoll_server.linux.cpp` is `*.linux.cpp` (skipped, hand-reviewed) |

**`49-PROJECTS` (PHASE 33)** — connected, difficulty-ordered projects, per the
spec's "concepts ko projects mein jodo". Each project: **spec → milestones
(v1→v2→v3) → concepts (folder links) → tests → extensions.**

- **`01` beginner (6)** — expression calculator (shunting-yard, `/0` handled) ·
  number-guess (I/O split from logic, `<random>` done right, binary-search
  optimal play) · student manager (`struct`+`vector`, invariants,
  `sort`/`accumulate`) · expense tracker (**integer money**, `partial_sort`
  top-N) · word stats (`istream&` interface, `tolower` on `unsigned char`) ·
  INI config parser (typed getters + defaults, **line-numbered errors**,
  round-trip).
- **`02` intermediate (5)** — warehouse inventory (invariants + **append-only
  log + `replay()`** = event sourcing) · bank simulation (**atomic transfer**,
  rational-rate integer interest, a global invariant checked in a 20k-op
  property test) · CSV parser+query (**4-state field machine**: quotes,
  embedded commas/newlines; byte-offset errors) · log analyzer (malformed →
  skip+count, per-minute buckets, running-baseline spike flag) · persistent KV
  store (**write-ahead log** with len+CRC, replay on open, **compaction** via
  temp-file + `rename`, torn-tail recovery).
- **`03` advanced (6)** — custom `Vector<T>` (raw storage + placement `new`,
  Rule of 5 via copy-and-swap, `move_if_noexcept` relocate, **strong exception
  guarantee**, contiguous iterators) · 3 allocators (arena / fixed-size pool /
  segregated size classes — **measured ~20–28× vs `::operator new` at `-O2`**,
  MinGW CRT caveat noted) · thread pool (`packaged_task`/`future`, exception
  propagation, idempotent shutdown) · concurrent queues (mutex bounded ·
  **lock-free SPSC ring, no CAS** · **MPSC Vyukov, `XCHG` tail** — stress tests
  prove in-order/no-loss) · **epoll echo server** (`EPOLLET` drain loops,
  partial-write buffering, self-pipe shutdown — Linux only) · JSON parser
  (recursive descent, `std::variant` tree, **`line:col` errors**, **depth
  limit** = stack-overflow guard, serializer + round-trip test).
- **`04` guidelines** — v1-first, structure (I/O at the edges), error-handling
  table, testing (invariant / property / golden / determinism-replay),
  measuring, and a full **code-review checklist**.
- **`05`** — points at folder 44 (the HFT project track) and maps each advanced
  project onto its HFT counterpart.

**Verification:** examples are in subdirs → `build.ps1 folder 49-PROJECTS` N/A;
covered by `checkall` (recursive). **Mojibake:** 0.

**Folders 00–49 are now all built.** Only the **final gap audit** (a
cross-repo review to drive every `WHAT-I-STILL-NEED-TO-LEARN.md` section to
"NONE" / explicit "SPECIALIZED") remains.

---

## Batch 11 (part 4) — ✅ COMPLETE (PHASE 33) — the quick-reference layer

| Folder | Files | Status |
|---|---|---|
| `48-CHEATSHEETS/` | 13 quick-reference sheets (`01`–`13`) + `00-README.md` | ✅ Full — markdown-only reference folder (no `.cpp`); mojibake 0; next-chain intact |

**`48-CHEATSHEETS` (PHASE 33)** — the **lookup layer**: a distillation of folders
`01`–`47` into 13 scannable sheets, each cross-linking its deep source folder.

- **`01` syntax** — decls/init forms, control flow (incl. `if`-init, structured
  bindings), functions + pass-by, lambdas + capture traps, classes (Rule 0/3/5),
  templates + concepts, enums/namespaces/aliases, error handling.
- **`02` STL containers** — memory model / complexity / iterator+ref
  **invalidation** per container, views & spans, "pick one" decision list.
- **`03` `<algorithm>`/`<numeric>`/ranges** — grouped by job (scan, binary
  search, modify, sort, heap, set ops, numeric, ranges), erase–remove idiom.
- **`04` complexity tables** — growth rates, sorting matrix, structure ops,
  graph algos, problem patterns, and the amortized-vs-worst-case (p99.9) trap.
- **`05` compiler flags** — warnings (this repo's strict set), `-O` levels,
  `-march`, sanitizers, codegen knobs with trade-offs, linking, the 3 builds
  you actually use.
- **`06` GDB** — run/step, breakpoints + watchpoints + `commands`, inspect
  (`x`, `p`, slices), threads (deadlock signature), `-O2` debugging reality, TUI,
  `.gdbinit` starter.
- **`07` perf/valgrind/sanitizers** — `perf stat` signal→fix table, `perf` for
  latency bugs, valgrind tools, the sanitizer matrix + CI golden build, a
  no-dependency benchmark harness.
- **`08` HFT tuning checklist** — BIOS → kernel cmdline (`isolcpus`/`nohz_full`/
  `rcu_nocbs`) → IRQs → memory (`mlockall` + prefault + hugetlbfs) → pinning →
  network (folder 42) → warm-up → continuous `perf` verification + a gotchas table.
- **`09` memory ordering** — `atomic` vs `volatile`, the 6 orders, the
  release/acquire handoff, fences, `cmpxchg` weak/strong, ABA + fixes, lock-free
  vs wait-free, x86-TSO vs weak.
- **`10` latency numbers** — the core ladder (L1 ~1 ns … DRAM ~60–100 ns …
  same-DC RTT ~10–100 µs), bandwidth, sizes, derived rules of thumb; consistent
  with folder 46/12.
- **`11` UB catalog** — memory/pointer, integer/arithmetic, language, STL, and
  the "worked on my machine" traps; how to catch each.
- **`12` HFT glossary** — order book, order types, participants & economics,
  microstructure/strategy, infrastructure, protocols, regulation/venues.
- **`13` interview revision** — 13 one-page sheets (one per topic, night-before
  pass) + the 6 behavioural stories + the make-a-market drill.

**No `.cpp`, no new coverage rows** — this folder *condenses* folders 01–47.
**Mojibake sweep:** 0.

---

## Batch 11 (part 3) — ✅ COMPLETE (PHASE 33) — the practice bank

| Folder | Files | Status |
|---|---|---|
| `47-CODING-PROBLEMS/` | 10 themed problem files (**250 problems**) + `11-solutions/` (README + 10 per-category worked-code writeups) + 10 verified runnable examples | ✅ Full — `./build.ps1 folder 47-CODING-PROBLEMS` → **10/10 OK** under strict warnings; all 10 binaries run + assert |

**`47-CODING-PROBLEMS` (PHASE 33)** — a graded **practice bank**, one file per
theme, every problem = statement + `Pattern:` hint + `<details>` (approach +
complexity). "Pehle khud solve karo, phir kholo."

- **`01` basics (30)** — integer math, bit tricks; the traps: reverse-int
  overflow (check *before* the multiply), `nCr` multiply-then-divide,
  overflow-safe midpoint, float sum order (Kahan).
- **`02` arrays & strings (40)** — two-pointer, sliding window, prefix sum +
  hash map, in-place index-as-hash; Kadane, trapping rain water, first missing
  positive, KMP.
- **`03` pointers & memory (25)** — arena / free-list / slab allocators,
  `unique_ptr` from scratch, `memmove`, intrusive lists, Floyd, tagged pointers,
  hazard pointers.
- **`04` OOP & design (20)** — Rule of 3/5 + copy-and-swap, virtual-dtor leak,
  LRU class, `ScopeGuard`, type-erased `Function`, order-book / matching-engine
  ownership, `Result<T,E>`.
- **`05` STL (35)** — right-container choice, iterator-invalidation quiz,
  `reserve` vs `resize`, `string_view` pitfalls, strict-weak-ordering,
  `flat_map`, custom hashes, `pmr`.
- **`06` templates (20)** — traits, `requires` vs `void_t` detection, tag
  dispatch vs `if constexpr`, CRTP, variadic folds, mini `tuple` / `variant`,
  expression templates.
- **`07` concurrency (25)** — blocking queue, thread pool, deadlock + 3 fixes,
  false sharing, `std::barrier`/`latch`, MPSC design, the ARM-only
  memory-ordering bug.
- **`08` lock-free (15)** — TTAS spinlock, Treiber stack + ABA + tagged-pointer
  fix, SPSC ring, Vyukov MPSC, seqlock, hazard pointers / RCU, `consume` (why
  avoid), sharded vs contended counter.
- **`09` optimize-this (20)** — slow code + a **target**, grounded in folders
  32/43's *measured* numbers: column→row major, AoS→SoA, `map`→flat→direct
  index, `endl` flush, division-by-constant (~13–17×), branch mispredict,
  `shared_ptr`/`std::function` on the hot path, false sharing, denormals.
  Rule 2 carried through (SW-prefetch is often *slower*).
- **`10` HFT (20)** — fixed-width + ASCII price parsing, O(1) book add/cancel +
  BBO, price-time matching + fills, feed gap detection + A/B arbitration, the
  ordered 5-check risk gate, SPSC handoff, latency histogram, `rdtsc` pitfalls,
  timer wheel, defending a p99.9.

**Examples (`examples/*.cpp`, 10, one per theme, all run + assert):**
`01_basics_kata` · `02_arrays_strings_kata` · `03_arena_and_pool` ·
`04_scope_guard_and_result` · `05_flat_map` · `06_crtp_and_detect` ·
`07_blocking_queue` (N-prod/M-cons checksum) · `08_seqlock` (2M writes,
**torn reads = 0**) · `09_optimize_row_vs_col` (**measured ~45–70× at `-O2`** —
Rule 2: bigger than the textbook 7×, cache + TLB + vectorization stack) ·
`10_order_book_ops` (add/cancel/match + `crossed()` invariant).

**No new C++-language coverage** — this folder *drills* folders 01–46.
**Mojibake sweep:** 0.

---

## Batch 11 (part 2) — ✅ COMPLETE (PHASE 33) — interview prep

| Folder | Files | Status |
|---|---|---|
| `46-INTERVIEW-PREP/` | 20 lessons (`01`–`20`) + 8 verified coding examples + 4 design docs + 4 mock transcripts | ✅ Full — `./build.ps1 folder 46-INTERVIEW-PREP` → **8/8 OK** under strict warnings |

**`46-INTERVIEW-PREP` (PHASE 33)** — a **layered** question bank + interview
strategy, per the spec's "don't jump straight to HFT interviews, build
toward them" rule:

- **`01`** — how HFT interviews work: firm types (market makers vs
  latency/prop vs quant vs exchange infra), the pipeline (recruiter →
  OA → phone → superday), the 3 grading axes (C++ depth / systems-latency
  intuition / problem-solving), how to talk out loud, red flags,
  questions to ask.
- **`02`–`14`** — Layer 1–13 Q&A banks, beginner → HFT: types/control/
  functions · pointers/memory/RAII · classes/vtables/virtual-dtor · STL/
  complexity/cache · templates/SFINAE/concepts/CRTP · value categories/
  move/forwarding/Rule-of-5 · threads/mutex/cv/deadlock · atomics/memory-
  ordering/happens-before/lock-free · Linux syscalls/scheduling/mmap ·
  TCP-UDP/multicast/epoll/kernel-bypass · CPU/branch-prediction/cache/
  false-sharing (with the latency numbers to memorize) · profiling/
  optimization/latency-vs-throughput · HFT architecture (feed handler,
  order book, risk, OMS, the tick-to-trade pipeline). Every answer
  cross-refs the source course folder.
- **`15`** — system-design rounds: the arc (clarify → constraints →
  interfaces → components → hot path → bottleneck → failure modes →
  trade-offs → measure) + 4 worked designs + a rubric.
- **`16`** — brainteasers & probability (Optiver/Jane Street style): EV,
  the make-a-market game (two-sided quote + update on the trade +
  inventory), classic puzzles (Monty Hall, 100 seats, HH vs HT expected
  wait = 6 vs 4, 25 horses, ants on a stick), Fermi estimation, mental-
  math drills, Kelly / volatility-drag intuition.
- **`17`** — C++ trick questions: 30+ classic traps (unsequenced
  modification, signed overflow UB, dangling `const&`/`string_view`,
  slicing, most-vexing-parse, `vector<bool>`, `reserve` vs `resize`,
  `printf` format mismatch, macro precedence, one-past-the-end pointer)
  each with the **right answer** and *why*.
- **`18`** — behavioural round: STAR, the 6 stories to prepare (hard bug /
  optimization / disagreement / proud build / "didn't know" / why HFT),
  handling "I don't know" live, discussing this course's projects
  credibly, what NOT to say.
- **`19`** — 4 full mock scripts with strong/weak answer tracks + rubrics.
- **`20`** — HFT resume & projects: structure, bullet formula (built X
  using Y achieving *measured* Z vs baseline), the 7 projects that matter
  (order book, matching engine, SPSC queue, feed handler, pipeline
  optimization, mini HFT engine, async logger — all folders 39–44),
  GitHub hygiene, common resume mistakes.

**Examples:** `examples/*.cpp` — 8 classic coding problems (reverse list,
LRU, **lock-free SPSC ring**, fixed-point price parse, object pool, L2
top-of-book, atoi + overflow clamp, O(1) moving average + division-free
signal), each with an assertion `main()`, complexity notes, and the HFT
follow-up. `examples/design/` — 4 system-design worked answers.
`examples/mocks/` — 4 transcripts with rubrics.

**No new C++-language coverage** — this folder *tests* folders 01–45.
Completes `HFT-COMPLETENESS-AUDIT.md` Section L. **Mojibake sweep:**
6 → **0**.

---

## Batch 11 (part 1) — ✅ COMPLETE (PHASE 33) — wrap-up folders shuru

| Folder | Files | Status |
|---|---|---|
| `45-DEBUGGING/` | 13 lessons (`01`–`13`) + 5 standalone examples + 10 "find & fix" buggy programs | ✅ Full — `./build.ps1 folder 45-DEBUGGING` → **5/5 OK** under strict warnings; buggy programs compile-clean in `checkall` |

**`45-DEBUGGING` (PHASE 33)** — a **skill folder** (jaise 20-DSA / 35-PROFILING):
skim once, return per-bug. Covers the full debugging toolchain:

- **Mindset** (`01`) — hypothesis→test loop, reproduce-first, `git bisect`,
  root cause ≠ symptom. Mirrors folder 43's optimization loop.
- **GDB** (`02`–`03`) — break/step/print/bt/frame/finish; watchpoints,
  conditional breakpoints, `commands`, `tbreak`, `catch throw`, scripting.
  **Real transcripts** captured on this box (MinGW GCC 15.1.0, GDB 16.3)
  against `01_gdb_practice.cpp`.
- **Crashes** (`04`) — SIGSEGV/SIGABRT/SIGFPE/SIGBUS, exit `128+sig`, core
  dumps (`ulimit -c`, `coredumpctl`, `bt full`), stack overflow, Windows
  WER note.
- **Optimized builds** (`05`) — `-O2 -g`, `<optimized out>`, garbage
  function-entry values (real transcript: `accts` = "length 364181"),
  `factorial(5)` constant-folded away, inlined frames, `-Og`,
  frame-pointer, UB-only-at-`-O2` table.
- **Sanitizers** (`06`) — ASan (shadow memory, redzones, quarantine),
  UBSan, TSan (happens-before), MSan; combine rules; CI golden build.
  (MinGW has no libasan/libubsan/libtsan → Linux/WSL workflow; expected
  output in the example footers.)
- **Valgrind** (`07`) — memcheck (definitely/indirectly/possibly lost),
  helgrind, DRD, massif; vs-sanitizers table.
- **Multithreaded** (`08`) — `thread apply all bt`, deadlock signature
  (≥2 in `__lll_lock_wait`), missing-unlock vs cycle, heisenbugs.
- **Reverse debugging** (`09`) — `rr record`/`replay`, `watch` +
  `reverse-continue` = "who set this value" in one step; `--chaos`.
- **Logging** (`10`) — levels, structured logs, log-and-throw antipattern,
  **HFT async logging**: hot thread enqueues a POD (~20–40 ns) to an SPSC
  ring, background thread formats+writes; queue-full → drop + count,
  never block; binary log, mmap ring, pinned logger core.
- **`perf` for bugs** (`11`) — off-CPU analysis, syscall storms
  (`perf trace`), page-fault storms, false sharing (`perf c2c`) — "correct
  but 100× slow" bugs.
- **Bug catalog** (`12`) — 40+ patterns across Memory / Concurrency /
  Logic / Integer / Lifetime, each **symptom → tool → fix**, cross-refs
  throughout; a symptom→family quick index.

**Examples:** `01_gdb_practice.cpp` (bug-free, for practice) + `02_segfault_debug.cpp`
(NULL-deref) + `03_memory_bugs.cpp` (6-mode menu: leak/uaf/oob/double/uninit/stackuaf)
+ `04_race_debug.cpp` (data race; `--safe` = atomic) + `05_deadlock_debug.cpp`
(AB/BA; default safe, `--deadlock` hangs) + `06_buggy_programs/` (10 drills,
compile-clean, runtime-buggy, `// BUG/SYMPTOM/TOOL/FIX` footers).

**Rule-2 note carried through:** `10_data_race.cpp` races ~15–20% of runs
at `-O0` but **hides at `-O2`** (register promotion of the RMW) — taught,
not hidden. **Mojibake sweep:** 34 → **0**.

---

## Batch 10 (part 9) — ✅ COMPLETE (PHASE 32) — 🏆 THE CAPSTONE

| Folder | Files | Status |
|---|---|---|
| `44-HFT-PROJECTS/` | 15 lessons (`01`–`15`) + 12 example drivers + 11 shared `mh_*.hpp` headers | ✅ Full — `./build.ps1 folder 44-HFT-PROJECTS` → **12/12 OK** under strict warnings |

**`44-HFT-PROJECTS` (PHASE 32) — the course climax.** Everything from
folders 01–43 assembled into one `MiniHftEngine`:

```
MarketData → Parser → L2Book → Strategy → Risk → OMS → Venue → fills → PnL
```

Single-threaded, **fully deterministic** — no wall clock anywhere; every
timestamp comes from the event stream, so replay is byte-identical.

**Reuse over rewrite (the capstone's whole point):**
- `mh_types.hpp` `#include`s folder-40's `matching_engine.hpp` — the
  `Price`/`Qty`/`OrderId` types **and** the `MatchingEngine` itself, used
  as "the venue" via `ExecutionSimulator`.
- `07_spsc_queue.cpp` `#include`s folder-41's `spsc_queue.hpp` for the
  wire→engine MdMessage hand-off.
- book / pools / strategy follow folder 39 / 14 / 36 / 43 patterns.

**The capstone optimization (folder-43 methodology, applied end-to-end):**
`MiniHftEngine` is `template <class Venue>`.
- `NaiveEngine` = `ExecutionSimulator` (folder-40 `std::map` `MatchingEngine`
  — one tree insert + one `std::list`-node `malloc` per market message).
- `OptimizedEngine` = `FastVenue` (`mh_fast_venue.hpp`) — flat-array
  aggregate book + per-level FIFO + IOC sweep.

Profiling found the venue mirror was the `book` stage's bulk (~150 ns/msg).
`FastVenue` halves it (~67 ns/msg) → **~1.5–1.6× end-to-end** (this
unpinned Zen 2 box; p50/p99 the reliable comparison — the `max` outlier is
scheduler jitter, 41/13). **Correctness gate first:** `12_integration_tests.cpp`
proves `NaiveEngine == OptimizedEngine` byte-for-byte (fills, qty, P&L,
position) across **5 seed/config combos**, both deterministic, invariants
(|position| ≤ risk max, zero sequence gaps, OMS always settles) held.

**Per-component measured (this box, ratios):**
- L2Book apply ~17 ns/msg; BBO matches a `std::map` reference **exactly**
- FixedPool alloc+free **~2.0× (p50) / ~5.0× (p99.9)** vs `new`/`delete`
- ObjectPool: stale handle → `nullptr` **even after the slot recycles**
  (use-after-free guard)
- SPSC hand-off **~6 M msg/s**, every message in order
- RiskEngine **14/14** checks; rate-limit does NOT trip the kill switch
- OMS accounting always settles (no leaked orders, no phantom fills)
- Feed parser v1 vs v3: **~1.0× at `-O2`** — honest Rule-2 null (compiler
  already optimal at this scale, cf. 43/08)

**No alpha.** `SpreadCrossStrategy` is a mechanical rule to exercise the
pipeline; its backtest P&L has zero predictive meaning (37 SPECIALIZED
list). **The HFT build track (folders 36–44) is now COMPLETE.**
**Mojibake sweep:** 16 → **0**.

---

## Batch 10 (part 8) — ✅ COMPLETE (PHASE 31)

| Folder | Files | Status |
|---|---|---|
| `43-HFT-OPTIMIZATION/` | 17 lessons (`01`–`17`) + 8 examples (`pipeline.hpp` shared header + `02_profile_analysis.sh` + 7 portable `.cpp`) | ✅ Full — `./build.ps1 folder 43-HFT-OPTIMIZATION` → **7/7 OK** under strict warnings; benchmarks measured |

**`43-HFT-OPTIMIZATION` (PHASE 31)** — the end-to-end optimization
*methodology* folder: measure → profile → hypothesize → change (one) →
re-measure → **explain what changed and why**, plus a **correctness gate
before the speedup gate**. Folder 36 taught the individual techniques in
isolation; this folder applies them to a **connected tick-to-order
pipeline** and attributes every delta.

The spine is `pipeline.hpp` with two full pipelines on one deterministic
feed:
- **`PipelineV0`** (realistic-naive) — `substr` + `std::stod` parse,
  `std::map<double>` + `std::list` book, `std::deque` re-sum SMA,
  `std::string` order encode.
- **`PipelineV3`** (optimized) — hand integer parse + fixed-point price,
  flat-array book + cached top-of-book + dense-id direct index,
  ring-buffer running-sum SMA with **zero division**, fixed-layout POD
  encode.

V3's signal math is the **exact integer equivalent** of V0's float
cross-condition. `04_before_after.cpp` runs both in one process and
checks the **order-fire tick stream is identical BEFORE reporting
speedup** (110/110 identical this run — fixed-point reproduced float
exactly).

**Measured (this box — AMD Ryzen 7 4700U Zen 2, unpinned; ratios/shapes,
absolutes ~20% run-to-run):**
- end-to-end **~60–80×** (v0 ~1.9–2.7 µs/tick → v3 ~25–45 ns/tick),
  parse ratio ~25×, book ratio ~25–29×
- division: `div` → shift **~17×**, → magic-multiply (const) ~13×,
  → reciprocal-multiply (loop-invariant) ~7× — *only the divide timed*
  (the CLAUDE.md warning about a prior benchmark hiding `%` in a loop
  explicitly heeded)
- struct: field reorder 24 B → 16 B (34% smaller, 0 behaviour change);
  fat-struct scan → SoA **~8×** (memory-traffic-bound)
- fixed-point: `0.1` added 10× (double) = `0.99999999999999988898` ≠ 1.0;
  int64 exact — correctness is the point, the ~20% speed edge is a bonus
- **hot/cold split ~1% on this box — an honest Rule-2 null result**
  (frontend is not the bottleneck at this scale; carries 36/12's finding).
  `[[gnu::cold]]` verified in asm to move code to `.text.unlikely`;
  attribute cost is 0 so it's kept regardless.

`02_profile_analysis.sh` (a `perf stat`/`record`/`annotate`/`c2c`
bottleneck-hunt with a signal→lesson map) is Linux-only — `bash -n`
syntax-checked, not run (no `perf` on Windows). **Mojibake sweep:** 298
stray Devanagari/Cyrillic homoglyphs found across the drafts and fixed →
**0** remaining (folder + audits clean).

---

## Batch 10 (part 7) — ✅ COMPLETE (PHASE 30)

| Folder | Files | Status |
|---|---|---|
| `42-HFT-NETWORKING/` | 17 lessons (`01`–`17`) + 8 examples | ✅ Full — 6/8 examples Linux-only (hand-reviewed, not compile-verified on this box) |

**`42-HFT-NETWORKING` (PHASE 30)** — the full wire-to-wire networking
path. An `INetworkReceiver` abstraction (`08_bypass_abstraction.hpp`)
makes the kernel-bypass backend (kernel-socket now; Onload/ef_vi/DPDK
config-swappable later) a deployment decision rather than an
application rewrite. The full bypass ladder explained with an honest
trade-off table (latency vs application-rewrite vs vendor-lock):
kernel-socket → `SO_BUSY_POLL` → Onload (`LD_PRELOAD` transparent
intercept) → ef_vi (raw pre-post/poll NIC queues) → DPDK (poll-mode
driver, hugepages, NIC fully removed from OS).

**Two genuine technical fixes to earlier limitations, found and
corrected during this folder's own development:**
1. **Hardware timestamping** — 30/09 could only measure RX-kernel-ts
   vs app-steady_clock (different epochs, relative-jitter-only). This
   folder takes TX timestamps too (`MSG_ERRQUEUE`, async retry-poll)
   from the SAME kernel clock domain as RX — the subtraction is now a
   genuinely meaningful absolute latency number.
2. **TCP writev claim, corrected before shipping** — a draft comment
   claimed `writev`'s single-packet win would recreate 30/03's ~40ms
   Nagle/delayed-ACK stall on a real network. Caught on review: that
   specific stall requires the SENDER's Nagle to be ON, and this
   example sets `TCP_NODELAY` on both ends specifically to prevent it
   — corrected to the accurate mechanism (writev's real win is a
   smaller, consistent one: fewer syscalls, fewer packets/ACKs).

**NIC/IRQ tuning** as a real, `bash -n` syntax-checked script (ring
buffers, coalescing, GRO/LRO off, RSS+per-queue IRQ pinning kept OFF
41/08's hot-path cores, pause frames, sysctls, verify-by-drop-counters).
**TCP order-gateway tuning**: `TCP_NODELAY` both ends, `writev` to
combine header+body into one packet, keepalive triple +
`TCP_USER_TIMEOUT` (fail-closed in seconds, 37/13's principle). A full
**wire-to-wire capstone** (`06_wire_to_wire.linux.cpp`) chains kernel
timestamps across 2 network hops (market-data receive, order send) +
an internal-processing stage, all in one shared clock domain — an
explicit breakdown, not one opaque total.

**Majority Linux-only, hand-reviewed not compile-verified:** this dev
box is Windows/MinGW with no WSL installed, so the 6 `.linux.cpp`
examples (multicast sockets, `SO_BUSY_POLL`, `SO_TIMESTAMPING`,
`MSG_ERRQUEUE`, TCP tuning) could not be compiled or run here —
consistent with 29/30's established `.linux.cpp` pattern
(`./build.ps1 folder`/`checkall` skip them by design). They were
written carefully, reusing 30's already-verified structural patterns,
and reviewed by hand for the usual pitfalls: a `-Wsign-compare` risk
(`ssize_t` vs `sizeof`, fixed with explicit casts in 3 spots), an
unbounded-blocking-`recv()` hang risk (fixed with `SO_RCVTIMEO` safety
nets in 3 files), and empty-vector `.back()` UB in report functions
(guarded in 3 files). Every number in every example is presented as an
explicit `EXPECTED (... NAHI napa gaya)` estimate — never fabricated as
"measured." **Mojibake sweep:** 0 → **0** (clean on first pass).

---

## Batch 10 (part 6) — ✅ COMPLETE (PHASE 29)

| Folder | Files | Status |
|---|---|---|
| `41-HFT-CONCURRENCY/` | 14 lessons (`01`–`14`) + 8 examples + 2 shared headers | ✅ Full — verified, benchmarks measured |

**`41-HFT-CONCURRENCY` (PHASE 29)** — turns 40's single-threaded matching
engine into a real multi-threaded, thread-per-stage HFT pipeline. A
production `SpscQueue<T,Capacity>` (28's padded+cached-index design,
packaged as a reusable template), a working simplified **LMAX
Disruptor built from scratch** (single-producer, N *independent*
fan-out consumers — every consumer sees every event, unlike an SPSC
queue's work-distribution — gating sequences, natural batching), a
`Seqlock<T>` snapshot mechanism, and a genuine capstone
(`08_pipeline_demo.cpp`): a real 3-stage pipeline (feed/match/sink)
running **40's actual `MatchingEngine`**, not a toy stand-in.

**Two headline measured findings:**
1. **Seqlock beats `std::shared_mutex` by ~2500-3500×** under an
   adversarial max-rate-writer stress test — far more dramatic than
   28's original ~80-100× under gentler conditions (`shared_mutex`
   collapses to ~0.04 M reads/s under this contention; both `torn=0`).
2. **Core-pinning reduced hiccup FREQUENCY but produced a WORSE single
   max-delay outlier** (511 vs 782 hiccups / 20M iterations, but max
   15.6M ticks vs 829K unpinned) — an honest, measured demonstration
   that thread *affinity* is not the same thing as core *isolation*
   (real tail-latency control needs `isolcpus`/`nohz_full`, already
   built in folder 29).

**A real toolchain quirk found and fixed:** `std::thread::native_handle()`
on this MinGW posix-threading build returns a `pthread_t`, not a Win32
`HANDLE` — passing it straight to `GetThreadTimes()` fails silently
(`ERROR_INVALID_HANDLE`). Fixed via `DuplicateHandle(GetCurrentThread())`
called from inside the thread, needed to measure the REAL per-thread
CPU-time half of the busy-spin-vs-blocking trade-off (28/06 had only
measured latency).

**Examples portable `.cpp`** (Windows-native `<windows.h>` used
deliberately in 05/06 — Linux equivalents already in 29) —
(`./build.ps1 folder 41-HFT-CONCURRENCY` → **8/8 OK**). Measured on
**AMD Ryzen 7 4700U (Zen 2, ~2 GHz, Windows/MinGW, unpinned, 8 logical
cores)** — tails here are UNUSUALLY jitter-dominated (see below):
- `02` SPSC bench: **hand-off p50 400.8 / p99.9 1,880,224.6 ns**;
  throughput 16.1 M msg/s.
- `03` Disruptor: 4M events, 2 independent consumers, **both process
  ALL 4M, 0 mismatches**, ~11% of reads were multi-event batches.
- `04` seqlock vs shared_mutex: **seqlock ~103-129 M reads/s vs
  shared_mutex ~0.036-0.041 M reads/s** (torn=0 both).
- `05` spin/block/hybrid: **p50 210.4 / 8997.1 / 7354.0 ns; CPU ~99.8% /
  27.9% / 24.9%** of one core respectively.
- `06` core pinning: **unpinned p99 140 / max 829,657 ticks; pinned p99
  80 / max 15,600,420 ticks** — fewer hiccups, one huge outlier.
- `07` async logger: **naive 1082.1 ns vs async 70.1 ns p50 (~13.9× mean)**.
- `08` pipeline capstone: **end-to-end p50 971.8 ns** (394547 trades from
  500000 orders through the REAL `MatchingEngine`); tail OS-jitter-
  dominated — a slower feed pace made p99 WORSE not better (longer test
  duration = more scheduler-jitter exposure, not more backlog — a
  genuine counter-intuitive Rule-2 finding, documented in lesson 13).

**Mojibake sweep:** 0 → **0** (clean on first pass).

---

## Batch 10 (part 5) — ✅ COMPLETE (PHASE 28)

| Folder | Files | Status |
|---|---|---|
| `40-MATCHING-ENGINE/` | 17 lessons (`01`–`17`) + 8 examples + 2 shared headers | ✅ Full — verified, benchmarks measured |

**`40-MATCHING-ENGINE` (PHASE 28)** — price-time-priority matching on top
of 39's order book. All 4 order types (Limit/Market/IOC/FOK), 3
self-trade-prevention modes (CancelNewest/CancelOldest/CancelBoth),
deterministic event sequencing (a single shared monotonic `seq` counter
spans both order arrivals and trades), and event-sourcing replay.

**Central correctness finding:** a naive FOK precheck ("sum total resting
qty at crossable levels, check `>= target`") silently violates FOK's
all-or-nothing contract once combined with self-trade prevention — STP
can skip (`CancelOldest`) or abort (`CancelNewest`/`CancelBoth`) on
self-owned liquidity that the naive sum still counted, producing a
*partial* fill on an order that promised "full or nothing." Fixed with
an STP-mode-aware precheck (`available_qty()`) that walks the exact same
priority order the real match uses. Verified two ways: a hand-crafted
test demonstrating the exact divergence (naive sum 70 ≥ target 50 →
would wrongly PASS; true available 30 < 50 → correctly REJECTS), and a
30000-command fuzz run against an independent `O(n)` reference engine
(`RefEngine`) whose own FOK precheck uses a *completely different*
strategy (copy-the-state, run-a-practice-match, discard) — **0
disagreements, 0 self-trade leaks, 0 FOK violations across 3774 FOK
orders hit**.

**Examples portable `.cpp`** (`./build.ps1 folder 40-MATCHING-ENGINE` →
**8/8 OK**). Measured on **AMD Ryzen 7 4700U (Zen 2, ~2 GHz, Windows/
MinGW, unpinned)** — ratios/shapes port, tail absolutes don't:
- `06` tests: **43/43 pass** (basic matching + FIFO, leftover-rests-own-
  side, market sweep, IOC, FOK, **FOK+STP interaction**, 3 STP modes,
  duplicate-id/cancel-nonexistent/replace).
- `07` fuzz: **30000 commands, 0 disagreements** vs an independent
  reference engine; order-type mix hit FOK=3774 IOC=3757 Market=3683.
- `05` event sourcing: identical 20000-command log replayed into two
  independent fresh engines — **byte-identical** (4342 trades,
  resting_count=5245, best_bid/ask match).
- `08` bench (N=150000, `-O2`): **ALL ops p50 140.3 / p99.9 1763.4 ns**;
  Limit submit p50 160.3 but p99.9 6171.7 ns (new-price-level allocation
  — same `std::map`+`std::list` tail-latency mechanism as 39's V1, dobara
  measured, confirmed generic not folder-specific); Market submit p50
  250.5 ns (multi-level sweeps); IOC/FOK submit p50 ~40-50 ns (often an
  early-exit no-cross/reject path); cancel p50 60.1 ns (fastest, no
  allocation). Throughput ~5.63M ops/sec (single-threaded, this workload
  mix, explicitly caveated as an average that hides the tail).

**Traps caught during development:** a use-after-erase dangling-reference
bug (reading `resting.qty` after `std::list::erase()` had already freed
the node) — caught before it shipped, fixed by capturing the qty into a
local before erasing. The FOK+STP precheck bug above.

**Mojibake sweep:** 1 → **0** stray Devanagari tokens.

---

## Batch 10 (part 4) — ✅ COMPLETE (PHASE 27)

| Folder | Files | Status |
|---|---|---|
| `39-ORDER-BOOK/` | 17 lessons (`01`–`17`) + 9 examples + 5 shared headers | ✅ Full — verified, benchmarks measured |

**`39-ORDER-BOOK` (PHASE 27) — "HFT ka sabse classic interview question
aur sabse important data structure hai":** built **3 times** on an
identical workload, measured each time (the spec's mandatory process).
V1 `std::map`+`std::list`+`unordered_map` (correct baseline) → V2 sorted
`std::vector`+`std::deque` (**a genuine measured regression — overall
WORSE than V1**, root-caused: Add improved 3.9× via contiguous-array
access, but the order-id index can no longer store a stable iterator
across vector mutation, so cancel/execute falls back to an `O(level
size)` linear scan) → V3 tick-indexed flat `std::array` + intrusive
arena-linked-list (`uint32_t` index links, not pointers) + a custom
tombstone-based open-addressed flat hash for order-id lookup (wins every
metric). Price-time priority (FIFO) proven identical across all three via
an `ids_at_price()` accessor. Explicit top-of-book caching in V3 (V1/V2
get it "free" from container invariants), honestly characterized as
"typical O(1), worst-case O(NUM_LEVELS) scan" — not oversold. Bounded
price-range and order-capacity trade-offs documented, not hidden.

**Examples portable `.cpp`** (`./build.ps1 folder 39-ORDER-BOOK` →
**9/9 OK**). Measured on **AMD Ryzen 7 4700U (Zen 2, ~2 GHz, Windows/
MinGW, unpinned)** — ratios/shapes port, tail absolutes don't:
- `02` V1: **ALL ops p50 140.3 / p99.9 1502.9 ns**; Add p99.9 10700.3 ns
  (new-price-level = tree-node allocation + rebalance).
- `04` V2: **ALL ops p50 150.3 / p99.9 2745.2 ns — WORSE than V1**; Add
  p99.9 2755.2 ns (3.9× better than V1) but Execute/Cancel p50 360.7 ns
  (1.8× worse than V1's 200.4 ns) — the net effect is negative because
  Execute+Cancel are ~29% of the workload.
- `06` V3: **ALL ops p50 120.2 / p99.9 661.3 ns** — wins every metric;
  Add p99.9 531.0 ns, Execute/Cancel p50 130.2 ns.
- `07` comparison suite: same run, same workload — **p99.9 ratio V1/V3 =
  2.6×**; cross-version state (order_count, best_bid, best_ask,
  best_bid_qty) **exactly identical** across all three after 200000 ops.
- `08` tests: **58/58 pass** (7 scripted scenarios × 3 versions + 4
  seeded cross-version-equivalence runs, checked after every op).
- `09` fuzz: **30000 ops, ~3000 deliberately-invalid injected, 0
  disagreements** vs an independent O(n) reference model.

**Real bug caught during test-writing:** `best_bid()` called without
`has_bid()` on an out-of-range V3 test crashed on an out-of-bounds
`std::array` read — fixed (test prices redesigned) and documented as an
explicit precondition (11), not silently patched over.

**Mojibake sweep:** 3 → **0** stray Devanagari tokens.

---

## Batch 10 (part 3) — ✅ COMPLETE (PHASE 26)

| Folder | Files | Status |
|---|---|---|
| `38-MARKET-DATA/` | 16 lessons (`01`–`16`) + `17-exercises.md` + 10 examples + shared `wire_protocol.hpp` | ✅ Full — verified, benchmarks measured |

**`38-MARKET-DATA` (PHASE 26) — domain track ka pehla BUILD:** ek teaching-
purpose ITCH-style binary feed handler, **poore CLAUDE.md process ke
saath** — build simple/correct (`03_simple_parser.cpp`) → measure
(`04_parser_benchmark.cpp`) → optimize (`05_zero_copy_parser.cpp`) →
re-measure/compare (`06_parser_comparison.cpp`) → poore pipeline mein
integrate (`10_feed_handler.cpp`). L1/L2/L3 · snapshots vs incremental
(self-sufficiency problem) · sequence numbers/gap detection · **shared
protocol** (16-byte header + 5 message types: Add/Execute/Cancel/Delete/
Replace, `wire_protocol.hpp`) · ITCH-style layout · FIX/FAST (kab dikhte
hain) · SBE (schema-first, native-endian ki honest wajah) · **zero-copy
parsing** (measured) · endianness/byteswap (measured) · message framing
(length-prefix, partial reads, TCP-vs-UDP) · **A/B feed arbitration**
(measured) · recovery/retransmission (3-layer ladder) · timestamping/
clocks/PTP/clock-skew · conflation (kab safe hai) · poora feed-handler
build (capstone).

**Examples portable `.cpp`** (`./build.ps1 folder 38-MARKET-DATA` →
**10/10 OK**). Measured on **AMD Ryzen 7 4700U (Zen 2, ~2 GHz, Windows/
MinGW, unpinned)** — ratios/shapes port, tail absolutes don't:
- `02` byteswap: **0.753 ns/swap** — corrects "binary fast because swap
  avoided" myth; real win is eliminating text-parsing (05), feeds into
  SBE's honest native-endian explanation (08).
- `04` naive parser (owned struct + growing vector): **p50 30.1 / p99.9
  771.5 / max 1728075.1 ns**.
- `05` zero-copy parser (overlay-read, fixed sink): **p50 30.1 / p99.9
  40.1 / max 6121.6 ns**.
- `06` same feed, same run, both parsers: **p99.9 ratio 22.2×**, **p50
  identical** — the allocation tail, not typical-case work, is what moved.
- `07` simulator: message mix Add 55.2% / Exec 17.9% / Cancel 11.3% /
  Delete 9.0% / Replace 6.6%; deterministic (byte-identical re-run proof).
- `08` gap detection: **96/20000 gaps, sanity-verified exact match**
  against injected drops.
- `09` A/B arbitration: single-feed loss ~0.99% → arbitrated **~0.02%
  (~49.5× better, zero round-trips)**.
- `10` end-to-end feed handler: **p50 30.1 / p99.9 40.1 ns** — matches
  the standalone zero-copy parser exactly; framing+gap-detection added
  zero extra tail.

**Mojibake sweep:** 4 → **0** stray Devanagari/Cyrillic tokens.

---

## Batch 10 (part 2) — ✅ COMPLETE (PHASE 25)

| Folder | Files | Status |
|---|---|---|
| `37-HFT-FUNDAMENTALS/` | 16 lessons (`01`–`16`) + `17-exercises.md` + 4 examples | ✅ Full — verified, domain knowledge (no benchmarks — concept level) |

**`37-HFT-FUNDAMENTALS` (PHASE 25) — first folder of the HFT domain track:**
**domain knowledge**, not more C++ mechanism — HFT kya hai/nahi hai (myths vs
reality) · exchange architecture (gateway → matching engine → market-data-out
vs trade-confirm, two alag paths) · market microstructure (liquidity vs
volume, price discovery, **adverse selection** — the fundamental
market-making risk) · order types (market/limit/IOC/FOK/stop/iceberg/
post-only — exact fill behavior each) · bid-ask-spread (bps normalization,
**microprice**, imbalance) · order book concept (integer ticks not `double`
— ties back to 03-VARIABLES' float-equality trap) · **price-time priority vs
pro-rata matching — measured side-by-side, same book same order different
fills** · tick size/lot size/price bands · maker-taker fees/rebates ·
strategy categories overview (market making / stat-arb / event-driven —
**concepts only, explicitly no alpha/signal research**) · colocation (the
speed-of-light propagation floor, cross-connects, fair access) · the full
HFT system architecture diagram (mapped box-by-box to folders 38–44) · risk
systems (pre-trade checks, fat-finger, kill switch, **fail-closed**) ·
tick-to-trade latency budget (measured tool — finds the ONE stage whose tail
overrun dwarfs the rest, not just "which stages are over") · India vs US
market structure (consolidated vs fragmented, ITCH/OUCH, Reg NMS) ·
regulatory basics (algo-ID tagging, audit trail as a hot-path engineering
problem).

**Examples portable `.cpp`** (`./build.ps1 folder 37-HFT-FUNDAMENTALS` →
**4/4 OK**). All 4 are **conceptual/deterministic** (no timing measurement —
this folder has no benchmarks, unlike 35/36): `01_orderbook_concept.cpp`
(simple two-sided `std::map` book, integer-tick prices), `02_spread_
calculator.cpp` (spread bps / microprice / imbalance across 5 quotes),
`03_latency_budget.cpp` (illustrative tick-to-trade budget tool — auto-finds
the worst tail-overrun stage), `04_matching_rules.cpp` (FIFO vs pro-rata
matching, same book/order, different fills).

**Mojibake sweep:** 3 → **0** stray Devanagari tokens.

---

## Batch 10 (part 1) — ✅ COMPLETE (PHASE 24)

| Folder | Files | Status |
|---|---|---|
| `36-LOW-LATENCY-CPP/` | 24 lessons (`01`–`24`) + `25-exercises.md` + 12 examples | ✅ Full — verified, benchmarks measured |

**`36-LOW-LATENCY-CPP` (PHASE 24) — "measure karo, guess mat karo; har trick ke
saath trade-off":** the synthesis folder — everything from folders 12–35 applied
to hot-path engineering, **every technique paired with its cost and a "when NOT
to"**. latency/throughput/jitter (three axes, budget thinking) · tail latency
(p99.9 is the scorecard; **C++ has no GC** — the tail sources are different) ·
jitter sources (a hot-path audit checklist → eliminate / bound / make-rare) ·
**allocation avoidance** (measured tail; the hidden allocations — vector grow /
string SSO / map node / `std::function` capture / `throw` / page fault) ·
**pre-allocation** (warm-up vs steady state; *prove* zero-alloc with
`null_memory_resource` / a `new` hook / `perf page-faults`) · **memory pools**
(build + benchmark a `FixedPool` — intrusive free list) · **object pools**
(construct-on-acquire vs recycle-pre-constructed) · **arenas** (bump allocator,
monotonic buffer, PMR) · **custom allocators** (PMR vs classic `Allocator<T>`) ·
cache locality (hot/cold split, AoS/SoA, flat structures) · **false sharing**
(measured 3.5–44×, run-to-run — a *jitter* source) · **branch-free** (when it
wins, when it *loses*; `-O2` if-conversion) · **virtual dispatch elimination**
(CRTP / variant / switch / fn-table, homo vs hetero data, measured) ·
**`std::function` cost** (`function_ref`, the SBO cliff) · **ring buffers**
(pow-2 `& mask`, monotonic counters, release/acquire, cached opposite index) ·
**batching** (throughput vs head-of-line latency; opportunistic batching) ·
**syscall avoidance** (busy-poll cost, `io_uring`+`SQPOLL`, kernel bypass) ·
**page-fault avoidance** (`MAP_POPULATE` / `mlockall` / write-touch / stack
pre-fault; huge pages & THP jitter) · **CPU pinning** (`isolcpus` / `nohz_full` /
`rcu_nocbs`, SMT, NUMA, the `SCHED_FIFO` hazard) · **cache warming** (cold start
vs decay; dry-run + the `dry_run`-flag layout requirement) · **I-cache**
(Frontend Bound; hot/cold split; PGO/LTO/BOLT; why `always_inline` everything
*hurts*) · **zero-copy** (views/spans/overlays; `from_chars`; the 4 overlay
caveats) · **compile-time dispatch** (`if constexpr`, non-type params, startup
fn-pointer pick; instantiation bloat → Frontend Bound) · **HONEST trade-offs**
(each technique's hidden cost table; six "when NOT to"; the measure → profile →
one change → re-measure → explain process).

**Examples portable `.cpp`** (`./build.ps1 folder 36-LOW-LATENCY-CPP` →
**12/12 OK** under strict flags). Measured on **AMD Ryzen 7 4700U (Zen 2, ~2 GHz,
Windows/MinGW, unpinned, SSE2 baseline)** — ratios/shapes port, tail absolutes
don't (per-op-timed loops' `max` = OS-interrupt noise; p50/p99/p99.9 are the
signal):
- `01` allocation cost: `new[64]` p50 40 / p99.9 120 ns; **mixed 8–8192 B churn
  p50 80 / p99 561 / p99.9 2585 ns** — the tail is the disaster.
- `02` memory pool: `new[64]` p50 70 / p99.9 180 ns vs **`FixedPool::allocate`
  p50 20 / p99.9 30 ns — flat** (no mmap/coalesce/size-class/lock).
- `03` object pool: `new Order()` p50 50 / **construct 30 / recycle+reset 20 ns**
  (recycle skips the ctor).
- `04` arena: new/delete per msg p50 501 ns vs **arena+reset p50 30 ns (~16×)**.
- `05` PMR: `std::vector` p50 270 ns vs **`pmr::vector` on stack buffer p50 40 ns,
  0 global `new`** over 200k messages; `null_memory_resource` tripwire works.
- `07` dispatch (hetero data): **virtual 7.0 ns vs CRTP 0.6 ns**; variant/switch
  ~4.5 ns (inlined body degrades less).
- `09` ring buffer: try_push / try_pop **p50 20 ns, p99.9 30 ns**.
- `10` batching: **B=1 6.7 ns/item, HoL 6.7 ns → B=1024 1.5 ns/item, HoL 3095 ns**
  — knee ~B=16–32.
- `11` page fault: **COLD mean 271 / p99 2875 ns vs WARM mean 21 / p99 30 ns**
  (~13× / ~95×).

**⚠️ CLAUDE.md Rule 2 in folder 36 (three times, all taught):** (a) `06` — the
"branchy" `if` gave *identical* sorted vs unsorted times → `-O2` if-converted it
to `cmov`, no branch to mispredict (check the asm — 33/08); (b) `08` — templated
/ fn-ptr / `function_ref` tie at ~1.5 ns because the loop is *latency-bound* on a
carried hash (OoO hides the call); a throughput loop would let templated pull
ahead; (c) `12` — hot/cold split A/B/C measured **no difference** (the micro-
bench's hot loop fits L1i, so layout had nothing to fix) — kept in the folder
deliberately: a technique that "should" help showing nothing in a given context
is the norm; measure in situ (35/11).

**Mojibake sweep:** 11 → **0** stray Devanagari tokens.

---

## Batch 9 (part 5) — ✅ COMPLETE (PHASE 23) — **BATCH 9 DONE**

| Folder | Files | Status |
|---|---|---|
| `35-PROFILING-BENCHMARKING/` | 16 lessons (`01`–`16`) + `17-exercises.md` + 8 examples | ✅ Full — verified, benchmarks measured |

**`35-PROFILING-BENCHMARKING` (PHASE 23) — "measure karo, guess mat karo":**
why measure (intuition fails, **Amdahl's law**, premature-optimization ka asli
matlab, throughput vs latency) · **`<chrono>`** (steady vs system vs
high_resolution, resolution vs precision vs self-cost, `clock()` CPU-time) ·
**`rdtsc`** (fencing, ticks→ns calibration, core-hop — builds on 34/11, the
benchmarking angle) · **statistics** (mean lies on skewed latency, median/min/max,
σ useless for latency, MAD/CV, bimodal = two code paths) · **percentiles**
(nearest-rank, "nines", **fan-out tail amplification** — "the tail at scale",
**coordinated omission** + HdrHistogram correction, percentiles don't average) ·
**jitter** (SW sources: timer tick / scheduler / IRQ / page fault / malloc /
syscall / lock; HW/firmware: freq scaling / C-states / **SMI** / SMT sibling /
NUMA; measuring; the "quiet core" recipe — `isolcpus`+`nohz_full`+`rcu_nocbs`) ·
**histograms** (linear buckets fail for latency, **log-linear / HdrHistogram**,
relative-error ↔ `SUB_BITS`, CDF / percentile plot > PDF bars) · **Google
Benchmark** (`State` loop, `DoNotOptimize`/`ClobberMemory`, `Range`/`Args`,
`PauseTiming`, fixtures, the mistakes the library can't fix) · **benchmark
pitfalls** (DCE / const-fold / hoist / cold-start / timer-overhead / one-run /
**alignment & layout noise** / **frequency scaling** — report cycles/op too /
denormals / bench≠reality) · **`perf` basics** (stat vs record vs annotate,
IPC, cache-miss rates, **top-down** Retiring/Frontend/Backend/Bad-Spec,
`perf list`, `perf_event_paranoid`) · **`perf` advanced** (annotate + skid,
**PEBS/IBS `:pp`** precise events, `cycle_activity.stalls_l3_miss`, **`perf mem`**
per-access latency+source, **`perf c2c`** false sharing / HITM, LBR call graphs
→ AutoFDO, counter multiplexing) · **flame graphs** (axes: width=time /
X=alphabetical / Y=depth, top plateau, `stackcollapse`→`flamegraph.pl`, on-CPU
vs **off-CPU**, differential, icicle) · **Valgrind** (cachegrind `Ir` =
deterministic CI gate, callgrind + KCachegrind, massif heap-over-time, **DHAT**
per-alloc-site usage/churn — all vs `perf`) · **VTune / top-down** (recursive
Backend→Memory→DRAM→Bandwidth-vs-Latency tree, analysis types, **`toplev.py`**
for the same on plain `perf`, AMD uProf / Instruments) · **sanitizers**
(ASan/UBSan/TSan/MSan — what each catches, slowdowns, `volatile` vs
`std::atomic`, why never for perf numbers, MinGW fallback) · **production
measurement** (inline `rdtsc` + per-thread HdrHistogram, **SPSC ring →
aggregator**, sampling strategies, coordinated omission in prod = timestamp at
arrival, white box vs black box / NIC HW timestamps).

**Examples portable `.cpp`** (`./build.ps1 folder 35-PROFILING-BENCHMARKING` →
**5/5 OK** under strict flags; `05_google_benchmark/` is multi-file `.cxx`/`.hpp`
— a Google-Benchmark **shim**; `06_perf_workflow.sh` + `07_flamegraph.sh` are
Linux-only, self-contained). Measured on **AMD Ryzen 7 4700U (Zen 2, ~2 GHz,
Windows, unpinned)** — ratios/shapes port, tail absolutes don't:
- `01` timing: chrono resolution **100 ns**; `clock()` **1 ms** (MinGW
  `CLOCKS_PER_SEC=1000`); TSC calibration **~1.996 ticks/ns**; self-cost
  `steady_clock::now()` **~38 ns** / plain rdtsc **~9 ns** / fenced **~18 ns**;
  94 µs workload — chrono vs calibrated rdtsc agree to **0.02%**.
- `02` percentiles: 200k samples, mean ≈ median (bulk ~symmetric) yet
  **max/median 60–175×**, p99.99/median **7–26× (run-to-run)** — "mean fine ≠
  tail fine". Linear histogram can't hold bulk + tail → log buckets.
- `03` jitter: identical work, **min 200 ns both phases**; CLEAN spikes(>2×)
  **~2800/300k**, NOISY **~6300/300k** (`new`/`delete`+`yield`) — spike count +
  p99.9 robust, single `max` noisy. Unpinned Windows → even CLEAN has ~1% >2×.
- `04` benchmark bugs (`-O2`): DCE / const-fold / hoist all **BUG 0.000** vs
  FIX ~0.85–1.0 ns/op; timer-per-op **36.8** (≈ `now()` self-cost) vs batched
  **2.99**; one-run vs min-of-50 spread **35%**.
- `05` GB shim: `BM_reduce_NO_barrier` **0.49 ns/iter (DELETED)** vs
  `_WITH_barrier` **1030**; `memcpy/4096` **32 ns (128 GB/s)** vs `manual_copy`
  **1003 ns (4 GB/s)** ~30×; `StringCopy/8` **3 ns (SSO)** → `/64` **49 ns**.
- `08` recorder: HdrHistogram-lite **~2 ns/record**, **29.5 KB fixed**,
  percentiles vs exact **≤1.3%** (p50 +0.4%, p99 +1.3%); log-plot shows the
  simulated stream is **bimodal** (bulk ~276 ns + spike hump ~10–22 µs).

**⚠️ CLAUDE.md Rule 2 in folder 35:** the "BUG" benchmarks in `04`/`05` really do
report **0.000 / 0.49 ns/op** because `-O2` deletes them — that's the taught
result, sitting next to the FIX. `04` uses a `volatile` global sink + source
(not the asm `keep()`) precisely because `keep()`'s `"+r,m"` hits "impossible
constraint" on a constant-folded value — the same gotcha logged in folder 34.
`04` case 4 "cold start" is only **~1.2×** here (Windows commits pages eagerly;
kept honest with the note that it scales with working-set size / backing store).
`02`/`03` tail numbers are deliberately **run-to-run unstable** — the lesson is
that this is an unpinned desktop; a pinned Linux isolated core would be tight
(taught in 03/06).

**Mojibake sweep:** 52 stray Devanagari/Cyrillic tokens (my Hinglish typing) →
**0 residual** (two-pass: token map + codepoint cleanup).

---

## Batch 9 (part 4) — ✅ COMPLETE (PHASE 22)

| Folder | Files | Status |
|---|---|---|
| `33-COMPILER-OPTIMIZATION/` | 15 lessons + `16-exercises.md` + 8 examples | ✅ Full — verified, benchmarks measured |
| `34-ASSEMBLY/` | 13 lessons (`01`–`12` + `13-exercises.md`) + 7 examples | ✅ Full — verified |

**`33-COMPILER-OPTIMIZATION` (PHASE 22) — "compiler ke saath kaam karo, khilaaf
nahi":** `-O` levels (`-O0`→`-Ofast`, per-file override, `-g` on release,
`-O0`→`-O1` = the big cliff) · **godbolt workflow** (what to check on hot asm,
`-fopt-info-*`, `objdump`, `llvm-mca`) · **inlining** (`inline` = ODR not "inline
me", heuristics, `always_inline`/`noinline`/`flatten`, the cross-TU / LTO gap,
inlining as the *enabling* optimization) · **loop opts** (LICM, strength
reduction, unroll + the "one accumulator" trap, fusion/fission, interchange
`-O3`-only, unswitch, rotation, IV elimination) · **auto-vectorization** (the
conditions, why a float reduction needs `-ffast-math`/`#pragma omp simd`,
`-fopt-info-vec[-missed]`, `ivdep`/`assume_safety` as unchecked promises, SoA as
the prereq) · **constant folding / propagation / DCE** (whole functions vanish,
`constexpr`/`consteval`/`constinit`, `if constexpr`, `[[assume]]`, benchmark
implications) · **devirtualization** (exact-type / `final` / speculative-via-LTO /
PGO; the honest "don't rely on it for a heterogeneous container") · **branch
hints** (`[[likely]]`/`[[unlikely]]`/`__builtin_expect` = *layout* not
prediction, small measured effect, wrong hint = regression, PGO is better) ·
**aliasing & `__restrict`** (how it blocks LICM/CSE/vectorize, TBAA / strict
aliasing UB, `bit_cast` not pointer-cast, local-copy fix, inline/LTO solves it) ·
**LTO** (the boundary it removes, cross-TU inline/const-prop/devirt/DCE, ThinLTO,
flag consistency, the ODR-exposure risk) · **PGO** (3-step + AutoFDO, what it
improves, representative-profile hazard, HFT = replayed session) · **`-march`/
`-mtune`** (x86-64-v1..v4, `native` wrong for a shipped binary, AVX-512
downclock, function multi-versioning) · **`-ffast-math` dangers** (the 8
sub-flags, `-ffinite-math-only` deletes NaN guards, `-fassociative-math` changes
results, `-ffp-contract`, safe scoped alternatives) · **preventing optimization**
(`DoNotOptimize`/`ClobberMemory` = zero instructions, `volatile` sink cost,
latency-vs-throughput barrier placement) · **reading optimized output** (the
verification checklist, vectorized vs scalar signature, `idiv` in a loop,
right-asm-wrong-time = memory-bound).

**Examples portable `.cpp`** (`./build.ps1 folder 33-COMPILER-OPTIMIZATION` →
**6/6 OK**; `06_lto_demo/` is multi-file `.cxx`; `07_pgo_workflow.sh` is a
script). Benchmarks at `-O2` (AMD Zen 2, ~2 GHz; plain `-O2` = SSE2 baseline
here):
- `01` -O levels: **-O0 80 ms → -O1 13.4 ms (~6× cliff)**; -O1/-O2/-O3/-Os equal
  for this reduction workload (nothing to vectorize).
- `02` inlining: noinline **1.31 ns/iter** vs inlined **~1.0** (~1.3×); `normal
  == always_inline`.
- `03` vectorization: MAP scalar→vectorized **~3.5×** (SSE2); float **REDUCE
  scalar-speed at `-O2`, ~4× with `-ffast-math`** (reassociation — Rule 2);
  PREFIX never vectorizes.
- `04` aliasing: may-alias vs `__restrict` **~3.5×** (functions `[[gnu::noinline]]`
  — inlined/LTO the compiler solves it itself, which is the lesson).
- `05` branch hints: **~1.3×** (HW predictor already nails a 1/1000 branch; the
  hint is code layout).
- `06` LTO: throughput loop **NO LTO 1.75 → `-flto` 0.76 ns/elem (~2.3×)**; ⚠️ a
  *carried* loop showed **no** LTO gain (latency-bound on the hash critical path
  — Rule 2 nuance).
- `08` barriers: **no barrier 0.00 ms (loop DELETED)** vs DoNotOptimize ~48 ms.

**⚠️ CLAUDE.md Rule 2 in folder 33:** (a) `03` float reduction correctly refuses
to vectorize at `-O2` (reassociation not allowed) — verified ~4× with
`-ffast-math`; (b) `04` the aliasing pessimism only appears with
`[[gnu::noinline]]` — inlining/LTO gives the compiler the caller's context and it
proves non-aliasing itself; (c) `06` LTO helps a *throughput* loop but not a
*carried* one. All taught explicitly.

**`34-ASSEMBLY` (PHASE 22) — "likhni nahi, PADHNI":** why read asm (verification /
optimization / debugging) · x86-64 registers + sub-register zeroing + `xmm`/`ymm`/
`zmm` + rflags · AT&T vs Intel (the 5 differences, which tool gives which) · the
~20 common instructions (`mov`/`lea`/`add`/`imul`/`cmp`/`test`/`jXX`/`call`/`ret`/
`cmov`/`setXX`, + recognize `div`/`rep movs`/`lock`) · addressing modes
(`base+index*scale+disp` → recover `sizeof`/field offsets) · **stack frames**
(prologue/epilogue with/without frame pointer, spills = register pressure, tail
call → `jmp`, Win64 shadow space) · **calling conventions** (System V vs Windows
x64 — where `this` is, large-struct return, `extern "C"`, varargs `al`) ·
**pattern recognition** (`if` / if-converted `cmov` / loop / `while` / dense &
sparse `switch` / call / virtual `call [reg+off]` / constant-folded / division) ·
**SIMD asm** (scalar `ss`/`sd` vs packed `ps`/`pd`/`d`, width from `add ptr,
16/32`, the horizontal-reduce cluster, `vzeroupper`, `vgather`) · **inline asm**
(the 4 sections, `"=r"`/`"+r"`/`"memory"`/`"cc"`, missing-clobber = `-O2`-only
corruption, when to use vs intrinsics = almost never) · **`rdtsc` timing** (what
it counts, fencing, ticks≠cycles≠ns, core hopping, when `clock_gettime` instead)
· **disassembly tools** (`g++ -S` vs `objdump -dS` vs `perf annotate` vs `gdb`
vs `addr2line` vs `llvm-mca` — which for which question).

`./build.ps1 folder 34-ASSEMBLY` → **5/5 OK** (`.cpp`; `01_simple_functions.s` +
`07_asm_puzzles.md` are reference docs). Measured (`05_rdtsc.cpp`, ~2 GHz):
calibration **~2.0 ticks/ns**; `__rdtsc` self-cost **~1 tick (~0.5 ns)**;
`lfence;rdtsc;lfence` / `rdtscp+lfence` **~20 ticks (~10 ns)**; 100M dependent-LCG
loop **~1.98 ticks/iter**. `06_inline_asm.cpp`: CPUID vendor `AuthenticAMD`,
barrier emits zero instructions. Examples `02`/`03`/`04` drop `keep()` (non-static
functions emit standalone asm anyway; `keep()` on a folded constant → "impossible
constraint" at `-O2`).

**Mojibake sweep:** folder 33 (14 → 0), folder 34 (3 → 0).

---

## Batch 9 (part 3) — ✅ COMPLETE (PHASE 21)

| Folder | Files | Status |
|---|---|---|
| `32-CACHE-MEMORY-PERFORMANCE/` | 15 lessons + `16-exercises.md` + 8 examples + `09_perf_analysis.sh` | ✅ Full — verified, benchmarks measured |

**`32-CACHE-MEMORY-PERFORMANCE` (PHASE 21) — "performance engineering ka sabse
important folder":** memory hierarchy (registers→L1→L2→L3→DRAM→disk, the "1
second = L1" latency ladder, this box's geometry) · **cache lines** (64 B unit
of transfer, why 64, `hardware_destructive_interference_size`, line straddle,
`alignas(64)`) · **cache organization** (direct-mapped→set-associative,
index/tag/offset, ways, pseudo-LRU/RRIP, **critical stride** = size/assoc, why
power-of-two dims are poison, VIPT) · **the 3 C's** (compulsory/capacity/conflict
+ coherence, a diagnosis table, MLP — which misses OoO can hide, which it can't)
· **locality** (spatial/temporal, loop interchange/fusion/fission/tiling, the
prefetcher's favourite/least-favourite patterns) · **prefetching** (HW
prefetchers: next-line/adjacent/stride/region + their limits; `__builtin_prefetch`
rw/locality + distance tuning; ⚠️ **measured marginal-or-harmful on a big OoO
core**) · **false sharing deep** (MESI ping-pong, `perf c2c`, padding /
per-thread-local, and why it's *jittery* not just slow) · **cache-friendly data
structures** (flat vs pointer-based, `map`/`unordered_map`/`btree_map`/
`flat_hash_map` miss counts, arena+index links, CSR) · **AoS vs SoA deep** (the
three access patterns → three winners, AoSoA hybrid, vectorization needs SoA) ·
**data-oriented design** (DOD philosophy, `vector<Base*>`+`virtual` hot-loop cost,
existence-based processing, handles+generation) · **TLB & huge pages** (4-level
page walk, dTLB/STLB reach, 2 MiB pages → 512× reach, explicit hugetlbfs vs THP
downsides, pre-fault) · **store buffers** (RFO, store-to-load forwarding stalls,
write-combining, non-temporal stores + `sfence`) · **memory bandwidth**
(latency- vs bandwidth-bound, Little's Law → one core ≈ 18% of socket peak,
STREAM, roofline / arithmetic intensity, "SIMD a memory-bound loop = wasted") ·
**measuring** (`perf stat` + MPKI, `--topdown`, `perf record`/`annotate`,
`perf c2c`, cachegrind, VTune/uProf) · **optimization recipes** (11 recipes in
impact order, each with its trade-off; the apply-one-remeasure discipline).

**Examples portable `.cpp`** (all run on x86-64 incl. MinGW/Windows — no Linux
needed; `09_perf_analysis.sh` is a Linux `perf` workflow, not compiled).
`./build.ps1 folder 32-CACHE-MEMORY-PERFORMANCE` → **8/8 OK** under strict flags
(`-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wcast-align
-Wnull-dereference -Wdouble-promotion`). Benchmarks at `-O2`, **AMD Ryzen 7 4700U
(Zen 2, ~2 GHz throttled)** — shapes/ratios, not production absolutes.

**⚠️ CLAUDE.md Rule 2 in action (three times):**
- `03` — `-O2` col-major traversal **~10× slower**; at **`-O3 -march=native`
  GCC `-ftree-loop-interchange` swaps the nest → ratio ~1.0** (compiler fixed
  the cache-hostile loop). Taught, not hidden.
- `06` — SW prefetch: **~1.1× (marginal)** on a light gather (MLP already
  overlaps ~10 misses), **~0.33× = 3× SLOWER** on a heavy bucket-scan (memory
  already saturated, prefetch requests contend). "Prefetch is not free money."
- `07` — naive 64×64 matmul blocking is **~10-15% *slower* than a plain `ikj`
  loop order** on this box (ikj auto-vectorizes + L3 absorbs the reuse); blocking
  needs a tuned microkernel to pay. Loop order is the real ~4× win.

**Measured (this box, ~2 GHz — ratios port, absolutes don't):**
- `01` sequential stride ramp: ns/access ~0.32 (1 B) → ~3.4 (64 B) → ~5.4
  (128 B); knee **smeared 64→128 B by the HW prefetcher** (sharp cliff is in `02`).
- `02` sequential vs random line access: **~7×** (14 GB/s vs 2 GB/s); random still
  < true ~80 ns (MLP overlaps ~10 independent misses).
- `03` row- vs column-major 4096² traversal: **~10×** at `-O2`.
- `04` false sharing: **~6× to ~44×, run-to-run** (padded ~0.2 ns/inc stable;
  packed swings with scheduling) — the *variance is the lesson*.
- `05` AoS vs SoA: scan-few-fields **SoA ~2.0×**; scan-most-fields **SoA ~2.4×**
  (gap doesn't shrink — SoA vectorizes); random whole-record **AoS ~3×**.
- `08` TLB/cache latency cliff: **~1.2 ns (1 page) → ~15 ns (4 MiB) → ~95 ns
  (8 MiB+)** — STLB reach (~6 MiB) + L3→DRAM (~8 MiB) coincide with 4 KiB pages
  (honest limitation stated in-file).

**Mojibake sweep:** 139 → 8 → **0** stray Devanagari/Cyrillic tokens
(two-pass: token map + Cyrillic-cluster cleanup).

---

## Batch 9 (part 2) — ✅ COMPLETE (PHASE 20)

| Folder | Files | Status |
|---|---|---|
| `31-CPU-ARCHITECTURE/` | 15 lessons + `16-exercises.md` + 8 examples | ✅ Full — verified, benchmarks measured |

**`31-CPU-ARCHITECTURE` (PHASE 20):** fetch-decode-execute-retire + clock/cycles
vs ns · **registers** (GP sub-register scheme, `rflags`, SIMD `xmm`/`ymm`/`zmm`,
architectural vs physical + renaming, spilling, `mxcsr` flush-to-zero) · **ISA**
(x86-64, CISC front-end / RISC µop back-end, macro/micro-fusion, **microcode**:
`div`/denormals/`gather`/transcendentals, `-march=x86-64-v2/v3/v4`) ·
**pipelining** (latency vs throughput, the 4 hazard types, flush/refill
~15–20 cyc) · **superscalar** (execution ports, ILP, IPC, "which port is the
bottleneck") · **out-of-order** (ROB / register renaming / reservation stations,
speculation + rollback, the OoO window, why pointer-chasing can't be hidden) ·
**branch prediction** (gshare/TAGE, BTB/RAS, indirect/virtual, ~6–7× measured
misprediction cost, `[[likely]]`, warm-up) · **speculative execution**
(Spectre/Meltdown in one paragraph, KPTI/retpoline cost, **`mitigations=off`**
trade-off + preconditions) · **instruction latency/throughput** (Agner Fog /
uops.info / `llvm-mca`, computing the critical path, the division problem) ·
**SIMD basics** (SSE2→AVX→AVX2+FMA→AVX-512, what SIMD is good/bad at, SoA
prerequisite, alignment, AVX-512 downclock) · **SIMD intrinsics** (`_mm256_*`
naming, load/store/arith/cmp→mask/blend/movemask/shuffle/hreduce, tail handling,
auto-vec vs manual vs Highway/xsimd) · **hyperthreading** (SMT shares
L1/ports/ROB/predictor → jitter → **HFT disables it**, or isolates the sibling) ·
**frequency & power** (P-states/turbo, C-states + ~tens-µs wake, thermal
throttle, AVX frequency offset, uncore — **lock frequency, kill deep C-states**)
· **NUMA hardware** (sockets + UPI/Infinity Fabric, chiplet/CCX & Intel SNC =
NUMA-within-a-socket, the latency ladder, NIC's node) · **CPU differences**
(Intel vs AMD, per-CCX L3, `pdep` microcoded on Zen 1/2, ARM64/Graviton
re-audit, "benchmark on the deployment target, frequency-locked").

**Examples portable `.cpp`** (SIMD + `rdtsc` + CPUID all run on x86-64 incl.
MinGW/Windows). `./build.ps1 folder 31-CPU-ARCHITECTURE` → **8/8 OK**; benchmarks
at `-O2`. Measured on **AMD Ryzen 7 4700U (Zen 2, ~2 GHz throttled, no
AVX-512)** — the numbers illustrate **shapes/ratios**, not production absolutes
(files 13/15 explain: frequency scaling; measure on the locked deployment box).

**⚠️ CLAUDE.md Rule 2 in action:** examples `01`/`03` initially showed *no
effect* — `-O2` DCE'd the ADD/DIV dependency chains (`01` ADD latency = `0.000`)
and if-converted the branch to a `cmovge` (`03` RANDOM == SORTED). Fixed with
`keep()` inline-asm optimization barriers, `#pragma GCC optimize("no-if-
conversion")`, and a threaded `carry` to defeat call-hoisting. The "the compiler
already made it branchless" observation is now **taught explicitly** (files 03,
07) as the lesson, not hidden.

**Measured (this box, ~2 GHz):** serial `imul` chain vs 4 independent chains
**~4×** (ILP, ex 02); unpredictable branch **~6–7×** slower than predictable
(ex 03); branchless **~6–7×** on random data, branchy ~1.0–1.2× on sorted
(ex 04); float sum scalar → SSE **4.1×** → AVX2 **~9.5×** (L2-resident;
converges to ~1× when bandwidth-bound) (ex 05); same-code auto-vectorized
**~2.5–3×** (ex 06); `rdtsc` self-cost ~20 cyc vs `lfence`/`rdtscp` serialized
~40–90 cyc (ex 08); calibrated ~2.0 cyc/ns.

**Mojibake sweep:** 13 stray Devanagari/Cyrillic tokens → 0.

---

## Batch 9 (part 1) — ✅ COMPLETE (PHASE 18–19)

| Folder | Files | Status |
|---|---|---|
| `29-LINUX-SYSTEMS/` | 18 lessons + exercises + 10 examples | ✅ Full — lessons verified; examples Linux-only (see note) |
| `30-NETWORKING/` | 16 lessons + exercises + 10 examples | ✅ Full — lessons verified; examples Linux-only (see note) |

**Yeh do folders pehle truly Linux-only topics hain, aur is repo ka toolchain
MinGW-w64 on Windows hai (no WSL distro).** User ne explicitly choose kiya:
**"all real Linux code, marked"** — har example correct, idiomatic Linux C++20
hai (`fork`/`mmap`/`sched_setaffinity`/`epoll`/POSIX sockets/`SO_TIMESTAMPING`
…), par is box pe compile nahi hota.

- **`build.ps1` skip-hook added:** `*.linux.cpp` files ko `folder` aur `checkall`
  dono `SKIP (linux-only)` karte hain (naya bucket, `FAIL (real)` nahi). ~4 lines
  in each target + header NOTE. `./build.ps1 folder 29-LINUX-SYSTEMS` → 10/10
  SKIP, 0 fail; same for 30.
- **Benchmark numbers:** har example ke `EXPECTED OUTPUT` comment block mein
  **TYPICAL Linux x86-64 / loopback** figures hain, **explicitly "not measured on
  your machine" labelled**. CLAUDE.md Rule 2 ka honest treatment — Linux syscalls
  is box pe chal hi nahi sakte, to fabricate karke "measured" nahi bola;
  published/typical ranges + saaf disclaimer. Asli numbers `NN-exercises.md`
  Part D mein learner khud Linux/WSL pe leta hai. Lesson hamesha **ratio** mein
  (syscall vs userspace ~100–300×, cold vs warm fault ~20–60×, unpinned tail vs
  pinned ~5–50×, Nagle OFF vs ON ~40 ms vs ~µs, blocking vs busy-poll ~2× + tail,
  epoll vs poll flat vs O(N)).
- **Mojibake sweep:** 186 (folder 29) + 30 (folder 30) stray Devanagari/Cyrillic
  tokens found (my Hinglish typing) → all fixed → **0 residual** both folders +
  audit files.

**`29-LINUX-SYSTEMS` (PHASE 18) — 18 lessons:** kernel vs userspace / ring 0-3 ·
**syscalls** (user→kernel transition cost, vDSO, mitigations) · **processes**
(`fork` COW, `exec`, `wait`, zombie/orphan, `clone`) · **signals**
(async-signal-safety, `sigaction`, `signalfd`, hot-thread masking) · **file
descriptors** (fd table 3 levels, short read/write, `O_CLOEXEC`, `dup2`) ·
**`/proc` & `/sys`** (introspection + sysctl tuning knobs) · **`mmap`** (file /
anonymous / shared, lazy fault) · **shared memory** (`shm_open`, cross-process
SPSC rings, layout rules) · **pipes/FIFOs/UDS** (fd passing, latency vs shm) ·
**CPU scheduling** (CFS/EEVDF, `SCHED_FIFO`, `nice`, "isolated CFS busy-poll
often beats FIFO") · **CPU affinity** (`sched_setaffinity`, `isolcpus`,
`nohz_full`, `rcu_nocbs`, SMT siblings) · **huge pages** (TLB reach, THP vs
hugetlbfs, `khugepaged` stall) · **page faults & mlock** (minor/major/COW cost,
`mlockall` + pre-fault + dry-run = zero-fault steady state) · **NUMA** (first-
touch, `numactl`/`mbind`, `numa_balancing` off) · **IRQ affinity** (hard IRQ →
softirq, `/proc/irq/*/smp_affinity`, `irqbalance` off, coalescing) · **clocks &
timers** (`CLOCK_MONOTONIC` vs `_RAW` vs `_COARSE`, TSC/`rdtscp`, vDSO,
busy-spin for precise timing) · **cgroups & rlimits** (`RLIMIT_MEMLOCK/NOFILE/
RTPRIO`, `cpu.max` throttle = spike, container-awareness) · **HFT tuning
checklist** (BIOS, kernel cmdline, sysctl, runtime, systemd unit, app-side,
verify-by-effect). 10 examples (all `*.linux.cpp`, 10/10 SKIP on this box):
`01_syscall_cost` (userspace vs vDSO vs raw trap), `02_fork_exec`,
`03_file_io_raw` (per-line vs buffered writes), `04_mmap_demo` (lazy fault cost),
`05_shared_memory` (fork + shm + monotonic hand-off), `06_cpu_affinity` (pinning
kills the tail), `07_page_faults` (cold vs warm vs mlock+populate),
`08_hugepages` (Sattolo pointer-chase, 4K vs 2M), `09_clock_comparison`,
`10_realtime_thread` (CFS vs SCHED_FIFO jitter).

**`30-NETWORKING` (PHASE 19) — 16 lessons:** OSI/TCP-IP + encapsulation + wire
overhead · **IP** (subnets, routing, MTU, fragmentation, PMTUD black hole, ARP) ·
**TCP deep** (handshake, seq/ack/window, RTO vs fast retransmit, SACK, cwnd,
TIME_WAIT) · **TCP latency issues** (Nagle + delayed-ACK ~40 ms deadlock,
`TCP_CORK`, slow-start-after-idle, head-of-line blocking) · **UDP** (why market
data is UDP; loss/reorder handling; A/B feeds; kernel drops; `recvmmsg`) ·
**multicast** (`IP_ADD_MEMBERSHIP`, IGMP + snooping, interface selection, SSM,
A/B arbitration) · **sockets API** (call sequence, `recv` 0 vs <0, framing,
`getaddrinfo` at startup) · **blocking vs non-blocking** (`EAGAIN`, busy-poll,
`SO_BUSY_POLL`, ET draining) · **select/poll** (O(N), `FD_SETSIZE`, why epoll
exists) · **epoll deep** (LT vs ET, event-loop design, `EPOLLOUT` discipline,
`EPOLLONESHOT`/`EXCLUSIVE`, timerfd/signalfd/eventfd) · **socket options** (the
HFT short list, `SO_RCVBUF` cap + doubling, `SO_REUSEPORT`, keepalive +
`TCP_USER_TIMEOUT`) · **zero-copy** (`sendfile`, `splice` for capture/replay,
`MSG_ZEROCOPY`, `io_uring` SQ/CQ + SQPOLL) · **kernel bypass** (DPDK / Onload /
ef_vi / VMA / AF_XDP — what's removed, what you take on, the adoption ladder) ·
**timestamping** (`SO_TIMESTAMPING` HW/SW, clock domains, PTP + `phc2sys`,
regulatory) · **network tuning** (NIC rings, coalescing, GRO/LRO off, RSS/RPS,
qdisc, sysctl) · **building servers** (blocking echo → epoll echo → UDP multicast
feed handler, the V1→V2→V3 progression). 10 examples (all `*.linux.cpp`, 10/10
SKIP): `01_tcp_echo_server` + `02_tcp_client` (RT latency histogram),
`03_nagle_demo` (OFF ~40 ms vs ON ~µs), `04_udp_sender` + `05_udp_receiver`
(seq/loss/reorder + `recvmmsg`), `06_multicast_receiver` (self-contained join),
`07_epoll_server` (full edge-triggered), `08_socket_options`,
`09_timestamping` (`SO_TIMESTAMPING` cmsg parse), `10_latency_measure`
(blocking vs busy-poll histogram).

---

## Batch 8 (part 3) — ✅ COMPLETE (PHASE 17–18) — **BATCH 8 DONE**

| Folder | Files | Status |
|---|---|---|
| `27-ATOMICS-MEMORY-MODEL/` | 16 lessons + exercises + 8 examples | ✅ Full — verified |
| `28-LOCK-FREE/` | 14 lessons + exercises + 7 examples | ✅ Full — verified |

**`27-ATOMICS-MEMORY-MODEL` (PHASE 17) — "course ka sabse mushkil folder":**
data race **formal definition** (same location, ≥1 write, no happens-before,
non-atomic, diff threads → whole-program UB) · **atomicity** (indivisible;
non-atomic RMW = load-modify-store; torn/misaligned; x86 aligned ≤8B hw-atomic
but still a data race) · `std::atomic<T>` (trivially-copyable, non-copyable,
brace-init, `is_always_lock_free`, `atomic_flag`, C++20 wait/notify) · **atomic
ops** (`load`/`store`/`exchange`/`fetch_*`; x86 `mov`/`xchg`/`lock xadd`/`lock
cmpxchg`; `fetch_add` returns OLD; no `fetch_max`) · **CAS** (`weak` vs `strong`,
spurious failure, `expected` overwrite, CAS loops, success/failure orders) · **why
ordering** (compiler + CPU reorder; store buffer) · `relaxed` (atomicity only) ·
**acquire/release** (synchronizes-with, publish/subscribe) · **seq_cst** (single
total order, default, the `mfence` cost) · **happens-before / synchronizes-with /
sequenced-before** (the formal vocabulary) · **fences** (`atomic_thread_fence` vs
`atomic_signal_fence`; x86: only `seq_cst` fence emits `mfence`) · **x86-TSO vs
ARM/POWER** (multi-copy atomicity, per-op cost on ARM) · `std::atomic_ref`
(C++20) · **lock-free / wait-free / obstruction-free** · **ABA** (tagged word) ·
**litmus tests** (SB / MP / LB / IRIW). Examples (8/8 OK, `-Wall -Wextra
-Wpedantic -Wconversion …` clean): `01_atomic_counter` — non-atomic **~3M / 16M
WRONG** (`volatile` used only to defeat `-O2` coalescing) vs atomic exact;
`05_reordering_demo` — **store buffering observed on x86**: `r1==r2==0` for
`relaxed` ~70–330/200k, `release/acquire` **~1000–4200/200k** (rel/acq does NOT
stop store→load), `seq_cst` **always 0**; `06_memory_order_bench` — store
relaxed/release ~0.72 ns vs **seq_cst ~13 ns (~18×)**, load ~0.74 ns *all*
orders, `fetch_add` ~13 ns *all* orders, contended ~27 ns/op (ns drift with
machine state — *ratios* stable); `07_aba_problem` — ABA reproduced
deterministically, `{idx:32,tag:32}` fix rejects the stale CAS; `08_litmus_tests`
— MP + IRIW: 0 bad reads / 0 disagreements at every order on x86 (expected —
x86 forbids all litmus reorderings except SB, and is multi-copy-atomic).
⚠️ TSan/UBSan not on this MinGW; DWCAS (`__atomic_*_16`) doesn't link → ABA
example uses a 64-bit packed word, not pointer+tag.

**`28-LOCK-FREE` (PHASE 17/18):** why lock-free (contended mutex costs: futex
syscall + context switch + preemption stall; priority inversion / convoying /
deadlock) · **rules** (no locks/alloc/blocking on the shared path; pre-allocate;
deliberate memory order; ABA-proof; helping) · **cache-line padding & false
sharing** · **SPSC ring buffer** (release-store / acquire-load of two indices, no
CAS, wait-free in practice, no ABA — monotonic counters) · **SPSC optimizations**
(mask, padding, **cached opposite index**, batching) · **Vyukov bounded MPMC**
(per-cell `seq` turnstile, monotonic positions) · **Michael-Scott queue** (dummy
node, helping, the reclamation problem) · **Treiber stack** + ABA-in-practice
(tagged head) · **memory reclamation** (fixed pool / refcount / hazard pointers /
epochs / QSBR) · **hazard pointers** (bounded garbage) · **epoch-based / RCU**
(cheap read side, unbounded-memory failure mode) · **seqlock** (1-writer
N-reader snapshot — *the* HFT market-data primitive) · **testing** (unit →
invariant stress → TSan → model checkers → forced interleaving) · **when NOT
lock-free** (the honest trade-offs + the decision checklist). Examples (7/7 OK):
`01_spsc_ring_buffer` — 20 M msgs, ordering+loss checksum OK, naive ~19 M msg/s;
`02_spsc_optimized` — **the ladder**: V0 naive ~25–27 ns/msg → V1 +padding
**~24–32 ns/msg (padding ALONE ≈ noise / sometimes worse — producer still
reloads `tail_` every push)** → V2 +cached index ~16–20 ns/msg (**~1.3–1.6×, the
real win**) — CLAUDE.md Rule 2 result; `03_mpmc_queue` — Vyukov, sum/count
invariant OK, **~6.5–7.5 M op/s vs `mutex+deque` ~4 M (~1.6×)**; `04_lock_free_stack`
— tagged Treiber, multiset invariant OK, no ABA, ⚠️ **~7 M op/s vs `mutex+vector`
~40 M op/s → lock-free ~5× SLOWER** (retry storm on one head; lock-free ≠ fast);
`05_seqlock` — 1 writer + 4 readers, **torn = 0**, **~180 M reads/s vs
`shared_mutex` ~2 M (~80–100×)**; `06_mutex_vs_lockfree` — SPSC hand-off: A
`mutex+queue` p50 ~1.2 µs / B `mutex+cv` p50 ~6 µs (futex wake) / **C lock-free
SPSC p50 ~0.4 µs, ~15 M msg/s** — here lock-free clearly wins (contrast `04`);
`07_false_sharing_fix` — pure false sharing on an index pair: **adjacent ~24
ns/op vs `alignas(64)` ~6 ns/op → ~3.5–4×**. `checkall` → **199 OK / 0 real fail
/ 1 expected**.

---

## Batch 8 (part 2) — ✅ COMPLETE (PHASE 15–16)

| Folder | Files | Status |
|---|---|---|
| `25-OBJECT-MODEL/` | 16 lessons + exercises + 8 examples (7 `.cpp` + `02_static_init_fiasco/`) | ✅ Full — verified |
| `26-CONCURRENCY/` | 17 lessons + exercises + 9 examples | ✅ Full — verified |

**`25-OBJECT-MODEL` (PHASE 15):** what an object *is* (region of storage,
subobjects, `sizeof ≥ 1`) · **object lifetime** (ctor-complete → dtor-start,
storage vs lifetime, reuse, `std::launder`, implicit-lifetime types) · four
storage durations · **static init order fiasco** + construct-on-first-use +
`constinit` · **temporaries & lifetime extension** (the 4 dangling cases) ·
**trivial / trivially-copyable / standard-layout / POD /
`has_unique_object_representations`** · object vs value representation + padding ·
**alignment deep** (`alignas`, over-aligned types, aligned `new` vs `malloc`,
`std::align`) · **placement new** · **strict aliasing** · type punning · **the
four casts** (`dynamic_cast` internals) · **vtable layout exact** (MI thunks,
virtual inheritance) · **ABI** (Itanium, break catalog, stable-API design) ·
**UB catalog** (50+ by category) · **how the compiler exploits UB**. Examples
(7/7 `.cpp` OK): `02_static_init_fiasco/` — **fiasco reproduced**, link order B
prints `BADLOG[1/(null)]` (the `std::string` member's ctor hadn't run);
`06_strict_aliasing` — `aliasing_trap` `delta` real at `-O0`, **`0` at `-O2
-fstrict-aliasing`**; `08_ub_examples` — `overflow_check(INT_MAX)` = `1` at every
`-O`, `0` only with `-fwrapv`; `04_type_properties` — trait matrix + **`memcmp ==
-1` without `memset` first**; `07_vtable_inspect` — MI → two vptr, subobject at
offset 8.

**`26-CONCURRENCY` (PHASE 16):** concurrency vs parallelism (+ Amdahl) · process
vs thread · `std::thread` (join-or-`terminate`) · passing data (decay-copy,
`std::ref`, dangling captures) · **race conditions & data races** · critical
sections · `std::mutex` (uncontended CAS vs contended futex) · **lock guards** ·
**deadlock** (Coffman 4 + fixes) · `std::shared_mutex` (when the snapshot
pattern beats it) · **condition variables** (predicate form, lost/spurious
wakeup) · futures & promises · **build a thread pool** · **C++20 sync**
(`jthread`/`stop_token`/`latch`/`barrier`/`counting_semaphore`) · `thread_local`
· **false sharing** · concurrency bug catalog + TSan. Examples (9/9 OK, all
`-Wall -Wextra -Wpedantic ...` clean): `02_race_condition` — **~70–75% lost
updates, different every run**; `03_mutex_fix` (`-O2`, 16M inc): mutex **~1248
ms** / atomic **~430 ms** / local+combine **~1.9 ms**; `07_thread_pool` (`-O2`,
fib(30)×64, 8 cores): serial **~271** / async/task **~52** / pool(8) **~48 ms**;
`08_false_sharing` — **measured ~10×** (packed **~11258 ms** vs `alignas(64)`
**~1130 ms**); `04_deadlock` — opposite lock order shown safely via `timed_mutex`
+ `try_lock_for` (~10 near-deadlocks/run) + 2 fixes; `09_jthread_cpp20` — all 5
C++20 primitives. ⚠️ TSan/ASan not on this MinGW — race/deadlock examples show
the *effect*; READMEs give the Linux `-fsanitize` commands. `02_static_init_fiasco/`
uses `.cxx`/`.hpp` so the repo `*.cpp` check skips it.

---

## Batches 1 + 2 — ✅ COMPLETE (PHASE 0, 1, 2 done)

**Kya bana:**

| Folder | Files | Status |
|---|---|---|
| `00-START-HERE/` | 9 files | ✅ Full |
| `01-PROGRAMMING-BASICS/` | 13 lessons + 5 examples | ✅ Full — **PHASE 0** |
| `02-CPP-FIRST-STEPS/` | 12 lessons + 7 examples | ✅ Full — **PHASE 1** |
| `03-VARIABLES-DATA-TYPES/` | 17 lessons + 10 examples | ✅ Full — **PHASE 2** |
| `04-INPUT-OUTPUT/` | 13 lessons + 8 examples | ✅ Full — **PHASE 2** |
| `05-OPERATORS/` | 12 lessons + 6 examples | ✅ Full — **PHASE 2** |
| `06` – `49` | Detailed syllabus README each | ✅ Structure |
| Root | `README.md`, `Makefile`, `.gitignore` | ✅ |

**Saare 36 example files `-Wall -Wextra -Wshadow` ke saath compile-verified hain**,
aur benchmarks pe real measured numbers hain (guesses nahi).

**Aap abhi kya kar sakte ho:**
Setup karo → folder 01 → 02 → 03 → 04 → 05. Yeh **6–10 hafton** ka serious kaam hai
agar aap exercises aur examples poore karo.

---

## Batch 8 (part 1) — ✅ COMPLETE (PHASE 13–14)

| Folder | Files | Status |
|---|---|---|
| `23-ERROR-HANDLING/` | 13 lessons + exercises + 7 examples | ✅ Full — verified |
| `24-COMPILATION-LINKING/` | 15 lessons + exercises + 8 examples (6 multi-file dirs + a script + 2 `.cpp`) | ✅ Full — verified |

**`23-ERROR-HANDLING` (PHASE 13):** the whole landscape (return codes / `errno` /
exceptions / `expected` / `error_code` / assertions — *when which*) · exceptions
mechanics (`throw`/`try`/`catch`, catch order, rethrow, `exception_ptr`) ·
**stack unwinding** (ctor-mid throw, `noexcept` boundary → `terminate`,
`-fno-exceptions` RAII still works) · **exception safety** (basic/strong/nothrow,
copy-and-swap, `noexcept` move ⇒ `vector` realloc moves not copies) · `noexcept`
deep · custom exception hierarchy (`throw_with_nested`) · **measured exception
cost** · **`-fno-exceptions`** (why HFT, what's lost, the toolkit) ·
**`std::expected`** (monadic `and_then`/`transform`/`or_else`/`transform_error`)
· `std::error_code` / `error_condition` / custom categories · assertions /
`static_assert` / `std::unreachable` / `[[assume]]` / contracts · **UB catalog**
+ sanitizers. Examples (7/7 OK): `01_exceptions_basics` · `02_exception_safety`
(**leak-on-throw vs RAII rollback shown via a live-object counter**) ·
`03_raii_unwinding` · `04_exception_cost` (**happy path try/catch vs return-code
~1.0× (zero-cost); ~6000+ ns per throw+catch vs ~1.5 ns return → ~4000×; 0.1%
error rate → ~5×**) · `05_expected` (**builds under `-std=c++20` via a hand-rolled
`Expected<T,E>` AND `-std=c++23` real `std::expected`**) · `06_error_code`
(custom `order` category + portable `== std::errc::…`) · `07_no_exceptions`
(**compiles + runs with and without `-fno-exceptions`**).

**`24-COMPILATION-LINKING` (PHASE 14):** translation units + compilation model ·
preprocessor deep (macros, `#`/`##`, `__VA_OPT__`, X-macros, predefined macros,
`_Pragma`, traps) · include guards / `#pragma once` / IWYU / self-contained
headers / pImpl · **ODR deep** (loud "multiple definition" vs silent IFNDR,
`-Wodr`, `_GLIBCXX` flag drift) · linkage (internal/external/module, `static`,
anon namespaces, `-fvisibility`) · storage (`static`, `extern`, `inline` vars,
`thread_local`, `constinit`, init-order fiasco) · name mangling / `extern "C"` /
ABI / `c++filt` · object files & ELF (sections, `.symtab`, relocations, `.bss`) ·
**static vs dynamic linking** (`.a` member selection, PLT/GOT cost, `-fPIC`,
RUNPATH, HFT: static) · every common linker error + fix · Make (`-MMD -MP`
auto-deps, order-only prereqs, `.PHONY`) · CMake (targets +
`PUBLIC`/`PRIVATE`/`INTERFACE` usage requirements, `find_package`, generator
expressions, install) · build performance (header hygiene, PCH, unity builds,
ccache, ninja, `mold`) · binary tools (`nm`/`objdump`/`readelf`/`ldd`/`strings`/
`size`/`strip`/`addr2line`) · **LTO and PGO** (whole-program opt, profile-guided,
the HFT release config). Examples: `01_multi_file_project` (3 TUs → cross-TU
resolve) · `02_odr_violation` (**DEMO 1: `ld: multiple definition`; DEMO 2:
silent — `sizeof(Config)` 12 vs 16, caught only by `-flto -Wodr`**) ·
`03_static_library` (**`nm app` shows used members, NOT `calc::huge_unused`**) ·
`04_shared_library` (`greet.dll` + import lib; rebuild lib only → app output
changes) · `05_makefile_project` (**`touch header` → all `.o` rebuild via
auto-deps**) · `06_cmake_project` (CMake 4.0.2, `-DENGINE_FAST_PATH=ON` flips a
compile definition, `cmake --install`) · `07_binary_inspection.sh` ·
`08_preprocessor_demo.cpp` + `09_linkage_storage.cpp` (self-contained, pass the
folder check). Directory examples use `.cxx`/`.hpp` so the repo `*.cpp` checker
skips them.

---

## Batch 7 (part 2) — ✅ COMPLETE (PHASE 10–12)

| Folder | Files | Status |
|---|---|---|
| `20-ALGORITHMS-DSA/` | 17 lessons + problem-sets + 8 examples | ✅ Full — verified |
| `21-TEMPLATES/` | 16 lessons + exercises + 8 examples | ✅ Full — verified |
| `22-MODERN-CPP/` | 14 lessons + exercises + 8 examples (incl. multi-file modules demo) | ✅ Full — verified |

**`20-ALGORITHMS-DSA` (PHASE 10):** complexity + **its limits** · arrays
(two-pointer / sliding window / prefix sums) · sorting family (implement + bench)
· **`std::sort` internals (introsort)** · binary search (half-open invariant,
"search the answer") · linked lists (+ intrusive-arena pattern) · stacks/queues
(+ ring buffer) · **write your own hash table** (open addressing) · trees/BST ·
heaps · tries · graphs (BFS/DFS/Dijkstra/topo) · greedy (+ how to prove it) · DP
· bitmask · string algos (KMP / rolling hash / Z) · **cache-aware DSA** (the
synthesis) · graded problem sets. Examples (8/8 OK): `01_sorting_all`
(**bubble/selection ~600–730 ms vs `std::sort` ~1 ms @ n=20k; median-of-3 bug
found+fixed**) · `02_binary_search` · `03_linked_list` (**list sum 5M ~70 ms vs
vector ~1.7 ms → ~40×**) · `04_hash_table` (**open-addr build ~66 / lookup ~45
ns/op vs `unordered_map` ~295 / ~83**) · `05_bst` (ascending → height 1023 vs
middle-out → 10) · `06_graph_algorithms` · `07_dp_problems` (**fib(40) naive ~244
ms vs memo ~0.02 ms → ~11,500×**) · `08_flat_vs_pointer` (**flat tree traversal
4–11× a pointer tree**).

**`21-TEMPLATES` (PHASE 11):** why templates (zero-cost codegen) · function &
class templates · deduction · non-type / template-template params · full +
partial specialization · variadic + fold expressions · `if constexpr` ·
`<type_traits>` internals + your own · **SFINAE** · **concepts (C++20)** ·
**CRTP** + policy-based design · tag dispatch · TMP (`constexpr` over recursive
templates) · two-phase lookup / `typename`/`template` · instantiation model +
code bloat + `extern template` · **templates in HFT** (compile-time dispatch vs
`virtual`). Examples (8/8 OK): `01_function_templates` · `02_class_templates` ·
`03_variadic` · `04_if_constexpr` · `05_sfinae` · `06_concepts` · `07_crtp_policy`
(**CRTP ~0.56 ns/call vs virtual (real boundary) ~2.43 ns; `sizeof` no vptr**) ·
`08_compile_time_dispatch` (**virtual ~2.51 / template ~1.14 / `variant`+`visit`
~1.13 ns per call**).

**`22-MODERN-CPP` (PHASE 12):** standard-by-standard audit C++11→23 (move / auto
/ lambdas / smart pointers · generic lambdas / `make_unique` / relaxed
`constexpr` · `optional`/`variant`/`string_view` / structured bindings / `if
constexpr` / guaranteed copy elision · concepts / ranges / coroutines / modules /
`<=>` / `<bit>` / `std::format` · `std::expected` / `std::print` /
`std::generator` / deducing `this`) + deep dives on lambdas ·
`constexpr`/`consteval`/`constinit` · ranges · coroutines · modules · `<=>` ·
attributes · `std::format` · legacy→modern migration. Examples (8, 7 `.cpp` + the
multi-file `05_modules_demo/`): `01_lambdas_all` (`sizeof` closure 16 / empty 1 /
`std::function` 32) · `02_structured_bindings` · `03_ranges_pipelines`
(**pipeline ~6.5 ms vs hand loop ~10 ms — pipeline ~1.5× FASTER**, vectorization,
taught not hidden) · `04_coroutines_generator` (hand-rolled `Generator<T>`) ·
`05_modules_demo/` (`export module` / `import`, `-fmodules-ts` verified) ·
`06_spaceship` · `07_format_print` (custom `std::formatter` + `format_to_n`) ·
`08_legacy_to_modern`.

---

## Batch 7 (part 1) — ✅ COMPLETE (PHASE 7–9)

| Folder | Files | Status |
|---|---|---|
| `17-RAII/` | 11 lessons + exercises + 7 examples | ✅ Full — verified |
| `18-COPY-MOVE/` | 14 lessons + exercises + 9 examples | ✅ Full — verified |
| `19-STL/` | 26 lessons + exercises + 11 examples | ✅ Full — verified |

**`17-RAII` (PHASE 7):** what-is-a-resource · the RAII idiom (acquire-in-ctor /
release-in-dtor) · why it works (stack unwinding, the 5 dtor-skip gaps) ·
`unique_ptr` (zero overhead, move-only, `sizeof`==8 via EBO) · `shared_ptr`
(control block, atomic refcount, `make_shared` 1 alloc vs `shared_ptr(new)` 2) ·
`weak_ptr` (breaks cycles) · custom deleters (stateless→EBO→8, fn-ptr→16) ·
ownership-semantics spectrum + sink idiom · RAII for fd/lock/FILE · Rule of Zero ·
smart-pointer perf. Examples (7/7 OK): `01_raii_basics` · `02_unique_ptr` ·
`03_shared_ptr` · `04_weak_ptr` (bad cycle → 2 blocks leaked vs good) ·
`05_custom_deleter` (size table 8/8/16/16) · `06_exception_safety` (raw leaks 2
on throw vs RAII 0) · `07_smartptr_benchmark` (**shared_ptr copy ~33.6 ns ≈ 93x
raw**; unique_ptr create/destroy == raw).

**`18-COPY-MOVE` (PHASE 8):** copy ctor · copy assignment (self-guard,
allocate-before-free, copy-and-swap) · Rule of Three · value categories
(lvalue/prvalue/xvalue, 2-question model, `decltype((x))`) · rvalue refs +
forwarding refs + reference collapsing · move ctor / move assign (steal+null,
"valid but unspecified") · `std::move` = `static_cast<T&&>` (gotchas: const→copy,
`return std::move` pessimization) · Rule of Five · Rule of Zero · copy elision
(RVO guaranteed C++17, NRVO best-effort) · perfect forwarding (`std::forward`,
factory) · `noexcept` move (`move_if_noexcept`, vector growth) · move in practice.
Examples (9/9 OK): `01_copy_semantics` · `02_rule_of_three` (counted `new[]` →
`outstanding=0`) · `03_value_categories` · `04_move_semantics` · `05_std_move_demo`
(**1 intentional `-Wpessimizing-move`**) · `06_copy_elision` · `07_perfect_forwarding`
· `08_noexcept_vector` (**noexcept move ~152 ms vs copy ~468 ms, ~3x**) ·
`09_copy_vs_move_bench` (**copy 1M strings ~177 ms vs move vector ~0.0001 ms**).

**`19-STL` (PHASE 9):** STL architecture (M+N, half-open ranges, iterator
categories) · `vector` deep · `array`/`span` · `deque`/`list`/`forward_list` ·
`map`/`set` (RB-tree) · unordered containers (buckets, load factor, open
addressing) · container adapters · iterators + invalidation table · algorithms
×4 (non-modifying / modifying + erase-remove / sorting / binary-search+set+heap) ·
numeric (`accumulate` vs `reduce`, scans) · `<ranges>` (lazy views) · utility
types (`optional`/`variant`/`expected`) · `<functional>` (`std::function` cost) ·
`<chrono>` (timing correctly) · `<random>` · `<filesystem>` · `<regex>` (why
slow) · `<type_traits>` · `<bit>` · allocators · PMR · container performance
table · **STL in HFT** (the synthesis). Examples (11/11 OK): `01_vector_deep` ·
`02_map_vs_unordered` (**map ~1049 ns vs sorted-vec ~357 vs unordered ~113
ns/lookup**) · `03_algorithms_tour` (30+) · `04_erase_remove` · `05_ranges_demo` ·
`06_optional_variant` · `07_chrono_timing` · `08_std_function_cost` (**templated
~1.48 ns vs std::function ~5.11 ns; string closure +1 heap alloc**) ·
`09_custom_allocator` (logging + arena/bump) · `10_pmr_demo` (**stack-buffer pmr
vector → 0 global `new`**) · `11_container_benchmark` (**iterate: vector 0.67 /
list 18.0 / set 197 ms per 1M; membership: unordered_set 49 vs set 1066 ms**).

---

## Batch 6 — ✅ COMPLETE (PHASE 6)

| Folder | Files | Status |
|---|---|---|
| `15-CLASSES/` | 13 lessons + exercises + 8 examples | ✅ Full — verified |
| `16-OOP/` | 15 lessons + exercises + 8 examples | ✅ Full — verified |

**`15-CLASSES`:** what-is-a-class · members/`this` · access specifiers ·
constructors · **member init lists** (order trap) · destructors · `const` methods
(+ `mutable`) · static members · **operator overloading** · `friend` (hidden
friend) · `explicit` · nested/local classes · class layout (EBO). Examples (8/8
OK): `01_first_class` · `02_constructors` · `03_initializer_list`
(**2 intentional warnings**: `-Wreorder` + `-Wuninitialized`) · `04_const_methods` ·
`05_static_members` · `06_operator_overload` (`Money`, `<=>`) · `07_class_layout`
(Empty=1, Bad 24 vs Good 16, `is_polymorphic`) · `08_order_class` (HFT `Order`
state machine, `static_assert` trivially-copyable + 32 B).

**`16-OOP`:** inheritance · ctor/dtor order · virtual functions · **vtable/vptr
deep dive** · `override`/`final` · abstract classes · **virtual destructors** ·
object slicing · multiple inheritance · virtual inheritance/diamond ·
RTTI/`dynamic_cast` · **virtual dispatch cost (measured)** · **CRTP** ·
composition vs inheritance · SOLID. Examples (8/8 OK): `01_inheritance` ·
`02_virtual_functions` · `03_vtable_layout` · `04_virtual_destructor`
(**1 intentional** `-Wdelete-non-virtual-dtor`; counted-`new` → "2 blocks leaked") ·
`05_slicing` · `06_diamond` (non-virtual 2× `Device` vs `virtual` 1 shared) ·
`07_dispatch_benchmark` (**virtual ~23 ns vs direct/CRTP ~2.2 ns vs `variant` ~16 ns**) ·
`08_crtp` (mixins, static-dispatch strategy).

---

## Batch 5 — ✅ COMPLETE (PHASE 5)

| Folder | Files | Status |
|---|---|---|
| `12-POINTERS/` | 13 lessons + exercises + 8 examples | ✅ Full — verified |
| `13-REFERENCES/` | 9 lessons + exercises + 5 examples | ✅ Full — verified |
| `14-MEMORY/` | 11 lessons + exercises + 7 examples | ✅ Full — verified |

**`12-POINTERS`:** what-is-a-pointer · `&` · `*` · `nullptr` · pointer arithmetic ·
pointers+arrays · `const`+pointers · pointers-to-structs (`->`) · `int**` ·
`void*` · **function pointers** (→ virtual dispatch) · dangling pointers ·
pointer-bug catalog. Examples (8/8 OK): `01_basic_pointers` · `02_pointer_arithmetic` ·
`03_const_pointers` · `04_pointers_structs` · `05_function_pointers` ·
`06_dangling_pointer` (⚠️ deliberate dangling/UAF; 2 intentional `-Wreturn-local-addr`
/ `-Wdangling-pointer`) · `07_pointer_diagrams` (memory-state table) ·
`08_swap_via_pointers`.

**`13-REFERENCES`:** what-is-a-reference (alias) · reference vs pointer (full table) ·
references as parameters · **`const T&`** + lifetime-extension rules ·
returning references (dangling) · references in loops (`auto`/`auto&`/`const auto&`) ·
reference members (+ `reference_wrapper`) · **`T&&` / move intro** · reference bugs.
Examples (5/5 OK): `01_references_basics` · `02_pass_by_reference` ·
`03_const_ref_performance` (**by-value vs `const&` `std::vector` ~190x** measured) ·
`04_dangling_reference` (⚠️ deliberate; 2 intentional `-Wreturn-local-addr` /
`-Wdangling-reference`) · `05_ref_vs_ptr`.

**`14-MEMORY`:** layout revisit · **stack deep dive** · **heap deep dive** ·
`new`/`delete` · **leaks** · **UAF / double-free** · `static` / `thread_local`
(+ init-order fiasco) · **allocation cost (measured)** · **fragmentation** ·
placement-new / arena / pool intro · memory tools (ASan/Valgrind/heaptrack +
MinGW workarounds). Examples (7/7 OK): `01_memory_layout` · `02_stack_vs_heap`
(**stack vs heap ~83x**) · `03_new_delete` · `04_memory_leak` (⚠️ deliberate;
counted-`new` → "1006 blocks never freed") · `05_use_after_free` (⚠️ deliberate;
stale read + block reuse + double-free-commented) · `06_allocation_benchmark`
(**latency distribution p50/p99.9/max via rdtsc** — the tail problem) ·
`07_simple_pool` (**fixed pool vs `new` ~190x** + flat tail; placement new).

---

## Batch 4 — ✅ COMPLETE (PHASE 4)

| Folder | Files | Status |
|---|---|---|
| `09-ARRAYS/` | 11 lessons + exercises + 7 examples | ✅ Full — verified |
| `10-STRINGS/` | 9 lessons + exercises + 6 examples | ✅ Full — verified |
| `11-STRUCTS/` | 12 lessons + exercises + 8 examples | ✅ Full — verified |

**`09-ARRAYS`:** what-is-an-array · declaring/init · `a[i]==*(a+i)` · arrays+loops ·
**array decay** · 2D row-major · array params · `std::array` · **`std::span`** ·
array performance · common bugs. Examples (7/7 OK): `01_array_basics` ·
`02_array_decay` (intentional `-Wsizeof-array-argument`) · `03_2d_arrays` ·
`04_std_array` · `05_span_demo` · `06_oob_asan` (deliberate OOB; `_GLIBCXX_ASSERTIONS`) ·
`07_aos_vs_soa` (**~4x** measured).

**`10-STRINGS`:** C-strings · `std::string` basics · operations · **SSO deep dive** ·
**`string_view`** · conversions · performance · Unicode · bugs. Examples (6/6 OK):
`01_c_strings` · `02_std_string` · `03_sso_demo` (**15-char threshold measured, `-O0`**) ·
`04_string_view` · `05_fast_parsing` (**`from_chars` ~8x vs `stoi`, ~30x vs `stringstream`**) ·
`06_csv_parser` (zero-copy).

**`11-STRUCTS`:** what-is-a-struct · init (aggregate/DMI/designated) · nested ·
functions · **padding & alignment** · packed structs · **AoS vs SoA** · unions ·
**`std::variant`** · `enum class` · bitfields · `struct` vs `class`. Examples (8/8 OK):
`01_struct_basics` · `02_padding_demo` (**24→16 B reorder**) · `03_struct_optimization`
(**40→24 B**) · `04_aos_vs_soa` (**~1.6x**) · `05_unions` · `06_variant` · `07_enums` ·
`08_market_data_struct` (packed 32 B, `static_assert` locks).

**`build.ps1` update:** `san` target ab ASan na milne pe (MinGW) `-D_GLIBCXX_ASSERTIONS`
+ `-fstack-protector-all` pe fall back karta hai.

---

## Batch 3 — ✅ COMPLETE (PHASE 3)

| Folder | Files | Status |
|---|---|---|
| `06-CONDITIONS/` | 8 lessons + exercises + 4 examples | ✅ Full — verified |
| `07-LOOPS/` | 9 lessons + exercises + 6 examples | ✅ Full — verified |
| `08-FUNCTIONS/` | 13 lessons + exercises + 7 examples | ✅ Full — verified |

**`06-CONDITIONS` (Batch 3, folder 1/3):**
- Lessons: `if` · `else`/`else if` · nested + guard clauses · `switch` ·
  `switch` vs `if` (jump table, real `-O2` assembly) · `if`-with-initializer ·
  condition bugs · branch-prediction intro
- Examples (`make folder DIR=06-CONDITIONS` → 4/4 OK):
  `01_if_else` · `02_switch_demo` · `03_condition_bugs` (jaan-boojh kar warnings) ·
  `04_branch_benchmark` (measured **~7x** sorted vs unsorted, GCC 15.1 `-O2`)

**`07-LOOPS` (Batch 3, folder 2/3):**
- Lessons: `while` · `do-while` · `for` · range-`for` · nested loops + complexity ·
  `break`/`continue`/`goto` · loop bugs · loop patterns · loop performance
- Examples (`make folder DIR=07-LOOPS` → 6/6 OK):
  `01_loop_types` · `02_range_based` (**~50x** copy cost) · `03_nested_patterns` ·
  `04_loop_bugs` (safety-capped, 1 intentional `-Wtype-limits`) ·
  `05_cache_locality` (**~8x** row vs column) · `06_loop_unroll` (flag-dependent result)

**`08-FUNCTIONS` (Batch 3, folder 3/3):**
- Lessons: what-is-a-function · declaration/definition/ODR · params (value/ref/ptr) ·
  return values/RVO · **call stack deep dive** (frames, prologue/epilogue, real asm) ·
  scope/lifetime · default args · overload resolution · recursion · `inline` (ODR) ·
  `constexpr` functions · attributes · `main(argc, argv)`
- Examples (`make folder DIR=08-FUNCTIONS` → 7/7 compile OK):
  `01_first_functions` · `02_call_stack_trace` (frame addresses) · `03_overloading` ·
  `04_recursion` (**fib exponential ~11x/+5 measured**) ·
  `05_stack_overflow` (⚠️ deliberate runtime crash; 1 intentional `-Winfinite-recursion`) ·
  `06_inline_asm_check` (**inline vs call ~6x, ~1.9 ns/call**) · `07_command_line_args`

---

## Aage ke batches

| Batch | Content | Status |
|---|---|---|
| 3 | ~~`06-CONDITIONS` · `07-LOOPS` · `08-FUNCTIONS`~~ ✅ (PHASE 3) | ✅ |
| 4 | ~~`09-ARRAYS`, `10-STRINGS`, `11-STRUCTS`~~ ✅ (PHASE 4) | ✅ |
| 5 | ~~`12-POINTERS`, `13-REFERENCES`, `14-MEMORY`~~ ✅ (PHASE 5) | ✅ |
| 6 | ~~`15-CLASSES`, `16-OOP`~~ ✅ (PHASE 6) | ✅ |
| 7 | ~~`17-RAII`, `18-COPY-MOVE`, `19-STL`~~ ✅ (PHASE 7–9) | ✅ |
| 7 | ~~`20-ALGORITHMS-DSA`, `21-TEMPLATES`, `22-MODERN-CPP`~~ ✅ (PHASE 10–12) | ✅ |
| 7 (rest) | ~~`23-ERROR-HANDLING`~~ ✅ (PHASE 13) | ✅ |
| 8 | ~~`23`~~ · ~~`24-COMPILATION-LINKING`~~ ✅ (13–14) · ~~`25-OBJECT-MODEL`~~ · ~~`26-CONCURRENCY`~~ ✅ (15–16) · ~~`27-ATOMICS-MEMORY-MODEL`~~ · ~~`28-LOCK-FREE`~~ ✅ (17–18) | ✅ **DONE** |
| 9 | ~~`29`~~ · ~~`30`~~ ✅ (18–19) · ~~`31`~~ ✅ (20) · ~~`32`~~ ✅ (21) · ~~`33`~~ · ~~`34`~~ ✅ (22) · ~~`35-PROFILING-BENCHMARKING`~~ ✅ (23) | ✅ **DONE** |
| 10 | ~~`36-LOW-LATENCY-CPP`~~ ✅ (24) · ~~`37-HFT-FUNDAMENTALS`~~ ✅ (25) · ~~`38-MARKET-DATA`~~ ✅ (26) · ~~`39-ORDER-BOOK`~~ ✅ (27) · ~~`40-MATCHING-ENGINE`~~ ✅ (28) · ~~`41-HFT-CONCURRENCY`~~ ✅ (29) · ~~`42-HFT-NETWORKING`~~ ✅ (30) · ~~`43-HFT-OPTIMIZATION`~~ ✅ (31) · ~~`44-HFT-PROJECTS`~~ ✅ (32) | ✅ **DONE — HFT build track complete** |
| 11 | ~~`45-DEBUGGING`~~ ✅ (33) · ~~`46-INTERVIEW-PREP`~~ ✅ (33) · ~~`47-CODING-PROBLEMS`~~ ✅ (33) · ~~`48-CHEATSHEETS`~~ ✅ (33) · ~~`49-PROJECTS`~~ ✅ (33) · ~~**final gap audit**~~ ✅ (34) | ✅ **DONE — 🏁 COURSE STRUCTURALLY COMPLETE** |

**🏁 Saare 50 content folders (00–49) ban chuke, aur final gap audit (PHASE 34)
bhi ho gaya.** `WHAT-I-STILL-NEED-TO-LEARN.md` ke saare 5 sections ab "NONE"
padhte hain; genuinely-specialist topics explicit SPECIALIZED list mein hain.
**HFT build track (36–44) COMPLETE. Course structurally complete** — ab yeh
*seekhne* ki cheez hai, banane ki nahi: code likho, chalao, todo, samjho.

---

## Mera progress tracker

Yahan apna progress mark karo (`[x]` laga do jab folder complete karo):

```
[ ] 00-START-HERE          setup done, roadmap padha
[ ] 01-PROGRAMMING-BASICS   <- PHASE 0  (content ready)
[ ] 02-CPP-FIRST-STEPS      <- PHASE 1  (content ready)
[ ] 03-VARIABLES-DATA-TYPES <- PHASE 2  (content ready)
[ ] 04-INPUT-OUTPUT         <- PHASE 2  (content ready)
[ ] 05-OPERATORS            <- PHASE 2  (content ready)
[ ] 06-CONDITIONS          <- PHASE 3  (content ready)
[ ] 07-LOOPS               <- PHASE 3  (content ready)
[ ] 08-FUNCTIONS           <- PHASE 3  (content ready)
[ ] 09-ARRAYS              <- PHASE 4  (content ready)
[ ] 10-STRINGS             <- PHASE 4  (content ready)
[ ] 11-STRUCTS             <- PHASE 4  (content ready)
[ ] 12-POINTERS           <- PHASE 5  (content ready)
[ ] 13-REFERENCES         <- PHASE 5  (content ready)
[ ] 14-MEMORY             <- PHASE 5  (content ready)
[ ] 15-CLASSES           <- PHASE 6  (content ready)
[ ] 16-OOP               <- PHASE 6  (content ready)
[ ] 17-RAII              <- PHASE 7   (content ready)
[ ] 18-COPY-MOVE         <- PHASE 8   (content ready)
[ ] 19-STL               <- PHASE 9   (content ready)
[ ] 20-ALGORITHMS-DSA    <- PHASE 10  (content ready)
[ ] 21-TEMPLATES         <- PHASE 11  (content ready)
[ ] 22-MODERN-CPP        <- PHASE 12  (content ready)
[ ] 23-ERROR-HANDLING     <- PHASE 13  (content ready)
[ ] 24-COMPILATION-LINKING <- PHASE 14  (content ready)
[ ] 25-OBJECT-MODEL       <- PHASE 15  (content ready)
[ ] 26-CONCURRENCY        <- PHASE 16  (content ready)
[ ] 27-ATOMICS-MEMORY-MODEL <- PHASE 17     (content ready)
[ ] 28-LOCK-FREE            <- PHASE 17/18  (content ready)
[ ] 29-LINUX-SYSTEMS        <- PHASE 18     (content ready; examples Linux-only)
[ ] 30-NETWORKING          <- PHASE 19     (content ready; examples Linux-only)
[ ] 31-CPU-ARCHITECTURE     <- PHASE 20     (content ready; benchmarks measured)
[ ] 32-CACHE-MEMORY-PERFORMANCE <- PHASE 21 (content ready; benchmarks measured)
[ ] 33-COMPILER-OPTIMIZATION <- PHASE 22 (content ready; benchmarks measured)
[ ] 34-ASSEMBLY             <- PHASE 22 (content ready; read-not-write)
[ ] 35-PROFILING-BENCHMARKING <- PHASE 23 (content ready; benchmarks measured) <- BATCH 9 DONE
[ ] 36-LOW-LATENCY-CPP     <- PHASE 24 (content ready; benchmarks measured)
[ ] 37-HFT-FUNDAMENTALS    <- PHASE 25 (content ready; domain knowledge, concept-level)
[ ] 38-MARKET-DATA         <- PHASE 26 (content ready; benchmarks measured)
[ ] 39-ORDER-BOOK          <- PHASE 27 (content ready; benchmarks measured)
[ ] 40-MATCHING-ENGINE     <- PHASE 28 (content ready; tested + fuzzed + benchmarked)
[ ] 41-HFT-CONCURRENCY    <- PHASE 29 (content ready; pipeline built + benchmarked)
[ ] 42-HFT-NETWORKING     <- PHASE 30 (content ready; majority Linux-only, hand-reviewed)
[ ] 43-HFT-OPTIMIZATION   <- PHASE 31 (content ready; 7/7 examples verified, benchmarks measured)
[ ] 44-HFT-PROJECTS       <- PHASE 32 (THE CAPSTONE; 12/12 verified, mini HFT engine end-to-end)
[ ] 45-DEBUGGING          <- PHASE 33 (skill folder; 13 lessons + 5 examples + 10 buggy drills, verified)
[ ] 46-INTERVIEW-PREP     <- PHASE 33 (20 lessons + 8 verified coding examples + 4 design + 4 mocks)
[ ] 47-CODING-PROBLEMS    <- PHASE 33 (practice bank; 250 problems, 10 solution writeups, 10/10 examples verified)
[ ] 48-CHEATSHEETS        <- PHASE 33 (13 quick-reference sheets; markdown-only distillation of folders 01-47)
[ ] 49-PROJECTS           <- PHASE 33 (5 lesson files + 17 verified reference implementations; last content folder)
[ ] FINAL GAP AUDIT       <- PHASE 34 (all 5 WHAT-I-STILL sections -> NONE; specialists -> explicit SPECIALIZED list) 🏁
```
