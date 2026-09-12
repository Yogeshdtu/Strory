# 18 — HFT production tuning checklist

## Prerequisites
- Poora folder 29 (`01`–`17`)

## Yeh topic abhi kyun
Yeh folder ke saare knobs ko ek jagah, ek deployable checklist mein. CLAUDE.md
ka spec: "poora production tuning checklist — kernel params, BIOS, boot flags."
Har item ke saath: kya, kyun, aur kaunse lesson mein detail hai. Yeh reference
hai — deploy karte waqt tick karo, `dmesg`/`/proc`/`/sys` se verify karo, aur
har change ke baad **measure karo** (folder `35`).

> ⚠️ Har box alag hai. Yeh starting point hai, gospel nahi. Har knob ka apna
> trade-off hai (power, security, throughput). Change one thing, measure,
> keep or revert.

---

## 0. BIOS / firmware (reboot required)

| Setting | Value | Kyun |
|---|---|---|
| Hyper-Threading / SMT | **often OFF** (ya on + siblings idle, `11`) | sibling contention → IPC jitter |
| Turbo Boost | OFF, or fixed all-core freq | turbo transitions = frequency jitter |
| C-states (package/core) | C1 max (disable C3/C6) | deep idle wakeup ~µs (`06`) |
| P-states / SpeedStep | disabled / fixed | frequency scaling latency |
| Power profile | "Max Performance" / "Latency Optimized" | vendor bundles the above |
| NUMA | enabled (node-per-socket), Sub-NUMA Clustering per policy | predictable topology (`14`) |
| Prefetchers | ON (usually) — test | rarely helps to disable |
| SR-IOV / VT-d | ON if kernel-bypass NIC (`30/13`) | VFIO passthrough |
| Watchdog timers | OFF | periodic NMI/SMI |

**SMIs** (System Management Interrupts) — firmware-level, invisible to the OS,
can stall ALL cores for 10s–100s of µs. Check with `hwlatdetect` /
`turbostat --show SMI`. Bad BIOS/firmware → update or disable SMI sources
(fan control quirks, memory scrubbing).

---

## 1. Kernel boot cmdline

`/etc/default/grub` → `GRUB_CMDLINE_LINUX`, then `update-grub`, reboot.

```
isolcpus=2-9,22-29
nohz_full=2-9,22-29
rcu_nocbs=2-9,22-29
rcu_nocb_poll
irqaffinity=0-1,20-21
nosmt                       # if disabling SMT via kernel instead of BIOS
processor.max_cstate=1
intel_idle.max_cstate=1
idle=poll                   # aggressive: housekeeping cores never deep-idle (power cost)
clocksource=tsc
tsc=reliable
nmi_watchdog=0
mce=ignore_ce               # optional: skip corrected-error logging storms
transparent_hugepage=madvise
default_hugepagesz=2M hugepagesz=2M hugepages=8192   # 16 GiB pool
audit=0                     # if no auditd requirement
mitigations=off             # ONLY on isolated, non-internet-facing, trusted boxes
skew_tick=1
```

Verify after reboot:
```bash
cat /proc/cmdline
cat /sys/devices/system/cpu/isolated          # 2-9,22-29
cat /sys/devices/system/cpu/nohz_full
cat /sys/devices/system/clocksource/clocksource0/current_clocksource   # tsc
grep -o 'mitigations[^ ]*' /proc/cmdline
```

| Param | Lesson |
|---|---|
| `isolcpus`, `nohz_full`, `rcu_nocbs`, `irqaffinity` | `11`, `15` |
| `*.max_cstate`, `idle=poll` | `06` |
| `clocksource=tsc`, `tsc=reliable` | `16` |
| `transparent_hugepage`, `hugepages=` | `12` |
| `mitigations=off` | `02` |

---

## 2. sysctl (`/etc/sysctl.d/99-hft.conf`, then `sysctl --system`)

