# 19 — Exercises: Linux systems programming

## Prerequisites
- Poora folder 29 (`01`–`18`)

## Kaise use karein
- Part A: output/behaviour predict karo, phir Linux/WSL pe verify.
- Part B: find-the-bug — har snippet mein ek systems bug hai.
- Part C: design — architecture decisions, HFT context.
- Part D: challenge — build + measure. Numbers likho, guess nahi (CLAUDE.md
  Rule 2).

> Examples `*.linux.cpp` hain — is Windows box pe compile nahi honge (`build.ps1
> folder 29-LINUX-SYSTEMS` unhe SKIP karta). Linux ya WSL chahiye:
> `g++ -std=c++20 -O2 -pthread file.linux.cpp -o file && ./file`

---

## Part A — Predict the behaviour

### A1
```c
#include <unistd.h>
#include <stdio.h>
int main() {
    printf("before ");           // no newline -> buffered
    fork();
    printf("after\n");
    return 0;
}
```
Terminal pe kitni baar "before" aur kitni baar "after" chhapega? Agar output ek
pipe (`./a | cat`) ho to?

<details><summary>Answer</summary>

**Terminal:** "before after\nafter\n" — wait. stdout terminal pe line-buffered.
`printf("before ")` mein `\n` nahi → buffer mein "before " baitha. `fork` ke
baad **buffer bhi copy** hota. Phir dono `printf("after\n")` → dono ka buffer
"before after\n" → dono flush → output: `before after` do baar (2× "before",
2× "after").

Wait — line-buffered mein `before ` (no `\n`) flush nahi hota turant, to haan
woh buffer mein rehta aur fork copy karta. Output: `before after` twice.

**Pipe:** stdout fully-buffered (block-buffered). Same logic, aur pakka — "before "
kabhi flush nahi hota until exit → fork copies it → 2× "before after".

**Sabak:** `fork` se pehle `fflush(NULL)`. (`03` trap 1.)
</details>

### A2
Ek program `strace -c ./prog` ke output mein `clock_gettime` ki **0 calls**
dikhti hain, jabki code loop mein `clock_gettime(CLOCK_MONOTONIC)` 1M baar
call karta hai. Kaise?

<details><summary>Answer</summary>

vDSO. `CLOCK_MONOTONIC` (with `clocksource=tsc`) kernel ke shared vDSO page se
serve hota — userspace mein ek `rdtsc` + arithmetic, koi `syscall` instruction
nahi → `strace` (jo `ptrace` se syscalls intercept karta) ko kuch dikhta hi
nahi. Agar `clocksource=hpet` hota to woh real syscalls hote aur `strace` mein
dikhte (~1M calls, ~1 µs each). (`02`, `16`.)
</details>

### A3
```c
void *p = mmap(NULL, 1UL<<32, PROT_READ|PROT_WRITE,
               MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);   // 4 GiB
// p != MAP_FAILED on a 2 GiB-RAM machine. Kaise?
```
Aur `top` mein `VIRT` aur `RES` ko kya hoga?

<details><summary>Answer</summary>

`mmap` sirf **virtual** address space reserve karta — 4 GiB ki ek VMA banti,
0 physical pages. 2 GiB RAM par bhi succeed (jab tak 64-bit address space mein
jagah hai aur `vm.overcommit` allow kare). `top`: `VIRT` += 4 GiB turant;
`RES` unchanged (0 pages touched). Jaise-jaise pages likhoge, minor faults se
`RES` badhega — 2 GiB tak pahunchte-pahunchte OOM-kill / swap. (`07`, `13`.)
</details>

### A4
Ek `SCHED_FIFO` priority-90 thread ek dedicated core pe pinned hai aur ek
`while(true) { poll_market_data(); }` busy loop chala raha (koi sleep/yield
nahi). Us core par `ksoftirqd`, RCU callbacks, aur ek `kworker` bhi "hain".
Kya hota?

<details><summary>Answer</summary>

FIFO-90 thread yield nahi karta → us core pe koi CFS/lower-RT thread (including
`ksoftirqd/N`, `kworker/N`, `rcuc/N`) chal hi nahi sakta. Agar `kernel.
sched_rt_runtime_us` default (950000) hai → har 1s mein ~50 ms ke liye kernel
FIFO thread ko **force-throttle** karta taaki woh kernel threads chal sakein →
ek periodic 50 ms jitter cliff. `-1` set karo **par tabhi** jab: (a) core
isolated + `rcu_nocbs` (RCU callbacks kahin aur), (b) IRQs kahin aur (`irqaffinity`),
(c) tumhe pata woh kernel threads is core pe kaam nahi karenge. Warna system
weirdness / watchdog. (`10`.)
</details>

