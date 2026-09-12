# 15 — Sanitizers: ASan, UBSan, TSan, MSan

## Prerequisites
- `14-MEMORY` (heap, stack, lifetime, dangling), `17-RAII`, `18-COPY-MOVE`
- `26-CONCURRENCY` / `27-ATOMICS` (data races — TSan ke liye)
- `23-ERROR-HANDLING` (UB kya hai)

## Yeh topic abhi kyun
Profiling ka pehla rule: **jo measure kar rahe ho woh correct ho.** Undefined
behaviour (UB) ka code:
- galat result deta (benchmark meaningless),
- **unpredictable performance** deta — compiler UB assume karke aggressive
  optimize karta, ek chhota change se behaviour flip,
- **heisenbug jitter** — ek uninitialized read ya race kabhi 5 ns kabhi
  5000 ns.

Sanitizers = compiler-inserted runtime checks jo yeh bugs **exact line +
stack** ke saath pakadte. Har C++ engineer ka CI mein hona chahiye. Yeh
lesson: chaaron, kya pakadte, cost, aur kaise use.

---

## Chaar sanitizers

| Sanitizer | Flag | Pakadta | Slowdown | Memory |
|---|---|---|---|---|
| **ASan** (Address) | `-fsanitize=address` | heap/stack/global **buffer overflow**, **use-after-free**, use-after-return/scope, double-free, leaks (with LSan) | ~2x | ~3x |
| **UBSan** (Undefined Behavior) | `-fsanitize=undefined` | signed overflow, `nullptr` deref, misaligned access, OOB shift, bad enum/bool, `nullptr` passed to nonnull, some type confusion | ~1.2x | ~1x |
| **TSan** (Thread) | `-fsanitize=thread` | **data races**, deadlocks (lock-order), race on `std::mutex` misuse | ~5–15x | ~5–10x |
| **MSan** (Memory) | `-fsanitize=memory` | **reads of uninitialized memory** | ~3x | ~2x |

```bash
g++ -std=c++20 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined app.cpp -o app
./app                                    # bug -> report with file:line + stack
```

- **`-O1`** — sanitizers `-O0` pe bhi kaam karte par slow; `-O2` pe kuch
  checks weaker. `-O1 -g -fno-omit-frame-pointer` = sweet spot.
- **ASan + UBSan combine** ho sakte (`-fsanitize=address,undefined`).
- **TSan aur MSan alag** — ASan ke saath incompatible, ek dusre ke saath
  bhi. Alag builds.
- **MSan** ke liye poori program (incl. libc++/STL) instrumented honi
  chahiye warna false positives → practically clang + instrumented libc++.
  GCC MSan nahi deta. Isliye MSan least-used; UBSan + ASan cover most.

---

## ASan — memory errors

Sabse high-value. Pakadta:

```cpp
int a[10];
a[10] = 1;                    // stack-buffer-overflow

int* p = new int[5];
delete[] p;
p[0] = 7;                     // heap-use-after-free

int& dangling() { int x = 5; return x; }
int& r = dangling(); r = 1;   // stack-use-after-return (needs
                              //   ASAN_OPTIONS=detect_stack_use_after_return=1)

std::vector<int> v{1,2,3};
int* q = &v[0];
v.push_back(4);               // realloc
*q = 9;                       // heap-use-after-free (dangling into moved buffer)
```

Report format:
```
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x...
WRITE of size 4 at 0x... thread T0
    #0 0x... in main app.cpp:14
    #1 0x... in __libc_start_main
0x... is located 0 bytes inside of 20-byte region freed by:
    #0 operator delete[]
    #1 in main app.cpp:13
previously allocated by:
    #0 operator new[]
    #1 in main app.cpp:12
```
→ exact free site, alloc site, and use site. Debugging ka 90% kaam done.

**Leak detection (LSan)** — ASan ke saath default on Linux (`ASAN_OPTIONS=
detect_leaks=1`), program exit pe unfreed allocations + their alloc stacks.