```ini
# --- memory ---
vm.swappiness = 1
vm.zone_reclaim_mode = 0
vm.max_map_count = 1048576          # many mmaps
kernel.numa_balancing = 0          # (14)
vm.stat_interval = 120             # less vmstat overhead

# --- scheduler ---
kernel.sched_rt_runtime_us = -1    # (10) -- ONLY with isolated, well-behaved RT threads
kernel.sched_autogroup_enabled = 0
kernel.sched_min_granularity_ns = 10000000
kernel.sched_wakeup_granularity_ns = 15000000

# --- network (kernel path; less relevant if bypass) ---
net.core.rmem_max = 134217728
net.core.wmem_max = 134217728
net.core.netdev_max_backlog = 250000
net.core.busy_poll = 50            # (30/11)
net.core.busy_read = 50
net.ipv4.tcp_timestamps = 1
net.ipv4.tcp_sack = 1
net.ipv4.udp_mem = 8388608 12582912 16777216

# --- misc ---
kernel.perf_event_paranoid = 0     # dev boxes; -1 for full perf access
kernel.watchdog = 0
kernel.hung_task_timeout_secs = 0
```

Verify: `sysctl -a | grep -E 'swappiness|numa_balancing|sched_rt_runtime'`.

---

## 3. Runtime setup (systemd unit / provisioning script — no reboot)

### CPU / power
```bash
# governor: performance on all cores
for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do echo performance > $g; done
# disable deep C-states at runtime
cpupower idle-set -D 0
# turbo off (Intel) for consistency
echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo
```

### THP defrag
```bash
echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
echo defer   > /sys/kernel/mm/transparent_hugepage/defrag
echo 0       > /sys/kernel/mm/transparent_hugepage/khugepaged/defrag
```

### IRQs (`15`)
```bash
systemctl disable --now irqbalance
for irq in $(grep -E 'eth0|ens' /proc/interrupts | awk -F: '{print $1}' | tr -d ' '); do
    echo 0-1 > /proc/irq/$irq/smp_affinity_list 2>/dev/null
done
# RPS off for hot NIC queues (softirq stays on housekeeping)
for q in /sys/class/net/eth0/queues/rx-*/rps_cpus; do echo 0 > $q; done
```

### NIC (`15`, `30/15`)
```bash
ethtool -C eth0 adaptive-rx off adaptive-tx off rx-usves 0 rx-frames 1 tx-usves 0
ethtool -G eth0 rx 4096 tx 4096                  # big rings
ethtool -K eth0 gro off lro off                  # no coalescing/aggregation (latency)
ethtool -A eth0 rx off tx off                    # disable pause frames (debatable)
ethtool -L eth0 combined 4                       # RSS queues = housekeeping core count
```

### cgroups (`17`)
```bash
# fence the noise onto housekeeping cores
systemctl set-property system.slice AllowedCPUs=0-1,20-21 CPUWeight=20
systemctl set-property user.slice   AllowedCPUs=0-1,20-21 CPUWeight=10
```

### Misc jitter sources
```bash
# stop cron, updates, mandb, mlocate, tuned's dynamic tuning, etc. on trading hosts
systemctl disable --now cron atd man-db.timer plocate-updatedb.timer unattended-upgrades
# kernel: disable the kernel samepage merging scanner
echo 0 > /sys/kernel/mm/ksm/run
# disable NMI watchdog at runtime
echo 0 > /proc/sys/kernel/nmi_watchdog
# writeback: keep dirty pages small so flush bursts are small
echo 5 > /proc/sys/vm/dirty_ratio ; echo 2 > /proc/sys/vm/dirty_background_ratio
```

---

## 4. systemd unit for the trader

