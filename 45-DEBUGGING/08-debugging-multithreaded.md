# 08 — Debugging multithreaded: races, deadlocks, thread inspection

## Prerequisites
- `26-CONCURRENCY` (poora — threads, mutex, race conditions, `09-deadlock.md`, `17-concurrency-bugs.md`)
- `27-ATOMICS-MEMORY-MODEL/01-data-race-definition.md`
- `02-gdb-basics.md`, `06-sanitizers-practical.md` (TSan)

## Yeh topic abhi kyun

Single-threaded bug: same input → same wrong output → step through → fixed.
Multithreaded bug: **timing-dependent**. Debugger attach karo aur woh chhup
jaata ("heisenbug" — observing changes it). Yeh lesson: races aur deadlocks
ke liye specific tools + gdb ki thread commands.

Examples: `04_race_debug.cpp` (data race), `05_deadlock_debug.cpp`
(AB/BA deadlock — iska gdb inspection file mein documented hai).

> **Windows/MinGW note:** gdb ka async/non-stop mode aur running-thread
> interrupt MinGW pe flaky hai. Thread commands (`info threads`, `thread
> apply all bt` **stopped** process pe, deadlock/crash ke baad) kaam karte.
> Live "interrupt a spinning thread" workflow Linux/WSL pe reliable.

---

## Multithreaded bug ke do bade family

| | Data race | Deadlock |
|---|---|---|
| Symptom | Galat/garbage data, non-deterministic, kabhi crash | Program **hang** — 0% CPU, koi progress nahi |
| Root | Shared mutable state bina sync (`27/01`) | Lock-order cycle (A→B vs B→A), ya missing unlock, ya wait-without-notify |
| Best tool | **TSan** (`-fsanitize=thread`), helgrind | **gdb attach** + `thread apply all bt`; helgrind (potential) |
| Prevention | atomics / mutex / sharding / immutability | global lock order, `std::scoped_lock`, lock hierarchies |

Plus chhote: **livelock** (threads active par no progress — retry storm),
**lost wakeup** (`notify` before `wait`), **ABA** (`28-LOCK-FREE`),
**false sharing** (perf bug, not correctness — `43/08`), **TOCTOU**
(check-then-act gap).

---

## gdb — thread commands

```
(gdb) info threads
  Id   Target Id                     Frame
* 1    Thread 0x7ffff7a ... (LWP 40)  0x... in main () at srv.cpp:88
  2    Thread 0x7ffff6f ... (LWP 41)  0x... in __lll_lock_wait ()
  3    Thread 0x7ffff5e ... (LWP 42)  0x... in __lll_lock_wait ()
  4    Thread 0x7ffff4d ... (LWP 43)  0x... in read () from libc

(gdb) thread 3            # thread 3 pe switch
(gdb) bt                  # us thread ka stack
(gdb) thread apply all bt # SAB threads ki stacks -- deadlock triage ka #1 command
(gdb) thread apply all bt full   # + har frame ke locals
```

`* ` current thread. `LWP` = Linux kernel thread id (`/proc/<pid>/task/`).
Frame column se turnat pata: kaun `__lll_lock_wait` (mutex pe atka), kaun
`read`/`futex_wait` (I/O / cv pe), kaun actually running.

### Scheduler control

```
(gdb) set scheduler-locking on    # step/continue karne pe SIRF current thread chale
(gdb) set scheduler-locking step  # step pe current only; continue pe sab (default-ish)
(gdb) set scheduler-locking off   # sab threads free (default)
```
`on` = ek thread ko isolate karke step karo bina doosre aage badhe.
Race ke around determinism ke liye useful; par **artificial** timing —
kabhi race ko hi mask kar deta.

### Non-stop mode

```
(gdb) set non-stop on             # (launch se pehle) ek thread ruke, baaki chalte rahein
```
Default "all-stop": ek thread ruka → sab ruke. Non-stop: sirf woh thread
jispe breakpoint laga. Live servers inspect karne ke liye (baaki requests
serve hote rahein). Advanced; MinGW pe support adhoora.

---

## Deadlock — gdb se diagnose

`examples/05_deadlock_debug.cpp --deadlock` chalao (hang), phir attach.
(Poora documented transcript file ke neeche hai.) Recipe:

```bash
./dl --deadlock &          # hangs; note the PID
gdb -p <PID>
(gdb) thread apply all bt
```

Dekho:
```
Thread 2 (bad_t1):
  #0  __lll_lock_wait ()
  #3  bad_t1 () at 05_deadlock_debug.cpp:54    # wants mtx_b
      (holds mtx_a since line 52)

Thread 3 (bad_t2):
  #0  __lll_lock_wait ()
  #3  bad_t2 () at 05_deadlock_debug.cpp:60    # wants mtx_a
      (holds mtx_b since line 58)
```

**Cycle:** T2 holds A wants B; T3 holds B wants A. Neither yields →
deadlock. Signature: **≥2 threads in `__lll_lock_wait`/`pthread_mutex_lock`**,
aur unke stacks ek-doosre ke held locks pe wait karte.

