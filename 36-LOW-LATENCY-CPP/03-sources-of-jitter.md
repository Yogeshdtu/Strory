# 03 — Jitter ke sources: ek hot-path audit checklist

## Prerequisites
- `35-PROFILING-BENCHMARKING/06-jitter-and-tail-latency.md` (poori sources
  table + "quiet core" recipe)
- `02-tail-latency.md`

## Yeh topic abhi kyun
Folder 35 lesson 06 ne jitter ke saare sources aur unhe measure/reduce
karne ka poora system diya. Yahan woh ek **actionable audit** ke roop mein:
apne hot path ki har line dekho aur poocho "yeh kabhi slow ho sakta?"

---

## GC: C++ mein NAHI hai

Sabse pehle yeh — kyunki Java/Go/C# se aane wale log isko pehle sochte hain:

- **C++ mein koi tracing garbage collector nahi.** Koi background sweep,
  koi stop-the-world pause.
- Memory **deterministically** free hoti: `delete`, dtor at scope end (RAII),
  `shared_ptr` ka refcount 0 pe.
- **Par** `new`/`delete`/`malloc`/`free` ka apna tail hai — allocator
  internals (example 01: mixed-churn p99.9 ~2.5 µs, max ~176 µs). Yeh
  eliminate ho sakta (pool/arena) — GC ke unlike.

Baaki lesson C++-specific jitter sources ke baare mein hai.

---

## Hot-path audit — har line, ek sawaal

### Memory
| Dikhta hai | Jitter? | Kahan |
|---|---|---|
| `new` / `make_unique` / `make_shared` | ✅ allocator tail | ex 01; fix: pool (04–07) |
| `std::vector::push_back` (grow) | ✅ realloc + copy | fix: `reserve` at startup (05) |
| `std::string` past SSO (~15 chars) | ✅ heap alloc | fix: fixed buffer / `string_view` |
| `std::map` / `unordered_map` insert | ✅ node alloc / rehash | fix: pre-sized / flat_hash / arena |
| `std::function` with big capture | ✅ heap alloc in ctor | ex 08; fix: template / `function_ref` (14) |
| first write to fresh memory | ✅ page fault (~µs) | ex 11; fix: pre-fault + `mlock` (18) |
| passing big struct by value | ✅ copy (+ maybe alloc) | fix: `const&` / `span` / move |

### CPU / control flow
| Dikhta hai | Jitter? | Kahan |
|---|---|---|
| `if` on unpredictable data | ✅ ~10-15 cyc mispredict | ex 06; fix: branchless (12) |
| `virtual` call on heterogeneous objects | ✅ indirect mispredict | ex 07; fix: CRTP/variant (13) |
| pointer chase (linked list / tree) | ✅ ~200 cyc DRAM miss | fix: flat layout (10, folder 32) |
| variable-length loop (book depth, msg size) | ✅ data-dependent time | fix: bound / incremental (01 ex 3) |
| big rarely-taken branch inline in the loop | ✅ I-cache pressure | ex 12; fix: hot/cold split (21) |
| `%` / `/` by a runtime value | ✅ ~20-40 cyc divider | fix: `& mask` (pow2), reciprocal-mul (34/08) |

### OS / syscalls / threads
| Dikhta hai | Jitter? | Kahan |
|---|---|---|
| any syscall (`read`/`write`/`clock_gettime`(non-vDSO)/`futex`) | ✅ ~hundreds ns + maybe reschedule | fix: busy-poll, batch, `io_uring` (17) |
| `std::mutex::lock` (contended) | ✅ futex + context switch | fix: lock-free (28) / per-thread / seqlock |
| logging on the hot path (format + write + maybe fsync) | ✅ big spike | fix: SPSC ring → logger thread (17, 35/16) |
| `std::this_thread::yield` / `sleep` | ✅ reschedule + cache cold | fix: busy-poll |
| shared counter / flag written by ≥2 threads on one cache line | ✅ false sharing ping-pong | ex 11; fix: pad / per-thread (11) |
| thread not pinned | ✅ migration → cross-core cache/TLB cold | fix: pin + isolate (19) |
| `new`/`delete` from multiple threads | ✅ allocator arena lock | fix: per-thread pool |

### Hardware / firmware (mostly ops, not code — 35/06)
Frequency scaling, C-state wake, SMI, SMT sibling contention, NUMA remote,
thermal throttle. Yeh **box tuning** hai (19, 20), code nahi — par hot-path
engineer ko inka pata hona chahiye taaki "code theek hai par p99.9 bura" ko
diagnose kar sake.

---

## Audit ka nateeja: 3 buckets

Har jitter source ko ek bucket mein daalo:

1. **Eliminate** — hot path se hata do (allocation → startup; syscall →
   housekeeping thread; unpredictable branch → branchless).
2. **Bound** — worst case ko cap karo (book depth → top N; message size →
   max; loop → fixed trip count).
