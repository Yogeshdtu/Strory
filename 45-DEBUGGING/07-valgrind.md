# 07 — Valgrind: memcheck, helgrind, DRD, massif

## Prerequisites
- `06-sanitizers-practical.md` (sanitizers ke saath compare)
- `14-MEMORY` (heap model, leaks)
- `35-PROFILING-BENCHMARKING/13-cachegrind-callgrind.md` (cachegrind wahi tool-family)

## Yeh topic abhi kyun

Valgrind ka ek superpower hai jo sanitizers ke paas nahi: **recompile
nahi chahiye.** Koi bhi binary — third-party, release, "source kahan hai
pata nahi" — `valgrind ./prog` aur memcheck har invalid access, leak,
uninit read pakad leta. Cost: ~20–30× slow. Isliye: dev-loop mein
sanitizers, "recompile impossible" ya "deep leak audit" pe valgrind.

> **Linux-only.** MinGW/Windows pe valgrind nahi chalta. Yeh lesson ka
> workflow WSL ya Linux pe. Yahan commands + real-shape output.

---

## Valgrind kya hai

Ek **dynamic binary instrumentation** framework. Tumhara program ek
synthetic CPU (VEX IR) pe chalta hai; har memory access, har branch
valgrind ke "tool" se guzarta. Tools:

| Tool | `--tool=` | Kaam |
|---|---|---|
| **Memcheck** (default) | `memcheck` | invalid r/w, leaks, uninit values, bad free, overlap |
| **Helgrind** | `helgrind` | data races, lock-order (deadlock potential), misuse of pthreads |
| **DRD** | `drd` | data races (alt algo, kam RAM), lock contention stats |
| **Massif** | `massif` | heap profiler — kya, kab, kitni memory |
| **DHAT** | `dhat` | heap "hotness" — short-lived allocs, unused blocks |
| Cachegrind/Callgrind | (`35/13`) | cache + branch sim, call graph |

---

## Memcheck — sabse zyada use hota

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
         --error-exitcode=1 ./prog args
```

- `--leak-check=full` — har leak ki allocation stack.
- `--track-origins=yes` — uninit value **kahan se** aayi (2× slow, worth it).
- `--error-exitcode=1` — koi error → non-zero exit (CI).
- `-g` build behtar output deta (line numbers), par **zaroori nahi**.

### Invalid read/write

```
==1234== Invalid write of size 4
==1234==    at 0x1091AB: mode_oob(int) (03_memory_bugs.cpp:78)
==1234==  Address 0x4a8f048 is 0 bytes after a block of size 16 alloc'd
==1234==    at 0x484A2F3: operator new[](unsigned long)
==1234==    by 0x10919C: mode_oob(int) (03_memory_bugs.cpp:74)
```
"0 bytes after a block of size 16" = ek element aage. Alloc site bhi.
(Yeh `examples/03_memory_bugs.cpp` `oob` mode ka valgrind view — ASan
"heap-buffer-overflow" ka equivalent.)

### Use-after-free

```
==1234== Invalid read of size 4
==1234==    at 0x...: mode_uaf(int) (03_memory_bugs.cpp:67)
==1234==  Address 0x... is 0 bytes inside a block of size 32 free'd
==1234==    at 0x...: operator delete(void*, unsigned long)
==1234==    by 0x...: mode_uaf(int) (03_memory_bugs.cpp:66)
==1234==  Block was alloc'd at
==1234==    by 0x...: mode_uaf(int) (03_memory_bugs.cpp:63)
```

### Uninitialised values

```
==1234== Conditional jump or move depends on uninitialised value(s)
==1234==    at 0x...: mode_uninit(int) (03_memory_bugs.cpp:106)
==1234==  Uninitialised value was created by a stack allocation
==1234==    at 0x...: mode_uninit(int) (03_memory_bugs.cpp:93)   # <- --track-origins
```
Memcheck **uninit propagate** karta — value tabhi "error" jab woh ek
**observable** decision (branch, syscall arg, output) ko affect kare.
Isliye "created at" (declaration) aur "used at" (branch) alag lines.

### Leak summary

```
==1234== LEAK SUMMARY:
==1234==    definitely lost: 64 bytes in 1 blocks
==1234==    indirectly lost: 0 bytes in 0 blocks
==1234==      possibly lost: 0 bytes in 0 blocks
==1234==    still reachable: 1,024 bytes in 3 blocks
==1234==         suppressed: 0 bytes in 0 blocks
```

| Kind | Matlab | Action |
|---|---|---|
| **definitely lost** | koi pointer nahi bacha | **fix** — asli leak |
| **indirectly lost** | ek "definitely lost" block ke andar se reachable | parent fix karo, yeh bhi jaayega |
| **possibly lost** | pointer block ke **andar** point karta (interior) | usually fix; custom allocators/`std` mein kabhi noise |
| **still reachable** | exit pe pointer tha, free nahi kiya | aksar OK (global caches, `std::cout` buffers); zero karna optional |
| **suppressed** | suppression file ne chhupa diya | — |

---

## Helgrind — races aur lock order

```bash
valgrind --tool=helgrind ./prog
```
```
==1234== Possible data race during write of size 8 at 0x... by thread #3
==1234==    at 0x...: worker(bool) (04_race_debug.cpp:45)
==1234==  This conflicts with a previous write of size 8 by thread #2
==1234==    at 0x...: worker(bool) (04_race_debug.cpp:45)
==1234==  Location 0x... is 0 bytes inside global var "g_counter_racy"
```

Deadlock potential (`examples/05_deadlock_debug.cpp --deadlock`, ya even
safe path agar lock order kabhi inconsistent):
```
==1234== Thread #3: lock order "0x... before 0x..." violated
==1234==   Observed (incorrect) order: acquiring 0x...B, holding 0x...A
==1234==   Required order was established by acquiring 0x...A then 0x...B
```
Helgrind **potential** deadlock bina hang hue pakadta — dono orders kabhi
dekhe to warn. TSan (`06`) bhi yehi karta; helgrind ko recompile nahi
chahiye.

**Helgrind bahut slow + memory-hungry.** DRD (`--tool=drd`) alternative:
kam RAM, plus `--exclusive-threshold` / lock-contention stats.

---

## Massif — heap profiler

```bash
valgrind --tool=massif --time-unit=B ./prog
ms_print massif.out.<pid>   # ASCII graph + detailed snapshots
```
```
    MB
 19....^                                             #
      |                                       @@@@@@@#
      |                                  @@@@@@@@@@@@@#::
      |                            :::::::@@@@@@@@@@@@@#::
    0 +----------------------------------------------------->
      0                                                 GB