### A5
`clock_nanosleep(CLOCK_MONOTONIC, 0, &ts_1us, NULL)` — `ts` = 1 microsecond.
Actual wakeup kitni der baad, aur kyun?

<details><summary>Answer</summary>

Typically **~50–100 µs later** (not 1 µs) on a normal box: sleep expiry ek
timer interrupt / tick pe check hota (`CONFIG_HZ`), aur wakeup ke baad scheduler
ko thread ko waapas run karna padta (runqueue, possible preemption of current).
`nohz`/high-res timers help par sub-µs precision nahi. Precise "act at T" ke
liye busy-spin to a monotonic deadline. (`16`.)
</details>

### A6
Do process `mmap(MAP_SHARED)` se ek `std::atomic<uint64_t> counter` share karte
hain. Process A `counter.fetch_add(1, relaxed)` karta, Process B padhta. Kaam
karega? Ordering guarantee milegi?

<details><summary>Answer</summary>

Haan, bilkul kaam karega — hardware cache coherence physical addresses pe kaam
karta, process boundary irrelevant. `fetch_add` atomic RMW instruction (`lock
xadd`) dono ke liye same. `relaxed` = sirf atomicity, koi ordering w.r.t. other
memory. Agar B ko "counter dekha, to X bhi visible hona chahiye" chahiye → A
`release`, B `acquire` (folder `27`). Ek gotcha: `std::atomic` shared memory
mein tabhi safe jab woh lock-free ho (`is_always_lock_free` — integers/pointers
haan). (`08`.)
</details>

### A7
`getrusage(RUSAGE_SELF)` startup pe `ru_minflt = 12000`, warm-up ke baad
`ru_minflt = 480000`, phir 1 ghante trading ke baad `ru_minflt = 480200`,
`ru_majflt = 0`. Interpretation?

<details><summary>Answer</summary>

Warm-up ne ~468k minor faults consume kiye — yani ~1.8 GB memory touch/pre-fault
kiya (468k × 4 KiB). Trading hour mein sirf +200 minor faults (~800 KB) — kuch
lazily-touched code path ya container buffer resize, largely fine par ~perfect
warm-up nahi (ideal: +0). `ru_majflt = 0` = **koi swap-in / file-page eviction
nahi** — memory locked/resident, exactly right. Action: woh +200 kahan se aa
raha trace karo (perhaps an error path or a rare message type) aur usse bhi
warm-up mein cover karo. (`13`.)
</details>

---

## Part B — Find the bug

### B1
```c
int fd = open("/data/md.bin", O_RDONLY);
char buf[4096];
ssize_t n = read(fd, buf, sizeof buf);
process(buf, sizeof buf);        // <-- ?
close(fd);
```

<details><summary>Answer</summary>

Do bugs: (1) `read` **short read** kar sakta — `n < 4096` possible (file
chhoti, pipe, signal). `process(buf, sizeof buf)` galat length use kar raha —
`process(buf, n)` hona chahiye, aur `n < 0` (error) check. (2) `open` fail
(`fd < 0`) check nahi. (3) Non-critical: `O_CLOEXEC` missing. Fix: check `fd`,
loop `read` until desired bytes or EOF, use actual byte count. (`05`.)
</details>

### B2
```c
volatile int g_stop = 0;
void handler(int sig) {
    printf("caught signal %d, cleaning up...\n", sig);
    cleanup_orders();            // malloc, mutex, file I/O
    g_stop = 1;
}
// main: signal(SIGTERM, handler); while (!g_stop) trade();
```

<details><summary>Answer</summary>

Async-signal-safety violation. `printf`, `cleanup_orders` (malloc, mutex, I/O)
handler mein — deadlock/corruption if the signal arrives mid-`malloc` or while a
lock is held. Also `signal()` (use `sigaction`), and `g_stop` should be
`volatile sig_atomic_t` / `atomic<int>`. Fix: handler sirf `g_stop = 1`; main
loop, next safe point pe, `cleanup_orders()` + exit. Better: `signalfd` on a
control thread. (`04`.)
</details>

### B3
```c
pid_t pid;
for (int i = 0; i < 100; i++) {
    pid = fork();
    if (pid == 0) { do_task(i); _exit(0); }
}
// parent continues, never calls wait()
```

<details><summary>Answer</summary>

