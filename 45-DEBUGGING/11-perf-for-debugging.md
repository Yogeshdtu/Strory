# 11 — `perf` for debugging (performance bugs)

## Prerequisites
- `35-PROFILING-BENCHMARKING` (poora — `perf stat`, `perf record`, flame graphs)
- `29-LINUX-SYSTEMS/02-syscalls.md`, `29/13-page-faults-and-mlock.md`
- `43-HFT-OPTIMIZATION/03-finding-bottlenecks.md`

## Yeh topic abhi kyun

`35-PROFILING` ne `perf` ko **optimization** ke liye sikhaya — "kahan time
ja raha, kaise tez karein". Yahan alag lens: `perf` se **bug** dhoondhna —
ek "correctness-ish" performance bug. Symptoms jaise:

- "Kabhi-kabhi 100× dheema" (tail latency blowup)
- "Memory theek hai par program crawl kar raha"
- "CPU 100% par kaam nahi ho raha"
- "Ek accidental syscall / allocation hot loop mein ghus gaya"

Yeh sab functionally "sahi" output de sakte hain — bas galat *tarike* se.
`perf` inhe expose karta.

> **Linux-only** (`perf` = kernel `perf_events`). Windows: ETW / WPA,
> VTune, ya `35/14`. Yahan Linux workflow.

---

## `perf stat` — pehla scan

```bash
perf stat -d ./prog                    # basic + cache/branch detail
perf stat -e task-clock,context-switches,cpu-migrations,page-faults,\
minor-faults,major-faults,cycles,instructions,branch-misses ./prog
```

Bug-hunting ke liye jo dekhna:

| Counter | High/weird → suspect |
|---|---|
| **context-switches** | thread thrashing, lock contention, blocking syscalls in hot path |
| **cpu-migrations** | no affinity — thread cores badal raha, cache blown har baar (`29/11`) |
| **major-faults** | swapping! ya `mmap` file demand-paging in hot path — 1000× slower than minor |
| **minor-faults** (steady, non-zero) | allocation churn — new pages har iteration (`36/04-05`) |
| **instructions** way up vs expected | accidental O(n²), retry loop, wrong branch |
| **IPC (ins/cyc)** very low (<0.5) | stalls — memory-bound, or... |
| **branch-misses** high % | data-dependent branches, or a mispredict storm from corrupted state |

Example — "slow sometimes" pipeline:
```
   1,234,567  context-switches       #  1.973 K/sec     <-- BAHUT zyada
      45,678  cpu-migrations
   9,876,543  major-faults           <-- swapping ya mmap fault storm
```
→ yeh functional bug nahi, par "sahi answer, 50× der se". Neeche dig.

---

## `perf record` / `report` — kahan

```bash
perf record -g --call-graph dwarf ./prog       # -g: call graphs
#   (better: build with -fno-omit-frame-pointer, then --call-graph fp -- sasta)
perf report                                     # interactive TUI
perf report --stdio | head -40
```

Bug patterns in `perf report`:

- **Ek function jo wahan hona hi nahi chahiye** — `report` mein top pe
  `malloc`/`free`/`operator new` in a supposedly alloc-free hot loop
  (`43`), ya `__memmove_avx` (a `std::vector` insert in a churn loop —
  `39/06`), ya `std::_Rb_tree` (a `std::map` you thought was a flat array).
- **`__lll_lock_wait` / `futex`** high — lock contention (`08`).
- **`clock_gettime` / `localtime`** — time calls in hot path (`10`).
- **`__stack_chk_fail` samples** — canary checks everywhere (huge stack
  arrays).
- **kernel frames dominate** (`entry_SYSCALL_64`, `copy_user`) — syscalls
  in the hot loop.

### `perf annotate` — kaunsi line

```bash
perf annotate -s hot_function --stdio | head -50
```
```
 0.4 :  mov    rax,QWORD PTR [rdi+0x8]
42.1 :  idiv   rcx                      <-- 42% samples on one idiv (43/10)
 1.2 :  mov    QWORD PTR [rdi+0x10],rax
```
Ek instruction/line pe disproportionate samples = wahi bottleneck /
accidental cost.

---

## `perf trace` — strace, lekin sasta

```bash
perf trace -s ./prog                    # syscall summary (count, time, errors)
perf trace ./prog 2>&1 | head -50       # live syscall stream
```
```
   syscall     calls   errors  total (ms)   avg (ms)
   futex       182934      120    4521.3      0.025      <-- lock contention
   write        90123        0     812.4      0.009      <-- per-line logging (10)
   mmap         41200        0     390.1      0.009      <-- allocation churn
   clock_gettime 2M          0     ...                   <-- time in hot loop
```
"Yeh function har call pe ek `write` kar raha" ya "1M `clock_gettime`" —
turant dikhta. Hot path mein **koi bhi** unexpected syscall = bug.