```ini
[Unit]
Description=Trading Engine
After=network-online.target

[Service]
Type=simple
ExecStart=/opt/trader/bin/engine --config /etc/trader/engine.toml
# resources
LimitMEMLOCK=infinity
LimitNOFILE=1048576
LimitRTPRIO=99
LimitCORE=infinity
LimitRTTIME=infinity
# placement
CPUAffinity=2-9 22-29
NUMAPolicy=bind
NUMAMask=0
Slice=hot.slice
# survival
OOMScoreAdjust=-1000
Restart=on-failure
RestartSec=1
# no cgroup CPU/mem throttle
CPUQuota=
MemoryMax=infinity

[Install]
WantedBy=multi-user.target
```

---

## 5. Application-side (folder `13`, `11`, `16` — engine code)

- `mlockall(MCL_CURRENT | MCL_FUTURE)` at startup.
- Allocate every pool / arena / ring / lookup table at full size; `memset` /
  `MAP_POPULATE` every page (pre-fault).
- `mmap(MAP_HUGETLB)` (or hugetlbfs files) for the big structures.
- Pin each hot thread to a dedicated **physical** core, read from topology at
  startup; assert the core is in `/sys/devices/system/cpu/isolated` and its SMT
  sibling is unused. **Hard-fail if not.**
- Bind memory to the pipeline's NUMA node (`mbind` / `numa_set_membind`).
- Calibrate `cycles_per_ns` (`rdtscp` vs `CLOCK_MONOTONIC`); use `CLOCK_MONOTONIC`
  (vDSO) or calibrated `rdtscp` for timestamps.
- Block all signals on hot threads; handle via `signalfd` on a control thread
  (`04`).
- No `malloc` / `new` / `mutex` / syscall / `mmap` / logging on the hot path.
  Async logging via a lock-free ring + a housekeeping-core writer thread.
- Dry-run the full pipeline N thousand times before "go live" (warm `.text`,
  branch predictors, allocator, containers).
- Self-monitor: `ru_majflt` (must stay 0), `nonvoluntary_ctxt_switches` (~0 on
  hot threads), `/proc/interrupts` diff on hot cores, `cpu.stat` `nr_throttled`,
  TSC frequency drift.

---

## 6. Verify / measure (folder `35` deep-dives this)

```bash
# jitter / latency of the OS itself
hwlatdetect --duration=60          # SMI / hardware-induced stalls
cyclictest -m -p 99 -t 8 -a 2-9 -i 200 -D 60   # scheduling latency on isolated cores
turbostat --interval 5             # actual freq, C-state residency, SMI count

# per-thread
grep -E 'ctxt_switches|VmLck' /proc/$(pidof engine)/task/*/status
cat /proc/$(pidof engine)/task/*/stat | awk '{print $1, $39}'   # tid, current CPU (stable?)

# faults
/usr/bin/time -v ./engine 2>&1 | grep -i 'page fault'

# interrupts on hot cores (should be ~flat)
cat /proc/interrupts > /tmp/a; sleep 10; cat /proc/interrupts > /tmp/b; diff /tmp/a /tmp/b

# perf: cache/TLB/branch behaviour of the hot loop
perf stat -e cycles,instructions,cache-misses,dTLB-load-misses,branch-misses -p $(pidof engine) sleep 10
```

**Acceptance rough targets** (box + workload dependent): `cyclictest` max on
isolated cores < ~10 µs; hot-thread `nonvoluntary_ctxt_switches` = 0 over an
hour; `ru_majflt` = 0; hot-core IRQ counts flat; loop-time p99.9 within a few %
of p50.

---

## ⚠️ Traps

### Trap 1 — sab knobs ek saath, bina measure
Kuch help karte, kuch nothing, kuch **regress** (e.g. `idle=poll` on
housekeeping cores wastes power + heat → thermal throttle → worse). Change,
measure, keep/revert.

### Trap 2 — `mitigations=off` blindly
Sirf isolated, non-internet-facing, single-tenant, trusted-code boxes. Corporate
policy / shared infra pe forbidden. Big syscall/ctx-switch win (`02`), real
security cost.

