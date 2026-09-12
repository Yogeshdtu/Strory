# 06 — /proc aur /sys: introspection aur tuning knobs

## Prerequisites
- `05-file-descriptors.md` ("sab kuch file hai")
- `01-linux-architecture.md` (kernel subsystems)

## Yeh topic abhi kyun
`/proc` aur `/sys` = kernel ke andar jhaankne aur use tune karne ka
**filesystem interface**. Koi special API nahi — `cat`, `echo`, `open`/`read`.
HFT tuning ka **90%** yahin hota: `clocksource` set karna, `nr_hugepages`
reserve karna, CPU governor lock karna, IRQ affinity, `swappiness`, THP off.
Aur runtime introspection (RSS, page faults, context switches, socket stats)
bhi yahin. Yeh folder `18` (tuning checklist) ka toolbox hai.

---

## `/proc` — processes + kernel state

**Pseudo-filesystem** (disk pe kuch nahi; read pe kernel on-the-fly generate
karta).

### Per-process: `/proc/<pid>/` (ya `/proc/self/` = calling process)

| Path | Kya |
|---|---|
| `status` | human-readable: VmRSS, VmHWM, Threads, `voluntary_ctxt_switches`, `Cpus_allowed_list`, SigQ |
| `stat` | machine-readable ek line: utime, stime, num_threads, starttime, `processor` (last CPU) |
| `maps` / `smaps` | memory regions: `[heap]`, `[stack]`, `[vdso]`, mmap'd files, per-region RSS/dirty (`smaps`) |
| `fd/` | open fds (symlinks) — `05` |
| `task/<tid>/` | per-thread: har thread ka apna `stat`, `status`, `comm`, affinity |
| `sched` / `schedstat` | scheduler stats: run time, wait time, timeslices |
| `limits` | `RLIMIT_*` current/max |
| `environ` / `cmdline` | NUL-separated env aur argv |
| `io` | rchar/wchar (bytes), syscr/syscw (read/write syscall counts) |
| `numa_maps` | har region kaunse NUMA node pe (`14`) |

### System-wide: `/proc/`

| Path | Kya |
|---|---|
| `/proc/cpuinfo` | per-core: model, MHz, flags (`constant_tsc`, `tsc_reliable`, `pdpe1gb` for 1GB pages) |
| `/proc/meminfo` | MemFree, HugePages_Total/Free, AnonHugePages, Dirty, Mlocked |
| `/proc/interrupts` | per-CPU interrupt counts per IRQ — NIC IRQ kaunse core pe (`15`) |
| `/proc/stat` | boot time, per-CPU jiffies (user/nice/sys/idle/iowait/irq/softirq) |
| `/proc/loadavg` | 1/5/15-min run-queue length + running/total procs |
| `/proc/sys/...` | **tunable** kernel params (= `sysctl`) — neeche |
| `/proc/pressure/{cpu,memory,io}` | PSI — "kitni der tasks resource ke liye stalled the" |
| `/proc/schedstat`, `/proc/softirqs`, `/proc/net/*` | scheduler / softirq / network stack counters |

---

## `/proc/sys` = `sysctl` — kernel tunables

`echo` se likho (runtime), `/etc/sysctl.d/*.conf` se persist.

```bash
# padho
cat /proc/sys/vm/swappiness                 # ya: sysctl vm.swappiness
# likho (runtime)
echo 1 > /proc/sys/vm/swappiness            # ya: sysctl -w vm.swappiness=1
```

HFT-relevant knobs (detail unke apne lessons mein):

| sysctl | Default | HFT setting | Kyun |
|---|---|---|---|
| `vm.swappiness` | 60 | `1` (ya 0) | trading memory kabhi swap na ho |
| `vm.nr_hugepages` | 0 | reserve N | explicit 2MB pages (`12`) |
| `kernel.numa_balancing` | 1 | `0` | auto page migration = surprise stalls (`14`) |
| `kernel.sched_rt_runtime_us` | 950000 | `-1` (careful) | SCHED_FIFO ko throttle na karo (`10`) |
| `net.core.busy_poll` / `busy_read` | 0 | `50` µs | socket recv spin, syscall-block se bacho (`30/11`) |
| `net.ipv4.tcp_low_latency` | 0 | (legacy, largely no-op modern) | — |
| `net.core.rmem_max` / `wmem_max` | ~208 KB | bade | burst market data drop na ho |
| `kernel.perf_event_paranoid` | 2 | `-1` (dev) | `perf` full access (`35`) |

Non-`/proc/sys` tuning `/sys` mein:

```bash
# CPU frequency governor -- performance pe lock (no ramp-up latency)
echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
# Transparent Huge Pages -- latency-critical pe often 'madvise' ya 'never'
echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
# clocksource -- tsc chahiye (hpet slow)
cat /sys/devices/system/clocksource/clocksource0/available_clocksource
echo tsc > /sys/devices/system/clocksource/clocksource0/current_clocksource
# IRQ affinity -- IRQ 129 ko sirf CPU 2 par
echo 4 > /proc/irq/129/smp_affinity        # bitmask: 0b100 = CPU2
```