---

## `perf top` — live, running system

```bash
perf top                        # poore system ka live profile
perf top -p <pid>               # ek process
```
Ek server abhi slow chal raha (attach nahi karna, disturb nahi karna) —
`perf top -p` se live dekho kaunsa function abhi CPU khaa raha. Ek runaway
loop, ek spin, ek regex, ek accidental recompute — instantly visible.

---

## Off-CPU: "CPU 0% par kaam nahi ho raha"

`perf record` **on-CPU** samples leta — jab thread block hai (mutex, I/O,
sleep) woh dikhta hi nahi. Off-CPU analysis ke liye:

```bash
# bcc/bpftrace (eBPF)
offcputime-bpfcc -p <pid> 5           # 5s: kahan-kahan block hua, kitni der
# ya perf sched
perf sched record -- sleep 5
perf sched latency                    # per-task scheduling latency
perf sched timehist                   # timeline: kab kaunsa thread on/off CPU
```

"Request 40 ms leti hai par CPU sirf 2 ms" → 38 ms blocked kahan?
`offcputime` batata: `futex_wait` (lock), `io_schedule` (disk),
`nanosleep` (a `sleep_for` koi bhool gaya), `epoll_wait` (waiting for
data — maybe fine).

> **HFT relevance:** ek order path jo p99 pe 50 µs spike karta. On-CPU
> profile "clean" — kyunki spike **off-CPU** hai: ek `futex` (logger lock,
> `10`), ya ek minor page fault (`mlockall` miss, `29/13`), ya ek
> involuntary context switch (thread not pinned / `isolcpus` not set,
> `29/11`, `29/15`). `perf sched timehist` + `perf stat -e page-faults,
> context-switches,cpu-migrations` in signals ko pakadta. Fix aksar
> tuning (`29/18` checklist), code nahi — par tumhe pehle **measure**
> karke prove karna hoga (`43/01`).

---

## `perf c2c` — false sharing / cache-line contention

```bash
perf c2c record ./prog
perf c2c report
```
```
HITM (remote): 1.2M   cacheline 0x...40
  offset 0x00: writer thread A  (enqueue)
  offset 0x08: writer thread B  (dequeue)
```
Do threads alag variables likhte par **same cache line** → line
CPU-to-CPU ping-pong (`43/08`, `31-CPU`, `27`). Functionally correct,
10–100× slower on that access. Fix: `alignas(64)` + padding.

---

## `perf stat` interpretation cheat-sheet (bug lens)

| Observation | Likely bug |
|---|---|
| major-faults > 0 (should be ~0 after warmup) | swapping / mmap demand-fault in hot path → `mlockall`, prefault |
| minor-faults grow linearly with work | allocation per unit work → pool / reserve (`36/04-05`) |
| context-switches huge | lock contention / oversubscription / blocking call in loop |
| cpu-migrations non-trivial | no `taskset`/affinity → pin threads (`29/11`) |
| instructions 10× expected | accidental O(n²), retry storm, wrong loop bound |
| IPC < 0.3 + LLC-misses huge | pointer-chasing / bad layout / working set blown (`43/07`) |
| branch-misses spike only sometimes | data-dependent branch on corrupted/unexpected input |

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `perf record` off-CPU bug ke liye
Blocked time samples nahi hote. "profile clean but slow" → off-CPU tools
(`offcputime`, `perf sched`).

### Trap 2 — `--call-graph dwarf` ka overhead
DWARF unwinding heavy (stack copy har sample). `-fno-omit-frame-pointer`
build + `--call-graph fp` sasta aur aksar kaafi (`05` trap 6, `43/04`).

### Trap 3 — `perf` bina symbols
Stripped binary → `perf report` mein hex addresses. `-g` / separate
debuginfo / `debuginfod`. `perf buildid-cache` se symbol server.

### Trap 4 — `perf_event_paranoid` / permissions
`perf record` → "Permission denied". `sudo sysctl kernel.perf_event_paranoid=1`
(ya `-1`), ya `sudo perf`, ya `CAP_PERFMON`. Containers mein aksar blocked.

### Trap 5 — Sampling frequency vs rare event
Default ~1–4 kHz sampling. Ek bug jo har 100ms pe 5µs ka spike hai —
sampling shayad miss kare. `-F 10000` (higher freq), ya event-based
(`-e` a specific tracepoint), ya `perf sched` (every switch, not sampled).

### Trap 6 — Measuring the sanitizer/debug build
ASan/`-O0` ke `perf` numbers meaningless. `-O2 -g -fno-omit-frame-pointer`.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`perf` = optimization only" | Off-CPU, syscall storms, page faults, false sharing = *bugs* it finds. |
| "Profile clean → not a perf bug" | On-CPU only. Blocked time invisible. `offcputime` / `perf sched`. |
| "Slow = need faster algorithm" | Aksar: accidental syscall/alloc/migration/fault. Measure first. |
| "100% CPU = working hard" | Could be a spin/retry loop doing nothing. `perf top`. |
| "`perf stat` numbers are just perf" | major-faults, migrations, ctx-switches → concrete misconfig/bugs. |

