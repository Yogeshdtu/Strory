# 17 — cgroups, ulimit, resource isolation

## Prerequisites
- `11-cpu-affinity.md` (cpuset, isolation)
- `13-page-faults-and-mlock.md` (RLIMIT_MEMLOCK)
- `10-cpu-scheduling.md` (scheduling classes)

## Yeh topic abhi kyun
cgroups + rlimits = "ek process (ya group of processes) kitne resources use kar
sakta". Yeh do tarah relevant hai: (1) **defensive** — non-critical workloads
(logging, monitoring, research jobs) ko box karo taaki woh trading process ke
resources na kha jaayein; (2) **awareness** — agar tumhara trading process khud
ek container/cgroup mein chal raha hai, to `hardware_concurrency()`,
`/proc/cpuinfo`, memory queries jhoot bol sakti hain (`06`). systemd ke andar
sab kuch cgroups pe hai — jaan lena zaroori.

---

## `ulimit` / `RLIMIT_*` — per-process limits

Per-process (child inherit karte). `bash`: `ulimit -a`. Code: `getrlimit` /
`setrlimit`. Soft limit (current, badha sakte up to hard) + hard limit (ceiling,
sirf root badhata).

| RLIMIT | `ulimit` | HFT-relevant kyun |
|---|---|---|
| `RLIMIT_MEMLOCK` | `-l` | `mlockall` ke liye — default 64 KiB useless; `unlimited` chahiye (`13`) |
| `RLIMIT_NOFILE` | `-n` | many sockets/fds — 1024 default too low, raise to 1M (`05`) |
| `RLIMIT_RTPRIO` | `-r` | `SCHED_FIFO` priority max jo unprivileged thread set kar sake (`10`) |
| `RLIMIT_RTTIME` | `-R` | ek RT thread continuous CPU bina blocking → `SIGXCPU` (runaway guard) |
| `RLIMIT_NICE` | `-e` | `nice` value floor |
| `RLIMIT_STACK` | `-s` | thread stack size (pre-fault, `13`) |
| `RLIMIT_CORE` | `-c` | core dump size — crash diagnostics (`04`) |
| `RLIMIT_AS` / `RLIMIT_DATA` | `-v` / `-d` | virtual memory ceiling — sparse mappings pe careful |

```cpp
struct rlimit rl;
getrlimit(RLIMIT_NOFILE, &rl);
rl.rlim_cur = rl.rlim_max;               // soft ko hard tak
setrlimit(RLIMIT_NOFILE, &rl);
```

**systemd** service unit se (recommended — process ko khud raise nahi karna
padta):
```ini
[Service]
LimitMEMLOCK=infinity
LimitNOFILE=1048576
LimitRTPRIO=99
LimitCORE=infinity
```

---

## cgroups v2 — hierarchical resource control

Single unified hierarchy at `/sys/fs/cgroup/`. Har directory ek cgroup;
processes `cgroup.procs` mein; controllers (`cpu`, `memory`, `io`, `cpuset`,
`pids`) parent se enable hote (`cgroup.subtree_control`).

```
/sys/fs/cgroup/
  cpu.max                 "max 100000"  (quota_us period_us) -> 1 core
  memory.max              "8G"
  memory.high             "6G"          (soft -- throttle, reclaim pressure)
  cpuset.cpus             "4-7"
  cpuset.mems             "0"
  pids.max               "512"
  io.max                 "259:0 rbps=104857600"
  system.slice/          (systemd: daemons)
  user.slice/            (user sessions)
  hot.slice/            (tumhara trading -- ya isolated cpuset)
```

### Key knobs