---

## `/sys` — device model, tunables, hardware topology

`sysfs` = kernel object hierarchy as directories. Zyada structured than `/proc`.

| Path | Kya |
|---|---|
| `/sys/devices/system/cpu/cpuN/topology/` | `thread_siblings_list` (hyperthread pairs), `core_id`, `physical_package_id` |
| `/sys/devices/system/cpu/cpuN/cache/indexK/` | L1/L2/L3 size, `coherency_line_size` (= cache line, usually 64), `shared_cpu_list` |
| `/sys/devices/system/node/nodeN/` | NUMA: `cpulist`, `meminfo`, `distance` (`14`) |
| `/sys/class/net/eth0/` | `mtu`, `speed`, `queues/`, `statistics/` (rx/tx bytes, drops) |
| `/sys/kernel/mm/transparent_hugepage/` | THP policy (`12`) |
| `/sys/fs/cgroup/` | cgroup v2 hierarchy — limits, `cpu.max`, `memory.max` (`17`) |

**Cache line size programmatically:**
```bash
cat /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size   # 64
```
(Yeh wahi 64 hai jo folder `28` ke `alignas(64)` / `hardware_destructive_
interference_size` mein aata tha.)

---

## Reading from C++

Bas file I/O — koi library nahi:

```cpp
#include <fstream>
#include <string>

long read_long(const char* path) {
    std::ifstream f(path);
    long v = -1; f >> v; return v;
}

// self RSS (KB):
// /proc/self/status me "VmRSS:   12345 kB" line dhoondho
```

Ek chhoti stat-scraper HFT process ke andar useful hai: har few seconds
`/proc/self/status` se RSS, `voluntary_ctxt_switches`,
`nonvoluntary_ctxt_switches` padho aur metrics pe bhejo. **`nonvoluntary`
context switches badhna = tumhe preempt kiya jaa raha** — pinning/isolation
check karo (`11`).

---

## Internal working

- `/proc` reads **snapshot** hote — `open` pe kuch nahi, `read` pe kernel us
  waqt ka data format karke deta. Isi liye ek badi `/proc/<pid>/maps` ko
  padhte-padhte process khud change ho sakta (torn view).
- `/proc/<pid>/stat` ek hi line — `atomic` read ke liye ek `read()` mein poora
  buffer mein le lo, phir parse (`comm` field mein spaces/parens ho sakte —
  last `)` se parse karo).
- `sysfs`/`procfs` writes turant effect (`echo performance > governor` = abhi).
  Persist ke liye `sysctl.conf`, `tuned` profile, kernel cmdline, ya udev rules.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `/proc` ko hot loop mein parse karna
`/proc/self/stat` padhna = ek `open`+`read`+`close` (3 syscalls) + text parse.
Monitoring thread mein har 1–5 s theek; hot path mein kabhi nahi.

### Trap 2 — `sysctl -w` runtime change ko permanent samajhna
Reboot pe gayab. `/etc/sysctl.d/99-hft.conf` + `sysctl --system`, ya `tuned`
profile, ya kernel boot cmdline (`isolcpus`, `hugepagesz`).

### Trap 3 — container mein `/proc` host ka dikhta
`/proc/cpuinfo` sab host cores dikhata hai chahe cgroup ne 2 cores diye hon.
`std::thread::hardware_concurrency()` bhi host count deta. cgroup-aware code:
`/sys/fs/cgroup/cpu.max` ya `sched_getaffinity` padho (`17`).

### Trap 4 — `smaps` bahut slow
`/proc/<pid>/smaps` har mapping ka detailed accounting — ek bade process pe
padhne mein ms lag sakte (kernel har VMA walk karta). Monitoring mein `status`
(VmRSS) prefer karo; `smaps_rollup` (aggregated) sasta.

### Trap 5 — IRQ `smp_affinity` set kiya, `irqbalance` ne ulta diya
`irqbalance` daemon periodically IRQ affinity redistribute karta. HFT box pe
`systemctl disable --now irqbalance`, phir manually pin (`15`).

### Trap 6 — `scaling_governor` "performance" par bhi turbo/C-states jitter
Governor sirf frequency scaling. Deep C-states (C6) se wakeup latency (~µs) aur
`intel_pstate` turbo transitions bache rehte. Boot: `intel_idle.max_cstate=1
processor.max_cstate=1 idle=poll` (power ki keemat pe).

---

## > **HFT relevance**

> - **Provisioning script** har trading box pe: governor=performance,
>   clocksource=tsc, THP=madvise/never, swappiness=1, hugepages reserved,
>   `irqbalance` off + NIC IRQs on housekeeping cores, `numa_balancing`=0. Sab
>   `/proc`/`/sys` writes. File `18` = complete list.
> - **Self-monitoring:** process apne `/proc/self/status` se `VmRSS`,
>   `nonvoluntary_ctxt_switches`, `/proc/self/io` se syscall counts har few
>   seconds scrape kare → agar nonvol ctxt switches spike → alert (koi hot
>   thread preempt ho raha).
> - **`/proc/interrupts` diff** — do snapshots ka farak batata NIC IRQs sahi
>   core pe ja rahe ya housekeeping core "chori" ho raha.
> - **`/proc/pressure/cpu`** — PSI se pata "kya tasks CPU ke liye stall ho
>   rahe" bina full profiling ke.