---

## Hands-on (Linux / WSL2)

```bash
g++ -std=c++20 -O2 -g -fno-omit-frame-pointer prog.cpp -o prog

perf stat -d ./prog                                  # scan the counters
perf trace -s ./prog                                 # syscall summary -- anything unexpected?
perf record -g --call-graph fp ./prog && perf report # where
perf annotate -s suspicious_fn --stdio | head        # which line/insn

# off-CPU (needs bcc-tools / bpftrace)
sudo offcputime-bpfcc -p $(pgrep prog) 5

# false sharing
perf c2c record ./prog && perf c2c report
```

Repo se: folder 43 ke pipeline examples pe `perf stat -d` chalao,
`43-HFT-OPTIMIZATION/examples/02_profile_analysis.sh` ke steps follow karo,
aur `perf trace -s` se confirm karo v3 pipeline mein hot loop ke andar
**koi** syscall nahi hai (v0 mein `std::string` alloc → `mmap`/`brk`
dikhega).

---

## Exercises

1. `perf stat`: `major-faults: 2,847` ek 3-second run mein. Functionally
   output sahi. Kya ho raha, kya fix?
   <details><summary>Answer</summary>
   **Major page faults** = disk se page laana (swap-in ya file-backed
   `mmap` demand paging). 2847 major faults = ~2847 disk round-trips
   (~ms each) hidden in a "correct" run. Wajah: process swapped (RAM
   pressure), ya ek bada `mmap`'d file jise pehli baar touch kiya ja raha.
   Fix: `mlockall(MCL_CURRENT|MCL_FUTURE)` + prefault (`29/13`), RAM
   badhao / working set ghatao, ya file ko warm karo (`readahead`).
   HFT: yeh unacceptable in the hot path — `29/18` checklist.
   </details>

2. `perf report` top: `38% __lll_lock_wait`, `22% pthread_mutex_lock`.
   Kya class, agla step?
   <details><summary>Answer</summary>
   **Lock contention** — 60% CPU sirf mutex ke liye wait/acquire mein.
   Agla: `perf record` ke call-graph se dekho **kaunsa** mutex (kaunse
   critical section se `lock` call ho raha), phir: critical section
   chhota karo, lock sharding, reader-writer lock, per-thread state
   (`08`, `41`), ya lock-free (`28`). `perf c2c` bhi — mutex ki line pe
   HITM.
   </details>

3. Server p99 latency kharab, par `perf record`/`report` bilkul flat
   aur clean dikhta (koi hot function nahi). Kaunse tools ab?
   <details><summary>Answer</summary>
   Spike **off-CPU** hai — `perf record` (on-CPU sampling) use miss
   karta. Use: `offcputime-bpfcc` / `bpftrace` (kahan block hua + kitni
   der), `perf sched record` + `perf sched timehist`/`latency` (scheduler
   delays, involuntary switches), `perf stat -e page-faults,context-
   switches,cpu-migrations` (per-run counts). Aksar culprit: minor page
   fault, futex, cpu migration, ya a `nanosleep`.
   </details>

4. `perf trace -s`: `clock_gettime` called **4.1 million times** in a
   2-second run. Diagnosis?
   <details><summary>Answer</summary>
   Time being read in a hot loop — probably per-message/per-iteration
   `now()` for logging, rate-limiting, or timestamping. Even vDSO
   `clock_gettime` (~20 ns, no real syscall) x 4M = 80 ms wasted; if it's
   a real syscall (some clocks/containers), 4M x ~500 ns = 2 seconds —
   the whole run. Fix: read time once per batch, use TSC (`rdtsc`) with
   offline calibration (`10`, `35/06`, `43`), or a coarse cached clock
   updated by a timer thread.
   </details>

---

## Interview questions

1. `perf` se **bug** kaise dhoondhoge (vs sirf "slow function")? 3
   examples.
2. On-CPU vs off-CPU profiling — ek latency spike jo `perf record` miss
   karega, aur usko kaise pakdoge.
3. `perf stat` mein major-faults / cpu-migrations / context-switches high
   — har ek kya bug batata?
4. `perf c2c` kya detect karta, aur functionally-correct code mein woh
   kaise dikhta?
5. `--call-graph dwarf` vs `fp` — trade-off, aur build ko kaise ready
   karoge.
6. Hot path mein ek accidental syscall — kaunse `perf` subcommand se
   turant pakdoge?

---

## Next
→ [`12-common-bug-patterns.md`](12-common-bug-patterns.md)