"Kaunsa mutex" — `pthread_mutex_lock`'s frame mein `print *(pthread_mutex_t*)$rdi`
(arg = mutex address), ya source se dekh lo kaunsa `lock_guard` line pe hai.
`__owner` field batata kis LWP ne hold kiya.

### Missing-unlock deadlock

Sirf **ek** thread `__lll_lock_wait` mein, baaki idle. Koi thread ne lock
liya aur `unlock` nahi kiya (early return, exception before `unlock`, `lock`
without RAII). `bt` un threads ka jo aage badh gaye — kis path se woh
`unlock` skip kar gaye. Fix: **hamesha `std::lock_guard`/`scoped_lock`**,
kabhi raw `.lock()`/`.unlock()` nahi.

---

## Data race — the workflow

1. **Reproduce reliably** — `01`: loop mein chalao.
   `for i in $(seq 1 200); do ./race || break; done`. `04_race_debug.cpp`
   `racy` mode har run alag total deta — that IS the reproduction.
2. **TSan** — `-fsanitize=thread`, ek race-y run, exact 2 lines + var.
   Yeh 95% cases mein khatam.
3. **TSan nahi** (opaque binary) → **helgrind/DRD**.
4. **gdb** — race ko *dhoondhne* mein weak (timing), par *samajhne* mein
   useful: suspect variable pe `watch`, `thread apply all bt` fire pe →
   dekho do threads same var likhte, koi common lock nahi.
5. **Fix** — atomic / mutex / shard / immutable (`04_race_debug.cpp`
   footer mein teeno). **Ek** choose karo based on access pattern.
6. **Verify** — 100–1000 runs clean, ya TSan clean. Ek bhi fail = adhoora.

### gdb se "kaun likhta hai bina lock"

```
(gdb) watch g_counter_racy
(gdb) continue
Hardware watchpoint: g_counter_racy
Old value = 41287
New value = 41288
worker () at 04_race_debug.cpp:45
(gdb) bt
(gdb) thread                       # kaunsa thread? T2.
(gdb) p $_siginfo                  # (crash ke liye)
(gdb) info threads                 # T3 bhi worker() mein? -> dono racing
```
Koi `lock_guard`/`mutex` frame `bt` mein nahi → unsynchronised. Confirm.

---

## Heisenbugs — jab observe karna bug ko badalta

Debugger/print ne timing badli → bug chhup gaya. Strategies:

| Technique | Kya |
|---|---|
| **Stress** | thread count ↑, iterations ↑, `sched_yield()` inject, run on more cores |
| **TSan** | timing-independent — happens-before analyse karta, na ki "is baar race hua" |
| **Delay injection** | suspect section ke aas-paas `std::this_thread::sleep_for(random)` — race window widen |
| **`rr`** (`09`) | record once (race included), phir deterministic replay — as many times as you want |
| **Logging (lock-free)** | `10` — hot path ko disturb kiye bina timeline capture |
| **`__thread` counters** | per-thread event counts, end mein compare — kaunsa thread kitna chala |

---

## `-D_GLIBCXX_DEBUG` aur assertions

Multi-thread ke liye specifically nahi, par: libstdc++ debug mode iterator
invalidation / range errors ko turant catch karta jo aksar
race-corrupted-container ke baad manifest hote. Ek cheap always-on check.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — Debugger attach kiya, bug gaya, "fixed maan liya"
Heisenbug. Timing badal gayi. TSan (timing-independent) ya stress-loop se
confirm karo bug abhi bhi hai. "gdb mein reproduce nahi hota" ≠ "fixed".

### Trap 2 — `set scheduler-locking on` ke saath race debug
Ek thread isolate karke step = artificial serialization = race window hi
band. Kabhi zaroori (ek thread ka logic dekhne ko) par race **timing** ke
liye off rakho + TSan.

### Trap 3 — Deadlock mein sirf ek thread ka `bt` dekhna
`thread apply all bt` — cycle **do (ya zyada)** threads milke banate.
Ek ka stack sirf "atka hai" batata, "kis pe" ka jawab doosre thread mein.

### Trap 4 — `printf` se multi-thread debug
`printf` khud ek lock leta (stdout) → timing badalta (Heisenbug) **aur**
interleaved garbage output. Lock-free ring buffer logging (`10`), ya
per-thread buffers.

### Trap 5 — Race "fix" kiya ek jagah mutex laga ke, doosri jagah miss
Woh **same** variable har jagah consistent policy chahiye. Ek unlocked
read baaki sab locked writes ke saath bhi = race. TSan poore program pe
re-run.