```
Har snapshot batata us waqt **kaunse call-stacks** ne kitna heap hold
kiya (`%`). "Memory kyun badh rahi", "kaunsa buffer 90% RAM khaa raha",
"peak kab" — sab yahan. `--pages-as-heap=yes` se `mmap` bhi count hota.

DHAT (`--tool=dhat`) alag angle: "kitne blocks allocate hue par kabhi
padhe nahi", "average lifetime", "read/write counts per block" — bloat
aur short-lived-alloc churn dhoondhne ke liye (`43/15`, `36/04-05`).

---

## Suppressions

Known-benign warnings (system libs, custom allocators) ko chhupao:
```
{
   libX_uninit_read
   Memcheck:Cond
   fun:some_libX_internal
   obj:*/libX.so.*
}
```
```bash
valgrind --suppressions=my.supp --gen-suppressions=all ./prog
```
`--gen-suppressions=all` har error ke liye ready-to-paste suppression
print karta. **Sirf genuinely-benign** cheezein suppress karo — apna bug
suppress karna = bug chhupana.

---

## Valgrind vs sanitizers — kab kya

| Situation | Tool |
|---|---|
| Dev inner loop, apna code, CI | **Sanitizers** (10× faster) |
| Recompile nahi kar sakte (3rd-party/release binary) | **valgrind memcheck** |
| Stack/global OOB | **ASan** (memcheck sirf heap) |
| Uninit reads, no Clang/MSan setup | **valgrind memcheck** |
| "Memory kyun badh rahi" (leak nahi, growth) | **massif / DHAT** |
| Deep one-time leak audit before release | **valgrind --leak-check=full** |
| Data race, can recompile | **TSan** |
| Data race, cannot recompile | **helgrind / DRD** |

> **HFT relevance:** hot path pe valgrind (30× slow) chala ke latency
> measure karna bemaani — woh `35-PROFILING` ka kaam (perf, rdtsc).
> Valgrind yahan **correctness gate** hai: nightly `memcheck` + `helgrind`
> run on the full engine with a replayed market-data session; `--error-
> exitcode=1` → koi naya invalid access / race → build red. Ek bhi "Invalid
> read" order book mein = potential wrong price = paisa. Massif se steady-
> state RSS verify (koi slow leak jo 6 ghante baad OOM kare).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — valgrind ke numbers ko performance samajhna
30× slow, serialized threads (helgrind), no real cache behaviour. Latency/
throughput ke liye **kabhi nahi**. `35-PROFILING`.

### Trap 2 — "still reachable" ko leak samajh ke panic
Global singletons, `std::cout` internal buffers, one-time caches — exit pe
"still reachable" dikhte hain, aksar theek. **definitely/indirectly lost**
pe focus.

### Trap 3 — `--track-origins` off, phir "kahan se uninit" na milna
Default off (2× cost). Uninit debug kar rahe ho → `--track-origins=yes`.

### Trap 4 — helgrind ke sa"Possible data race" ko ignore karna
"Possible" isliye ki happens-before via non-standard sync (custom spinlock,
`std::atomic` fence patterns) helgrind ko dikh nahi sakta. Zyaadatar
"possible" asli hote. Review karo, `ANNOTATE_HAPPENS_BEFORE` se genuine
custom-sync ko batao.

### Trap 5 — CI mein `--error-exitcode` bhool jaana
Default: valgrind errors print karke exit 0. CI green rehti. `--error-
exitcode=1` zaroori.

### Trap 6 — Multi-threaded program helgrind ke bina memcheck pe "clean"
Memcheck races **nahi** pakadta (sirf memory validity). "memcheck clean"
≠ "race-free". Helgrind/DRD alag chalao.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "valgrind = sab bugs" | memcheck: heap validity + leaks + uninit. Races → helgrind. Stack OOB → weak. |
| "still reachable = leak" | Aksar benign. definitely lost = leak. |
| "valgrind slow hai to bekaar" | Correctness gate, benchmark nahi. Nightly CI. |
| "sanitizer hai to valgrind ki zaroorat nahi" | Recompile-free + memcheck uninit + massif — unique. Complementary. |
| "-g nahi hai to valgrind kaam nahi karega" | Chalega — bas line numbers nahi, addresses (`addr2line` se resolve). |

---

## Hands-on (Linux / WSL)

```bash
g++ -std=c++20 -g -O1 45-DEBUGGING/examples/03_memory_bugs.cpp -o mb