Zombie leak — 100 children `_exit`, parent kabhi `wait`/`waitpid` nahi karta →
100 zombie processes (PID slots + task structs) jab tak parent na mare (phir
init reap karta). Under a long-running server this exhausts PIDs. Fix: `SIGCHLD`
handler with `while (waitpid(-1, &st, WNOHANG) > 0);`, or `signal(SIGCHLD,
SIG_IGN)` (Linux auto-reap), or a `waitpid` loop after the fork loop. (`03`.)
</details>

### B4
```c
// warm-up: pre-fault a 512 MiB arena
char *arena = malloc(512UL << 20);
for (size_t i = 0; i < (512UL << 20); i += 4096)
    arena[i] = 0;                // touch every page
// ... later, hot path uses arena ...
```
Compiled `-O2`. Warm-up "kaam nahi kar raha" — hot path pe abhi bhi faults.

<details><summary>Answer</summary>

`-O2` ne pre-fault loop ko **dead-store eliminate** kar diya — `arena[i] = 0`
ke stores kabhi read nahi hote, compiler unhe hata deta, pages touch hi nahi
hote. Fix: `volatile char *p = arena;` phir `p[i] = 0`; ya `memset(arena, 0,
size)` (compiler ise as-is chhodta); ya `asm volatile("" : : "r"(arena+i) :
"memory");` per iteration. Also `malloc` itself is lazy — same as `mmap`.
(`13` trap 3.)
</details>

### B5
```c
// pin this thread to core 3
cpu_set_t set;
CPU_SET(3, &set);                // <-- ?
pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
```

<details><summary>Answer</summary>

`CPU_ZERO(&set)` missing — `set` uninitialized stack memory. `CPU_SET(3, ...)`
sets bit 3, but the other bits are garbage → thread allowed to run on core 3
**and** whatever random cores the garbage bits name → migration continues,
pinning ineffective (or worse, EINVAL if garbage names non-existent CPUs). Fix:
`CPU_ZERO(&set); CPU_SET(3, &set);`. Also check the return value, and verify
via `/proc/self/stat` field 39 that the thread actually moved. (`11`.)
</details>

### B6
```c
struct SharedRing {
    std::atomic<uint64_t> head;
    std::atomic<uint64_t> tail;
    Msg buf[4096];
};
// creator:
int fd = shm_open("/ring", O_CREAT|O_RDWR, 0600);
ftruncate(fd, sizeof(SharedRing));
void *p = mmap(..., MAP_SHARED, fd, 0);
auto *r = new (p) SharedRing{};   // creator
// joiner (other process):
void *p2 = mmap(..., MAP_SHARED, fd2, 0);
auto *r2 = new (p2) SharedRing{}; // <-- ?
```

<details><summary>Answer</summary>

Joiner bhi `new (p2) SharedRing{}` (placement-new) kar raha — yeh `head`/`tail`
ko 0 kar deta aur `buf` reconstruct karta, **creator ka live state mita ke**.
Race: agar creator ne kuch push kar diya tha, joiner uska head/tail 0 kar deta
→ data loss / desync. Fix: joiner sirf `auto *r2 = static_cast<SharedRing*>(p2);`
(reinterpret, no construct). Sirf creator init kare, ek `O_EXCL` / init-flag se
decide. Also: `SharedRing` mein `alignas(64)` on `head`/`tail` (false sharing),
`static_assert(atomic<uint64_t>::is_always_lock_free)`, and `mlock` the region.
(`08`.)
</details>

---

## Part C — Design

### C1
Ek strategy process aur ek order-gateway process. Strategy per-tick ~200 ns
budget mein order intents generate karta hai, gateway unhe wire pe bhejta hai +
risk checks. Communication kaise? Kyun na sirf ek process, ek thread?

<details><summary>Answer</summary>

**Alag processes + shared-memory SPSC ring** (`08`). Reasons: (a) crash
isolation — gateway/risk bug se strategy nahi marti, strategy bug se orders ka
flow controlled shutdown ho sakta. (b) independent deploy/restart. (c) different
tuning (strategy = pure compute core; gateway = NIC-adjacent core). Communication:
lock-free SPSC ring in `/dev/shm` (or hugetlbfs), `release`/`acquire` index
hand-off, ~30–100 ns per message — thread jitna fast, process safety ke saath.
Single-thread would remove the ~100 ns but lose all isolation; not worth it for
the order path. (Hot *within* strategy — feed→book→signal — often is one thread.)
</details>

### C2
Naya trading box. Latency-critical thread ka p99.9 loop-time p50 se 30× hai.
Systematic diagnosis order kya hai?