| File | Meaning |
|---|---|
| `cpu.max` | `"$QUOTA $PERIOD"` µs — e.g. `"200000 100000"` = 2 CPUs worth. `"max"` = unlimited |
| `cpu.weight` | 1–10000, relative share when contended (cgroup-level `nice`) |
| `cpuset.cpus` / `cpuset.mems` | allowed CPUs / NUMA nodes (hard) |
| `memory.max` | hard limit — exceed → OOM-kill within the cgroup |
| `memory.high` | soft — reclaim + throttle before hitting `max` |
| `memory.min` / `memory.low` | protected memory (won't be reclaimed) |
| `io.max` / `io.weight` | block-IO bandwidth / IOPS caps |
| `pids.max` | fork-bomb guard |

### HFT usage pattern

- Trading process: **NOT** in a restrictive cgroup — ya to root/`hot.slice` with
  `cpu.max=max`, `memory.max=max`, aur `cpuset.cpus` = isolated hot cores; ya
  outside cgroup constraints entirely, pinned via `isolcpus` (`11`).
- Everything else in `system.slice` restricted to **housekeeping cores**
  (`cpuset.cpus=0-1`), memory-capped, `cpu.weight` low. Yeh trading process ko
  contention se bachata.
- `cpuset` cgroup = dynamic alternative to `isolcpus` (`11`) — but combine with
  `nohz_full` (boot-time).

---

## Running inside a container/cgroup: the awareness problem

Agar tumhara process ek cgroup (Docker/k8s/systemd) mein chal raha jisne 4 CPUs
diye:

| Query | Returns | Sahi source |
|---|---|---|
| `std::thread::hardware_concurrency()` | **host** count (e.g. 64) | `sched_getaffinity` count, ya `cpu.max` quota/period |
| `/proc/cpuinfo` | all host cores | `/sys/fs/cgroup/cpuset.cpus` |
| `sysconf(_SC_NPROCESSORS_ONLN)` | host (glibc) | as above |
| `/proc/meminfo` `MemTotal` | host RAM | `/sys/fs/cgroup/memory.max` |
| `free` | host | cgroup memory.* |

**Thread pool sizing:** `hardware_concurrency()` pe blindly N threads banaye →
64 threads on 4 allowed CPUs → massive context switching, throughput tank.
Read `sched_getaffinity` (allowed CPU count) **and** `cpu.max` (quota); use
`min`. Java/Go runtimes yeh detection karte; C++ mein tumhe khud.

```cpp
long allowed_cpus() {
    cpu_set_t set; CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof(set), &set) == 0) return CPU_COUNT(&set);
    return sysconf(_SC_NPROCESSORS_ONLN);
}
// aur cpu.max quota: "/sys/fs/cgroup/cpu.max" -> "50000 100000" => 0.5 CPU
```

---

## Internal working

- cgroup v2: har controller ek per-cgroup accounting + enforcement struct.
  `cpu.max` = CFS bandwidth control — har `period` mein cgroup ke tasks ko max
  `quota` CPU-µs; exceed → **throttled** (runqueue se hata, next period tak).
  Yeh throttling ek **latency spike** source hai agar quota tight (`cpu.stat`
  → `nr_throttled`, `throttled_usec`).
- `memory.max` hit → in-cgroup direct reclaim (slow path) → phir OOM-kill
  (cgroup ke andar highest-badness task). Host baaki fine.
- `cpuset` — same mechanism as `sched_setaffinity` but applied to a whole
  cgroup subtree, inherited.
- systemd har service/scope/slice ko ek cgroup deta; `systemctl set-property`
  se runtime, unit file se persistent.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — trading process ko `cpu.max` quota ke andar chalana
CFS bandwidth throttling: quota khatam → poore cgroup ke tasks next period tak
frozen (up to ~100 ms). Random, brutal latency spike. Trading process ko
**`cpu.max=max`** + `cpuset` isolation, quota nahi.

### Trap 2 — `hardware_concurrency()` se thread pool size
Container mein host count milta. 64 threads / 4 cores = thrash. `sched_getaffinity`
+ `cpu.max` se real budget.

### Trap 3 — `RLIMIT_MEMLOCK` default chhod dena
64 KiB → `mlockall` fail (`13`). systemd `LimitMEMLOCK=infinity` ya
`limits.conf`.

### Trap 4 — `RLIMIT_NOFILE` 1024 pe chhodna
Multi-venue + market data + IPC → `EMFILE` under load. Raise to 1M.

### Trap 5 — `memory.high` ko hard limit samajhna
`memory.high` throttle karta (reclaim pressure, allocations slow) par kill nahi.
`memory.max` kills. Latency box pe dono se door raho (headroom rakho); agar cap
chahiye to `max` set karo aur monitor.

### Trap 6 — cgroup v1 vs v2 confusion
Purane systems v1 (`/sys/fs/cgroup/cpu/`, `/sys/fs/cgroup/memory/` alag
hierarchies), naye v2 (unified). `mount | grep cgroup`, `stat -fc %T
/sys/fs/cgroup` (`cgroup2fs` = v2). API/knob names alag.

### Trap 7 — nested cgroup limits multiply
Parent cgroup `cpu.max` 2 CPU, child bhi 2 CPU — effective still ≤ parent's 2.
k8s pod → container → process: har level cap karta; effective = tightest.

---

## > **HFT relevance**

> - **Trading process: unconstrained CPU/memory, isolated cores.** No `cpu.max`
>   quota (throttle = spike). `cpuset.cpus` = the isolated hot cores (or
>   `isolcpus` + pin, `11`). `memory.max=max` with monitoring, not a tight cap.
> - **Fence off the noise:** `system.slice` (logging, monitoring, cron, SSH,
>   package updates) → `cpuset.cpus=0-1` (housekeeping), `cpu.weight=10` (low),
>   `memory.high` set. They can't steal the hot cores or RAM.
> - **Research/backtest jobs** → their own slice on the *other* socket
>   (`14`), memory + IO capped, `cpu.weight` low, maybe `SCHED_IDLE`.
> - **systemd unit** for the trader: `LimitMEMLOCK=infinity`,
>   `LimitNOFILE=1048576`, `LimitRTPRIO=99`, `CPUAffinity=` (or leave to the
>   app), `Slice=hot.slice`, `OOMScoreAdjust=-1000` (last to be OOM-killed).
> - **Container-awareness:** if the trader runs in a container, its startup must
>   compute the CPU/memory budget from `sched_getaffinity` + `cpu.max` +
>   `memory.max`, not from `/proc/cpuinfo` / `hardware_concurrency()`.

---

## Hands-on

```bash
# v1 ya v2?
stat -fc %T /sys/fs/cgroup            # cgroup2fs = v2
mount | grep -E 'cgroup'

# apna cgroup
cat /proc/self/cgroup                 # "0::/user.slice/..." (v2)
cat /sys/fs/cgroup/$(awk -F: '{print $3}' /proc/self/cgroup)/cpu.max 2>/dev/null

# throttling ho raha? (quota-limited cgroup mein)
cat /sys/fs/cgroup/.../cpu.stat        # nr_throttled, throttled_usec

# ek noisy daemon ko housekeeping cores + low weight
systemctl set-property some-daemon.service AllowedCPUs=0-1 CPUWeight=10 MemoryHigh=1G

# rlimits
ulimit -a
prlimit --pid $(pidof trader)

# container-aware CPU budget
python3 - <<'EOF'
import os
aff=len(os.sched_getaffinity(0))
try:
    q,p=open('/sys/fs/cgroup/cpu.max').read().split()
    quota = aff if q=='max' else max(1, int(int(q)/int(p)))
except Exception: quota=aff
print("allowed cpus:", aff, " cpu.max budget:", quota, " use:", min(aff,quota))
EOF
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| `hardware_concurrency()` = usable cores | container mein host count; use affinity + `cpu.max` |
| `cpu.max` quota trading ke liye safe | CFS bandwidth throttle = up to ~100ms freeze |
| `memory.high` = OOM limit | `high` throttles; `max` kills |
| `ulimit` change permanent | shell session only; systemd unit / `limits.conf` |
| cgroup v1 aur v2 same knobs | alag hierarchies + names; check which |
| nested limits add up | effective = tightest ancestor |

---

## Exercises

1. Trading process ek k8s pod mein `resources.limits.cpu: "2"` ke saath. p99.9
   mein har few seconds ~80 ms spike. Sabse likely wajah?

   <details><summary>Answer</summary>

   CFS bandwidth throttling. `cpu: "2"` → `cpu.max = "200000 100000"` (200ms
   quota per 100ms period → 2 CPUs). Agar process (its threads combined) 2
   CPU-seconds/second se thoda bhi upar jaaye (bursts) → quota khatam → poora
   cgroup next period tak throttled = up to ~100 ms freeze. `cpu.stat` →
   `nr_throttled` non-zero confirm karega. Fix: no CPU limit (`requests` only)
   for the latency-critical pod, isolated cores, `static` CPU manager policy.
   </details>

2. `mlockall` `-1`/`ENOMEM` de raha hai. `ulimit -l` = `64`. Fix (2 ways).

   <details><summary>Answer</summary>

   (1) systemd service: `LimitMEMLOCK=infinity` in the unit `[Service]` section,
   `systemctl daemon-reload` + restart. (2) `/etc/security/limits.conf` (PAM):
   `trader-user - memlock unlimited`, re-login. (3) Transient: run with
   `systemd-run -p LimitMEMLOCK=infinity ...`, or as root (`CAP_IPC_LOCK`), or
   `prlimit --memlock=unlimited:unlimited --pid <pid>`.
   </details>

3. Ek research backtest job accidentally trading box pe chala aur trading p99
   bigad gaya. cgroup se kaise prevent karo future mein?

   <details><summary>Answer</summary>

   `research.slice` banao: `AllowedCPUs=` sirf non-hot cores (ideally other
   socket, `14`), `CPUWeight=1` (lowest), `MemoryHigh=<small>` +
   `MemoryMax=<cap>`, `IOWeight=1`, optionally `CPUSchedulingPolicy=idle`
   (`SCHED_IDLE`). Sab research/dev jobs is slice mein launch (enforce via
   login/job scheduler). Trader `hot.slice` mein, `OOMScoreAdjust=-1000`. Ab
   research job hot cores ko chhoo hi nahi sakta.
   </details>

4. `cat /proc/self/cgroup` shows `0::/`. cgroup v1 ya v2, aur process kaha hai?

   <details><summary>Answer</summary>

   `0::/` = cgroup **v2** (single entry, hierarchy-id 0, no controller name),
   aur process root cgroup mein hai (no restrictions from cgroup). v1 hota to
   multiple lines like `4:cpu,cpuacct:/...`, `6:memory:/...`. `stat -fc %T
   /sys/fs/cgroup` = `cgroup2fs` confirms v2.
   </details>

5. systemd unit mein `CPUAffinity=2-9` set kiya, par app khud bhi
   `sched_setaffinity` karta hai per-thread. Conflict?

   <details><summary>Answer</summary>

   `CPUAffinity=` (via `cpuset`) ek **hard ceiling** — app cheeh se sirf us set
   ke andar hi apni per-thread affinity set kar sakta. `sched_setaffinity(CPU
   15)` jab cgroup 2–9 allow karta → `EINVAL` ya silently masked to the
   intersection (empty → error). Best: unit se `CPUAffinity=` (or `isolcpus`)
   define the pool, app se within-pool per-thread pinning. Ensure the app's
   hardcoded core numbers are inside the unit's set (better: app reads its
   allowed set at startup and picks from it).
   </details>

---

## Interview questions

1. `RLIMIT_MEMLOCK` / `RLIMIT_NOFILE` / `RLIMIT_RTPRIO` — HFT ke liye kyun raise.
2. cgroups v2 — `cpu.max` (bandwidth) vs `cpu.weight` (share).
3. CFS bandwidth throttling — latency spike kaise deta, trading process pe kyun avoid.
4. `memory.high` vs `memory.max` — behaviour ka farak.
5. Container mein `hardware_concurrency()` galat kyun, sahi budget kahan se.
6. Noisy neighbours ko cgroup se kaise fence karte ho.
7. systemd unit ke resource directives (`Limit*`, `AllowedCPUs`, `OOMScoreAdjust`).

---

## Next
→ [`18-hft-tuning-checklist.md`](18-hft-tuning-checklist.md)