---

## Hands-on

```bash
# Apne process ki live stats
watch -n1 'grep -E "VmRSS|ctxt_switches|Threads" /proc/self/status'

# Cache line size (= alignas value folder 28 se)
cat /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size

# Hyperthread pairs
for c in /sys/devices/system/cpu/cpu[0-9]*; do
  echo "$c -> siblings $(cat $c/topology/thread_siblings_list)"
done

# Clocksource (tsc chahiye)
cat /sys/devices/system/clocksource/clocksource0/current_clocksource

# NIC drops (rx_dropped badhna = buffers chhote / core busy)
cat /sys/class/net/eth0/statistics/rx_dropped
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`/proc` real files hain" | pseudo-fs; read pe kernel generate karta |
| "`sysctl -w` permanent hai" | runtime only; `sysctl.d` / cmdline se persist |
| "`/proc/cpuinfo` container ka apna" | host ka; cgroup limits alag jagah |
| "governor=performance = zero jitter" | frequency only; C-states/turbo alag |
| "`smaps` bas ek aur file" | mahenga; bade process pe ms lag sakte |
| IRQ pin kiya, ho gaya | `irqbalance` reset kar dega — pehle disable |

---

## Exercises

1. `/proc/self/status` mein `voluntary_ctxt_switches` vs
   `nonvoluntary_ctxt_switches` — HFT thread ke liye kaunsa alarming?

   <details><summary>Answer</summary>

   `nonvoluntary` — matlab scheduler ne tumhe force preempt kiya (timeslice
   khatam ya higher-prio thread aaya). Ek pinned, isolated hot thread pe yeh
   ~0 hona chahiye. Badhna = isolation toota. `voluntary` = tumne khud block
   kiya (`futex`, `read`, `sleep`) — spin-loop design mein woh bhi kam hona
   chahiye par utna alarming nahi.
   </details>

2. `clocksource` `hpet` pe set hai. `clock_gettime` pe kya asar, kaise fix?

   <details><summary>Answer</summary>

   `hpet` ek slow MMIO device — `clock_gettime` vDSO se bhi ~500–1000 ns
   (vs `tsc` pe ~20 ns). Fix: `echo tsc > .../current_clocksource` (agar CPU
   mein `constant_tsc` + `nonstop_tsc` flags hain — `/proc/cpuinfo` check).
   Persist: kernel cmdline `clocksource=tsc tsc=reliable`.
   </details>

3. Container ko cgroup ne 2 CPUs diye. `std::thread::hardware_concurrency()`
   16 return karta hai. Thread pool kitne threads banaye?

   <details><summary>Answer</summary>

   2 (ya 2 ke aas-paas), 16 nahi. `hardware_concurrency()` host ka count deta.
   `sched_getaffinity(0, ...)` se allowed CPU count, ya cgroup v2
   `/sys/fs/cgroup/cpu.max` (quota/period) padho. 16 threads on 2 cores =
   heavy context switching, throughput girega.
   </details>

4. NIC ka `rx_dropped` badh raha hai burst ke dauraan. Do possible wajah + fix.

   <details><summary>Answer</summary>

   (1) Socket/NIC ring buffers chhote — burst mein overflow. Fix:
   `net.core.rmem_max`/`SO_RCVBUF` badhao, `ethtool -G eth0 rx <N>` se NIC ring
   badhao. (2) Jo core packet process kar raha (softirq / app thread) busy /
   preempted — kernel enqueue nahi kar pa raha. Fix: IRQ + app thread alag
   dedicated cores pe, `net.core.netdev_max_backlog` badhao.
   </details>

5. `echo performance > scaling_governor` ke baad bhi p99.9 latency mein µs
   spikes. Ek aur `/sys`/cmdline knob jo dekhoge.

   <details><summary>Answer</summary>

   C-states. Deep idle states (C3/C6) se wakeup ~few µs leta. Ek core jo kabhi
   idle nahi jaata (busy-poll) to theek, par event-driven thread pe spikes.
   `cpupower idle-set -D 0`, ya cmdline `processor.max_cstate=1
   intel_idle.max_cstate=1 idle=poll`. Bonus: `intel_pstate` turbo transitions
   → `no_turbo` ya fixed freq for consistency.
   </details>

---

## Interview questions

1. `/proc` vs `/sys` — dono kya expose karte, structure ka farak?
2. `sysctl` = `/proc/sys` — runtime change persist kaise?
3. 4 HFT-relevant sysctl/sysfs knobs aur unka reason.
4. Container mein `/proc/cpuinfo` galat count kyun, sahi count kahan se?
5. `voluntary` vs `nonvoluntary` context switches — HFT ke liye kya batate?
6. Cache line size runtime pe kaise pata (`/sys/.../coherency_line_size`)?
7. `irqbalance` HFT box pe kyun disable?

---

## Next
→ [`07-mmap.md`](07-mmap.md)