<details><summary>Answer</summary>

Layer by layer, outside-in: (1) **Hardware/firmware:** `hwlatdetect` /
`turbostat --show SMI` — SMIs? C-states? thermal throttle? No OS tuning fixes
these. (2) **Scheduling:** `nonvoluntary_ctxt_switches` on the thread (`06`) —
preemption? Is the core in `/sys/devices/system/cpu/isolated`? SMT sibling
busy? migrations (`/proc/tid/stat` field 39)? (3) **Interrupts:** `/proc/
interrupts` diff on that core — NIC IRQ? timer (`LOC`, needs `nohz_full`)?
(4) **Memory:** `ru_majflt` (swap!), `ru_minflt` delta (cold pages), NUMA
placement (`numastat -p`). (5) **cgroup:** `cpu.stat` `nr_throttled`. (6) **App:**
`malloc`/lock/syscall on the hot path? Fix one layer, re-measure, next. (`18`.)
</details>

### C3
Tumhe ek "supervisor" process chahiye jo trading engine ko launch kare, uski
maut detect kare, restart kare. `fork`+`exec` vs `posix_spawn`? `SIGCHLD` vs
`pidfd`? Supervisor khud kitna heavy hona chahiye?

<details><summary>Answer</summary>

**`posix_spawn`** (ya `vfork`+`exec`) — supervisor is small, but `fork` still
copies page tables; `posix_spawn` skips that. **`pidfd_open`** at spawn time +
`poll(pidfd)` for exit — no `SIGCHLD` async handler (async-signal-safety
headache), no PID-reuse race, integrates into an `epoll` loop with other fds
(health-check sockets, config channel). Supervisor should be **tiny** (few MB
RSS, minimal deps, ideally static) so spawn is cheap and its own footprint
doesn't matter; it should `mlockall` too (a supervisor that pages out when the
engine dies can't restart it fast). On restart: exponential backoff, alert,
capture the engine's mini-dump. (`03`, `04`.)
</details>

### C4
Market-data feed handler ko 200 GB/day capture file replay karna hai backtester
mein, 128 GB RAM box pe, as fast as possible. File access strategy?

<details><summary>Answer</summary>

`mmap(MAP_PRIVATE, ...)` the whole file (virtual — 200 GB VMA fine on 64-bit),
iterate it as `const Packet*`. `madvise(MADV_SEQUENTIAL)` (aggressive readahead)
+ `madvise(MADV_DONTNEED)` on already-consumed regions (so RSS stays bounded,
no thrash on the 128 GB box). Effectively the OS streams it for you, zero
`read()` syscalls in the loop, no manual double-buffering. If the replay is
CPU-bound (parsing), overlap is automatic (readahead runs while you parse). If
you need it even faster and the format allows: `O_DIRECT` + `io_uring` with
big aligned reads to bypass page cache pollution. (`07`.)
</details>

### C5
2-socket box, one NIC on socket 1's PCIe. Design the thread + memory + IRQ
layout for a feed→book→strategy→gateway pipeline.

<details><summary>Answer</summary>

Everything on **socket 1** (with the NIC):
- Cores: pick 4 physical cores on node 1 (SMT siblings idle). feed-decode,
  book-build, strategy, order-encode — one pinned thread each.
- Boot: `isolcpus`/`nohz_full`/`rcu_nocbs` for those 4 cores + siblings;
  `irqaffinity` = a couple of node-1 housekeeping cores.
- NIC: RX/TX queue IRQs pinned to node-1 housekeeping cores; RSS queue count =
  that housekeeping core count; low coalescing; RX ring big.
- Memory: `numactl --membind=1` (or `mbind`); all pools/rings/tables on node 1;
  huge pages reserved on node 1; parallel first-touch by the owning threads.
- Socket 0: OS, logging, monitoring, and non-latency work (research/replay) — or
  a mirrored independent trading stack for a second NIC.
- Never cross the UPI link on the hot path. Verify with `numastat -p`,
  `/proc/interrupts` diff, `cyclictest -a <cores>`. (`14`, `15`, `11`, `18`.)
</details>

---

## Part D — Challenge (build + measure)

> Linux/WSL pe. Real numbers likho.

### D1 — Syscall cost on your machine
`examples/01_syscall_cost.linux.cpp` chalao. Apne box pe napo: userspace call,
`clock_gettime` (vDSO), raw `syscall(SYS_getpid)`. Ratios likho. Phir
`cat /sys/devices/system/cpu/vulnerabilities/*` dekho — kitni mitigations
active? Agar possible ho (VM / test box), `mitigations=off` boot karke dobara
napo. Kitna farak?