valgrind --leak-check=full --track-origins=yes ./mb leak      # "definitely lost: 64 bytes"
valgrind --leak-check=full --track-origins=yes ./mb uaf       # "Invalid read ... free'd"
valgrind --track-origins=yes                   ./mb uninit    # "uninitialised value ... created at"
valgrind --tool=helgrind  ./... 04_race_debug                 # "Possible data race"
valgrind --tool=massif ./mb leak && ms_print massif.out.*     # heap graph
```

---

## Exercises

1. `LEAK SUMMARY`: `definitely lost: 0`, `indirectly lost: 4,096`,
   `possibly lost: 0`. Kya ho raha, aur `indirectly` ko kaise fix?
   <details><summary>Answer</summary>
   Ek data structure (jaise linked list ya tree) ka **root** to reachable
   hai (definitely lost 0), par uske andar ke nodes jo sirf ek "definitely
   lost" parent se... ruko — indirectly-lost tabhi jab parent definitely
   lost ho. Yahan definitely=0 to shayad `--show-leak-kinds` reading;
   sambhav: ek container jise tumne `.clear()` nahi kiya but jiska handle
   still-reachable hai → us case mein woh "still reachable" hota, "indirect"
   nahi. Agar genuinely indirect: parent block ko free karo (uske dtor/
   cleanup ko sahi karo) — child blocks uske saath chale jaayenge. RAII
   (`unique_ptr` in the nodes) se yeh class structurally khatam.
   </details>

2. Program valgrind ke **bina** theek, valgrind **ke saath** crash. Kaise
   possible, do wajah?
   <details><summary>Answer</summary>
   (1) Timing — valgrind 30× slow, ek race/timeout ab alag manifest.
   (2) valgrind memory layout badalta (apni redzones, alag `malloc`),
   ek pehle-se-maujood OOB/UAF ab ek alag (crash-karne wali) jagah hit
   karta — yaani bug pehle bhi tha, "lucky" tha. Dono cases: valgrind ne
   ek real bug expose kiya. Report padho.
   </details>

3. helgrind "Possible data race" par tum sure ho ki tumhare custom
   seqlock ne ordering guarantee kiya hai. Kya karoge?
   <details><summary>Answer</summary>
   Pehle **double-check** ki tumhara seqlock genuinely correct hai
   (`27-ATOMICS`, acquire/release fences). Agar haan: helgrind ko batao
   via `ANNOTATE_HAPPENS_BEFORE(&x)` / `ANNOTATE_HAPPENS_AFTER(&x)` (from
   `valgrind/helgrind.h`) us sync point pe — ab helgrind us edge ko
   samajhta, false positive gaya. Ya us access ke liye targeted
   suppression. **TSan bhi** try karo — `std::atomic` ko woh natively
   samajhta, custom asm ko nahi.
   </details>

4. massif graph: heap steady 50 MB pe, phir har 10 min +2 MB, kabhi kam
   nahi hota. `definitely lost` = 0. Diagnosis?
   <details><summary>Answer</summary>
   **Slow leak jo technically "reachable" hai** — ek ever-growing container
   (cache without eviction, `std::vector` jisme push hota rehta, log buffer,
   `std::map` of stale sessions). valgrind ise "leak" nahi bolta (pointer
   zinda hai) par yeh production OOM ka classic. massif ke detailed
   snapshot mein dekho **kaunsa call-stack** grow kar raha, phir bound/
   evict lagao. DHAT bhi useful ("blocks never read").
   </details>

---

## Interview questions

1. valgrind memcheck kaise uninit values track karta (propagation +
   "observable use")?
2. "definitely lost" vs "indirectly lost" vs "still reachable" — action
   har ek pe?
3. valgrind vs ASan — 3 concrete cases jahan har ek jeeta.
4. memcheck multi-threaded program mein races kyun nahi pakadta? Kya
   chalao?
5. Massif kis sawaal ka jawab deta jo memcheck nahi de sakta?
6. `--error-exitcode=1` CI mein kyun zaroori?

---

## Next
→ [`08-debugging-multithreaded.md`](08-debugging-multithreaded.md)