### Trap 3 — `idle=poll` everywhere
Hot cores already busy-poll (app-level). `idle=poll` kernel-forces housekeeping
cores to spin too → power + heat + possible turbo/thermal effects on the whole
package. Scope it or skip it.

### Trap 4 — `sched_rt_runtime_us=-1` without isolation
Runaway RT thread with no CFS reservation → kernel threads (RCU, workqueue)
starve → hangs. Only with isolated cores + well-behaved RT (`10`).

### Trap 5 — tuning drift
`irqbalance` re-enabled by an update; a new kernel resets `isolcpus` naming;
`tuned` profile overrides your sysctls; cloud image regenerates grub. Make it
**declarative + idempotent** (Ansible/config-mgmt), re-assert on boot, and
**alert on drift** (a monitor that re-checks every item).

### Trap 6 — verifying config, not effect
`cat /proc/cmdline` shows `isolcpus` — but a stray thread is still on core 5.
Verify the **outcome** (`cyclictest`, ctx-switch counts, IRQ diffs), not just
that the string is set.

---

## > **HFT relevance**

> This whole lesson is the HFT relevance. One framing to keep: **you are moving
> every cost off the critical path and making what remains deterministic.**
> BIOS/cmdline kills hardware/kernel-induced stalls (SMI, C-states, ticks,
> IRQs). sysctl + cgroups keep the OS and neighbours off the hot cores.
> Application-side warming + pinning + no-syscall discipline makes the steady
> state fault-free and preemption-free. Then you *measure* — `cyclictest`,
> `perf`, self-monitoring — because an unverified tuning is a guess, and a
> regressed tuning is worse than none.

---

## Hands-on

Ek chhota `verify.sh` likho jo har section ka ek key item check kare aur
PASS/FAIL de:

```bash
#!/usr/bin/env bash
chk(){ printf '%-40s ' "$1"; shift; eval "$@" && echo PASS || echo FAIL; }
chk "clocksource = tsc"        '[ "$(cat /sys/devices/system/clocksource/clocksource0/current_clocksource)" = tsc ]'
chk "isolcpus set"             'grep -q isolcpus /proc/cmdline'
chk "nohz_full set"            '[ -s /sys/devices/system/cpu/nohz_full ]'
chk "irqbalance disabled"      '! systemctl is-active --quiet irqbalance'
chk "swappiness <= 1"          '[ "$(cat /proc/sys/vm/swappiness)" -le 1 ]'
chk "numa_balancing off"       '[ "$(cat /proc/sys/kernel/numa_balancing)" = 0 ]'
chk "THP not always"           '! grep -q "\[always\]" /sys/kernel/mm/transparent_hugepage/enabled'
chk "hugepages reserved"       '[ "$(cat /proc/sys/vm/nr_hugepages)" -gt 0 ]'
chk "governor = performance"   'grep -qx performance /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor'
chk "nmi_watchdog off"         '[ "$(cat /proc/sys/kernel/nmi_watchdog)" = 0 ]'
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "checklist apply = tuned box" | verify the *effect* (cyclictest/perf), not the strings |
| "`mitigations=off` is standard HFT" | only isolated, trusted, non-internet boxes |
| "more knobs = faster" | some do nothing, some regress; measure each |
| "set once, forever" | updates/tuned/cloud images drift it; declarative + alerting |
| "`idle=poll` everywhere helps" | hot cores already spin; forcing housekeeping = power/heat |
| "RT + `rt_runtime=-1` always" | needs isolation + well-behaved threads, else hangs |

---

## Exercises

1. Checklist poora apply kiya, `cyclictest` max abhi bhi ~120 µs. cmdline mein
   sab set hai. Kya dekhoge?

   <details><summary>Answer</summary>

   (1) SMI/firmware — `turbostat --show SMI` / `hwlatdetect`. Firmware stall
   `isolcpus` se nahi rukta — BIOS update / disable fan-control quirk / memory
   patrol scrub. (2) Ek stray thread hot core pe — `ps -eLo psr,comm | awk
   '$1>=2 && $1<=9'`. (3) C-states actually disabled? `turbostat` C-state
   residency — agar hot core C6 mein ja raha, `idle=poll` ya `cpupower idle-set
   -D 0`. (4) Turbo/thermal — sustained load pe freq drop? `turbostat` Bzy_MHz.
   (5) `tuned` daemon overriding — `tuned-adm active`, disable.
   </details>

2. `mitigations=off` set karne se pehle 3 preconditions.

   <details><summary>Answer</summary>

   (a) Box isolated / not internet-facing / not shared-tenant — no untrusted
   code ever runs. (b) Corporate/regulatory security policy allows it (many
   don't). (c) You've measured the actual gain on *your* workload (syscall +
   ctx-switch heavy → big; pure busy-poll no-syscall hot path → small) and it's
   worth the risk. Also: document it, and have compensating controls (network
   isolation, host firewall, no user logins).
   </details>

3. `nohz_full=2-9` set, par core 3 pe timer interrupts (`LOC`) abhi bhi aa rahe.
   Kyun?

   <details><summary>Answer</summary>

   `nohz_full` core pe tick tabhi rukti jab `nr_running <= 1`. Core 3 pe do (ya
   zyada) runnable threads pinned hain → tick chaalu. Ya: `rcu_nocbs` set nahi →
   RCU ko periodic tick chahiye us core pe. Ya: `nohz_full` ke liye kernel
   `CONFIG_NO_HZ_FULL` compiled nahi. Fix: ek core = ek hot thread; add
   `rcu_nocbs=2-9`; verify `cat /sys/devices/system/cpu/nohz_full`.
   </details>

4. Tuning "drift" se kaise bacho production mein?

   <details><summary>Answer</summary>

   (1) Declarative + idempotent config management (Ansible/Salt) — har boot /
   har hour re-apply, converge to desired state. (2) `verify.sh`-style monitor
   that re-checks every item and alerts on any FAIL (irqbalance re-enabled,
   THP flipped to always, isolcpus missing after kernel upgrade, tuned
   overriding sysctls). (3) Immutable images where possible (build the tuned
   image, don't mutate live). (4) Kernel-upgrade runbook (isolcpus core
   numbering, cmdline regeneration). (5) `tuned-adm off` or a custom locked
   profile.
   </details>

5. Ek naya HFT box mila. Pehle 3 cheezein measure/check karoge (before any
   tuning)?

   <details><summary>Answer</summary>

   (1) `hwlatdetect` / `turbostat --show SMI` — baseline hardware/firmware
   jitter. Agar SMIs 100 µs stalls de rahe, koi bhi OS tuning bekaar jab tak
   firmware fix na ho. (2) `lscpu -e` + `numactl -H` + `lspci` NUMA nodes —
   topology samjho (kaunse cores kaunse socket, NIC kis node pe) before
   deciding the layout. (3) `cyclictest` baseline (untuned) — number pe judge
   karne ke liye reference. Phir tune, aur har change ke baad ye teen dobara.
   </details>

---

## Interview questions

1. BIOS-level: SMT, turbo, C-states — HFT ke liye kya aur kyun.
2. Kernel cmdline ke 5 key params aur unka effect.
3. `mitigations=off` — gain, cost, preconditions.
4. sysctl: `swappiness`, `numa_balancing`, `sched_rt_runtime_us` — HFT values.
5. IRQ/NIC tuning: coalescing, RSS queues, `irqbalance`.
6. systemd unit: `LimitMEMLOCK`, `CPUAffinity`, `OOMScoreAdjust`, no `CPUQuota`.
7. "Verify the effect, not the config" — 3 measurement tools (`cyclictest`,
   `turbostat`, `perf stat`, ctx-switch counts).
8. Tuning drift — kaise detect aur prevent.

---

## Next
→ [`19-exercises.md`](19-exercises.md)