**`ASAN_OPTIONS`** knobs: `detect_stack_use_after_return=1`,
`halt_on_error=0` (report all, don't stop), `abort_on_error=1` (core dump),
`detect_leaks=0` (disable LSan).

---

## UBSan — undefined behaviour

Cheapest, always run it:

```cpp
int x = INT_MAX; x += 1;              // signed integer overflow -> UB
int y = 1 << 33;                       // shift >= width -> UB
int* p = nullptr; int z = *p;          // null deref
double d = 1e300; int i = (int)d;      // out-of-range float->int cast
struct S { int a; } __attribute__((packed)) s;
int* pa = &s.a;                        // misaligned pointer (on strict targets)
```

Each → `runtime error: signed integer overflow: 2147483647 + 1 cannot be
represented in type 'int'` + `file:line`.

**`-fsanitize=undefined` sub-checks** you can select individually:
`signed-integer-overflow`, `shift`, `null`, `alignment`, `bounds` (with
`-fsanitize=bounds` + `_FORTIFY`), `vptr` (bad virtual call / type
confusion — needs RTTI), `float-cast-overflow`, `integer-divide-by-zero`,
`return` (missing return), `unreachable`.

**`-fsanitize=undefined -fno-sanitize-recover=all`** → abort on first (CI:
fail the build). Default is "report and continue".

`-fsanitize=integer` (clang) adds *unsigned* overflow (not UB, but often a
bug) — noisier, opt-in.

---

## TSan — data races

```cpp
int counter = 0;                       // plain int, no atomic, no lock
std::thread t1([&]{ for (int i=0;i<100000;++i) counter++; });
std::thread t2([&]{ for (int i=0;i<100000;++i) counter++; });
```
TSan: `WARNING: ThreadSanitizer: data race` + **both** stacks (the two
conflicting accesses) + the allocation of the racy object.

Catches (folder 26–28):
- Unsynchronized read/write of the same location from 2 threads.
- Missing `std::atomic` / wrong memory order that still races.
- Lock held on one path, not another.
- **Lock-order inversion** (potential deadlock) — even if it didn't deadlock
  this run.
- Races on `std::mutex`/`std::condition_variable` misuse.

TSan **only reports races that actually executed** (it's dynamic) — so
coverage matters. Run your concurrency tests under TSan in CI. ~5–15x
slowdown + big memory → run a subset.

Note: TSan needs position-independent code; on some setups add `-pie
-fPIE`. And it shadows all memory — very large working sets may not fit.

---

## MSan — uninitialized reads

```cpp
int x;                                 // uninitialized
if (x == 42) foo();                    // MSan: use-of-uninitialized-value
```
GCC nahi deta; clang, aur **poori program (STL included)** instrumented
honi chahiye → `-stdlib=libc++` bhi instrumented. High setup cost. Practical
alternative: `-ftrivial-auto-var-init=pattern` (fill uninitialized locals
with `0xAA...` → deterministic crashes) + ASan + good tests + `-Wmaybe-
uninitialized`. MSan tab jab tumhare paas ek clean instrumented toolchain
ho (Google-style).

---

## Sanitizers vs Valgrind memcheck

| | **ASan/MSan** | **Valgrind memcheck** |
|---|---|---|
| How | compile-time instrumentation | runtime binary instrumentation, no recompile |
| Speed | ~2–3x | ~20–30x |
| Stack overflows | ✅ (ASan) | ❌ (memcheck: heap only) |
| Uninit reads | MSan ✅ | ✅ memcheck |
| Needs rebuild | yes | no |
| Coverage | only instrumented code | any binary incl. third-party libs |

Use **ASan+UBSan in CI** (fast, rebuild is fine); Valgrind memcheck for a
binary you can't rebuild or a final deep pass.

---

## CI recipe

```bash
# build matrix — separate binaries:
g++ -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all  ...  # asan+ubsan
g++ -O1 -g -fno-omit-frame-pointer -fsanitize=thread            -fno-sanitize-recover=all  ...  # tsan
# run the full test suite under each. Fail the build on any report.
# UBSan is cheap enough to also ship in some staging/canary builds.
```

**Never ship a sanitizer build to production for performance-sensitive
paths** — 2–15x slower, changes timing (hides/creates jitter), and ASan's
redzones change cache behaviour. Sanitizers are a **correctness** tool;
performance numbers must come from a clean release build.

---

## MinGW / Windows note

Is repo ke box pe (MinGW-w64) ASan/UBSan **reliably nahi chalte** —
`libasan`/`libubsan` link issues common. `build.ps1 san` isliye fallback
karta: `-D_GLIBCXX_ASSERTIONS` (STL bounds checks) + `-fstack-protector-all`
(stack canaries). Woh raw C-array OOB nahi pakadta — uske liye **Linux/WSL +
GCC/Clang**, ya **clang-cl + ASan** (MSVC toolchain, jo Windows pe ASan
support karta). Serious sanitizer work → Linux.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sanitizer build ke numbers ko "performance" maanna
2–15x slow, redzones/shadow change cache + timing. Perf sirf clean release
build se.

### Trap 2 — ASan + TSan ek saath
Incompatible. Alag builds. (ASan + UBSan OK.)

### Trap 3 — TSan clean = "no races"
TSan dynamic — sirf executed code paths. Coverage (concurrency tests) chahiye.

### Trap 4 — UBSan "report and continue" CI mein
Default recovers. CI mein `-fno-sanitize-recover=all` → first UB = build fail.

### Trap 5 — MSan without instrumented STL
False positives everywhere. Need `-stdlib=libc++` instrumented (clang, MSan-
libc++). Otherwise use `-ftrivial-auto-var-init=pattern` + `-Wuninitialized`.

### Trap 6 — "sanitizers slow, skip in CI"
UBSan is ~1.2x — basically free. ASan ~2x — run nightly if not per-commit.
The bugs they catch (UAF, overflow, race) are exactly the ones that waste
days and cause production jitter.

### Trap 7 — `_FORTIFY_SOURCE` / `-D_GLIBCXX_ASSERTIONS` ke bharose baithna
Woh cheap hardening hai (some `str*`/`mem*` bounds, STL `operator[]` checks)
— useful, par ASan ke barabar nahi (no arbitrary heap/stack OOB, no UAF).
Ship hardening; test with ASan.

---

## Hands-on

```bash
# Linux/WSL:
g++ -std=c++20 -O1 -g -fsanitize=address,undefined 15_demo.cpp -o d && ./d
# deliberately: a[10] on int a[10];  ->  stack-buffer-overflow at 15_demo.cpp:NN

# this repo (Windows fallback):
./build.ps1 san 34-ASSEMBLY/examples/05_rdtsc.cpp
#   -> tries ASan; on MinGW falls back to _GLIBCXX_ASSERTIONS + stack-protector
```

Every `.cpp` example in this repo is compile-verified under strict warnings
(CLAUDE.md Rule 1); for the HFT track code, also run it under ASan+UBSan on
Linux before trusting any latency number from it.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "sanitizer build ka time = real time" | 2–15x + timing changes; release build only |
| "TSan pass = thread-safe" | only executed paths; needs concurrency test coverage |
| "ASan catches uninitialized reads" | that's MSan; ASan = OOB / UAF / leaks |
| "UBSan too slow for CI" | ~1.2x — basically free, always run |
| "hardening flags = sanitizers" | hardening is a subset; test with real ASan |
| "MinGW ASan works" | flaky; use Linux/WSL or clang-cl+ASan |

---

## Exercises

1. A benchmark gives 3.1 ns/op normally, but 3.1 → 12.0 ns/op after an
   unrelated one-line change elsewhere in the file, and back to 3.1 after
   reverting. No obvious reason. Before blaming alignment noise — what
   sanitizer would you run and why?

   <details><summary>Answer</summary>

   **UBSan** first (`-fsanitize=undefined`), then **ASan**. A change that
   flips performance dramatically with no logical connection often means the
   compiler was exploiting **undefined behaviour** — e.g. signed overflow it
   assumed couldn't happen, a strict-aliasing violation, an OOB read that
   happened to land on benign memory, or an uninitialized value. When the
   surrounding code changes, the optimizer's assumptions shift and the
   "lucky" fast codegen turns into slow (or vice versa). UBSan will flag the
   overflow / bad shift / misaligned access at a specific line; ASan will
   flag the OOB / uninitialized-driven branch. If both are clean, *then* it's
   genuinely alignment/layout noise (lesson 09) — measure across layouts and
   check `perf stat` counters. But UB is the first suspect for "spooky
   action at a distance" in performance.
   </details>

2. TSan reports a data race on a `bool done;` flag between a worker and a
   watcher thread. The code "works fine in testing". Is TSan wrong? What's
   the fix, and is `volatile bool` the fix?

   <details><summary>Answer</summary>

   **TSan is right; "works fine" is luck + a specific compiler/CPU.** An
   unsynchronized `bool` written by one thread and read by another is a data
   race = **undefined behaviour** in C++. It "works" today because x86 has
   strong memory ordering and the compiler happened not to hoist/cache the
   read — but `-O2` is allowed to hoist `while (!done) {}` into `if (!done)
   while(true){}` (the read is loop-invariant to the compiler, which doesn't
   know another thread writes it), giving an infinite loop. **`volatile
   bool` is NOT the fix** — `volatile` prevents the compiler from *eliding*
   the access but provides **no atomicity and no cross-thread ordering
   guarantees** (it's for MMIO/signal handlers, not threads — folder 27).
   The fix: **`std::atomic<bool> done;`** with `done.store(true,
   std::memory_order_release)` / `done.load(std::memory_order_acquire)` (or
   just default `seq_cst` if unsure). That's a real happens-before edge, and
   TSan goes quiet.
   </details>

3. Your CI runs ASan+UBSan and TSan on every commit. A teammate proposes
   also running the **performance** benchmark suite under the ASan build "to
   save CI time / one binary". Why is that a bad idea?

   <details><summary>Answer</summary>

   ASan changes performance in ways that make the numbers meaningless *and*
   misleading: (1) **~2x slower overall**, non-uniformly (instrumented loads/
   stores, quarantine for freed memory). (2) **Redzones** around every
   allocation change data layout and cache-line occupancy → cache-miss rates
   and false-sharing behaviour differ from release. (3) **Shadow memory**
   (1/8th of address space) competes for cache/TLB. (4) `malloc`/`free` go
   through ASan's allocator with different cost and locking → allocation-
   heavy code looks different. (5) **Timing/jitter changes** — a race or UB
   that ASan's layout hides (or exposes) shifts tail latency. So an ASan
   benchmark could show a "regression" or "improvement" that vanishes in
   release, or hide a real one. Correct setup: ASan/UBSan/TSan builds for
   **correctness** gates; a **separate clean `-O2` release build** on an
   isolated/pinned runner for the performance suite (min-of-N / percentiles,
   lesson 04). They're different jobs answering different questions.
   </details>

---

## Interview questions

1. ASan / UBSan / TSan / MSan — har ek kya pakadta, approx slowdown.
2. `volatile bool` vs `std::atomic<bool>` for a cross-thread flag — kyun volatile galat.
3. Sanitizer build ke performance numbers kyun trust nahi karte (3 reasons).
4. TSan "clean" ka matlab kya aur kya nahi (dynamic, coverage).
5. `-fno-sanitize-recover=all` — CI mein kyun.
6. Sanitizers vs Valgrind memcheck — kab kaunsa.
7. MSan itna kam kyun use hota (instrumented STL requirement); alternative.

---

## Next
→ [`16-production-measurement.md`](16-production-measurement.md)