<details><summary>Expected shape</summary>

Userspace call ~1–3 ns. vDSO `clock_gettime` ~15–30 ns. Raw syscall trap
~250–700 ns with mitigations on. `mitigations=off` → syscall ~120–200 ns
(roughly 2–3× faster). vDSO/userspace barely change. The syscall/userspace
ratio (~100–300×) is the lesson: hot path pe traps GINO. (`02`.)
</details>

### D2 — Page fault storm, then warm it
`examples/07_page_faults.linux.cpp` chalao. Cold pass ka avg ns/page aur "worst"
note karo. Phir `mlockall` + `MAP_POPULATE` + `memset` wala pass — faults 0,
per-page ~10 ns. Ab ek variant likho jo warm-up ke **baad** `fork()` karta hai
aur child mein pehli write measure karta — COW fault dikhega. Numbers?

<details><summary>Expected shape</summary>

Cold minor fault ~200–600 ns avg, worst ~10–100 µs (zeroing / PT alloc / THP).
Warm ~10 ns/page, 0 faults. Post-`fork` first write in either process → COW
fault ~1–3 µs per page (copy + TLB) — warm-up ka "0 faults" property tut gaya.
Sabak: warm **after** fork, per process (`13`).
</details>

### D3 — Pinning kills the tail
`examples/06_cpu_affinity.linux.cpp` chalao unpinned, phir `taskset -c 3`, phir
(agar core 3 isolated kar sako: `isolcpus=3 nohz_full=3` boot) teesri baar.
Har run ka p50, p99.9, max likho. Kitna gap p50 aur max mein har case mein?
Phir `grep nonvoluntary_ctxt_switches /proc/self/status` before/after.

<details><summary>Expected shape</summary>

Unpinned: p50 ~p99 close, par p99.9/max 5–50× (migration + preemption).
`taskset` pinned: p99.9/max tighten to ~2–5× p50 (no migration; still OS
interference on a shared core). Isolated (`isolcpus`+`nohz_full`) + pinned:
max within ~10–30% of p50, `nonvoluntary_ctxt_switches` ~0. The tail is where
tuning shows up — p50 barely moves. (`11`.)
</details>

### D4 — mmap vs read for a large file
Ek ~4 GB file banao (`fallocate -l 4G /tmp/big`). Do programs: (a) `read()` in
a 64 KiB-buffer loop, checksum every byte; (b) `mmap` + checksum. Cold cache
(`echo 3 | sudo tee /proc/sys/vm/drop_caches`) aur warm cache dono pe time karo.
`strace -c` se syscall counts. Kya farak, kab kaunsa jeetta?

<details><summary>Expected shape</summary>

Cold: both ~disk-bound (similar wall time); `read` version does ~65k `read`
syscalls, `mmap` does ~0 (page faults instead). Warm cache: `mmap` a bit faster
(no user↔kernel copy), but TLB pressure on huge files can eat that. `mmap` wins
for random access / shared-across-processes; `read` with a big buffer wins for
one-shot sequential (readahead already optimal, simpler error handling). (`07`.)
</details>

### D5 — Clock cost + resolution
`examples/09_clock_comparison.linux.cpp` chalao. Har clock ka ns/read aur min
observable step likho. `cat /sys/devices/system/clocksource/clocksource0/
current_clocksource` — `tsc`? Agar `hpet`/`acpi_pm` mile aur badal sako, `tsc`
pe switch karke dobara napo — `CLOCK_MONOTONIC` kitna tez hua? Phir ek calibrated
`rdtscp`-to-ns converter likho aur uski accuracy `CLOCK_MONOTONIC` ke against
check karo over 10 seconds.

<details><summary>Expected shape</summary>

With `tsc`: `CLOCK_MONOTONIC` ~15–25 ns, `_COARSE` ~5–8 ns (but ~1 ms step),
`_RAW` ~250+ ns (syscall), `rdtscp` ~8–15 ns. With `hpet`: `CLOCK_MONOTONIC`
~500–1000 ns — switching to `tsc` is a ~30–50× win. Calibrated `rdtscp`
should track `CLOCK_MONOTONIC` to within a few ppm over 10 s if `constant_tsc`
+ `nonstop_tsc` are set. (`16`.)
</details>

---

## Next
→ [`../30-NETWORKING/00-README.md`](../30-NETWORKING/00-README.md)