3. **Make rare** — path ko so unlikely banao ki p99.9 tak na pahunche
   (cold path out-of-line + `[[unlikely]]`; fallback allocation with a
   pool that's sized so it never fills in practice).

Agar teenon mein se koi nahi ho sakta → woh source tumhara p99.9 floor hai.
Us pe honesty rakho (lesson 24).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "sirf ek chhota malloc"
`std::vector` grow, `std::string` SSO overflow, `std::function` capture,
exception throw, `std::stringstream` — sab chhupe allocations. Audit karo.

### Trap 2 — `clock_gettime` ko free maanna
vDSO wala ~20 ns (fine), `CLOCK_MONOTONIC_RAW` ek real syscall (~250 ns).
Hot-path timestamps = `rdtsc` (35/03) ya vDSO `CLOCK_MONOTONIC`.

### Trap 3 — logging "bas ek printf"
Format + buffer + maybe a lock + maybe a write + maybe fsync = a big,
variable spike. Hot path se async ring pe bhejo (35/16).

### Trap 4 — pinning ke bina "deterministic"
Unpinned thread har scheduler tick pe migrate/preempt ho sakta. Pinning +
isolcpus foundation hai (19).

### Trap 5 — GC se darna (C++ mein)
GC nahi hai. Energy allocator-tail / faults / syscalls / locks pe lgao.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "C++ mein GC pause" | GC nahi; deterministic teardown |
| "ek chhoti allocation OK hai" | har allocation ek tail source; hot path pe zero |
| "logging sasta hai" | format+write+lock = variable spike; async |
| "`clock_gettime` free" | vDSO ~20 ns; raw = syscall ~250 ns; use rdtsc |
| "code theek hai to p99.9 theek" | box tuning (freq/C-state/SMI/pin) bhi zaroori |

---

## Exercises

1. Yeh hot-path function audit karo — har jitter source list karo aur
   uska bucket (eliminate / bound / make-rare):
   ```cpp
   void on_quote(const Quote& q) {
       auto* ev = new BookEvent(q);                 // (a)
       book_[q.symbol].apply(*ev);                  // (b) std::unordered_map<string,Book>
       if (book_[q.symbol].depth() > 5)             // (c)
           for (auto& lvl : book_[q.symbol].levels()) recompute(lvl);  // (d)
       log_ << "quote " << q.symbol << " " << q.px << "\n";            // (e)
       delete ev;                                   // (f)
   }
   ```
   <details><summary>Answer</summary>

   (a) `new BookEvent` — **allocator tail** + (f) `delete`. *Eliminate*:
   `BookEvent` ko stack pe banao (yahan `ev` local hi hai — no need for
   heap at all), ya ek per-thread pool. Actually `ev` never escapes → just
   `BookEvent ev(q);`.
   (b) `book_[q.symbol]` — `unordered_map<std::string, Book>`: (i) `std::string`
   key lookup = hash a string + maybe SSO/heap; (ii) `operator[]` inserts if
   missing → node alloc + maybe rehash. **Allocator + hash tail.** *Eliminate*:
   symbols ko integers mein map karo at startup (a `symbol_id`), `std::vector<Book>`
   indexed by id — O(1), no hash, no alloc. And it's looked up **3 times** —
   cache the reference: `Book& b = book_[id];`.
   (c)/(d) `depth() > 5` then loop over **all** levels — **data-dependent
   work** jitter (deep book → long loop). *Bound*: top-N levels only, ya
   *incremental* — sirf changed level `recompute` karo, poora book nahi.
   (e) `log_ << ...` — **format + stream + maybe lock + maybe flush** = a
   variable spike, aur `<<` of a `std::string`/`double` may allocate.
   *Eliminate from hot path*: push a fixed-size log record into an SPSC ring;
   a logger thread formats + writes.
   Net: `on_quote` becomes — resolve `id` (precomputed), `Book& b = book_[id]`,
   `b.apply(q)` (incremental), maybe push one ring record. Zero allocation,
   bounded work, no syscall.
   </details>

2. Ek function ka p99.9 = 300 ns, p50 = 90 ns. Tumne allocation, syscall,
   lock, unpredictable branch — sab check kiye, koi nahi hai. `perf stat`
   dikhata IPC 2.5 (healthy), koi cache-miss spike nahi. Phir 300 ns kahan
   se? (Do candidates jo code audit se nahi dikhte.)

   <details><summary>Answer</summary>

   Code clean hai → jitter **bahar se** aa raha (35/06 HW/OS):
   (1) **Timer interrupt / scheduler tick** — har CPU pe ~100-1000 Hz ek
   IRQ; agar function ~90 ns hai aur ~µs ka tick occasionally overlap kare
   → ~0.1-1% samples 300+ ns. Fix: `nohz_full` + isolate the core, move IRQs
   off it (19).
   (2) **Frequency transition** — core light load pe turbo, phir ek P-state
   change (~tens of µs to settle, but individual samples during the
   transition run at lower clock). Fix: lock the frequency (`performance`
   governor / fixed freq, turbo off for consistency — 35/09, folder 31/13).
   (3) **SMI** — firmware core churata hai, OS-invisible, 10s-100s of µs.
   Fix: BIOS SMI knobs; detect with `hwlatdetect`.
   (4) **C-state exit** — agar function bursty hai (gaps mein core soya),
   wake latency ~µs. Fix: `processor.max_cstate=1` / `idle=poll` on the
   trading core.
   Diagnose: `perf record -e irq:*` / `perf sched` us 0.1% ko catch karne
   ko; `cyclictest` / `rtla timerlat` to characterize the OS/HW floor.
   </details>

---

## Interview questions

1. C++ mein GC kyun nahi; C++ ka equivalent memory-management tail source.
2. 8 hot-path jitter sources aur har ek ka bucket (eliminate/bound/make-rare).
3. "Chhupe" allocations — 4 places jahan STL silently heap hit karti.
4. Logging hot path pe kyun jitter; sahi pattern.
5. Code clean hone ke bawajood p99.9 bura — 3 external causes.

---

## Next
→ [`04-allocation-avoidance.md`](04-allocation-avoidance.md)