### Trap 6 — TSan false-positive maan ke ignore
TSan ke false positives durlabh hain (custom asm sync, ya un-instrumented
lib). "False positive" bolne se pehle: happens-before edge kahan hai?
Naam se batao (`ANNOTATE_*`) ya woh edge add karo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "gdb mein race dhoondhunga" | Timing → chhup jaata. TSan pehle. gdb race *samajhne* ko. |
| "Program hang = infinite loop" | Ho sakta deadlock (0% CPU) ya livelock (100% CPU, no progress). `top` + `bt`. |
| "Ek thread ka bt kaafi" | Deadlock cycle = multi-thread. `thread apply all bt`. |
| "Mutex laga diya, race gaya" | *Har* access same lock ke andar? Ek reader chhoot gaya to race abhi bhi. |
| "Reproduce nahi hota ab" | Heisenbug. Stress + TSan + `rr`. Not fixed. |

---

## Hands-on

```bash
# race -- reproduction IS the wrong total
g++ -std=c++20 -O2 -g -pthread 45-DEBUGGING/examples/04_race_debug.cpp -o race
for i in $(seq 1 20); do ./race; done            # har baar alag < 2000000
./race --safe                                     # atomic version -> hamesha 2000000

# Linux/WSL: TSan
g++ -std=c++20 -O1 -g -pthread -fsanitize=thread 45-DEBUGGING/examples/04_race_debug.cpp -o race_t
./race_t                                          # data race report: worker() line 45 x2

# deadlock inspection
g++ -std=c++20 -O0 -g -pthread 45-DEBUGGING/examples/05_deadlock_debug.cpp -o dl
./dl                                              # safe: completes
./dl --deadlock &                                 # hangs
gdb -p $!  -batch -ex "thread apply all bt" -ex kill   # dono threads in lock_wait
```

---

## Exercises

1. `top` dikhata process 100% CPU par output nahi de raha (hang jaisa).
   Deadlock hai ya nahi? Kaise confirm?
   <details><summary>Answer</summary>
   **Deadlock nahi** — deadlock = threads blocked = ~0% CPU. 100% CPU +
   no progress = **livelock** (retry storm, spin without acquiring) ya ek
   infinite loop (shayad race-corrupted state ki wajah se). `gdb -p` →
   `thread apply all bt` kai baar → dekho threads kis loop mein spin kar
   rahe (CAS retry? `while(!flag)`?). Fix: backoff, fairness, ya lock.
   </details>

2. `thread apply all bt`: 8 threads, saare `pthread_mutex_lock` → ek hi
   mutex address pe wait. Deadlock cycle hai?
   <details><summary>Answer</summary>
   **Cycle nahi** (cycle ke liye ≥2 alag mutexes + ulta order chahiye).
   Yeh **ek thread ne woh mutex hold kiya aur chhoda nahi** — missing
   unlock (early return / exception before `unlock` / infinite loop while
   holding). Us mutex ka `__owner` (`print *(pthread_mutex_t*)addr`)
   batata kaunsa LWP hold kiye hai; us thread ka `bt` dekho — woh kahan
   atka/loop kar raha jabki lock hold hai. Fix: RAII lock guard, critical
   section chhoti.
   </details>

3. TSan ne race report kiya `std::shared_ptr` ke control block pe, do
   threads ke beech. Tum `shared_ptr` copy kar rahe the (refcount atomic
   hai na?). Kya galat?
   <details><summary>Answer</summary>
   `shared_ptr` ka **control block (refcount)** thread-safe hai — alag
   threads alag `shared_ptr` **instances** ko freely copy/destroy kar
   sakte. Par **ek hi `shared_ptr` instance** ko do threads se
   concurrently modify (ek `reset()`, doosra copy) = race on the
   `shared_ptr` object itself (pointer + control-block-ptr do words).
   Fix: har thread apni copy le (by value capture), ya us shared instance
   ko mutex/`atomic<shared_ptr>` se guard karo. (`26-CONCURRENCY`,
   `17-RAII`.)
   </details>

4. Ek race sirf 16-core machine pe reproduce hota, tumhare 4-core laptop
   pe kabhi nahi. Debug strategy?
   <details><summary>Answer</summary>
   (1) **TSan** — timing-independent, 4 cores pe bhi race ko structurally
   pakad lega. (2) Stress on laptop: threads >> cores (oversubscribe),
   `sched_yield()`/random `sleep` inject suspect window mein, `taskset`
   se cores 2 tak seemit karke context-switch storm. (3) `rr` on the
   16-core box: ek race-y run record → laptop pe replay deterministically.
   (4) CI job on a 16-core runner with the stress loop.
   </details>

---

## Interview questions

1. Data race vs deadlock — symptom, root cause, best tool har ek.
2. `thread apply all bt` deadlock triage mein pehla command kyun?
3. Heisenbug kya? Debugger use karne se bug kyun chhup jaata? 3 counters.
4. Deadlock ka gdb signature (`__lll_lock_wait` + ...)? Missing-unlock
   deadlock kaise alag dikhta?
5. `set scheduler-locking on` — kya karta, race debugging mein iska
   khatra?
6. TSan race ko ek "lucky" (non-racing) run mein bhi kaise pakad leta?

---

## Next
→ [`09-reverse-debugging.md`](09-reverse-debugging.md)
